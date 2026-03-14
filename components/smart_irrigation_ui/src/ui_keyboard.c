/**
 * @file ui_keyboard.c
 * @brief 26键中英文键盘组件实现
 */

#include "ui_keyboard.h"
#include "ui_common.h"
#include <string.h>

/*********************
 *  STATIC VARIABLES
 *********************/
static lv_obj_t *g_keyboard_bg = NULL;      /* 半透明背景遮罩 */
static lv_obj_t *g_keyboard_panel = NULL;   /* 键盘面板 */
static lv_obj_t *g_target_textarea = NULL;  /* 目标输入框 */
static lv_obj_t *g_keyboard = NULL;         /* LVGL键盘对象 */

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void bg_click_cb(lv_event_t *e);
static void keyboard_event_cb(lv_event_t *e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 显示26键中英文键盘
 */
void ui_keyboard_show(lv_obj_t *textarea, lv_obj_t *parent)
{
    if (!textarea || !parent) return;

    /* 如果已经显示，先关闭 */
    if (g_keyboard_bg) {
        ui_keyboard_close();
    }

    g_target_textarea = textarea;

    /* 创建透明背景遮罩 */
    g_keyboard_bg = lv_obj_create(parent);
    lv_obj_set_size(g_keyboard_bg, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(g_keyboard_bg, 0, 0);
    lv_obj_set_style_bg_opa(g_keyboard_bg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_keyboard_bg, 0, 0);
    lv_obj_set_style_radius(g_keyboard_bg, 0, 0);
    lv_obj_clear_flag(g_keyboard_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 点击背景关闭键盘 */
    lv_obj_add_event_cb(g_keyboard_bg, bg_click_cb, LV_EVENT_CLICKED, NULL);

    /* 获取输入框在屏幕上的位置 */
    lv_area_t ta_coords;
    lv_obj_get_coords(textarea, &ta_coords);

    lv_coord_t ta_x = ta_coords.x1;
    lv_coord_t ta_y = ta_coords.y1;
    lv_coord_t ta_width = lv_area_get_width(&ta_coords);
    lv_coord_t ta_height = lv_area_get_height(&ta_coords);

    /* 键盘面板尺寸 */
    int panel_width = 1000;
    int panel_height = 420;
    int gap = 5;

    /* 键盘右上角对齐输入框右下角 */
    lv_coord_t panel_x = ta_x + ta_width - panel_width;
    lv_coord_t panel_y = ta_y + ta_height + gap;

    /* 确保键盘不超出屏幕 */
    if (panel_x < 0) {
        panel_x = 10;
    }
    if (panel_y + panel_height > SCREEN_HEIGHT) {
        panel_y = ta_y - panel_height - gap;
        if (panel_y < 0) {
            panel_y = SCREEN_HEIGHT - panel_height - 10;
        }
    }

    /* 创建键盘面板 */
    g_keyboard_panel = lv_obj_create(g_keyboard_bg);
    lv_obj_set_size(g_keyboard_panel, panel_width, panel_height);
    lv_obj_set_pos(g_keyboard_panel, panel_x, panel_y);
    lv_obj_set_style_bg_color(g_keyboard_panel, lv_color_hex(0x2c2c2c), 0);
    lv_obj_set_style_border_width(g_keyboard_panel, 0, 0);
    lv_obj_set_style_radius(g_keyboard_panel, 10, 0);
    lv_obj_set_style_pad_all(g_keyboard_panel, 10, 0);
    lv_obj_clear_flag(g_keyboard_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建LVGL键盘 */
    g_keyboard = lv_keyboard_create(g_keyboard_panel);
    lv_obj_set_size(g_keyboard, panel_width - 20, panel_height - 20);
    lv_obj_set_pos(g_keyboard, 0, 0);

    /* 设置键盘样式 */
    lv_obj_set_style_bg_color(g_keyboard, lv_color_hex(0x2c2c2c), 0);
    lv_obj_set_style_border_width(g_keyboard, 0, 0);

    /* 设置按钮样式 */
    lv_obj_set_style_bg_color(g_keyboard, lv_color_hex(0x4a4a4a), LV_PART_ITEMS);
    lv_obj_set_style_text_color(g_keyboard, lv_color_white(), LV_PART_ITEMS);
    lv_obj_set_style_text_font(g_keyboard, &my_font_cn_16, LV_PART_ITEMS);

    /* 关联键盘和输入框 */
    lv_keyboard_set_textarea(g_keyboard, textarea);

    /* 添加键盘事件回调 */
    lv_obj_add_event_cb(g_keyboard, keyboard_event_cb, LV_EVENT_ALL, NULL);
}

/**
 * @brief 关闭26键键盘
 */
void ui_keyboard_close(void)
{
    if (g_keyboard_bg) {
        lv_obj_del(g_keyboard_bg);
        g_keyboard_bg = NULL;
        g_keyboard_panel = NULL;
        g_target_textarea = NULL;
        g_keyboard = NULL;
    }
}

/**
 * @brief 检查键盘是否正在显示
 */
bool ui_keyboard_is_visible(void)
{
    return (g_keyboard_bg != NULL);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 键盘事件回调
 */
static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *kb = lv_event_get_target(e);

    /* 当用户点击"关闭"按钮时 */
    if (code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
        ui_keyboard_close();
    }
}

/**
 * @brief 背景点击回调 - 关闭键盘
 */
static void bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);

    /* 只有点击背景本身才关闭 */
    if (target == g_keyboard_bg) {
        ui_keyboard_close();
    }
}
