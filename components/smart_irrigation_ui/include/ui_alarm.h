/**
 * @file ui_alarm.h
 * @brief 智慧种植园监控系统 - 告警管理界面头文件
 */

#ifndef UI_ALARM_H
#define UI_ALARM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui_common.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#define UI_ALARM_MAX_CURRENT 8
#define UI_ALARM_SETTINGS_COUNT 10

typedef struct {
    int64_t timestamp;
    char desc[64];
} ui_alarm_current_item_t;

typedef struct {
    char threshold[16];
    uint16_t duration_s;
    uint8_t action;
} ui_alarm_setting_item_t;

typedef struct {
    ui_alarm_setting_item_t items[UI_ALARM_SETTINGS_COUNT];
} ui_alarm_settings_t;

typedef esp_err_t (*ui_alarm_query_current_fn)(ui_alarm_current_item_t *items,
                                               size_t max_items,
                                               size_t *out_count);
typedef esp_err_t (*ui_alarm_clear_current_fn)(void);
typedef esp_err_t (*ui_alarm_load_settings_fn)(ui_alarm_settings_t *settings);
typedef esp_err_t (*ui_alarm_save_settings_fn)(const ui_alarm_settings_t *settings);

void ui_alarm_register_query_current_cb(ui_alarm_query_current_fn fn);
void ui_alarm_register_clear_current_cb(ui_alarm_clear_current_fn fn);
void ui_alarm_register_load_settings_cb(ui_alarm_load_settings_fn fn);
void ui_alarm_register_save_settings_cb(ui_alarm_save_settings_fn fn);

/**
 * @brief 显示告警管理对话框（全屏弹窗）
 * @param parent 父容器（通常是主屏幕）
 */
void ui_alarm_show(lv_obj_t *parent);

/**
 * @brief 关闭告警管理对话框
 */
void ui_alarm_close(void);

/**
 * @brief 检查告警对话框是否正在显示
 * @return true if visible, false otherwise
 */
bool ui_alarm_is_visible(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UI_ALARM_H */
