#include "alarm_manager.h"
#include "event_recorder.h"
#include "history_archive.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "alarm_manager";

#define NVS_NAMESPACE "alarm_mgr"
#define NVS_KEY_SETTINGS "settings"

typedef struct {
    uint16_t count;
    alarm_manager_current_alarm_t items[ALARM_MANAGER_MAX_CURRENT];
} alarm_manager_current_store_t;

static const alarm_manager_settings_t s_default_settings = {
    .items = {
        {{"0.20"}, 10, 0},
        {{"0.20"}, 10, 0},
        {{"4.00"}, 10, 0},
        {{"10.00"}, 10, 0},
        {{"0.10"}, 10, 0},
        {{"4.00"}, 5, 0},
        {{"1.00"}, 30, 0},
        {{"2.50"}, 3, 0},
        {{"0.05"}, 3, 0},
        {{"0.60"}, 5, 0},
    }
};

static SemaphoreHandle_t s_mutex = NULL;
static nvs_handle_t s_nvs_handle = 0;
static bool s_initialized = false;
static alarm_manager_current_store_t s_current_store;
static alarm_manager_settings_t s_cached_settings;

static esp_err_t alarm_manager_save_settings_locked(void)
{
    esp_err_t ret = nvs_set_blob(s_nvs_handle, NVS_KEY_SETTINGS,
                                 &s_cached_settings, sizeof(s_cached_settings));
    if (ret == ESP_OK) {
        ret = nvs_commit(s_nvs_handle);
    }
    return ret;
}

esp_err_t alarm_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_mutex != NULL, ESP_ERR_NO_MEM, TAG, "Failed to create mutex");

    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ret;
    }

    memset(&s_current_store, 0, sizeof(s_current_store));
    memcpy(&s_cached_settings, &s_default_settings, sizeof(s_cached_settings));

    size_t len = sizeof(s_cached_settings);
    ret = nvs_get_blob(s_nvs_handle, NVS_KEY_SETTINGS, &s_cached_settings, &len);
    if (ret == ESP_ERR_NVS_NOT_FOUND || len != sizeof(s_cached_settings)) {
        memcpy(&s_cached_settings, &s_default_settings, sizeof(s_cached_settings));
        ret = alarm_manager_save_settings_locked();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to persist default settings: %s", esp_err_to_name(ret));
        }
    } else if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load alarm settings, use defaults: %s", esp_err_to_name(ret));
        memcpy(&s_cached_settings, &s_default_settings, sizeof(s_cached_settings));
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Alarm manager initialized");
    return ESP_OK;
}

esp_err_t alarm_manager_get_current_alarms(alarm_manager_current_alarm_t *items,
                                           size_t max_items,
                                           size_t *out_count)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "Not initialized");
    ESP_RETURN_ON_FALSE(out_count != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid out_count");

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    size_t copy_count = s_current_store.count;
    if (copy_count > max_items) {
        copy_count = max_items;
    }

    if (items != NULL && copy_count > 0) {
        memcpy(items, s_current_store.items, copy_count * sizeof(alarm_manager_current_alarm_t));
    }
    *out_count = s_current_store.count;

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t alarm_manager_clear_current_alarms(void)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "Not initialized");

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (uint16_t i = 0; i < s_current_store.count; i++) {
        evt_record_t record = {0};
        esp_err_t ret;

        record.timestamp = s_current_store.items[i].timestamp;
        snprintf(record.desc, sizeof(record.desc), "%s", s_current_store.items[i].desc);

        ret = event_recorder_add_basic_record(EVT_TYPE_ALARM, &record);
        if (ret != ESP_OK) {
            xSemaphoreGive(s_mutex);
            ESP_LOGE(TAG, "Failed to archive alarm: %s", esp_err_to_name(ret));
            return ret;
        }

        ret = history_archive_enqueue_basic_record(EVT_TYPE_ALARM, &record);
        if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
            xSemaphoreGive(s_mutex);
            ESP_LOGE(TAG, "Failed to enqueue alarm archive: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    memset(&s_current_store, 0, sizeof(s_current_store));

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

esp_err_t alarm_manager_load_settings(alarm_manager_settings_t *settings)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "Not initialized");
    ESP_RETURN_ON_FALSE(settings != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid settings");

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(settings, &s_cached_settings, sizeof(*settings));
    xSemaphoreGive(s_mutex);

    return ESP_OK;
}

esp_err_t alarm_manager_save_settings(const alarm_manager_settings_t *settings)
{
    ESP_RETURN_ON_FALSE(s_initialized, ESP_ERR_INVALID_STATE, TAG, "Not initialized");
    ESP_RETURN_ON_FALSE(settings != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid settings");

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(&s_cached_settings, settings, sizeof(s_cached_settings));
    esp_err_t ret = alarm_manager_save_settings_locked();
    xSemaphoreGive(s_mutex);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save alarm settings: %s", esp_err_to_name(ret));
    }
    return ret;
}
