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
