/**
 * @file ui_numpad.h
 * @brief 通用数字键盘组件
 */

#ifndef UI_NUMPAD_H
#define UI_NUMPAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/*********************
 * GLOBAL PROTOTYPES
 *********************/

/**
 * @brief 显示数字键盘
 * @param textarea 目标输入框对象
 * @param parent 键盘显示的父容器（通常是主屏幕）
 */
void ui_numpad_show(lv_obj_t *textarea, lv_obj_t *parent);

/**
 * @brief 关闭数字键盘
 */
void ui_numpad_close(void);

/**
 * @brief 检查键盘是否正在显示
 * @return true if visible, false otherwise
 */
bool ui_numpad_is_visible(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UI_NUMPAD_H */
