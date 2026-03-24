/**
 * @file ui_alarm.c
 * @brief 智慧种植园监控系统 - 告警管理界面
 * 功能：显示当前报警、历史报警、报警设置、掉线记录、上电记录
 */

#include "ui_alarm.h"
#include "ui_alarm_records.h"
#include "ui_common.h"
#include "ui_numpad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TIME_SYNC_THRESHOLD 1704067200LL

/*********************
 *  DEFINES
 *********************/
#define TAB_COUNT 5  /* 标签页数量 */

/* 标签页枚举 */
typedef enum {
    TAB_CURRENT = 0,    /* 当前报警 */
    TAB_HISTORY,        /* 历史报警 */
    TAB_SETTINGS,       /* 报警设置 */
    TAB_OFFLINE,        /* 掉线记录 */
    TAB_POWERON         /* 上电记录 */
} alarm_tab_t;

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_tab_buttons(lv_obj_t *parent);
static void create_close_button(lv_obj_t *parent);
static void create_clear_alarm_button(lv_obj_t *parent);
static void tab_btn_click_cb(lv_event_t *e);
static void close_btn_click_cb(lv_event_t *e);
static void clear_alarm_btn_click_cb(lv_event_t *e);
static void switch_tab(alarm_tab_t tab);
static void get_today_str(char *buf, int buf_size);
static void calendar_set_from_input(lv_obj_t *calendar, lv_obj_t *input, lv_obj_t *ym_label);
static void create_current_alarm_content(lv_obj_t *parent);
static void create_history_alarm_content(lv_obj_t *parent);
static void create_settings_content(lv_obj_t *parent);
static void create_offline_content(lv_obj_t *parent);
static void create_poweron_content(lv_obj_t *parent);
static void settings_cancel_btn_cb(lv_event_t *e);
static void settings_save_btn_cb(lv_event_t *e);
static void ensure_settings_cache_loaded(void);
static void restore_settings_controls_from_cache(void);
static void collect_settings_from_controls(ui_alarm_settings_t *settings);
static void btn_calendar_start_cb(lv_event_t *e);
static void btn_calendar_end_cb(lv_event_t *e);
static void calendar_event_cb(lv_event_t *e);
static void btn_calendar_close_cb(lv_event_t *e);
static void btn_year_prev_cb(lv_event_t *e);
static void btn_year_next_cb(lv_event_t *e);
static void btn_month_prev_cb(lv_event_t *e);
static void btn_month_next_cb(lv_event_t *e);
static void input_click_cb(lv_event_t *e);
static void format_alarm_time(char *buf, size_t buf_size, int64_t timestamp);

/*********************
 *  STATIC VARIABLES
 *********************/
static lv_obj_t *g_alarm_dialog = NULL;      /* 告警对话框 */
static lv_obj_t *g_tab_content = NULL;       /* 标签页内容容器 */
static lv_obj_t *g_tab_buttons[TAB_COUNT] = {NULL};  /* 标签页按钮数组 */
static lv_obj_t *g_clear_btn = NULL;         /* 清除报警按钮 */
static alarm_tab_t g_current_tab = TAB_CURRENT;  /* 当前标签页 */
static lv_obj_t *g_input_start_date = NULL;   /* 开始日期输入框 */
static lv_obj_t *g_input_end_date = NULL;     /* 结束日期输入框 */
static lv_obj_t *g_calendar_popup = NULL;     /* 日历弹窗 */
static lv_obj_t *g_current_date_input = NULL; /* 当前选择日期的输入框 */
static lv_obj_t *g_calendar_widget = NULL;    /* 日历控件引用 */
static lv_obj_t *g_year_month_label = NULL;   /* 年月显示标签 */

static ui_alarm_query_current_fn s_query_current_cb = NULL;
static ui_alarm_clear_current_fn s_clear_current_cb = NULL;
static ui_alarm_load_settings_fn s_load_settings_cb = NULL;
static ui_alarm_save_settings_fn s_save_settings_cb = NULL;

static lv_obj_t *s_setting_threshold_inputs[UI_ALARM_SETTINGS_COUNT] = {NULL};
static lv_obj_t *s_setting_duration_inputs[UI_ALARM_SETTINGS_COUNT] = {NULL};
static lv_obj_t *s_setting_action_dropdowns[UI_ALARM_SETTINGS_COUNT] = {NULL};
static ui_alarm_settings_t s_cached_settings;
static bool s_settings_cache_valid = false;

static const ui_alarm_settings_t s_default_settings = {
    .items = {
        {"0.20", 10, 0},
        {"0.20", 10, 0},
        {"4.00", 10, 0},
        {"10.00", 10, 0},
        {"0.10", 10, 0},
        {"4.00", 5, 0},
        {"1.00", 30, 0},
        {"2.50", 3, 0},
        {"0.05", 3, 0},
        {"0.60", 5, 0}
    }
};

/* 标签页名称 */
static const char *tab_names[TAB_COUNT] = {
    "当前报警",
    "历史报警",
    "报警设置",
    "掉线记录",
    "上电记录"
};

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 显示告警管理对话框
 */
void ui_alarm_show(lv_obj_t *parent)
{
    /* 如果对话框已存在，先删除 */
    if (g_alarm_dialog != NULL) {
        lv_obj_del(g_alarm_dialog);
        g_alarm_dialog = NULL;
    }

    /* 创建外层蓝色边框 */
    g_alarm_dialog = lv_obj_create(parent);
    lv_obj_set_size(g_alarm_dialog, 1270, 790);
    lv_obj_center(g_alarm_dialog);
    lv_obj_set_style_bg_color(g_alarm_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_alarm_dialog, 0, 0);
    lv_obj_set_style_radius(g_alarm_dialog, 10, 0);
    lv_obj_set_style_pad_all(g_alarm_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(g_alarm_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景 */
    lv_obj_t *content = lv_obj_create(g_alarm_dialog);
    lv_obj_set_size(content, 1260, 780);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 8, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建标签页按钮 */
    create_tab_buttons(content);

    /* 创建内容区容器 */
    g_tab_content = lv_obj_create(content);
    lv_obj_set_size(g_tab_content, 1240, 680);  /* 内容区高度 */
    lv_obj_set_pos(g_tab_content, 10, 90);
    lv_obj_set_style_bg_color(g_tab_content, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_tab_content, 0, 0);
    lv_obj_set_style_radius(g_tab_content, 0, 0);
    lv_obj_set_style_pad_all(g_tab_content, 0, 0);
    lv_obj_clear_flag(g_tab_content, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建关闭按钮 */
    create_close_button(content);

    /* 创建清除报警按钮 */
    create_clear_alarm_button(content);

    /* 默认显示当前报警标签页 */
    g_current_tab = TAB_CURRENT;
    switch_tab(TAB_CURRENT);
}

/**
 * @brief 关闭告警管理对话框
 */
void ui_alarm_close(void)
{
    /* 先关闭日历弹窗(如果存在) */
    if (g_calendar_popup != NULL) {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
        g_current_date_input = NULL;
        g_calendar_widget = NULL;
        g_year_month_label = NULL;
    }

    ui_alarm_rec_invalidate();

    if (g_alarm_dialog != NULL) {
        lv_obj_del(g_alarm_dialog);
        g_alarm_dialog = NULL;
        g_tab_content = NULL;
        g_clear_btn = NULL;
        g_input_start_date = NULL;
        g_input_end_date = NULL;
        for (int i = 0; i < TAB_COUNT; i++) {
            g_tab_buttons[i] = NULL;
        }
    }
}

/**
 * @brief 检查告警对话框是否正在显示
 */
bool ui_alarm_is_visible(void)
{
    return (g_alarm_dialog != NULL);
}

void ui_alarm_register_query_current_cb(ui_alarm_query_current_fn fn)
{
    s_query_current_cb = fn;
}

void ui_alarm_register_clear_current_cb(ui_alarm_clear_current_fn fn)
{
    s_clear_current_cb = fn;
}

void ui_alarm_register_load_settings_cb(ui_alarm_load_settings_fn fn)
{
    s_load_settings_cb = fn;
    s_settings_cache_valid = false;
}

void ui_alarm_register_save_settings_cb(ui_alarm_save_settings_fn fn)
{
    s_save_settings_cb = fn;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 创建标签页按钮
 */
static void create_tab_buttons(lv_obj_t *parent)
{
    int btn_width = 150;
    int btn_height = 50;
    int btn_spacing = 10;
    int start_x = 20;  /* 往左移动，从80改为20 */
    int start_y = 20;

    for (int i = 0; i < TAB_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, btn_width, btn_height);
        lv_obj_set_pos(btn, start_x + i * (btn_width + btn_spacing), start_y);

        /* 默认未选中状态：白色背景，灰色边框 */
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_radius(btn, 8, 0);

        /* 添加点击事件 */
        lv_obj_add_event_cb(btn, tab_btn_click_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        /* 添加标签文字 */
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, tab_names[i]);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, COLOR_TEXT_MAIN, 0);
        lv_obj_center(label);

        g_tab_buttons[i] = btn;
    }

    /* 默认选中第一个标签页 */
    if (g_tab_buttons[0]) {
        lv_obj_set_style_bg_color(g_tab_buttons[0], COLOR_PRIMARY, 0);
        lv_obj_t *label = lv_obj_get_child(g_tab_buttons[0], 0);
        if (label) {
            lv_obj_set_style_text_color(label, lv_color_white(), 0);
        }
    }
}

/**
 * @brief 创建关闭按钮（右上角X）
 */
static void create_close_button(lv_obj_t *parent)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 50, 50);
    lv_obj_set_pos(btn, 1195, 15);  /* 右上角 */
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 25, 0);
    lv_obj_add_event_cb(btn, close_btn_click_cb, LV_EVENT_CLICKED, NULL);

    /* X符号 */
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT_GRAY, 0);
    lv_obj_center(label);
}

/**
 * @brief 创建清除报警标题
 */
static void create_clear_alarm_button(lv_obj_t *parent)
{
    /* 清除报警文字将在create_current_alarm_content中创建 */
    /* 这个函数现在是空的，但保留以保持代码结构 */
}

/**
 * @brief 标签页按钮点击回调
 */
static void tab_btn_click_cb(lv_event_t *e)
{
    alarm_tab_t tab = (alarm_tab_t)(intptr_t)lv_event_get_user_data(e);
    switch_tab(tab);
}

/**
 * @brief 关闭按钮点击回调
 */
static void close_btn_click_cb(lv_event_t *e)
{
    (void)e;
    ui_alarm_close();
}

/**
 * @brief 清除报警按钮点击回调
 */
static void clear_alarm_btn_click_cb(lv_event_t *e)
{
    (void)e;

    if (s_clear_current_cb) {
        s_clear_current_cb();
    }

    if (g_current_tab == TAB_CURRENT) {
        switch_tab(TAB_CURRENT);
    }
}

/**
 * @brief 切换标签页
 */
static void switch_tab(alarm_tab_t tab)
{
    if (tab >= TAB_COUNT) return;

    g_current_tab = tab;

    /* 关闭日历弹窗(如果存在) */
    if (g_calendar_popup != NULL) {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
        g_current_date_input = NULL;
        g_calendar_widget = NULL;
        g_year_month_label = NULL;
    }

    /* 更新标签页按钮样式 */
    for (int i = 0; i < TAB_COUNT; i++) {
        if (g_tab_buttons[i]) {
            lv_obj_t *label = lv_obj_get_child(g_tab_buttons[i], 0);

            if (i == tab) {
                /* 选中：蓝色背景，白色文字 */
                lv_obj_set_style_bg_color(g_tab_buttons[i], COLOR_PRIMARY, 0);
                lv_obj_set_style_border_color(g_tab_buttons[i], COLOR_PRIMARY, 0);
                if (label) {
                    lv_obj_set_style_text_color(label, lv_color_white(), 0);
                }
            } else {
                /* 未选中：白色背景，灰色边框，深色文字 */
                lv_obj_set_style_bg_color(g_tab_buttons[i], lv_color_white(), 0);
                lv_obj_set_style_border_color(g_tab_buttons[i], lv_color_hex(0xcccccc), 0);
                if (label) {
                    lv_obj_set_style_text_color(label, COLOR_TEXT_MAIN, 0);
                }
            }
        }
    }

    /* 清空内容区 */
    if (g_tab_content) {
        ui_alarm_rec_invalidate();
        lv_obj_clean(g_tab_content);

        /* 清空后，这些指针都失效了，需要重置 */
        g_clear_btn = NULL;
        g_input_start_date = NULL;
        g_input_end_date = NULL;
        memset(s_setting_threshold_inputs, 0, sizeof(s_setting_threshold_inputs));
        memset(s_setting_duration_inputs, 0, sizeof(s_setting_duration_inputs));
        memset(s_setting_action_dropdowns, 0, sizeof(s_setting_action_dropdowns));

        /* 根据标签页加载不同内容 */
        switch (tab) {
            case TAB_CURRENT:
                create_current_alarm_content(g_tab_content);
                break;

            case TAB_HISTORY:
                create_history_alarm_content(g_tab_content);
                break;

            case TAB_SETTINGS:
                create_settings_content(g_tab_content);
                break;

            case TAB_OFFLINE:
                create_offline_content(g_tab_content);
                break;

            case TAB_POWERON:
                create_poweron_content(g_tab_content);
                break;

            default:
                break;
        }
    }
}

/**
 * @brief 获取当前日期字符串 (YYYY-MM-DD)
 */
static void get_today_str(char *buf, int buf_size)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year + 1900 < 2024) {
        snprintf(buf, buf_size, "2026-01-01");
    } else {
        snprintf(buf, buf_size, "%04d-%02d-%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    }
}

/**
 * @brief 从输入框文本解析日期，设置日历显示月份
 */
static void calendar_set_from_input(lv_obj_t *calendar, lv_obj_t *input, lv_obj_t *ym_label)
{
    int year = 2026, month = 1, day = 1;
    if (input) {
        const char *text = lv_textarea_get_text(input);
        if (text && strlen(text) >= 10) {
            sscanf(text, "%d-%d-%d", &year, &month, &day);
        }
    }
    lv_calendar_set_showed_date(calendar, year, month);
    lv_calendar_set_today_date(calendar, year, month, day);
    if (ym_label) {
        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%04d-%02d", year, month);
        lv_label_set_text(ym_label, date_str);
    }
}

/**
 * @brief 创建当前报警内容
 */
static void create_current_alarm_content(lv_obj_t *parent)
{
    ui_alarm_current_item_t items[UI_ALARM_MAX_CURRENT] = {0};
    size_t count = 0;
    esp_err_t ret = ESP_ERR_NOT_FOUND;

    /* 创建表格表头背景 */
    lv_obj_t *header_bg = lv_obj_create(parent);
    lv_obj_set_size(header_bg, 1220, 50);
    lv_obj_set_pos(header_bg, 10, 10);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f8ff), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);
    lv_obj_clear_flag(header_bg, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "报警发生时间", "报警原因"};
    int header_widths[] = {150, 400, 670};
    int x_pos = 0;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(header_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_size(header_label, header_widths[i], 50);
        lv_obj_set_pos(header_label, x_pos, 0);
        lv_obj_set_style_text_align(header_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(header_label, 17, 0);
        x_pos += header_widths[i];
    }

    g_clear_btn = lv_label_create(header_bg);
    lv_label_set_text(g_clear_btn, "清除报警");
    lv_obj_set_style_text_font(g_clear_btn, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(g_clear_btn, COLOR_PRIMARY, 0);
    lv_obj_set_pos(g_clear_btn, 1120, 17);
    lv_obj_add_flag(g_clear_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_clear_btn, clear_alarm_btn_click_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *table_area = lv_obj_create(parent);
    lv_obj_set_size(table_area, 1220, 610);
    lv_obj_set_pos(table_area, 10, 60);
    lv_obj_set_style_bg_color(table_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(table_area, 0, 0);
    lv_obj_set_style_radius(table_area, 0, 0);
    lv_obj_set_style_pad_all(table_area, 0, 0);
    lv_obj_clear_flag(table_area, LV_OBJ_FLAG_SCROLLABLE);

    if (s_query_current_cb) {
        ret = s_query_current_cb(items, UI_ALARM_MAX_CURRENT, &count);
    }

    if (ret != ESP_OK || count == 0) {
        lv_obj_t *empty_label = lv_label_create(table_area);
        lv_label_set_text(empty_label, "暂无报警记录");
        lv_obj_set_style_text_font(empty_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(empty_label, COLOR_TEXT_GRAY, 0);
        lv_obj_center(empty_label);
        return;
    }

    for (size_t i = 0; i < count; i++) {
        int y = (int)i * 50;
        int cell_x = 0;
        char idx_buf[8];
        char time_buf[48];

        if ((i % 2U) == 1U) {
            lv_obj_t *row_bg = lv_obj_create(table_area);
            lv_obj_set_size(row_bg, 1220, 50);
            lv_obj_set_pos(row_bg, 0, y);
            lv_obj_set_style_bg_color(row_bg, lv_color_hex(0xf8f8f8), 0);
            lv_obj_set_style_border_width(row_bg, 0, 0);
            lv_obj_set_style_radius(row_bg, 0, 0);
            lv_obj_set_style_pad_all(row_bg, 0, 0);
            lv_obj_clear_flag(row_bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        }

        snprintf(idx_buf, sizeof(idx_buf), "%u", (unsigned)(i + 1U));
        format_alarm_time(time_buf, sizeof(time_buf), items[i].timestamp);

        lv_obj_t *idx_label = lv_label_create(table_area);
        lv_label_set_text(idx_label, idx_buf);
        lv_obj_set_style_text_font(idx_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(idx_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_size(idx_label, header_widths[0], 50);
        lv_obj_set_pos(idx_label, cell_x, y);
        lv_obj_set_style_text_align(idx_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(idx_label, 17, 0);
        cell_x += header_widths[0];

        lv_obj_t *time_label = lv_label_create(table_area);
        lv_label_set_text(time_label, time_buf);
        lv_obj_set_style_text_font(time_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(time_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_size(time_label, header_widths[1], 50);
        lv_obj_set_pos(time_label, cell_x, y);
        lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(time_label, 17, 0);
        cell_x += header_widths[1];

        lv_obj_t *desc_label = lv_label_create(table_area);
        lv_label_set_text(desc_label, items[i].desc);
        lv_obj_set_style_text_font(desc_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(desc_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_size(desc_label, header_widths[2], 50);
        lv_obj_set_pos(desc_label, cell_x, y);
        lv_obj_set_style_text_align(desc_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(desc_label, 17, 0);
    }
}

/**
 * @brief 创建历史报警内容
 */
static void create_history_alarm_content(lv_obj_t *parent)
{
    char today_buf[32];
    get_today_str(today_buf, sizeof(today_buf));
    int y_pos = 10;

    /* 日期标签 */
    lv_obj_t *date_label = lv_label_create(parent);
    lv_label_set_text(date_label, "日期:");
    lv_obj_set_pos(date_label, 20, y_pos + 8);
    lv_obj_set_style_text_font(date_label, &my_font_cn_16, 0);

    /* 起始日期输入框容器 */
    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 240, 35);
    lv_obj_set_pos(start_date_container, 80, y_pos + 3);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 1, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 5, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 起始日期输入框 */
    g_input_start_date = lv_textarea_create(start_date_container);
    lv_obj_set_size(g_input_start_date, 200, 33);
    lv_obj_set_pos(g_input_start_date, 1, 1);
    lv_textarea_set_one_line(g_input_start_date, true);
    lv_textarea_set_text(g_input_start_date, today_buf);
    lv_obj_set_style_text_font(g_input_start_date, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_input_start_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_input_start_date, 0, 0);
    lv_obj_set_style_pad_right(g_input_start_date, 0, 0);
    lv_obj_set_style_pad_top(g_input_start_date, 6, 0);
    lv_obj_set_style_pad_bottom(g_input_start_date, 0, 0);
    lv_obj_set_style_border_width(g_input_start_date, 0, 0);
    lv_obj_clear_flag(g_input_start_date, LV_OBJ_FLAG_CLICKABLE);

    /* 起始日期日历按钮 */
    lv_obj_t *btn_start_cal = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_start_cal, 33, 33);
    lv_obj_set_pos(btn_start_cal, 206, 1);
    lv_obj_set_style_bg_color(btn_start_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_start_cal, 0, 0);
    lv_obj_set_style_radius(btn_start_cal, 4, 0);
    lv_obj_add_event_cb(btn_start_cal, btn_calendar_start_cb, LV_EVENT_CLICKED, g_input_start_date);

    lv_obj_t *icon_start = lv_label_create(btn_start_cal);
    lv_label_set_text(icon_start, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_start, &my_font_cn_16, 0);
    lv_obj_center(icon_start);

    /* 至 */
    lv_obj_t *to_label = lv_label_create(parent);
    lv_label_set_text(to_label, "至");
    lv_obj_set_pos(to_label, 345, y_pos + 8);
    lv_obj_set_style_text_font(to_label, &my_font_cn_16, 0);

    /* 结束日期输入框容器 */
    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 240, 35);
    lv_obj_set_pos(end_date_container, 385, y_pos + 3);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 1, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 5, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 结束日期输入框 */
    g_input_end_date = lv_textarea_create(end_date_container);
    lv_obj_set_size(g_input_end_date, 200, 33);
    lv_obj_set_pos(g_input_end_date, 1, 1);
    lv_textarea_set_one_line(g_input_end_date, true);
    lv_textarea_set_text(g_input_end_date, today_buf);
    lv_obj_set_style_text_font(g_input_end_date, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_input_end_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_input_end_date, 0, 0);
    lv_obj_set_style_pad_right(g_input_end_date, 0, 0);
    lv_obj_set_style_pad_top(g_input_end_date, 6, 0);
    lv_obj_set_style_pad_bottom(g_input_end_date, 0, 0);
    lv_obj_set_style_border_width(g_input_end_date, 0, 0);
    lv_obj_clear_flag(g_input_end_date, LV_OBJ_FLAG_CLICKABLE);

    /* 结束日期日历按钮 */
    lv_obj_t *btn_end_cal = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_end_cal, 33, 33);
    lv_obj_set_pos(btn_end_cal, 206, 1);
    lv_obj_set_style_bg_color(btn_end_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_end_cal, 0, 0);
    lv_obj_set_style_radius(btn_end_cal, 4, 0);
    lv_obj_add_event_cb(btn_end_cal, btn_calendar_end_cb, LV_EVENT_CLICKED, g_input_end_date);

    lv_obj_t *icon_end = lv_label_create(btn_end_cal);
    lv_label_set_text(icon_end, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_end, &my_font_cn_16, 0);
    lv_obj_center(icon_end);

    /* 查询按钮 */
    lv_obj_t *btn_query = lv_btn_create(parent);
    lv_obj_set_size(btn_query, 100, 35);
    lv_obj_set_pos(btn_query, 670, y_pos + 3);
    lv_obj_set_style_bg_color(btn_query, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_query, 0, 0);
    lv_obj_set_style_radius(btn_query, 5, 0);
    lv_obj_add_event_cb(btn_query, ui_alarm_rec_history_query_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *query_label = lv_label_create(btn_query);
    lv_label_set_text(query_label, "查询");
    lv_obj_set_style_text_color(query_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(query_label, &my_font_cn_16, 0);
    lv_obj_center(query_label);

    lv_obj_t *header_bg = lv_obj_create(parent);
    lv_obj_set_size(header_bg, 1220, 50);
    lv_obj_set_pos(header_bg, 10, 60);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f8ff), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);
    lv_obj_clear_flag(header_bg, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "发生时间", "描述"};
    int header_widths[] = {150, 730, 340};
    int x_pos = 0;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(header_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_size(header_label, header_widths[i], 50);
        lv_obj_set_pos(header_label, x_pos, 0);
        lv_obj_set_style_text_align(header_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(header_label, 17, 0);
        x_pos += header_widths[i];
    }

    lv_obj_t *table_area = lv_obj_create(parent);
    lv_obj_set_size(table_area, 1220, 560);
    lv_obj_set_pos(table_area, 10, 110);
    lv_obj_set_style_bg_color(table_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(table_area, 0, 0);
    lv_obj_set_style_radius(table_area, 0, 0);
    lv_obj_set_style_pad_all(table_area, 0, 0);
    lv_obj_clear_flag(table_area, LV_OBJ_FLAG_SCROLLABLE);

    int page_y = 630;

    lv_obj_t *btn_first = lv_btn_create(parent);
    lv_obj_set_size(btn_first, 80, 35);
    lv_obj_set_pos(btn_first, 400, page_y);
    lv_obj_set_style_bg_color(btn_first, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_first, 5, 0);
    lv_obj_t *label_first = lv_label_create(btn_first);
    lv_label_set_text(label_first, "首页");
    lv_obj_set_style_text_font(label_first, &my_font_cn_16, 0);
    lv_obj_center(label_first);

    lv_obj_t *btn_prev = lv_btn_create(parent);
    lv_obj_set_size(btn_prev, 80, 35);
    lv_obj_set_pos(btn_prev, 490, page_y);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_prev, 5, 0);
    lv_obj_t *label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, "上一页");
    lv_obj_set_style_text_font(label_prev, &my_font_cn_16, 0);
    lv_obj_center(label_prev);

    lv_obj_t *page_info = lv_label_create(parent);
    lv_label_set_text(page_info, "0/0");
    lv_obj_set_pos(page_info, 595, page_y + 8);
    lv_obj_set_style_text_font(page_info, &my_font_cn_16, 0);

    lv_obj_t *btn_next = lv_btn_create(parent);
    lv_obj_set_size(btn_next, 80, 35);
    lv_obj_set_pos(btn_next, 655, page_y);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_next, 5, 0);
    lv_obj_t *label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, "下一页");
    lv_obj_set_style_text_font(label_next, &my_font_cn_16, 0);
    lv_obj_center(label_next);

    lv_obj_t *btn_last = lv_btn_create(parent);
    lv_obj_set_size(btn_last, 80, 35);
    lv_obj_set_pos(btn_last, 745, page_y);
    lv_obj_set_style_bg_color(btn_last, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_last, 5, 0);
    lv_obj_t *label_last = lv_label_create(btn_last);
    lv_label_set_text(label_last, "尾页");
    lv_obj_set_style_text_font(label_last, &my_font_cn_16, 0);
    lv_obj_center(label_last);

    ui_alarm_rec_setup_history_alarm(g_input_start_date, g_input_end_date,
        table_area, page_info, btn_first, btn_prev, btn_next, btn_last);
}

/**
 * @brief 创建报警设置内容
 */
static void create_settings_content(lv_obj_t *parent)
{
    typedef struct {
        const char *label;
        const char *dropdown_options;
    } alarm_setting_row_t;

    static const alarm_setting_row_t setting_rows[UI_ALARM_SETTINGS_COUNT] = {
        {"PH1和PH2差值大于", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"EC1和EC2差值大于(mS/cm):", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"PH低于:", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"PH高于:", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"EC低于(mS/cm):", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"EC高于(mS/cm):", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"主管道流速低于(m³/h):", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"肥桶液位高于(m):", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"肥桶液位低于(m):", "不触发报警\n仅触发报警\n报警并停止灌溉"},
        {"肥管压力高于(MPa):", "不触发报警\n仅触发报警\n报警并停止灌溉"}
    };

    int start_y = 15;
    int row_height = 55;
    int value_width = 280;
    int dropdown_width = 280;

    ensure_settings_cache_loaded();

    for (int i = 0; i < UI_ALARM_SETTINGS_COUNT; i++) {
        int y = start_y + i * row_height;

        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, setting_rows[i].label);
        lv_obj_set_pos(label, 20, y + 12);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, COLOR_TEXT_MAIN, 0);

        lv_obj_t *value_input = lv_textarea_create(parent);
        lv_obj_set_size(value_input, value_width, 40);
        lv_obj_set_pos(value_input, 280, y);
        lv_textarea_set_one_line(value_input, true);
        lv_obj_set_style_text_font(value_input, &my_font_cn_16, 0);
        lv_obj_set_style_text_align(value_input, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_bg_color(value_input, lv_color_hex(0xf5f5f5), 0);
        lv_obj_set_style_border_color(value_input, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_border_width(value_input, 1, 0);
        lv_obj_set_style_radius(value_input, 5, 0);
        lv_obj_set_style_pad_left(value_input, 10, 0);
        lv_textarea_set_accepted_chars(value_input, "0123456789.");
        lv_textarea_set_text_selection(value_input, true);
        lv_obj_add_event_cb(value_input, input_click_cb, LV_EVENT_CLICKED, NULL);
        s_setting_threshold_inputs[i] = value_input;

        lv_obj_t *duration_label = lv_label_create(parent);
        lv_label_set_text(duration_label, "持续时长(S):");
        lv_obj_set_pos(duration_label, 585, y + 12);
        lv_obj_set_style_text_font(duration_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(duration_label, COLOR_TEXT_MAIN, 0);

        lv_obj_t *duration_input = lv_textarea_create(parent);
        lv_obj_set_size(duration_input, 100, 40);
        lv_obj_set_pos(duration_input, 720, y);
        lv_textarea_set_one_line(duration_input, true);
        lv_obj_set_style_text_font(duration_input, &my_font_cn_16, 0);
        lv_obj_set_style_text_align(duration_input, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_bg_color(duration_input, lv_color_hex(0xf5f5f5), 0);
        lv_obj_set_style_border_color(duration_input, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_border_width(duration_input, 1, 0);
        lv_obj_set_style_radius(duration_input, 5, 0);
        lv_obj_set_style_pad_left(duration_input, 10, 0);
        lv_textarea_set_accepted_chars(duration_input, "0123456789");
        lv_textarea_set_text_selection(duration_input, true);
        lv_obj_add_event_cb(duration_input, input_click_cb, LV_EVENT_CLICKED, NULL);
        s_setting_duration_inputs[i] = duration_input;

        lv_obj_t *then_label = lv_label_create(parent);
        lv_label_set_text(then_label, "则");
        lv_obj_set_pos(then_label, 850, y + 12);
        lv_obj_set_style_text_font(then_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(then_label, COLOR_TEXT_MAIN, 0);

        lv_obj_t *dropdown = lv_dropdown_create(parent);
        lv_obj_set_size(dropdown, dropdown_width, 40);
        lv_obj_set_pos(dropdown, 900, y);
        lv_dropdown_set_options(dropdown, setting_rows[i].dropdown_options);
        lv_obj_set_style_text_font(dropdown, &my_font_cn_16, 0);
        lv_obj_set_style_text_font(lv_dropdown_get_list(dropdown), &my_font_cn_16, 0);
        lv_obj_set_style_bg_color(dropdown, lv_color_white(), 0);
        lv_obj_set_style_border_color(dropdown, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_border_width(dropdown, 1, 0);
        lv_obj_set_style_radius(dropdown, 5, 0);
        s_setting_action_dropdowns[i] = dropdown;
    }

    restore_settings_controls_from_cache();

    int btn_y = 610;
    int btn_width = 150;
    int btn_height = 50;

    lv_obj_t *cancel_btn = lv_btn_create(parent);
    lv_obj_set_size(cancel_btn, btn_width, btn_height);
    lv_obj_set_pos(cancel_btn, 450, btn_y);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_border_width(cancel_btn, 0, 0);
    lv_obj_set_style_radius(cancel_btn, 25, 0);
    lv_obj_add_event_cb(cancel_btn, settings_cancel_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "取消设置");
    lv_obj_set_style_text_font(cancel_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(cancel_label, COLOR_TEXT_MAIN, 0);
    lv_obj_center(cancel_label);

    lv_obj_t *save_btn = lv_btn_create(parent);
    lv_obj_set_size(save_btn, btn_width, btn_height);
    lv_obj_set_pos(save_btn, 630, btn_y);
    lv_obj_set_style_bg_color(save_btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(save_btn, 0, 0);
    lv_obj_set_style_radius(save_btn, 25, 0);
    lv_obj_add_event_cb(save_btn, settings_save_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "保存设置");
    lv_obj_set_style_text_font(save_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(save_label, lv_color_white(), 0);
    lv_obj_center(save_label);
}

/**
 * @brief 创建掉线记录内容
 */
static void create_offline_content(lv_obj_t *parent)
{
    char today_buf[32];
    get_today_str(today_buf, sizeof(today_buf));
    int y_pos = 10;

    /* 日期标签 */
    lv_obj_t *date_label = lv_label_create(parent);
    lv_label_set_text(date_label, "日期:");
    lv_obj_set_pos(date_label, 20, y_pos + 8);
    lv_obj_set_style_text_font(date_label, &my_font_cn_16, 0);

    /* 起始日期输入框容器 */
    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 240, 35);
    lv_obj_set_pos(start_date_container, 80, y_pos + 3);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 1, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 5, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 起始日期输入框 */
    lv_obj_t *input_start_date = lv_textarea_create(start_date_container);
    lv_obj_set_size(input_start_date, 200, 33);
    lv_obj_set_pos(input_start_date, 1, 1);
    lv_textarea_set_one_line(input_start_date, true);
    lv_textarea_set_text(input_start_date, today_buf);
    lv_obj_set_style_text_font(input_start_date, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_start_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_start_date, 0, 0);
    lv_obj_set_style_pad_right(input_start_date, 0, 0);
    lv_obj_set_style_pad_top(input_start_date, 6, 0);
    lv_obj_set_style_pad_bottom(input_start_date, 0, 0);
    lv_obj_set_style_border_width(input_start_date, 0, 0);
    lv_obj_clear_flag(input_start_date, LV_OBJ_FLAG_CLICKABLE);

    /* 起始日期日历按钮 */
    lv_obj_t *btn_start_cal = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_start_cal, 33, 33);
    lv_obj_set_pos(btn_start_cal, 206, 1);
    lv_obj_set_style_bg_color(btn_start_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_start_cal, 0, 0);
    lv_obj_set_style_radius(btn_start_cal, 4, 0);
    lv_obj_add_event_cb(btn_start_cal, btn_calendar_start_cb, LV_EVENT_CLICKED, input_start_date);

    lv_obj_t *icon_start = lv_label_create(btn_start_cal);
    lv_label_set_text(icon_start, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_start, &my_font_cn_16, 0);
    lv_obj_center(icon_start);

    /* 至 */
    lv_obj_t *to_label = lv_label_create(parent);
    lv_label_set_text(to_label, "至");
    lv_obj_set_pos(to_label, 345, y_pos + 8);
    lv_obj_set_style_text_font(to_label, &my_font_cn_16, 0);

    /* 结束日期输入框容器 */
    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 240, 35);
    lv_obj_set_pos(end_date_container, 385, y_pos + 3);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 1, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 5, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 结束日期输入框 */
    lv_obj_t *input_end_date = lv_textarea_create(end_date_container);
    lv_obj_set_size(input_end_date, 200, 33);
    lv_obj_set_pos(input_end_date, 1, 1);
    lv_textarea_set_one_line(input_end_date, true);
    lv_textarea_set_text(input_end_date, today_buf);
    lv_obj_set_style_text_font(input_end_date, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_end_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_end_date, 0, 0);
    lv_obj_set_style_pad_right(input_end_date, 0, 0);
    lv_obj_set_style_pad_top(input_end_date, 6, 0);
    lv_obj_set_style_pad_bottom(input_end_date, 0, 0);
    lv_obj_set_style_border_width(input_end_date, 0, 0);
    lv_obj_clear_flag(input_end_date, LV_OBJ_FLAG_CLICKABLE);

    /* 结束日期日历按钮 */
    lv_obj_t *btn_end_cal = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_end_cal, 33, 33);
    lv_obj_set_pos(btn_end_cal, 206, 1);
    lv_obj_set_style_bg_color(btn_end_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_end_cal, 0, 0);
    lv_obj_set_style_radius(btn_end_cal, 4, 0);
    lv_obj_add_event_cb(btn_end_cal, btn_calendar_end_cb, LV_EVENT_CLICKED, input_end_date);

    lv_obj_t *icon_end = lv_label_create(btn_end_cal);
    lv_label_set_text(icon_end, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_end, &my_font_cn_16, 0);
    lv_obj_center(icon_end);

    /* 查询按钮 */
    lv_obj_t *btn_query = lv_btn_create(parent);
    lv_obj_set_size(btn_query, 100, 35);
    lv_obj_set_pos(btn_query, 670, y_pos + 3);
    lv_obj_set_style_bg_color(btn_query, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_query, 0, 0);
    lv_obj_set_style_radius(btn_query, 5, 0);
    lv_obj_add_event_cb(btn_query, ui_alarm_rec_offline_query_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *query_label = lv_label_create(btn_query);
    lv_label_set_text(query_label, "查询");
    lv_obj_set_style_text_color(query_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(query_label, &my_font_cn_16, 0);
    lv_obj_center(query_label);

    /* 全部下拉框 */
    lv_obj_t *dropdown_all = lv_dropdown_create(parent);
    lv_obj_set_size(dropdown_all, 150, 35);
    lv_obj_set_pos(dropdown_all, 1060, y_pos + 3);
    lv_dropdown_set_options(dropdown_all, "全部");
    lv_obj_set_style_text_font(dropdown_all, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dropdown_all), &my_font_cn_16, 0);
    lv_obj_set_style_bg_color(dropdown_all, lv_color_white(), 0);
    lv_obj_set_style_border_color(dropdown_all, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_border_width(dropdown_all, 1, 0);
    lv_obj_set_style_radius(dropdown_all, 5, 0);

    /* 创建表格表头背景 */
    lv_obj_t *header_bg = lv_obj_create(parent);
    lv_obj_set_size(header_bg, 1220, 50);
    lv_obj_set_pos(header_bg, 10, 60);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f8ff), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);
    lv_obj_clear_flag(header_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头列 */
    const char *headers[] = {"序号", "发生时间", "类型"};
    int header_widths[] = {150, 730, 340};
    int x_pos = 0;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(header_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_size(header_label, header_widths[i], 50);
        lv_obj_set_pos(header_label, x_pos, 0);
        lv_obj_set_style_text_align(header_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(header_label, 17, 0);
        x_pos += header_widths[i];
    }

    /* 表格数据区域容器 */
    lv_obj_t *table_area = lv_obj_create(parent);
    lv_obj_set_size(table_area, 1220, 560);
    lv_obj_set_pos(table_area, 10, 110);
    lv_obj_set_style_bg_color(table_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(table_area, 0, 0);
    lv_obj_set_style_radius(table_area, 0, 0);
    lv_obj_set_style_pad_all(table_area, 0, 0);
    lv_obj_clear_flag(table_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 底部分页控件 */
    int page_y = 630;

    lv_obj_t *btn_first = lv_btn_create(parent);
    lv_obj_set_size(btn_first, 80, 35);
    lv_obj_set_pos(btn_first, 400, page_y);
    lv_obj_set_style_bg_color(btn_first, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_first, 5, 0);
    lv_obj_t *label_first = lv_label_create(btn_first);
    lv_label_set_text(label_first, "首页");
    lv_obj_set_style_text_font(label_first, &my_font_cn_16, 0);
    lv_obj_center(label_first);

    lv_obj_t *btn_prev = lv_btn_create(parent);
    lv_obj_set_size(btn_prev, 80, 35);
    lv_obj_set_pos(btn_prev, 490, page_y);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_prev, 5, 0);
    lv_obj_t *label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, "上一页");
    lv_obj_set_style_text_font(label_prev, &my_font_cn_16, 0);
    lv_obj_center(label_prev);

    lv_obj_t *page_info = lv_label_create(parent);
    lv_label_set_text(page_info, "0/0");
    lv_obj_set_pos(page_info, 595, page_y + 8);
    lv_obj_set_style_text_font(page_info, &my_font_cn_16, 0);

    lv_obj_t *btn_next = lv_btn_create(parent);
    lv_obj_set_size(btn_next, 80, 35);
    lv_obj_set_pos(btn_next, 655, page_y);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_next, 5, 0);
    lv_obj_t *label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, "下一页");
    lv_obj_set_style_text_font(label_next, &my_font_cn_16, 0);
    lv_obj_center(label_next);

    lv_obj_t *btn_last = lv_btn_create(parent);
    lv_obj_set_size(btn_last, 80, 35);
    lv_obj_set_pos(btn_last, 745, page_y);
    lv_obj_set_style_bg_color(btn_last, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_last, 5, 0);
    lv_obj_t *label_last = lv_label_create(btn_last);
    lv_label_set_text(label_last, "尾页");
    lv_obj_set_style_text_font(label_last, &my_font_cn_16, 0);
    lv_obj_center(label_last);

    /* 注册掉线记录模块 */
    ui_alarm_rec_setup_offline(input_start_date, input_end_date,
        table_area, page_info, btn_first, btn_prev, btn_next, btn_last);
}

/**
 * @brief 创建上电记录内容
 */
static void create_poweron_content(lv_obj_t *parent)
{
    char today_buf[32];
    get_today_str(today_buf, sizeof(today_buf));
    int y_pos = 10;

    /* 日期标签 */
    lv_obj_t *date_label = lv_label_create(parent);
    lv_label_set_text(date_label, "日期:");
    lv_obj_set_pos(date_label, 20, y_pos + 8);
    lv_obj_set_style_text_font(date_label, &my_font_cn_16, 0);

    /* 起始日期输入框容器 */
    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 240, 35);
    lv_obj_set_pos(start_date_container, 80, y_pos + 3);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 1, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 5, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 起始日期输入框 */
    lv_obj_t *input_start_date = lv_textarea_create(start_date_container);
    lv_obj_set_size(input_start_date, 200, 33);
    lv_obj_set_pos(input_start_date, 1, 1);
    lv_textarea_set_one_line(input_start_date, true);
    lv_textarea_set_text(input_start_date, today_buf);
    lv_obj_set_style_text_font(input_start_date, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_start_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_start_date, 0, 0);
    lv_obj_set_style_pad_right(input_start_date, 0, 0);
    lv_obj_set_style_pad_top(input_start_date, 6, 0);
    lv_obj_set_style_pad_bottom(input_start_date, 0, 0);
    lv_obj_set_style_border_width(input_start_date, 0, 0);
    lv_obj_clear_flag(input_start_date, LV_OBJ_FLAG_CLICKABLE);

    /* 起始日期日历按钮 */
    lv_obj_t *btn_start_cal = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_start_cal, 33, 33);
    lv_obj_set_pos(btn_start_cal, 206, 1);
    lv_obj_set_style_bg_color(btn_start_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_start_cal, 0, 0);
    lv_obj_set_style_radius(btn_start_cal, 4, 0);
    lv_obj_add_event_cb(btn_start_cal, btn_calendar_start_cb, LV_EVENT_CLICKED, input_start_date);

    lv_obj_t *icon_start = lv_label_create(btn_start_cal);
    lv_label_set_text(icon_start, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_start, &my_font_cn_16, 0);
    lv_obj_center(icon_start);

    /* 至 */
    lv_obj_t *to_label = lv_label_create(parent);
    lv_label_set_text(to_label, "至");
    lv_obj_set_pos(to_label, 345, y_pos + 8);
    lv_obj_set_style_text_font(to_label, &my_font_cn_16, 0);

    /* 结束日期输入框容器 */
    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 240, 35);
    lv_obj_set_pos(end_date_container, 385, y_pos + 3);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 1, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 5, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 结束日期输入框 */
    lv_obj_t *input_end_date = lv_textarea_create(end_date_container);
    lv_obj_set_size(input_end_date, 200, 33);
    lv_obj_set_pos(input_end_date, 1, 1);
    lv_textarea_set_one_line(input_end_date, true);
    lv_textarea_set_text(input_end_date, today_buf);
    lv_obj_set_style_text_font(input_end_date, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_end_date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_end_date, 0, 0);
    lv_obj_set_style_pad_right(input_end_date, 0, 0);
    lv_obj_set_style_pad_top(input_end_date, 6, 0);
    lv_obj_set_style_pad_bottom(input_end_date, 0, 0);
    lv_obj_set_style_border_width(input_end_date, 0, 0);
    lv_obj_clear_flag(input_end_date, LV_OBJ_FLAG_CLICKABLE);

    /* 结束日期日历按钮 */
    lv_obj_t *btn_end_cal = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_end_cal, 33, 33);
    lv_obj_set_pos(btn_end_cal, 206, 1);
    lv_obj_set_style_bg_color(btn_end_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_end_cal, 0, 0);
    lv_obj_set_style_radius(btn_end_cal, 4, 0);
    lv_obj_add_event_cb(btn_end_cal, btn_calendar_end_cb, LV_EVENT_CLICKED, input_end_date);

    lv_obj_t *icon_end = lv_label_create(btn_end_cal);
    lv_label_set_text(icon_end, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_end, &my_font_cn_16, 0);
    lv_obj_center(icon_end);

    /* 查询按钮 */
    lv_obj_t *btn_query = lv_btn_create(parent);
    lv_obj_set_size(btn_query, 100, 35);
    lv_obj_set_pos(btn_query, 670, y_pos + 3);
    lv_obj_set_style_bg_color(btn_query, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_query, 0, 0);
    lv_obj_set_style_radius(btn_query, 5, 0);
    lv_obj_add_event_cb(btn_query, ui_alarm_rec_poweron_query_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *query_label = lv_label_create(btn_query);
    lv_label_set_text(query_label, "查询");
    lv_obj_set_style_text_color(query_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(query_label, &my_font_cn_16, 0);
    lv_obj_center(query_label);

    /* 创建表格表头背景 */
    lv_obj_t *header_bg = lv_obj_create(parent);
    lv_obj_set_size(header_bg, 1220, 50);
    lv_obj_set_pos(header_bg, 10, 60);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f8ff), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);
    lv_obj_clear_flag(header_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头列 */
    const char *headers[] = {"序号", "发生时间", "描述"};
    int header_widths[] = {150, 730, 340};
    int x_pos = 0;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(header_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_size(header_label, header_widths[i], 50);
        lv_obj_set_pos(header_label, x_pos, 0);
        lv_obj_set_style_text_align(header_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_pad_top(header_label, 17, 0);
        x_pos += header_widths[i];
    }

    /* 表格数据区域容器 */
    lv_obj_t *table_area = lv_obj_create(parent);
    lv_obj_set_size(table_area, 1220, 560);
    lv_obj_set_pos(table_area, 10, 110);
    lv_obj_set_style_bg_color(table_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(table_area, 0, 0);
    lv_obj_set_style_radius(table_area, 0, 0);
    lv_obj_set_style_pad_all(table_area, 0, 0);
    lv_obj_clear_flag(table_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 底部分页控件 */
    int page_y = 630;

    lv_obj_t *btn_first = lv_btn_create(parent);
    lv_obj_set_size(btn_first, 80, 35);
    lv_obj_set_pos(btn_first, 400, page_y);
    lv_obj_set_style_bg_color(btn_first, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_first, 5, 0);
    lv_obj_t *label_first = lv_label_create(btn_first);
    lv_label_set_text(label_first, "首页");
    lv_obj_set_style_text_font(label_first, &my_font_cn_16, 0);
    lv_obj_center(label_first);

    lv_obj_t *btn_prev = lv_btn_create(parent);
    lv_obj_set_size(btn_prev, 80, 35);
    lv_obj_set_pos(btn_prev, 490, page_y);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_prev, 5, 0);
    lv_obj_t *label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, "上一页");
    lv_obj_set_style_text_font(label_prev, &my_font_cn_16, 0);
    lv_obj_center(label_prev);

    lv_obj_t *page_info = lv_label_create(parent);
    lv_label_set_text(page_info, "0/0");
    lv_obj_set_pos(page_info, 595, page_y + 8);
    lv_obj_set_style_text_font(page_info, &my_font_cn_16, 0);

    lv_obj_t *btn_next = lv_btn_create(parent);
    lv_obj_set_size(btn_next, 80, 35);
    lv_obj_set_pos(btn_next, 655, page_y);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_next, 5, 0);
    lv_obj_t *label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, "下一页");
    lv_obj_set_style_text_font(label_next, &my_font_cn_16, 0);
    lv_obj_center(label_next);

    lv_obj_t *btn_last = lv_btn_create(parent);
    lv_obj_set_size(btn_last, 80, 35);
    lv_obj_set_pos(btn_last, 745, page_y);
    lv_obj_set_style_bg_color(btn_last, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(btn_last, 5, 0);
    lv_obj_t *label_last = lv_label_create(btn_last);
    lv_label_set_text(label_last, "尾页");
    lv_obj_set_style_text_font(label_last, &my_font_cn_16, 0);
    lv_obj_center(label_last);

    /* 注册上电记录模块 */
    ui_alarm_rec_setup_poweron(input_start_date, input_end_date,
        table_area, page_info, btn_first, btn_prev, btn_next, btn_last);
}

static void ensure_settings_cache_loaded(void)
{
    if (s_settings_cache_valid) {
        return;
    }

    memcpy(&s_cached_settings, &s_default_settings, sizeof(s_cached_settings));
    if (s_load_settings_cb) {
        if (s_load_settings_cb(&s_cached_settings) != ESP_OK) {
            memcpy(&s_cached_settings, &s_default_settings, sizeof(s_cached_settings));
        }
    }

    s_settings_cache_valid = true;
}

static void restore_settings_controls_from_cache(void)
{
    ensure_settings_cache_loaded();

    for (int i = 0; i < UI_ALARM_SETTINGS_COUNT; i++) {
        if (s_setting_threshold_inputs[i]) {
            lv_textarea_set_text(s_setting_threshold_inputs[i], s_cached_settings.items[i].threshold);
        }
        if (s_setting_duration_inputs[i]) {
            char duration_buf[16];
            snprintf(duration_buf, sizeof(duration_buf), "%u", s_cached_settings.items[i].duration_s);
            lv_textarea_set_text(s_setting_duration_inputs[i], duration_buf);
        }
        if (s_setting_action_dropdowns[i]) {
            uint16_t action = s_cached_settings.items[i].action;
            if (action > 2U) {
                action = 0;
            }
            lv_dropdown_set_selected(s_setting_action_dropdowns[i], action);
        }
    }
}

static void collect_settings_from_controls(ui_alarm_settings_t *settings)
{
    if (!settings) {
        return;
    }

    memset(settings, 0, sizeof(*settings));
    for (int i = 0; i < UI_ALARM_SETTINGS_COUNT; i++) {
        if (s_setting_threshold_inputs[i]) {
            const char *text = lv_textarea_get_text(s_setting_threshold_inputs[i]);
            strncpy(settings->items[i].threshold, text ? text : "",
                sizeof(settings->items[i].threshold) - 1);
            settings->items[i].threshold[sizeof(settings->items[i].threshold) - 1] = '\0';
        }
        if (s_setting_duration_inputs[i]) {
            const char *text = lv_textarea_get_text(s_setting_duration_inputs[i]);
            settings->items[i].duration_s = (uint16_t)atoi(text ? text : "0");
        }
        if (s_setting_action_dropdowns[i]) {
            settings->items[i].action = (uint8_t)lv_dropdown_get_selected(s_setting_action_dropdowns[i]);
        }
    }
}

static void settings_cancel_btn_cb(lv_event_t *e)
{
    (void)e;
    restore_settings_controls_from_cache();
}

static void settings_save_btn_cb(lv_event_t *e)
{
    (void)e;

    ui_alarm_settings_t settings;
    collect_settings_from_controls(&settings);

    if (s_save_settings_cb) {
        if (s_save_settings_cb(&settings) != ESP_OK) {
            restore_settings_controls_from_cache();
            return;
        }
    }

    memcpy(&s_cached_settings, &settings, sizeof(s_cached_settings));
    s_settings_cache_valid = true;
}

static void format_alarm_time(char *buf, size_t buf_size, int64_t timestamp)
{
    if (!buf || buf_size == 0U) {
        return;
    }

    if (timestamp < TIME_SYNC_THRESHOLD) {
        snprintf(buf, buf_size, "时间未同步");
        return;
    }

    time_t ts = (time_t)timestamp;
    struct tm timeinfo;
    localtime_r(&ts, &timeinfo);
    snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}


/**
 * @brief 开始日期日历按钮回调
 */
static void btn_calendar_start_cb(lv_event_t *e)
{
    /* 获取传入的输入框引用 */
    lv_obj_t *input_box = (lv_obj_t *)lv_event_get_user_data(e);

    /* 如果已经有弹窗,先删除 */
    if (g_calendar_popup) {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
    }

    g_current_date_input = input_box ? input_box : g_input_start_date;

    /* 创建日历弹窗 */
    g_calendar_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_calendar_popup, 465, 390);
    lv_obj_center(g_calendar_popup);
    lv_obj_set_style_bg_color(g_calendar_popup, lv_color_white(), 0);
    lv_obj_set_style_radius(g_calendar_popup, 10, 0);
    lv_obj_clear_flag(g_calendar_popup, LV_OBJ_FLAG_SCROLLABLE);

    /* 年月选择控件区域 */
    int top_y = 10;

    /* 年份减少按钮 */
    lv_obj_t *btn_year_prev = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_year_prev, 40, 35);
    lv_obj_set_pos(btn_year_prev, 30, top_y);
    lv_obj_set_style_bg_color(btn_year_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_add_event_cb(btn_year_prev, btn_year_prev_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_year_prev = lv_label_create(btn_year_prev);
    lv_label_set_text(label_year_prev, LV_SYMBOL_LEFT);
    lv_obj_center(label_year_prev);

    /* 月份减少按钮 */
    lv_obj_t *btn_month_prev = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_month_prev, 40, 35);
    lv_obj_set_pos(btn_month_prev, 85, top_y);
    lv_obj_set_style_bg_color(btn_month_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_add_event_cb(btn_month_prev, btn_month_prev_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_month_prev = lv_label_create(btn_month_prev);
    lv_label_set_text(label_month_prev, "<");
    lv_obj_center(label_month_prev);

    /* 年月显示标签 */
    g_year_month_label = lv_label_create(g_calendar_popup);
    lv_obj_set_pos(g_year_month_label, 185, top_y + 8);
    lv_obj_set_style_text_font(g_year_month_label, &my_font_cn_16, 0);
    lv_label_set_text(g_year_month_label, "2026-01");

    /* 月份增加按钮 */
    lv_obj_t *btn_month_next = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_month_next, 40, 35);
    lv_obj_set_pos(btn_month_next, 305, top_y);
    lv_obj_set_style_bg_color(btn_month_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_add_event_cb(btn_month_next, btn_month_next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_month_next = lv_label_create(btn_month_next);
    lv_label_set_text(label_month_next, ">");
    lv_obj_center(label_month_next);

    /* 年份增加按钮 */
    lv_obj_t *btn_year_next = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_year_next, 40, 35);
    lv_obj_set_pos(btn_year_next, 360, top_y);
    lv_obj_set_style_bg_color(btn_year_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_add_event_cb(btn_year_next, btn_year_next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_year_next = lv_label_create(btn_year_next);
    lv_label_set_text(label_year_next, LV_SYMBOL_RIGHT);
    lv_obj_center(label_year_next);

    /* 创建日历控件 */
    g_calendar_widget = lv_calendar_create(g_calendar_popup);
    lv_obj_set_size(g_calendar_widget, 400, 260);
    lv_obj_set_pos(g_calendar_widget, 10, 55);
    lv_obj_add_event_cb(g_calendar_widget, calendar_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_clear_flag(g_calendar_widget, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(g_calendar_widget, &my_font_cn_16, 0);

    /* 根据输入框日期设置日历显示 */
    calendar_set_from_input(g_calendar_widget, g_current_date_input, g_year_month_label);

    /* 关闭按钮 */
    lv_obj_t *btn_close = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_close, 100, 40);
    lv_obj_set_pos(btn_close, (465-100)/2 - 20, 325);
    lv_obj_set_style_bg_color(btn_close, COLOR_PRIMARY, 0);
    lv_obj_add_event_cb(btn_close, btn_calendar_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_close = lv_label_create(btn_close);
    lv_label_set_text(label_close, "确认");
    lv_obj_set_style_text_font(label_close, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_close, lv_color_white(), 0);
    lv_obj_center(label_close);
}

/**
 * @brief 结束日期日历按钮回调
 */
static void btn_calendar_end_cb(lv_event_t *e)
{
    /* 获取传入的输入框引用 */
    lv_obj_t *input_box = (lv_obj_t *)lv_event_get_user_data(e);

    /* 如果已经有弹窗,先删除 */
    if (g_calendar_popup) {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
    }

    g_current_date_input = input_box ? input_box : g_input_end_date;

    /* 创建日历弹窗 */
    g_calendar_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_calendar_popup, 465, 390);
    lv_obj_center(g_calendar_popup);
    lv_obj_set_style_bg_color(g_calendar_popup, lv_color_white(), 0);
    lv_obj_set_style_radius(g_calendar_popup, 10, 0);
    lv_obj_clear_flag(g_calendar_popup, LV_OBJ_FLAG_SCROLLABLE);

    /* 年月选择控件区域 */
    int top_y = 10;

    /* 年份减少按钮 */
    lv_obj_t *btn_year_prev = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_year_prev, 40, 35);
    lv_obj_set_pos(btn_year_prev, 30, top_y);
    lv_obj_set_style_bg_color(btn_year_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_add_event_cb(btn_year_prev, btn_year_prev_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_year_prev = lv_label_create(btn_year_prev);
    lv_label_set_text(label_year_prev, LV_SYMBOL_LEFT);
    lv_obj_center(label_year_prev);

    /* 月份减少按钮 */
    lv_obj_t *btn_month_prev = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_month_prev, 40, 35);
    lv_obj_set_pos(btn_month_prev, 85, top_y);
    lv_obj_set_style_bg_color(btn_month_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_add_event_cb(btn_month_prev, btn_month_prev_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_month_prev = lv_label_create(btn_month_prev);
    lv_label_set_text(label_month_prev, "<");
    lv_obj_center(label_month_prev);

    /* 年月显示标签 */
    g_year_month_label = lv_label_create(g_calendar_popup);
    lv_obj_set_pos(g_year_month_label, 185, top_y + 8);
    lv_obj_set_style_text_font(g_year_month_label, &my_font_cn_16, 0);
    lv_label_set_text(g_year_month_label, "2026-01");

    /* 月份增加按钮 */
    lv_obj_t *btn_month_next = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_month_next, 40, 35);
    lv_obj_set_pos(btn_month_next, 305, top_y);
    lv_obj_set_style_bg_color(btn_month_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_add_event_cb(btn_month_next, btn_month_next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_month_next = lv_label_create(btn_month_next);
    lv_label_set_text(label_month_next, ">");
    lv_obj_center(label_month_next);

    /* 年份增加按钮 */
    lv_obj_t *btn_year_next = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_year_next, 40, 35);
    lv_obj_set_pos(btn_year_next, 360, top_y);
    lv_obj_set_style_bg_color(btn_year_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_add_event_cb(btn_year_next, btn_year_next_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_year_next = lv_label_create(btn_year_next);
    lv_label_set_text(label_year_next, LV_SYMBOL_RIGHT);
    lv_obj_center(label_year_next);

    /* 创建日历控件 */
    g_calendar_widget = lv_calendar_create(g_calendar_popup);
    lv_obj_set_size(g_calendar_widget, 400, 260);
    lv_obj_set_pos(g_calendar_widget, 10, 55);
    lv_obj_add_event_cb(g_calendar_widget, calendar_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_clear_flag(g_calendar_widget, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(g_calendar_widget, &my_font_cn_16, 0);

    /* 根据输入框日期设置日历显示 */
    calendar_set_from_input(g_calendar_widget, g_current_date_input, g_year_month_label);

    /* 关闭按钮 */
    lv_obj_t *btn_close = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_close, 100, 40);
    lv_obj_set_pos(btn_close, (465-100)/2 - 20, 325);
    lv_obj_set_style_bg_color(btn_close, COLOR_PRIMARY, 0);
    lv_obj_add_event_cb(btn_close, btn_calendar_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_close = lv_label_create(btn_close);
    lv_label_set_text(label_close, "确认");
    lv_obj_set_style_text_font(label_close, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_close, lv_color_white(), 0);
    lv_obj_center(label_close);
}

/**
 * @brief 日历选择事件回调
 */
static void calendar_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *calendar = lv_event_get_current_target(e);

    /* 只处理 VALUE_CHANGED 事件 */
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_calendar_date_t date;

        if (lv_calendar_get_pressed_date(calendar, &date) == LV_RESULT_OK) {
            if (g_current_date_input) {
                char date_str[16];
                snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
                         date.year, date.month, date.day);
                lv_textarea_set_text(g_current_date_input, date_str);

                /* 选择日期后异步关闭弹窗 */
                if (g_calendar_popup) {
                    lv_obj_delete_async(g_calendar_popup);
                    g_calendar_popup = NULL;
                }
            }
        }
    }
}

/**
 * @brief 日历关闭按钮回调
 */
static void btn_calendar_close_cb(lv_event_t *e)
{
    (void)e;
    if (g_calendar_popup) {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
    }
}

/**
 * @brief 年份减少按钮回调
 */
static void btn_year_prev_cb(lv_event_t *e)
{
    (void)e;
    if (g_calendar_widget) {
        const lv_calendar_date_t *date_ptr = lv_calendar_get_showed_date(g_calendar_widget);
        lv_calendar_date_t date = *date_ptr;
        date.year--;
        lv_calendar_set_showed_date(g_calendar_widget, date.year, date.month);

        /* 更新年月显示 */
        if (g_year_month_label) {
            char date_str[16];
            snprintf(date_str, sizeof(date_str), "%04d-%02d", date.year, date.month);
            lv_label_set_text(g_year_month_label, date_str);
        }
    }
}

/**
 * @brief 年份增加按钮回调
 */
static void btn_year_next_cb(lv_event_t *e)
{
    (void)e;
    if (g_calendar_widget) {
        const lv_calendar_date_t *date_ptr = lv_calendar_get_showed_date(g_calendar_widget);
        lv_calendar_date_t date = *date_ptr;
        date.year++;
        lv_calendar_set_showed_date(g_calendar_widget, date.year, date.month);

        /* 更新年月显示 */
        if (g_year_month_label) {
            char date_str[16];
            snprintf(date_str, sizeof(date_str), "%04d-%02d", date.year, date.month);
            lv_label_set_text(g_year_month_label, date_str);
        }
    }
}

/**
 * @brief 月份减少按钮回调
 */
static void btn_month_prev_cb(lv_event_t *e)
{
    (void)e;
    if (g_calendar_widget) {
        const lv_calendar_date_t *date_ptr = lv_calendar_get_showed_date(g_calendar_widget);
        lv_calendar_date_t date = *date_ptr;

        /* 处理月份边界 */
        if (date.month == 1) {
            date.month = 12;
            date.year--;
        } else {
            date.month--;
        }

        lv_calendar_set_showed_date(g_calendar_widget, date.year, date.month);

        /* 更新年月显示 */
        if (g_year_month_label) {
            char date_str[16];
            snprintf(date_str, sizeof(date_str), "%04d-%02d", date.year, date.month);
            lv_label_set_text(g_year_month_label, date_str);
        }
    }
}

/**
 * @brief 月份增加按钮回调
 */
static void btn_month_next_cb(lv_event_t *e)
{
    (void)e;
    if (g_calendar_widget) {
        const lv_calendar_date_t *date_ptr = lv_calendar_get_showed_date(g_calendar_widget);
        lv_calendar_date_t date = *date_ptr;

        /* 处理月份边界 */
        if (date.month == 12) {
            date.month = 1;
            date.year++;
        } else {
            date.month++;
        }

        lv_calendar_set_showed_date(g_calendar_widget, date.year, date.month);

        /* 更新年月显示 */
        if (g_year_month_label) {
            char date_str[16];
            snprintf(date_str, sizeof(date_str), "%04d-%02d", date.year, date.month);
            lv_label_set_text(g_year_month_label, date_str);
        }
    }
}

/**
 * @brief 输入框点击回调 - 弹出数字键盘
 */
static void input_click_cb(lv_event_t *e)
{
    lv_obj_t *textarea = lv_event_get_target(e);
    if (textarea) {
        /* 弹出数字键盘，父容器为主屏幕 */
        ui_numpad_show(textarea, lv_scr_act());
    }
}
