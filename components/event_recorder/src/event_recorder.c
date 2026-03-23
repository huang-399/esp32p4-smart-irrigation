#include "event_recorder.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdbool.h>
#include <string.h>
#include <time.h>

static const char *TAG = "event_recorder";

#define NVS_NAMESPACE "evt_rec"
#define NVS_KEY_OFFLINE "offline"
#define NVS_KEY_POWERON "poweron"
#define NVS_KEY_CONTROL "control"
#define NVS_KEY_OPERATION "operation"
#define NVS_KEY_MANUAL "manual_rec"
#define NVS_KEY_PROGRAM "program_rec"
#define TIME_SYNC_THRESHOLD 1704067200LL  /* 2024-01-01 00:00:00 UTC */

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

static evt_store_t s_basic_stores[EVT_TYPE_OPERATION + 1];
static evt_manual_store_t s_manual_store;
static evt_program_store_t s_program_store;
static SemaphoreHandle_t s_mutex = NULL;
static nvs_handle_t s_nvs_handle;
static bool s_initialized = false;

static const char *s_basic_nvs_keys[EVT_TYPE_OPERATION + 1] = {
    NVS_KEY_OFFLINE,
    NVS_KEY_POWERON,
    NVS_KEY_CONTROL,
    NVS_KEY_OPERATION
};

static bool is_basic_type(evt_type_t type)
{
    return type >= EVT_TYPE_OFFLINE && type <= EVT_TYPE_OPERATION;
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

static esp_err_t save_basic_store(evt_type_t type)
{
    esp_err_t ret = nvs_set_blob(s_nvs_handle, s_basic_nvs_keys[type],
                                 &s_basic_stores[type], sizeof(evt_store_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(s_nvs_handle);
    }
    return ret;
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

static esp_err_t save_manual_store(void)
{
    esp_err_t ret = nvs_set_blob(s_nvs_handle, NVS_KEY_MANUAL,
                                 &s_manual_store, sizeof(s_manual_store));
    if (ret == ESP_OK) {
        ret = nvs_commit(s_nvs_handle);
    }
    return ret;
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

static esp_err_t save_program_store(void)
{
    esp_err_t ret = nvs_set_blob(s_nvs_handle, NVS_KEY_PROGRAM,
                                 &s_program_store, sizeof(s_program_store));
    if (ret == ESP_OK) {
        ret = nvs_commit(s_nvs_handle);
    }
    return ret;
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
    if (!s_initialized || !is_basic_type(type)) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    evt_store_t *store = &s_basic_stores[type];
    evt_record_t *rec = &store->records[store->write_idx];
    time_t now;

    time(&now);
    memset(rec, 0, sizeof(*rec));
    rec->timestamp = (int64_t)now;
    strncpy(rec->desc, desc ? desc : "", sizeof(rec->desc) - 1);

    store->write_idx = (store->write_idx + 1) % EVT_RECORD_MAX;
    if (store->count < EVT_RECORD_MAX) {
        store->count++;
    }

    esp_err_t ret = save_basic_store(type);

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Added %s record: %s (total: %d)",
             record_type_name(type),
             desc ? desc : "", store->count);

    return ret;
}

static bool manual_record_equals(const evt_manual_record_t *a, const evt_manual_record_t *b)
{
    return a->start_ts == b->start_ts &&
           a->planned_minutes == b->planned_minutes &&
           a->actual_minutes == b->actual_minutes &&
           strcmp(a->status, b->status) == 0 &&
           strcmp(a->detail, b->detail) == 0;
}

static bool program_record_equals(const evt_program_record_t *a, const evt_program_record_t *b)
{
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
            if (manual_record_equals(&temp[j], &s_manual_store.records[idx])) {
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
            if (program_record_equals(&temp[j], &s_program_store.records[idx])) {
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

    for (int i = EVT_TYPE_OFFLINE; i <= EVT_TYPE_OPERATION; i++) {
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

    s_initialized = true;
    ESP_LOGI(TAG,
             "Event recorder initialized (offline:%d, poweron:%d, control:%d, operation:%d, manual:%d, program:%d)",
             s_basic_stores[EVT_TYPE_OFFLINE].count,
             s_basic_stores[EVT_TYPE_POWERON].count,
             s_basic_stores[EVT_TYPE_CONTROL].count,
             s_basic_stores[EVT_TYPE_OPERATION].count,
             s_manual_store.count,
             s_program_store.count);

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

    ESP_LOGI(TAG, "Added manual record: start=%lld status=%s total=%d",
             (long long)record->start_ts,
             record->status,
             s_manual_store.count);

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

esp_err_t event_recorder_fix_timestamps(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    time_t now;
    time(&now);
    if ((int64_t)now < TIME_SYNC_THRESHOLD) {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int t = EVT_TYPE_OFFLINE; t <= EVT_TYPE_OPERATION; t++) {
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
            save_basic_store((evt_type_t)t);
            ESP_LOGI(TAG, "Updated %s timestamps to %lld",
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
            save_manual_store();
            ESP_LOGI(TAG, "Updated manual record timestamps to %lld", (long long)now);
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
            save_program_store();
            ESP_LOGI(TAG, "Updated program record timestamps to %lld", (long long)now);
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}
