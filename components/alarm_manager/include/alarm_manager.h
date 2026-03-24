#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ALARM_MANAGER_MAX_CURRENT 8
#define ALARM_MANAGER_SETTINGS_COUNT 10

typedef struct {
    int64_t timestamp;
    char desc[64];
} alarm_manager_current_alarm_t;

typedef struct {
    char threshold[16];
    uint16_t duration_s;
    uint8_t action;
} alarm_manager_setting_item_t;

typedef struct {
    alarm_manager_setting_item_t items[ALARM_MANAGER_SETTINGS_COUNT];
} alarm_manager_settings_t;

esp_err_t alarm_manager_init(void);
esp_err_t alarm_manager_get_current_alarms(alarm_manager_current_alarm_t *items,
                                           size_t max_items,
                                           size_t *out_count);
esp_err_t alarm_manager_clear_current_alarms(void);
esp_err_t alarm_manager_load_settings(alarm_manager_settings_t *settings);
esp_err_t alarm_manager_save_settings(const alarm_manager_settings_t *settings);

#ifdef __cplusplus
}
#endif
