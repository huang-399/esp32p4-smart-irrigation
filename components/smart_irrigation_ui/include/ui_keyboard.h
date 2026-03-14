/**
 * @file ui_keyboard.h
 * @brief 26键中英文键盘组件头文件
 */

#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#include "lvgl.h"
#include <stdbool.h>

/**
 * @brief 显示26键中英文键盘
 * @param textarea 目标输入框
 * @param parent 父对象（通常是屏幕）
 */
void ui_keyboard_show(lv_obj_t *textarea, lv_obj_t *parent);

/**
 * @brief 关闭26键键盘
 */
void ui_keyboard_close(void);

/**
 * @brief 检查键盘是否正在显示
 * @return true 正在显示，false 未显示
 */
bool ui_keyboard_is_visible(void);

#endif /* UI_KEYBOARD_H */
