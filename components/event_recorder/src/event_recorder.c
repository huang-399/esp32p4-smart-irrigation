#include "event_recorder.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <time.h>

static const char *TAG = "event_recorder";

#define NVS_NAMESPACE "evt_rec"
#define NVS_KEY_OFFLINE "offline"
#define NVS_KEY_POWERON "poweron"

typedef struct {
    uint16_t count;
    uint16_t write_idx;
    evt_record_t records[EVT_RECORD_MAX];
} evt_store_t;

static evt_store_t s_stores[EVT_TYPE_MAX];
static SemaphoreHandle_t s_mutex = NULL;
static nvs_handle_t s_nvs_handle;
static bool s_initialized = false;

static const char *s_nvs_keys[EVT_TYPE_MAX] = {
    NVS_KEY_OFFLINE,
    NVS_KEY_POWERON
};

static esp_err_t load_store(evt_type_t type)
{
    size_t len = sizeof(evt_store_t);
    esp_err_t ret = nvs_get_blob(s_nvs_handle, s_nvs_keys[type], &s_stores[type], &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        memset(&s_stores[type], 0, sizeof(evt_store_t));
        return ESP_OK;
    }
    return ret;
}

static esp_err_t save_store(evt_type_t type)
{
    esp_err_t ret = nvs_set_blob(s_nvs_handle, s_nvs_keys[type],
                                  &s_stores[type], sizeof(evt_store_t));
    if (ret == ESP_OK) {
        ret = nvs_commit(s_nvs_handle);
    }
    return ret;
}

static esp_err_t add_record(evt_type_t type, const char *desc)
{
    if (!s_initialized || type >= EVT_TYPE_MAX) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    evt_store_t *store = &s_stores[type];
    evt_record_t *rec = &store->records[store->write_idx];

    time_t now;
    time(&now);
    rec->timestamp = (int64_t)now;
    strncpy(rec->desc, desc ? desc : "", sizeof(rec->desc) - 1);
    rec->desc[sizeof(rec->desc) - 1] = '\0';

    store->write_idx = (store->write_idx + 1) % EVT_RECORD_MAX;
    if (store->count < EVT_RECORD_MAX) {
        store->count++;
    }

    esp_err_t ret = save_store(type);

    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Added %s record: %s (total: %d)",
             type == EVT_TYPE_OFFLINE ? "offline" : "poweron",
             desc ? desc : "", store->count);

    return ret;
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

    for (int i = 0; i < EVT_TYPE_MAX; i++) {
        ret = load_store((evt_type_t)i);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to load store %d: %s", i, esp_err_to_name(ret));
            memset(&s_stores[i], 0, sizeof(evt_store_t));
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Event recorder initialized (offline:%d, poweron:%d)",
             s_stores[EVT_TYPE_OFFLINE].count, s_stores[EVT_TYPE_POWERON].count);

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

esp_err_t event_recorder_query(evt_type_t type, int64_t start_ts, int64_t end_ts,
                                uint16_t page, evt_query_result_t *result)
{
    if (!s_initialized || !result || type >= EVT_TYPE_MAX) return ESP_ERR_INVALID_ARG;

    memset(result, 0, sizeof(evt_query_result_t));
    result->current_page = page;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    evt_store_t *store = &s_stores[type];

    /* Collect matching records in reverse chronological order */
    int matched_indices[EVT_RECORD_MAX];
    int matched_count = 0;

    for (int i = 0; i < store->count; i++) {
        /* Walk backwards from most recent record */
        int idx = (store->write_idx - 1 - i + EVT_RECORD_MAX) % EVT_RECORD_MAX;
        evt_record_t *rec = &store->records[idx];

        /* Apply date filter if both start and end are specified.
         * Records with unsynchronized time (before 2024-01-01) always pass
         * the filter since we cannot meaningfully filter them by date. */
        if (start_ts > 0 && end_ts > 0 && rec->timestamp >= 1704067200LL) {
            if (rec->timestamp < start_ts || rec->timestamp > end_ts) {
                continue;
            }
        }

        matched_indices[matched_count++] = idx;
    }

    result->total_matched = matched_count;
    result->total_pages = (matched_count + EVT_PAGE_SIZE - 1) / EVT_PAGE_SIZE;
    if (result->total_pages == 0) result->total_pages = 1;

    /* Clamp page */
    if (page >= result->total_pages) {
        page = result->total_pages - 1;
        result->current_page = page;
    }

    /* Extract the requested page */
    int start_idx = page * EVT_PAGE_SIZE;
    int end_idx = start_idx + EVT_PAGE_SIZE;
    if (end_idx > matched_count) end_idx = matched_count;

    for (int i = start_idx; i < end_idx; i++) {
        int src_idx = matched_indices[i];
        memcpy(&result->records[result->count], &store->records[src_idx], sizeof(evt_record_t));
        result->count++;
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

#define TIME_SYNC_THRESHOLD 1704067200LL  /* 2024-01-01 00:00:00 UTC */

esp_err_t event_recorder_fix_timestamps(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    time_t now;
    time(&now);
    if ((int64_t)now < TIME_SYNC_THRESHOLD) {
        return ESP_ERR_INVALID_STATE;  /* Time still not synced */
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int t = 0; t < EVT_TYPE_MAX; t++) {
        evt_store_t *store = &s_stores[t];
        bool modified = false;

        /* Update unsynchronized timestamps */
        for (int i = 0; i < store->count; i++) {
            if (store->records[i].timestamp < TIME_SYNC_THRESHOLD) {
                store->records[i].timestamp = (int64_t)now;
                modified = true;
            }
        }

        /* Deduplicate records with same timestamp + description */
        if (modified && store->count > 1) {
            static evt_record_t temp[EVT_RECORD_MAX];
            int temp_count = 0;
            int old_count = store->count;

            /* Extract in chronological order (oldest first) */
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
                    memcpy(&temp[temp_count++], &store->records[idx],
                           sizeof(evt_record_t));
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
                         t == EVT_TYPE_OFFLINE ? "offline" : "poweron",
                         old_count, temp_count);
            }
        }

        if (modified) {
            save_store((evt_type_t)t);
            ESP_LOGI(TAG, "Updated %s timestamps to %lld",
                     t == EVT_TYPE_OFFLINE ? "offline" : "poweron",
                     (long long)now);
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}
