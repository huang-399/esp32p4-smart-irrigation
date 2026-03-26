#include "event_recorder.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <string.h>
#include <time.h>

static const char *TAG = "event_recorder";

#define NVS_NAMESPACE "evt_rec"
#define NVS_KEY_OFFLINE "offline"
#define NVS_KEY_POWERON "poweron"
#define NVS_KEY_CONTROL "control"
#define NVS_KEY_OPERATION "operation"
#define NVS_KEY_ALARM "alarm"
#define NVS_KEY_MANUAL "manual_rec"
#define NVS_KEY_PROGRAM "program_rec"
#define TIME_SYNC_THRESHOLD 1704067200LL  /* 2024-01-01 00:00:00 UTC */
#define FLUSH_TASK_STACK_SIZE 4096
#define FLUSH_TASK_PRIORITY 3
#define FLUSH_POLL_MS 200
#define FLUSH_IDLE_MS 800
#define FLUSH_RETRY_DELAY_MS 1000
#define FLUSH_TASK_NAME "evt_flush"
#define MAX_FLUSH_FAILURE_LOGS 5
#define FLUSH_FAILURE_LOG_INTERVAL 10
/* 实机已确认：event_recorder 触发 NVS/Flash 真正落盘时会引发蓝闪。
 * 当前先固定为“RAM 近期缓存”模式，保留查询与运行期记录能力，
 * 暂不把事件记录异步刷入 NVS，后续再切到 TF 历史归档/更安全的后台持久化策略。 */
#define EVT_RAM_CACHE_ONLY 1
#define EVT_PENDING_FLUSH_BASIC_MASK(type) (1U << (type))
#define EVT_PENDING_FLUSH_MANUAL_BIT       (1U << EVT_TYPE_MANUAL_RECORD)
#define EVT_PENDING_FLUSH_PROGRAM_BIT      (1U << EVT_TYPE_PROGRAM_RECORD)

static TaskHandle_t s_flush_task = NULL;
static volatile uint32_t s_pending_flush_mask = 0;
static volatile bool s_flush_task_stop = false;
static uint32_t s_flush_failure_count = 0;
static TickType_t s_last_dirty_tick = 0;

static esp_err_t schedule_flush_locked(uint32_t pending_bits);
static void flush_task(void *arg);
static esp_err_t flush_pending_stores(void);
static void log_flush_failure(esp_err_t ret);
static bool flush_failure_should_log(uint32_t failure_count);

typedef struct {
    uint16_t count;
    uint16_t write_idx;
    evt_record_t records[EVT_RECORD_MAX];
} evt_store_t;

typedef struct {
    uint16_t count;
    uint16_t write_idx;
    evt_manual_record_t records[EVT_RECORD_MAX];
} evt_manual_store_t;

typedef struct {
    uint16_t count;
    uint16_t write_idx;
    evt_program_record_t records[EVT_RECORD_MAX];
} evt_program_store_t;

static evt_store_t s_basic_stores[EVT_TYPE_ALARM + 1];
static evt_manual_store_t s_manual_store;
static evt_program_store_t s_program_store;
static bool s_basic_dirty[EVT_TYPE_ALARM + 1] = {0};
static bool s_manual_dirty = false;
static bool s_program_dirty = false;
static SemaphoreHandle_t s_mutex = NULL;
static nvs_handle_t s_nvs_handle;
static bool s_initialized = false;
static bool s_legacy_storage_cleared = false;

static const char *s_basic_nvs_keys[EVT_TYPE_ALARM + 1] = {
    NVS_KEY_OFFLINE,
    NVS_KEY_POWERON,
    NVS_KEY_CONTROL,
    NVS_KEY_OPERATION,
    NVS_KEY_ALARM
};

static esp_err_t set_manual_store_blob(void);
static esp_err_t set_program_store_blob(void);
static esp_err_t flush_dirty_stores_locked(void);
static esp_err_t clear_legacy_storage_locked(void);
static esp_err_t clear_legacy_storage_if_needed_locked(void);

static esp_err_t schedule_flush_locked(uint32_t pending_bits)
{
    if (pending_bits == 0) {
        return ESP_OK;
    }

#if EVT_RAM_CACHE_ONLY
    (void)pending_bits;
    return ESP_OK;
#else
    s_pending_flush_mask |= pending_bits;
    s_last_dirty_tick = xTaskGetTickCount();

    if (s_flush_task != NULL) {
        xTaskNotifyGive(s_flush_task);
    }

    return ESP_OK;
#endif
}

static bool flush_failure_should_log(uint32_t failure_count)
{
    return failure_count <= MAX_FLUSH_FAILURE_LOGS ||
           (failure_count % FLUSH_FAILURE_LOG_INTERVAL) == 0;
}

static void log_flush_failure(esp_err_t ret)
{
    s_flush_failure_count++;
    if (flush_failure_should_log(s_flush_failure_count)) {
        ESP_LOGW(TAG, "Async flush failed (%lu): %s",
                 (unsigned long)s_flush_failure_count,
                 esp_err_to_name(ret));
    }
}

static esp_err_t flush_pending_stores(void)
{
    if (s_mutex == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    uint32_t pending_mask = s_pending_flush_mask;
    if (pending_mask == 0) {
        xSemaphoreGive(s_mutex);
        return ESP_OK;
    }

    esp_err_t ret = flush_dirty_stores_locked();
    if (ret == ESP_OK) {
        s_pending_flush_mask &= ~pending_mask;
        if (s_pending_flush_mask == 0) {
            s_last_dirty_tick = 0;
        }
        s_flush_failure_count = 0;
    }

    xSemaphoreGive(s_mutex);
    return ret;
}

static void flush_task(void *arg)
{
    (void)arg;

    while (!s_flush_task_stop) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(FLUSH_POLL_MS));

        while (!s_flush_task_stop) {
            TickType_t pending_age = 0;

            xSemaphoreTake(s_mutex, portMAX_DELAY);
            if (s_pending_flush_mask != 0 && s_last_dirty_tick != 0) {
                pending_age = xTaskGetTickCount() - s_last_dirty_tick;
            }
            xSemaphoreGive(s_mutex);

            if (pending_age == 0) {
                break;
            }

            if (pending_age < pdMS_TO_TICKS(FLUSH_IDLE_MS)) {
                vTaskDelay(pdMS_TO_TICKS(FLUSH_POLL_MS));
                continue;
            }

            esp_err_t ret = flush_pending_stores();
            if (ret == ESP_OK) {
                break;
            }

            log_flush_failure(ret);
            vTaskDelay(pdMS_TO_TICKS(FLUSH_RETRY_DELAY_MS));
        }
    }

    s_flush_task = NULL;
    vTaskDelete(NULL);
}

static bool is_basic_type(evt_type_t type)
{
    return type >= EVT_TYPE_OFFLINE && type <= EVT_TYPE_ALARM;
}

static bool timestamp_matches_range(int64_t ts, int64_t start_ts, int64_t end_ts)
{
    if (start_ts > 0 && end_ts > 0 && ts >= TIME_SYNC_THRESHOLD) {
        if (ts < start_ts || ts > end_ts) {
            return false;
        }
    }

    return true;
}

static bool status_matches_filter(const char *status, evt_status_filter_t filter)
{
    if (filter == EVT_STATUS_FILTER_ALL) {
        return true;
    }

    if (filter == EVT_STATUS_FILTER_NORMAL) {
        return strcmp(status ? status : "", "正常") == 0;
    }

    if (filter == EVT_STATUS_FILTER_ABNORMAL) {
        return status && status[0] != '\0' && strcmp(status, "正常") != 0;
    }

    return true;
}

static esp_err_t load_basic_store(evt_type_t type)
{
    size_t len = sizeof(evt_store_t);
    esp_err_t ret = nvs_get_blob(s_nvs_handle, s_basic_nvs_keys[type], &s_basic_stores[type], &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        memset(&s_basic_stores[type], 0, sizeof(evt_store_t));
        return ESP_OK;
    }
    return ret;
}

static esp_err_t set_basic_store_blob(evt_type_t type)
{
    return nvs_set_blob(s_nvs_handle, s_basic_nvs_keys[type],
                        &s_basic_stores[type], sizeof(evt_store_t));
}

static esp_err_t clear_legacy_storage_locked(void)
{
    if (s_nvs_handle == 0) {
        s_legacy_storage_cleared = true;
        return ESP_OK;
    }

    esp_err_t ret = nvs_erase_all(s_nvs_handle);
    if (ret == ESP_OK) {
        ret = nvs_commit(s_nvs_handle);
    }
    if (ret == ESP_OK) {
        s_legacy_storage_cleared = true;
        memset(s_basic_dirty, 0, sizeof(s_basic_dirty));
        s_manual_dirty = false;
        s_program_dirty = false;
        ESP_LOGI(TAG, "Cleared legacy event NVS after TF archive sync");
    }
    return ret;
}

static esp_err_t clear_legacy_storage_if_needed_locked(void)
{
    if (s_legacy_storage_cleared) {
        return ESP_OK;
    }
    return clear_legacy_storage_locked();
}

static esp_err_t flush_dirty_stores_locked(void)
{
    bool wrote_any = false;

    for (int t = EVT_TYPE_OFFLINE; t <= EVT_TYPE_ALARM; t++) {
        if (!s_basic_dirty[t]) {
            continue;
        }

        esp_err_t ret = set_basic_store_blob((evt_type_t)t);
        if (ret != ESP_OK) {
            return ret;
        }
        wrote_any = true;
    }

    if (s_manual_dirty) {
        esp_err_t ret = set_manual_store_blob();
        if (ret != ESP_OK) {
            return ret;
        }
        wrote_any = true;
    }

    if (s_program_dirty) {
        esp_err_t ret = set_program_store_blob();
        if (ret != ESP_OK) {
            return ret;
        }
        wrote_any = true;
    }

    if (!wrote_any) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_commit(s_nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    memset(s_basic_dirty, 0, sizeof(s_basic_dirty));
    s_manual_dirty = false;
    s_program_dirty = false;
    return ESP_OK;
}

static esp_err_t save_basic_store(evt_type_t type)
{
    s_basic_dirty[type] = true;
    return schedule_flush_locked(EVT_PENDING_FLUSH_BASIC_MASK(type));
}

static esp_err_t load_manual_store(void)
{
    size_t len = sizeof(evt_manual_store_t);
    esp_err_t ret = nvs_get_blob(s_nvs_handle, NVS_KEY_MANUAL, &s_manual_store, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        memset(&s_manual_store, 0, sizeof(s_manual_store));
        return ESP_OK;
    }
    return ret;
}

static esp_err_t set_manual_store_blob(void)
{
    return nvs_set_blob(s_nvs_handle, NVS_KEY_MANUAL,
                        &s_manual_store, sizeof(s_manual_store));
}

static esp_err_t save_manual_store(void)
{
    s_manual_dirty = true;
    return schedule_flush_locked(EVT_PENDING_FLUSH_MANUAL_BIT);
}

static esp_err_t load_program_store(void)
{
    size_t len = sizeof(evt_program_store_t);
    esp_err_t ret = nvs_get_blob(s_nvs_handle, NVS_KEY_PROGRAM, &s_program_store, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        memset(&s_program_store, 0, sizeof(s_program_store));
        return ESP_OK;
    }
    return ret;
}

static esp_err_t set_program_store_blob(void)
{
    return nvs_set_blob(s_nvs_handle, NVS_KEY_PROGRAM,
                        &s_program_store, sizeof(s_program_store));
}

static esp_err_t save_program_store(void)
{
    s_program_dirty = true;
    return schedule_flush_locked(EVT_PENDING_FLUSH_PROGRAM_BIT);
}

static const char *record_type_name(evt_type_t type)
{
    switch (type) {
        case EVT_TYPE_OFFLINE:
            return "offline";
        case EVT_TYPE_POWERON:
            return "poweron";
        case EVT_TYPE_CONTROL:
            return "control";
        case EVT_TYPE_OPERATION:
            return "operation";
        case EVT_TYPE_ALARM:
            return "alarm";
        case EVT_TYPE_MANUAL_RECORD:
            return "manual";
        case EVT_TYPE_PROGRAM_RECORD:
            return "program";
        default:
            return "unknown";
    }
}

static esp_err_t add_record(evt_type_t type, const char *desc)
{
    evt_record_t record = {0};
    time_t now;

    time(&now);
    record.timestamp = (int64_t)now;
    snprintf(record.desc, sizeof(record.desc), "%s", desc ? desc : "");
    return event_recorder_add_basic_record(type, &record);
}

esp_err_t event_recorder_add_basic_record(evt_type_t type, const evt_record_t *record)
{
    if (!s_initialized || !is_basic_type(type) || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    evt_store_t *store = &s_basic_stores[type];
    evt_record_t *rec = &store->records[store->write_idx];

    memset(rec, 0, sizeof(*rec));
    rec->timestamp = record->timestamp;
    snprintf(rec->desc, sizeof(rec->desc), "%s", record->desc);

    store->write_idx = (store->write_idx + 1) % EVT_RECORD_MAX;
    if (store->count < EVT_RECORD_MAX) {
        store->count++;
    }

    esp_err_t ret = save_basic_store(type);

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Added %s record: %s (total: %d)",
             record_type_name(type),
             record->desc,
             store->count);

    return ret;
}

esp_err_t event_recorder_get_basic_records_snapshot(evt_type_t type,
                                                    evt_record_t *records,
                                                    size_t max_records,
                                                    size_t *out_count)
{
    if (!s_initialized || !is_basic_type(type) || !records || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    size_t count = s_basic_stores[type].count;
    if (count > max_records) {
        count = max_records;
    }

    for (size_t i = 0; i < count; i++) {
        int idx = (s_basic_stores[type].write_idx - (int)count + (int)i + EVT_RECORD_MAX) % EVT_RECORD_MAX;
        memcpy(&records[i], &s_basic_stores[type].records[idx], sizeof(evt_record_t));
    }

    xSemaphoreGive(s_mutex);
    *out_count = count;
    return ESP_OK;
}

esp_err_t event_recorder_add_offline(const char *desc)
{
    return add_record(EVT_TYPE_OFFLINE, desc);
}

esp_err_t event_recorder_add_poweron(const char *desc)
{
    return add_record(EVT_TYPE_POWERON, desc);
}

esp_err_t event_recorder_add_control(const char *desc)
{
    return add_record(EVT_TYPE_CONTROL, desc);
}

esp_err_t event_recorder_add_operation(const char *desc)
{
    return add_record(EVT_TYPE_OPERATION, desc);
}

esp_err_t event_recorder_add_alarm(const char *desc)
{
    return add_record(EVT_TYPE_ALARM, desc);
}

esp_err_t event_recorder_add_manual_record(const evt_manual_record_t *record)
{
    if (!s_initialized || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    evt_manual_record_t *dst = &s_manual_store.records[s_manual_store.write_idx];

    memset(dst, 0, sizeof(*dst));
    memcpy(dst, record, sizeof(*dst));

    s_manual_store.write_idx = (s_manual_store.write_idx + 1) % EVT_RECORD_MAX;
    if (s_manual_store.count < EVT_RECORD_MAX) {
        s_manual_store.count++;
    }

    esp_err_t ret = save_manual_store();

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Added manual record: start=%lld planned=%u actual=%u status=%s detail=%s",
             (long long)record->start_ts,
             record->planned_minutes,
             record->actual_minutes,
             record->status,
             record->detail);

    return ret;
}

esp_err_t event_recorder_add_program_record(const evt_program_record_t *record)
{
    if (!s_initialized || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    evt_program_record_t *dst = &s_program_store.records[s_program_store.write_idx];

    memset(dst, 0, sizeof(*dst));
    memcpy(dst, record, sizeof(*dst));

    s_program_store.write_idx = (s_program_store.write_idx + 1) % EVT_RECORD_MAX;
    if (s_program_store.count < EVT_RECORD_MAX) {
        s_program_store.count++;
    }

    esp_err_t ret = save_program_store();

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Added program record: name=%s start=%lld status=%s total=%d",
             record->program_name,
             (long long)record->start_ts,
             record->status,
             s_program_store.count);

    return ret;
}

bool event_recorder_manual_record_equals(const evt_manual_record_t *a, const evt_manual_record_t *b)
{
    if (!a || !b) {
        return false;
    }

    return a->start_ts == b->start_ts &&
           a->planned_minutes == b->planned_minutes &&
           a->actual_minutes == b->actual_minutes &&
           strcmp(a->status, b->status) == 0 &&
           strcmp(a->detail, b->detail) == 0;
}

bool event_recorder_program_record_equals(const evt_program_record_t *a, const evt_program_record_t *b)
{
    if (!a || !b) {
        return false;
    }

    return a->start_ts == b->start_ts &&
           a->planned_minutes == b->planned_minutes &&
           a->actual_minutes == b->actual_minutes &&
           strcmp(a->program_name, b->program_name) == 0 &&
           strcmp(a->trigger, b->trigger) == 0 &&
           strcmp(a->status, b->status) == 0 &&
           strcmp(a->detail, b->detail) == 0;
}

static void dedupe_basic_store(evt_type_t type)
{
    evt_store_t *store = &s_basic_stores[type];
    evt_record_t temp[EVT_RECORD_MAX];
    int temp_count = 0;
    int old_count = store->count;
    int start = (store->count < EVT_RECORD_MAX) ? 0 : store->write_idx;

    for (int i = 0; i < store->count; i++) {
        int idx = (start + i) % EVT_RECORD_MAX;
        bool dup = false;

        for (int j = 0; j < temp_count; j++) {
            if (temp[j].timestamp == store->records[idx].timestamp &&
                strcmp(temp[j].desc, store->records[idx].desc) == 0) {
                dup = true;
                break;
            }
        }

        if (!dup) {
            memcpy(&temp[temp_count++], &store->records[idx], sizeof(evt_record_t));
        }
    }

    if (temp_count < old_count) {
        memset(store->records, 0, sizeof(store->records));
        for (int i = 0; i < temp_count; i++) {
            memcpy(&store->records[i], &temp[i], sizeof(evt_record_t));
        }
        store->count = temp_count;
        store->write_idx = temp_count % EVT_RECORD_MAX;
        ESP_LOGI(TAG, "Deduped %s: %d -> %d records",
                 record_type_name(type), old_count, temp_count);
    }
}

static void dedupe_manual_store(void)
{
    evt_manual_record_t temp[EVT_RECORD_MAX];
    int temp_count = 0;
    int old_count = s_manual_store.count;
    int start = (s_manual_store.count < EVT_RECORD_MAX) ? 0 : s_manual_store.write_idx;

    for (int i = 0; i < s_manual_store.count; i++) {
        int idx = (start + i) % EVT_RECORD_MAX;
        bool dup = false;

        for (int j = 0; j < temp_count; j++) {
            if (event_recorder_manual_record_equals(&temp[j], &s_manual_store.records[idx])) {
                dup = true;
                break;
            }
        }

        if (!dup) {
            memcpy(&temp[temp_count++], &s_manual_store.records[idx], sizeof(evt_manual_record_t));
        }
    }

    if (temp_count < old_count) {
        memset(s_manual_store.records, 0, sizeof(s_manual_store.records));
        for (int i = 0; i < temp_count; i++) {
            memcpy(&s_manual_store.records[i], &temp[i], sizeof(evt_manual_record_t));
        }
        s_manual_store.count = temp_count;
        s_manual_store.write_idx = temp_count % EVT_RECORD_MAX;
        ESP_LOGI(TAG, "Deduped manual records: %d -> %d", old_count, temp_count);
    }
}

static void dedupe_program_store(void)
{
    evt_program_record_t temp[EVT_RECORD_MAX];
    int temp_count = 0;
    int old_count = s_program_store.count;
    int start = (s_program_store.count < EVT_RECORD_MAX) ? 0 : s_program_store.write_idx;

    for (int i = 0; i < s_program_store.count; i++) {
        int idx = (start + i) % EVT_RECORD_MAX;
        bool dup = false;

        for (int j = 0; j < temp_count; j++) {
            if (event_recorder_program_record_equals(&temp[j], &s_program_store.records[idx])) {
                dup = true;
                break;
            }
        }

        if (!dup) {
            memcpy(&temp[temp_count++], &s_program_store.records[idx], sizeof(evt_program_record_t));
        }
    }

    if (temp_count < old_count) {
        memset(s_program_store.records, 0, sizeof(s_program_store.records));
        for (int i = 0; i < temp_count; i++) {
            memcpy(&s_program_store.records[i], &temp[i], sizeof(evt_program_record_t));
        }
        s_program_store.count = temp_count;
        s_program_store.write_idx = temp_count % EVT_RECORD_MAX;
        ESP_LOGI(TAG, "Deduped program records: %d -> %d", old_count, temp_count);
    }
}

esp_err_t event_recorder_init(void)
{
    if (s_initialized) return ESP_OK;

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ret;
    }

    for (int i = EVT_TYPE_OFFLINE; i <= EVT_TYPE_ALARM; i++) {
        ret = load_basic_store((evt_type_t)i);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load store %d: %s", i, esp_err_to_name(ret));
            memset(&s_basic_stores[i], 0, sizeof(evt_store_t));
        }
    }

    ret = load_manual_store();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load manual store: %s", esp_err_to_name(ret));
        memset(&s_manual_store, 0, sizeof(s_manual_store));
    }

    ret = load_program_store();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load program store: %s", esp_err_to_name(ret));
        memset(&s_program_store, 0, sizeof(s_program_store));
    }

    s_flush_task_stop = false;
    BaseType_t task_ok = xTaskCreate(flush_task,
                                     FLUSH_TASK_NAME,
                                     FLUSH_TASK_STACK_SIZE,
                                     NULL,
                                     FLUSH_TASK_PRIORITY,
                                     &s_flush_task);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create flush task");
        nvs_close(s_nvs_handle);
        s_nvs_handle = 0;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG,
             "Event recorder initialized (offline:%d, poweron:%d, control:%d, operation:%d, alarm:%d, manual:%d, program:%d)",
             s_basic_stores[EVT_TYPE_OFFLINE].count,
             s_basic_stores[EVT_TYPE_POWERON].count,
             s_basic_stores[EVT_TYPE_CONTROL].count,
             s_basic_stores[EVT_TYPE_OPERATION].count,
             s_basic_stores[EVT_TYPE_ALARM].count,
             s_manual_store.count,
             s_program_store.count);

    return ESP_OK;
}

esp_err_t event_recorder_clear_legacy_storage(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    esp_err_t ret = clear_legacy_storage_if_needed_locked();
    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t event_recorder_query(evt_type_t type, int64_t start_ts, int64_t end_ts,
                               uint16_t page, evt_query_result_t *result)
{
    if (!s_initialized || !result || !is_basic_type(type)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(evt_query_result_t));
    result->current_page = page;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    evt_store_t *store = &s_basic_stores[type];
    int matched_indices[EVT_RECORD_MAX];
    int matched_count = 0;

    for (int i = 0; i < store->count; i++) {
        int idx = (store->write_idx - 1 - i + EVT_RECORD_MAX) % EVT_RECORD_MAX;
        evt_record_t *rec = &store->records[idx];

        if (!timestamp_matches_range(rec->timestamp, start_ts, end_ts)) {
            continue;
        }

        matched_indices[matched_count++] = idx;
    }

    result->total_matched = matched_count;
    result->total_pages = (matched_count + EVT_PAGE_SIZE - 1) / EVT_PAGE_SIZE;
    if (result->total_pages == 0) {
        result->total_pages = 1;
    }

    if (page >= result->total_pages) {
        page = result->total_pages - 1;
        result->current_page = page;
    }

    int start_idx = page * EVT_PAGE_SIZE;
    int end_idx = start_idx + EVT_PAGE_SIZE;
    if (end_idx > matched_count) {
        end_idx = matched_count;
    }

    for (int i = start_idx; i < end_idx; i++) {
        int src_idx = matched_indices[i];
        memcpy(&result->records[result->count], &store->records[src_idx], sizeof(evt_record_t));
        result->count++;
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t event_recorder_query_manual_records(int64_t start_ts, int64_t end_ts,
                                              evt_status_filter_t status_filter,
                                              uint16_t page,
                                              evt_manual_query_result_t *result)
{
    if (!s_initialized || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(evt_manual_query_result_t));
    result->current_page = page;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int matched_indices[EVT_RECORD_MAX];
    int matched_count = 0;

    for (int i = 0; i < s_manual_store.count; i++) {
        int idx = (s_manual_store.write_idx - 1 - i + EVT_RECORD_MAX) % EVT_RECORD_MAX;
        evt_manual_record_t *rec = &s_manual_store.records[idx];

        if (!timestamp_matches_range(rec->start_ts, start_ts, end_ts)) {
            continue;
        }
        if (!status_matches_filter(rec->status, status_filter)) {
            continue;
        }

        matched_indices[matched_count++] = idx;
    }

    result->total_matched = matched_count;
    result->total_pages = (matched_count + EVT_PAGE_SIZE - 1) / EVT_PAGE_SIZE;
    if (result->total_pages == 0) {
        result->total_pages = 1;
    }

    if (page >= result->total_pages) {
        page = result->total_pages - 1;
        result->current_page = page;
    }

    int start_idx = page * EVT_PAGE_SIZE;
    int end_idx = start_idx + EVT_PAGE_SIZE;
    if (end_idx > matched_count) {
        end_idx = matched_count;
    }

    for (int i = start_idx; i < end_idx; i++) {
        int src_idx = matched_indices[i];
        memcpy(&result->records[result->count], &s_manual_store.records[src_idx], sizeof(evt_manual_record_t));
        result->count++;
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t event_recorder_query_program_records(int64_t start_ts, int64_t end_ts,
                                               evt_status_filter_t status_filter,
                                               uint16_t page,
                                               evt_program_query_result_t *result)
{
    if (!s_initialized || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(result, 0, sizeof(evt_program_query_result_t));
    result->current_page = page;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int matched_indices[EVT_RECORD_MAX];
    int matched_count = 0;

    for (int i = 0; i < s_program_store.count; i++) {
        int idx = (s_program_store.write_idx - 1 - i + EVT_RECORD_MAX) % EVT_RECORD_MAX;
        evt_program_record_t *rec = &s_program_store.records[idx];

        if (!timestamp_matches_range(rec->start_ts, start_ts, end_ts)) {
            continue;
        }
        if (!status_matches_filter(rec->status, status_filter)) {
            continue;
        }

        matched_indices[matched_count++] = idx;
    }

    result->total_matched = matched_count;
    result->total_pages = (matched_count + EVT_PAGE_SIZE - 1) / EVT_PAGE_SIZE;
    if (result->total_pages == 0) {
        result->total_pages = 1;
    }

    if (page >= result->total_pages) {
        page = result->total_pages - 1;
        result->current_page = page;
    }

    int start_idx = page * EVT_PAGE_SIZE;
    int end_idx = start_idx + EVT_PAGE_SIZE;
    if (end_idx > matched_count) {
        end_idx = matched_count;
    }

    for (int i = start_idx; i < end_idx; i++) {
        int src_idx = matched_indices[i];
        memcpy(&result->records[result->count], &s_program_store.records[src_idx], sizeof(evt_program_record_t));
        result->count++;
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t event_recorder_get_manual_records_snapshot(evt_manual_record_t *records,
                                                     size_t max_records,
                                                     size_t *out_count)
{
    if (!s_initialized || !records || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    size_t count = s_manual_store.count;
    if (count > max_records) {
        count = max_records;
    }

    for (size_t i = 0; i < count; i++) {
        int idx = (s_manual_store.write_idx - (int)count + (int)i + EVT_RECORD_MAX) % EVT_RECORD_MAX;
        memcpy(&records[i], &s_manual_store.records[idx], sizeof(evt_manual_record_t));
    }

    xSemaphoreGive(s_mutex);
    *out_count = count;
    return ESP_OK;
}

esp_err_t event_recorder_get_program_records_snapshot(evt_program_record_t *records,
                                                      size_t max_records,
                                                      size_t *out_count)
{
    if (!s_initialized || !records || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    size_t count = s_program_store.count;
    if (count > max_records) {
        count = max_records;
    }

    for (size_t i = 0; i < count; i++) {
        int idx = (s_program_store.write_idx - (int)count + (int)i + EVT_RECORD_MAX) % EVT_RECORD_MAX;
        memcpy(&records[i], &s_program_store.records[idx], sizeof(evt_program_record_t));
    }

    xSemaphoreGive(s_mutex);
    *out_count = count;
    return ESP_OK;
}

esp_err_t event_recorder_fix_timestamps(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    time_t now;
    time(&now);
    if ((int64_t)now < TIME_SYNC_THRESHOLD) {
        return ESP_ERR_INVALID_STATE;
    }

    uint32_t pending_bits = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    bool any_modified = false;

    for (int t = EVT_TYPE_OFFLINE; t <= EVT_TYPE_ALARM; t++) {
        evt_store_t *store = &s_basic_stores[t];
        bool modified = false;

        for (int i = 0; i < store->count; i++) {
            if (store->records[i].timestamp < TIME_SYNC_THRESHOLD) {
                store->records[i].timestamp = (int64_t)now;
                modified = true;
            }
        }

        if (modified && store->count > 1) {
            dedupe_basic_store((evt_type_t)t);
        }

        if (modified) {
            s_basic_dirty[t] = true;
            pending_bits |= EVT_PENDING_FLUSH_BASIC_MASK(t);
            any_modified = true;
            ESP_LOGI(TAG, "Updated %s timestamps to %lld (deferred save)",
                     record_type_name((evt_type_t)t),
                     (long long)now);
        }
    }

    {
        bool modified = false;
        for (int i = 0; i < s_manual_store.count; i++) {
            if (s_manual_store.records[i].start_ts < TIME_SYNC_THRESHOLD) {
                s_manual_store.records[i].start_ts = (int64_t)now;
                modified = true;
            }
        }
        if (modified && s_manual_store.count > 1) {
            dedupe_manual_store();
        }
        if (modified) {
            s_manual_dirty = true;
            pending_bits |= EVT_PENDING_FLUSH_MANUAL_BIT;
            any_modified = true;
            ESP_LOGI(TAG, "Updated manual record timestamps to %lld (deferred save)", (long long)now);
        }
    }

    {
        bool modified = false;
        for (int i = 0; i < s_program_store.count; i++) {
            if (s_program_store.records[i].start_ts < TIME_SYNC_THRESHOLD) {
                s_program_store.records[i].start_ts = (int64_t)now;
                modified = true;
            }
        }
        if (modified && s_program_store.count > 1) {
            dedupe_program_store();
        }
        if (modified) {
            s_program_dirty = true;
            pending_bits |= EVT_PENDING_FLUSH_PROGRAM_BIT;
            any_modified = true;
            ESP_LOGI(TAG, "Updated program record timestamps to %lld (deferred save)", (long long)now);
        }
    }

    esp_err_t schedule_ret = ESP_OK;
    if (any_modified) {
        schedule_ret = schedule_flush_locked(pending_bits);
    }

    xSemaphoreGive(s_mutex);
    return schedule_ret;
}
