/**
 * @file ui_numpad.c
 * @brief 通用数字键盘组件实现
 */

#include "ui_numpad.h"
#include "ui_common.h"
#include <string.h>

/*********************
 *  STATIC VARIABLES
 *********************/
static lv_obj_t *g_numpad_bg = NULL;      /* 半透明背景遮罩 */
static lv_obj_t *g_numpad_panel = NULL;   /* 键盘面板 */
static lv_obj_t *g_target_textarea = NULL; /* 目标输入框 */

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void numpad_btn_cb(lv_event_t *e);
static void bg_click_cb(lv_event_t *e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 显示数字键盘
 */
void ui_numpad_show(lv_obj_t *textarea, lv_obj_t *parent)
{
    if (!textarea || !parent) return;

    /* 如果已经显示，先关闭 */
    if (g_numpad_bg) {
        ui_numpad_close();
    }

    g_target_textarea = textarea;

    /* 创建透明背景遮罩（不变暗，仅用于捕获点击事件） */
    g_numpad_bg = lv_obj_create(parent);
    lv_obj_set_size(g_numpad_bg, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(g_numpad_bg, 0, 0);
    lv_obj_set_style_bg_opa(g_numpad_bg, LV_OPA_TRANSP, 0);  /* 完全透明 */
    lv_obj_set_style_border_width(g_numpad_bg, 0, 0);
    lv_obj_set_style_radius(g_numpad_bg, 0, 0);
    lv_obj_clear_flag(g_numpad_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 点击背景关闭键盘 */
    lv_obj_add_event_cb(g_numpad_bg, bg_click_cb, LV_EVENT_CLICKED, NULL);

    /* 获取输入框在屏幕上的绝对位置和尺寸 */
    lv_area_t ta_coords;
    lv_obj_get_coords(textarea, &ta_coords);

    lv_coord_t ta_x = ta_coords.x1;
    lv_coord_t ta_y = ta_coords.y1;
    lv_coord_t ta_width = lv_area_get_width(&ta_coords);
    lv_coord_t ta_height = lv_area_get_height(&ta_coords);

    /* 计算键盘面板的位置和尺寸（缩小尺寸） */
    int panel_width = 300;   /* 缩小宽度 */
    int panel_height = 260;  /* 缩小高度 */
    int gap = 5;  /* 减小间距 */

    /* 键盘右上角对齐输入框右下角 */
    lv_coord_t panel_x = ta_x + ta_width - panel_width;  /* 右对齐 */
    lv_coord_t panel_y = ta_y + ta_height + gap;  /* 下方对齐 */

    /* 确保键盘不超出屏幕左侧 */
    if (panel_x < 0) {
        panel_x = 10;
    }

    /* 确保键盘不超出屏幕底部 */
    if (panel_y + panel_height > SCREEN_HEIGHT) {
        /* 如果下方空间不够，显示在输入框上方 */
        panel_y = ta_y - panel_height - gap;
        /* 如果上方也不够，就显示在屏幕底部 */
        if (panel_y < 0) {
            panel_y = SCREEN_HEIGHT - panel_height - 10;
        }
    }

    /* 创建键盘面板 - 深灰色背景 */
    g_numpad_panel = lv_obj_create(g_numpad_bg);
    lv_obj_set_size(g_numpad_panel, panel_width, panel_height);
    lv_obj_set_pos(g_numpad_panel, panel_x, panel_y);
    lv_obj_set_style_bg_color(g_numpad_panel, lv_color_hex(0x2c2c2c), 0);  /* 深灰色 */
    lv_obj_set_style_border_width(g_numpad_panel, 0, 0);
    lv_obj_set_style_radius(g_numpad_panel, 10, 0);
    lv_obj_set_style_pad_all(g_numpad_panel, 15, 0);
    lv_obj_clear_flag(g_numpad_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 键盘按钮布局 */
    const char *btn_labels[4][4] = {
        {"7", "8", "9", "删除"},
        {"4", "5", "6", "空格"},
        {"1", "2", "3", "回车"},
        {"0", ".", "-", "关闭"}
    };

    int btn_width = 55;   /* 缩小按钮宽度 */
    int btn_height = 50;  /* 缩小按钮高度 */
    int btn_gap = 8;      /* 减小按钮间距 */
    int start_x = 8;
    int start_y = 8;

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            lv_obj_t *btn = lv_btn_create(g_numpad_panel);

            /* 最后一列按钮宽度稍大 */
            int current_btn_width = (col == 3) ? 65 : btn_width;

            lv_obj_set_size(btn, current_btn_width, btn_height);
            lv_obj_set_pos(btn, start_x + col * (btn_width + btn_gap), start_y + row * (btn_height + btn_gap));

            /* 按钮颜色：数字和运算符为浅灰色，功能键为特殊颜色 */
            if (strcmp(btn_labels[row][col], "删除") == 0) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0xff6b6b), 0);  /* 红色 */
            } else if (strcmp(btn_labels[row][col], "关闭") == 0) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x95a5a6), 0);  /* 灰色 */
            } else if (strcmp(btn_labels[row][col], "回车") == 0) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x3498db), 0);  /* 蓝色 */
            } else if (strcmp(btn_labels[row][col], "空格") == 0) {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x7f8c8d), 0);  /* 深灰色 */
            } else {
                lv_obj_set_style_bg_color(btn, lv_color_hex(0x4a4a4a), 0);  /* 中灰色 */
            }

            lv_obj_set_style_border_width(btn, 0, 0);
            lv_obj_set_style_radius(btn, 8, 0);

            /* 按钮文字 */
            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, btn_labels[row][col]);
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
            lv_obj_set_style_text_font(label, &my_font_cn_16, 0);  /* 使用中文字体 */
            lv_obj_center(label);

            /* 添加点击事件 */
            lv_obj_add_event_cb(btn, numpad_btn_cb, LV_EVENT_CLICKED, (void*)btn_labels[row][col]);
        }
    }
}

/**
 * @brief 关闭数字键盘
 */
void ui_numpad_close(void)
{
    if (g_numpad_bg) {
        lv_obj_del(g_numpad_bg);
        g_numpad_bg = NULL;
        g_numpad_panel = NULL;
        g_target_textarea = NULL;
    }
}

/**
 * @brief 检查键盘是否正在显示
 */
bool ui_numpad_is_visible(void)
{
    return (g_numpad_bg != NULL);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 键盘按钮点击回调
 */
static void numpad_btn_cb(lv_event_t *e)
{
    const char *btn_label = (const char*)lv_event_get_user_data(e);

    if (!g_target_textarea || !btn_label) return;

    /* 处理不同按钮 */
    if (strcmp(btn_label, "关闭") == 0) {
        /* 关闭键盘 */
        ui_numpad_close();
    }
    else if (strcmp(btn_label, "删除") == 0) {
        /* 删除最后一个字符 */
        lv_textarea_delete_char(g_target_textarea);
    }
    else if (strcmp(btn_label, "空格") == 0) {
        /* 输入空格 */
        lv_textarea_add_char(g_target_textarea, ' ');
    }
    else if (strcmp(btn_label, "回车") == 0) {
        /* 回车 - 关闭键盘并确认输入 */
        ui_numpad_close();
    }
    else {
        /* 输入数字或符号 */
        lv_textarea_add_text(g_target_textarea, btn_label);
    }
}

/**
 * @brief 背景点击回调 - 关闭键盘
 */
static void bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);

    /* 只有点击背景本身才关闭（不是点击键盘面板） */
    if (target == g_numpad_bg) {
        ui_numpad_close();
    }
}
