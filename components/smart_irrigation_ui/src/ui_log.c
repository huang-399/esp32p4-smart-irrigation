/**
 * @file ui_log.c
 * @brief 日志界面实现
 */

#include "ui_common.h"
#include "ui_log_records.h"
#include "ui_numpad.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_tab_buttons(lv_obj_t *parent);
static void create_filter_section(lv_obj_t *parent);
static void create_log_table(lv_obj_t *parent);
static void create_pagination(lv_obj_t *parent);
static void show_placeholder_message(lv_obj_t *parent, const char *message);
static void tab_btn_cb(lv_event_t *e);
static void query_btn_cb(lv_event_t *e);
static void btn_calendar_start_cb(lv_event_t *e);
static void btn_calendar_end_cb(lv_event_t *e);
static void calendar_event_cb(lv_event_t *e);
static void btn_calendar_close_cb(lv_event_t *e);
static void btn_year_prev_cb(lv_event_t *e);
static void btn_year_next_cb(lv_event_t *e);
static void btn_month_prev_cb(lv_event_t *e);
static void btn_month_next_cb(lv_event_t *e);

/* 全局变量 */
static lv_obj_t *g_view_container = NULL;    /* 视图容器 */
static lv_obj_t *g_active_view = NULL;       /* 当前活动视图 */
static int g_active_tab = 0;                 /* 当前活动标签索引 */
static lv_obj_t *g_tab_buttons[5] = {NULL};   /* 标签按钮数组 */
static lv_obj_t *g_input_start_date = NULL;   /* 开始日期输入框 */
static lv_obj_t *g_input_end_date = NULL;     /* 结束日期输入框 */
static lv_obj_t *g_control_table_area = NULL; /* 控制日志表格区域 */
static lv_obj_t *g_control_page_info = NULL;  /* 控制日志页码 */
static lv_obj_t *g_control_query_btn = NULL;  /* 控制日志查询按钮 */
static lv_obj_t *g_control_btn_first = NULL;  /* 控制日志首页 */
static lv_obj_t *g_control_btn_prev = NULL;   /* 控制日志上一页 */
static lv_obj_t *g_control_btn_next = NULL;   /* 控制日志下一页 */
static lv_obj_t *g_control_btn_last = NULL;   /* 控制日志尾页 */
static lv_obj_t *g_operation_table_area = NULL; /* 操作日志表格区域 */
static lv_obj_t *g_operation_page_info = NULL;  /* 操作日志页码 */
static lv_obj_t *g_operation_btn_first = NULL;  /* 操作日志首页 */
static lv_obj_t *g_operation_btn_prev = NULL;   /* 操作日志上一页 */
static lv_obj_t *g_operation_btn_next = NULL;   /* 操作日志下一页 */
static lv_obj_t *g_operation_btn_last = NULL;   /* 操作日志尾页 */
static lv_obj_t *g_manual_table_area = NULL;    /* 手灌记录表格区域 */
static lv_obj_t *g_manual_page_info = NULL;     /* 手灌记录页码 */
static lv_obj_t *g_manual_btn_first = NULL;     /* 手灌记录首页 */
static lv_obj_t *g_manual_btn_prev = NULL;      /* 手灌记录上一页 */
static lv_obj_t *g_manual_btn_next = NULL;      /* 手灌记录下一页 */
static lv_obj_t *g_manual_btn_last = NULL;      /* 手灌记录尾页 */
static lv_obj_t *g_manual_status_dropdown = NULL; /* 手灌记录状态筛选 */
static lv_obj_t *g_program_table_area = NULL;   /* 程序记录表格区域 */
static lv_obj_t *g_program_page_info = NULL;    /* 程序记录页码 */
static lv_obj_t *g_program_btn_first = NULL;    /* 程序记录首页 */
static lv_obj_t *g_program_btn_prev = NULL;     /* 程序记录上一页 */
static lv_obj_t *g_program_btn_next = NULL;     /* 程序记录下一页 */
static lv_obj_t *g_program_btn_last = NULL;     /* 程序记录尾页 */
static lv_obj_t *g_program_status_dropdown = NULL; /* 程序记录状态筛选 */
static lv_obj_t *g_calendar_popup = NULL;     /* 日历弹窗 */
static lv_obj_t *g_current_date_input = NULL; /* 当前选择日期的输入框 */
static lv_obj_t *g_calendar_widget = NULL;    /* 日历控件引用 */
static lv_obj_t *g_year_month_label = NULL;   /* 年月显示标签 */

/* 前向声明 */
static void create_control_log_view(lv_obj_t *parent);
static void create_operation_log_view(lv_obj_t *parent);
static void create_manual_record_view(lv_obj_t *parent);
static void create_program_record_view(lv_obj_t *parent);
static void create_daily_flow_view(lv_obj_t *parent);
static void switch_to_tab(int tab_index);

/* ---- Date helpers (same as ui_alarm.c) ---- */

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

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 关闭日历弹窗（如果存在）
 */
void ui_log_close_calendar(void)
{
    if (g_calendar_popup) {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
        g_current_date_input = NULL;
        g_calendar_widget = NULL;
        g_year_month_label = NULL;
    }
}

/**
 * @brief 创建日志页面
 */
void ui_log_create(lv_obj_t *parent)
{
    /* 重置静态指针（旧对象已被 ui_switch_nav 中的 lv_obj_clean 销毁） */
    g_view_container = NULL;
    g_active_view = NULL;
    g_active_tab = 0;
    for (int i = 0; i < 5; i++) g_tab_buttons[i] = NULL;
    g_input_start_date = NULL;
    g_input_end_date = NULL;
    g_control_table_area = NULL;
    g_control_page_info = NULL;
    g_control_btn_first = NULL;
    g_control_btn_prev = NULL;
    g_control_btn_next = NULL;
    g_control_btn_last = NULL;
    g_operation_table_area = NULL;
    g_operation_page_info = NULL;
    g_operation_btn_first = NULL;
    g_operation_btn_prev = NULL;
    g_operation_btn_next = NULL;
    g_operation_btn_last = NULL;
    g_manual_table_area = NULL;
    g_manual_page_info = NULL;
    g_manual_btn_first = NULL;
    g_manual_btn_prev = NULL;
    g_manual_btn_next = NULL;
    g_manual_btn_last = NULL;
    g_manual_status_dropdown = NULL;
    g_program_table_area = NULL;
    g_program_page_info = NULL;
    g_program_btn_first = NULL;
    g_program_btn_prev = NULL;
    g_program_btn_next = NULL;
    g_program_btn_last = NULL;
    g_program_status_dropdown = NULL;
    /* g_calendar_popup 由 ui_log_close_calendar() 在 ui_switch_nav 中处理 */
    g_current_date_input = NULL;
    g_calendar_widget = NULL;
    g_year_month_label = NULL;

    /* 顶部标签页按钮 */
    create_tab_buttons(parent);

    /* 创建主容器 - 用于放置不同的视图 */
    g_view_container = lv_obj_create(parent);
    lv_obj_set_size(g_view_container, 1168, 660);
    lv_obj_set_pos(g_view_container, 5, 70);
    lv_obj_set_style_bg_opa(g_view_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_view_container, 0, 0);
    lv_obj_set_style_pad_all(g_view_container, 0, 0);
    lv_obj_clear_flag(g_view_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 懒加载：只创建默认视图（控制日志） */
    g_active_tab = 0;
    g_active_view = NULL;
    switch_to_tab(0);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 创建顶部标签页按钮
 */
static void create_tab_buttons(lv_obj_t *parent)
{
    const char *tab_names[] = {"控制日志", "操作日志", "手灌记录", "程序记录", "每日流量"};
    int btn_width = 150;
    int btn_height = 50;
    int x_start = 10;
    int y_pos = 10;

    for (int i = 0; i < 5; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, btn_width, btn_height);
        lv_obj_set_pos(btn, x_start + i * (btn_width + 10), y_pos);

        /* 第一个按钮默认选中 - 蓝色背景 */
        if (i == 0) {
            lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        }

        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);  /* 移除内边距 */
        lv_obj_add_event_cb(btn, tab_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        /* 按钮图标 */
        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, LV_SYMBOL_LIST);
        lv_obj_set_style_text_font(icon, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(icon, i == 0 ? lv_color_white() : COLOR_PRIMARY, 0);
        lv_obj_align(icon, LV_ALIGN_CENTER, -28, 0);  /* 中心偏左 */

        /* 按钮文字 */
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, tab_names[i]);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, i == 0 ? lv_color_white() : lv_color_hex(0x333333), 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 20, 0);  /* 中心偏右 */

        /* 保存按钮引用 */
        g_tab_buttons[i] = btn;
    }
}

/**
 * @brief 创建控制日志视图
 */
static void create_control_log_view(lv_obj_t *parent)
{
    /* 日期和筛选区域 */
    create_filter_section(parent);

    /* 日志表格 */
    create_log_table(parent);

    /* 分页控件 */
    create_pagination(parent);

    ui_log_rec_setup_control(
        g_input_start_date,
        g_input_end_date,
        g_control_table_area,
        g_control_page_info,
        g_control_query_btn,
        g_control_btn_first,
        g_control_btn_prev,
        g_control_btn_next,
        g_control_btn_last
    );
}

/**
 * @brief 创建筛选区域
 */
static void create_filter_section(lv_obj_t *parent)
{
    int y_pos = 5;  /* 向上调整位置 */
    char today_buf[32];
    get_today_str(today_buf, sizeof(today_buf));

    /* 日期标签 */
    lv_obj_t *date_label = lv_label_create(parent);
    lv_label_set_text(date_label, "日期:");
    lv_obj_set_pos(date_label, 10, y_pos + 8);
    lv_obj_set_style_text_font(date_label, &my_font_cn_16, 0);

    /* 起始日期输入框容器 */
    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 190, 30);
    lv_obj_set_pos(start_date_container, 60, y_pos + 3);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 1, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 5, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 起始日期输入框 */
    g_input_start_date = lv_textarea_create(start_date_container);
    lv_obj_set_size(g_input_start_date, 155, 28);
    lv_obj_set_pos(g_input_start_date, 1, 1);
    lv_textarea_set_one_line(g_input_start_date, true);
    lv_textarea_set_text(g_input_start_date, today_buf);
    lv_obj_set_style_text_font(g_input_start_date, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_input_start_date, LV_TEXT_ALIGN_CENTER, 0);  /* 文本水平居中 */
    lv_obj_set_style_pad_left(g_input_start_date, 0, 0);
    lv_obj_set_style_pad_right(g_input_start_date, 0, 0);
    lv_obj_set_style_pad_top(g_input_start_date, 4, 0);  /* 上边距4px */
    lv_obj_set_style_pad_bottom(g_input_start_date, 0, 0);
    lv_obj_set_style_border_width(g_input_start_date, 0, 0);
    lv_obj_clear_flag(g_input_start_date, LV_OBJ_FLAG_CLICKABLE); /* 禁止手动输入 */

    /* 起始日期日历按钮 - 放在输入框右侧内部 */
    lv_obj_t *btn_start_cal = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_start_cal, 28, 28);
    lv_obj_set_pos(btn_start_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_start_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_start_cal, 0, 0);
    lv_obj_set_style_radius(btn_start_cal, 4, 0);
    lv_obj_add_event_cb(btn_start_cal, btn_calendar_start_cb, LV_EVENT_CLICKED, g_input_start_date);

    lv_obj_t *icon_start = lv_label_create(btn_start_cal);
    lv_label_set_text(icon_start, LV_SYMBOL_BELL);  /* 使用铃铛图标代替日历 */
    lv_obj_set_style_text_font(icon_start, &my_font_cn_16, 0);
    lv_obj_center(icon_start);

    /* 至 */
    lv_obj_t *to_label = lv_label_create(parent);
    lv_label_set_text(to_label, "至");
    lv_obj_set_pos(to_label, 265, y_pos + 8);
    lv_obj_set_style_text_font(to_label, &my_font_cn_16, 0);

    /* 结束日期输入框容器 */
    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 190, 30);
    lv_obj_set_pos(end_date_container, 300, y_pos + 3);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 1, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 5, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 结束日期输入框 */
    g_input_end_date = lv_textarea_create(end_date_container);
    lv_obj_set_size(g_input_end_date, 155, 28);
    lv_obj_set_pos(g_input_end_date, 1, 1);
    lv_textarea_set_one_line(g_input_end_date, true);
    lv_textarea_set_text(g_input_end_date, today_buf);
    lv_obj_set_style_text_font(g_input_end_date, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_input_end_date, LV_TEXT_ALIGN_CENTER, 0);  /* 文本水平居中 */
    lv_obj_set_style_pad_left(g_input_end_date, 0, 0);
    lv_obj_set_style_pad_right(g_input_end_date, 0, 0);
    lv_obj_set_style_pad_top(g_input_end_date, 4, 0);  /* 上边距4px */
    lv_obj_set_style_pad_bottom(g_input_end_date, 0, 0);
    lv_obj_set_style_border_width(g_input_end_date, 0, 0);
    lv_obj_clear_flag(g_input_end_date, LV_OBJ_FLAG_CLICKABLE); /* 禁止手动输入 */

    /* 结束日期日历按钮 - 放在输入框右侧内部 */
    lv_obj_t *btn_end_cal = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_end_cal, 28, 28);
    lv_obj_set_pos(btn_end_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_end_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_end_cal, 0, 0);
    lv_obj_set_style_radius(btn_end_cal, 4, 0);
    lv_obj_add_event_cb(btn_end_cal, btn_calendar_end_cb, LV_EVENT_CLICKED, g_input_end_date);

    lv_obj_t *icon_end = lv_label_create(btn_end_cal);
    lv_label_set_text(icon_end, LV_SYMBOL_BELL);  /* 使用铃铛图标代替日历 */
    lv_obj_set_style_text_font(icon_end, &my_font_cn_16, 0);
    lv_obj_center(icon_end);

    /* 类型下拉框 */
    lv_obj_t *dropdown = lv_dropdown_create(parent);
    lv_obj_set_size(dropdown, 120, 30);
    lv_obj_set_pos(dropdown, 850, y_pos + 3);
    lv_dropdown_set_options(dropdown, "全部\n成功\n失败");
    lv_obj_set_style_text_font(dropdown, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dropdown), &my_font_cn_16, 0);

    /* 查询按钮 */
    g_control_query_btn = lv_btn_create(parent);
    lv_obj_set_size(g_control_query_btn, 100, 30);
    lv_obj_set_pos(g_control_query_btn, 1000, y_pos + 3);
    lv_obj_set_style_bg_color(g_control_query_btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_control_query_btn, 0, 0);
    lv_obj_set_style_radius(g_control_query_btn, 5, 0);
    lv_obj_add_event_cb(g_control_query_btn, ui_log_rec_control_query_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *query_label = lv_label_create(g_control_query_btn);
    lv_label_set_text(query_label, "查询");
    lv_obj_set_style_text_color(query_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(query_label, &my_font_cn_16, 0);
    lv_obj_center(query_label);
}

/**
 * @brief 创建日志表格
 */
static void create_log_table(lv_obj_t *parent)
{
    int table_y = 45;  /* 调整表格起始位置 */
    int table_height = 555;  /* 调整表格高度 */

    /* 表格容器 */
    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, table_height);
    lv_obj_set_pos(table_container, 0, table_y);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头 */
    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "记录时间", "日志描述"};
    int header_x[] = {20, 180, 600};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    g_control_table_area = lv_obj_create(table_container);
    lv_obj_set_size(g_control_table_area, 1138, table_height - 40);
    lv_obj_set_pos(g_control_table_area, 0, 40);
    lv_obj_set_style_bg_opa(g_control_table_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_control_table_area, 0, 0);
    lv_obj_set_style_radius(g_control_table_area, 0, 0);
    lv_obj_set_style_pad_all(g_control_table_area, 0, 0);
    lv_obj_clear_flag(g_control_table_area, LV_OBJ_FLAG_SCROLLABLE);
}

/**
 * @brief 创建分页控件
 */
static void create_pagination(lv_obj_t *parent)
{
    int pagination_y = 610;

    /* 首页按钮 */
    g_control_btn_first = lv_btn_create(parent);
    lv_obj_set_size(g_control_btn_first, 70, 35);
    lv_obj_set_pos(g_control_btn_first, 350, pagination_y);
    lv_obj_set_style_bg_color(g_control_btn_first, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_control_btn_first, 0, 0);
    lv_obj_set_style_radius(g_control_btn_first, 5, 0);

    lv_obj_t *first_label = lv_label_create(g_control_btn_first);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    g_control_btn_prev = lv_btn_create(parent);
    lv_obj_set_size(g_control_btn_prev, 80, 35);
    lv_obj_set_pos(g_control_btn_prev, 430, pagination_y);
    lv_obj_set_style_bg_color(g_control_btn_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_control_btn_prev, 0, 0);
    lv_obj_set_style_radius(g_control_btn_prev, 5, 0);

    lv_obj_t *prev_label = lv_label_create(g_control_btn_prev);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    g_control_page_info = lv_label_create(parent);
    lv_label_set_text(g_control_page_info, "0/0");
    lv_obj_set_pos(g_control_page_info, 540, pagination_y + 8);
    lv_obj_set_style_text_font(g_control_page_info, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(g_control_page_info, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    g_control_btn_next = lv_btn_create(parent);
    lv_obj_set_size(g_control_btn_next, 80, 35);
    lv_obj_set_pos(g_control_btn_next, 600, pagination_y);
    lv_obj_set_style_bg_color(g_control_btn_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_control_btn_next, 0, 0);
    lv_obj_set_style_radius(g_control_btn_next, 5, 0);

    lv_obj_t *next_label = lv_label_create(g_control_btn_next);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    g_control_btn_last = lv_btn_create(parent);
    lv_obj_set_size(g_control_btn_last, 70, 35);
    lv_obj_set_pos(g_control_btn_last, 690, pagination_y);
    lv_obj_set_style_bg_color(g_control_btn_last, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_control_btn_last, 0, 0);
    lv_obj_set_style_radius(g_control_btn_last, 5, 0);

    lv_obj_t *last_label = lv_label_create(g_control_btn_last);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);
}

/**
 * @brief 懒加载切换到指定标签页
 */
static void switch_to_tab(int tab_index)
{
    if (!g_view_container) return;

    ui_log_rec_invalidate();

    /* 销毁旧视图（及其所有子对象） */
    if (g_active_view) {
        lv_obj_del(g_active_view);
        g_active_view = NULL;
    }

    /* 创建新的视图容器 */
    g_active_view = lv_obj_create(g_view_container);
    lv_obj_set_size(g_active_view, 1168, 660);
    lv_obj_set_pos(g_active_view, 0, 0);
    lv_obj_set_style_bg_color(g_active_view, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_active_view, 0, 0);
    lv_obj_set_style_radius(g_active_view, 10, 0);
    lv_obj_set_style_pad_all(g_active_view, 15, 0);
    lv_obj_clear_flag(g_active_view, LV_OBJ_FLAG_SCROLLABLE);

    /* 根据标签索引创建对应视图内容 */
    switch (tab_index) {
        case 0: create_control_log_view(g_active_view); break;
        case 1: create_operation_log_view(g_active_view); break;
        case 2: create_manual_record_view(g_active_view); break;
        case 3: create_program_record_view(g_active_view); break;
        case 4: create_daily_flow_view(g_active_view); break;
        default: break;
    }

    g_active_tab = tab_index;
}

/**
 * @brief 标签页按钮回调
 */
static void tab_btn_cb(lv_event_t *e)
{
    int tab_index = (int)(intptr_t)lv_event_get_user_data(e);

    /* 关闭日历弹窗(如果存在) */
    ui_log_close_calendar();

    /* 更新所有标签按钮的颜色和文字颜色 */
    for (int i = 0; i < 5; i++) {
        if (g_tab_buttons[i]) {
            if (i == tab_index) {
                /* 选中：蓝色背景 */
                lv_obj_set_style_bg_color(g_tab_buttons[i], COLOR_PRIMARY, 0);

                /* 更新图标和文字颜色为白色 */
                lv_obj_t *icon = lv_obj_get_child(g_tab_buttons[i], 0);
                lv_obj_t *label = lv_obj_get_child(g_tab_buttons[i], 1);
                if (icon) lv_obj_set_style_text_color(icon, lv_color_white(), 0);
                if (label) lv_obj_set_style_text_color(label, lv_color_white(), 0);
            } else {
                /* 未选中:白色背景 */
                lv_obj_set_style_bg_color(g_tab_buttons[i], lv_color_white(), 0);

                /* 更新图标和文字颜色为深色 */
                lv_obj_t *icon = lv_obj_get_child(g_tab_buttons[i], 0);
                lv_obj_t *label = lv_obj_get_child(g_tab_buttons[i], 1);
                if (icon) lv_obj_set_style_text_color(icon, COLOR_PRIMARY, 0);
                if (label) lv_obj_set_style_text_color(label, lv_color_hex(0x333333), 0);
            }
        }
    }

    /* 根据选中的标签页切换视图 */
    switch_to_tab(tab_index);
}

/**
 * @brief 查询按钮回调
 */
static void query_btn_cb(lv_event_t *e)
{
    (void)e;
}

static void show_placeholder_message(lv_obj_t *parent, const char *message)
{
    if (!parent) return;

    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, message ? message : "当前版本暂未接入数据");
    lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT_GRAY, 0);
    lv_obj_center(label);
}

/**
 * @brief 开始日期日历按钮回调
 */
static void btn_calendar_start_cb(lv_event_t *e)
{
    /* 获取传入的输入框引用,如果没有则使用全局变量 */
    lv_obj_t *input_box = (lv_obj_t *)lv_event_get_user_data(e);

    /* 如果已经有弹窗,先删除 */
    if (g_calendar_popup) {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
    }

    g_current_date_input = input_box ? input_box : g_input_start_date;

    /* 创建日历弹窗 */
    g_calendar_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_calendar_popup, 465, 390);  /* 宽度465px, 高度390px */
    lv_obj_center(g_calendar_popup);
    lv_obj_set_style_bg_color(g_calendar_popup, lv_color_white(), 0);
    lv_obj_set_style_radius(g_calendar_popup, 10, 0);
    lv_obj_clear_flag(g_calendar_popup, LV_OBJ_FLAG_SCROLLABLE);  /* 禁止滚动 */

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
    lv_obj_clear_flag(g_calendar_widget, LV_OBJ_FLAG_SCROLLABLE);  /* 禁止滚动拖动 */
    lv_obj_set_style_text_font(g_calendar_widget, &my_font_cn_16, 0);

    /* 从输入框解析日期并设置日历显示 */
    calendar_set_from_input(g_calendar_widget, g_current_date_input, g_year_month_label);

    /* 关闭按钮 */
    lv_obj_t *btn_close = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_close, 100, 40);
    lv_obj_set_pos(btn_close, (465-100)/2 - 20, 325);  /* 放在日历控件下方,向左偏移20px */
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
    /* 获取传入的输入框引用,如果没有则使用全局变量 */
    lv_obj_t *input_box = (lv_obj_t *)lv_event_get_user_data(e);

    /* 如果已经有弹窗,先删除 */
    if (g_calendar_popup) {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
    }

    g_current_date_input = input_box ? input_box : g_input_end_date;

    /* 创建日历弹窗 */
    g_calendar_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_calendar_popup, 465, 390);  /* 宽度465px, 高度390px */
    lv_obj_center(g_calendar_popup);
    lv_obj_set_style_bg_color(g_calendar_popup, lv_color_white(), 0);
    lv_obj_set_style_radius(g_calendar_popup, 10, 0);
    lv_obj_clear_flag(g_calendar_popup, LV_OBJ_FLAG_SCROLLABLE);  /* 禁止滚动 */

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
    lv_obj_clear_flag(g_calendar_widget, LV_OBJ_FLAG_SCROLLABLE);  /* 禁止滚动拖动 */
    lv_obj_set_style_text_font(g_calendar_widget, &my_font_cn_16, 0);

    /* 从输入框解析日期并设置日历显示 */
    calendar_set_from_input(g_calendar_widget, g_current_date_input, g_year_month_label);

    /* 关闭按钮 */
    lv_obj_t *btn_close = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_close, 100, 40);
    lv_obj_set_pos(btn_close, (465-100)/2 - 20, 325);  /* 放在日历控件下方,向左偏移20px */
    lv_obj_set_style_bg_color(btn_close, COLOR_PRIMARY, 0);
    lv_obj_add_event_cb(btn_close, btn_calendar_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_close = lv_label_create(btn_close);
    lv_label_set_text(label_close, "确认");
    lv_obj_set_style_text_font(label_close, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_close, lv_color_white(), 0);
    lv_obj_center(label_close);
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
 * @brief 创建操作日志视图
 */
static void create_operation_log_view(lv_obj_t *parent)
{
    int y_pos = 5;
    char today_buf[32];
    get_today_str(today_buf, sizeof(today_buf));

    /* 日期标签 */
    lv_obj_t *date_label = lv_label_create(parent);
    lv_label_set_text(date_label, "日期:");
    lv_obj_set_pos(date_label, 10, y_pos + 8);
    lv_obj_set_style_text_font(date_label, &my_font_cn_16, 0);

    /* 起始日期输入框容器 */
    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 190, 30);
    lv_obj_set_pos(start_date_container, 60, y_pos + 3);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 1, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 5, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 起始日期输入框 */
    lv_obj_t *input_start = lv_textarea_create(start_date_container);
    lv_obj_set_size(input_start, 155, 28);
    lv_obj_set_pos(input_start, 1, 1);
    lv_textarea_set_one_line(input_start, true);
    lv_textarea_set_text(input_start, today_buf);
    lv_obj_set_style_text_font(input_start, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_start, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_start, 0, 0);
    lv_obj_set_style_pad_right(input_start, 0, 0);
    lv_obj_set_style_pad_top(input_start, 4, 0);
    lv_obj_set_style_pad_bottom(input_start, 0, 0);
    lv_obj_set_style_border_width(input_start, 0, 0);
    lv_obj_clear_flag(input_start, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_start_cal = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_start_cal, 28, 28);
    lv_obj_set_pos(btn_start_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_start_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_start_cal, 0, 0);
    lv_obj_set_style_radius(btn_start_cal, 4, 0);
    lv_obj_add_event_cb(btn_start_cal, btn_calendar_start_cb, LV_EVENT_CLICKED, input_start);

    lv_obj_t *icon_start = lv_label_create(btn_start_cal);
    lv_label_set_text(icon_start, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_start, &my_font_cn_16, 0);
    lv_obj_center(icon_start);

    lv_obj_t *to_label = lv_label_create(parent);
    lv_label_set_text(to_label, "至");
    lv_obj_set_pos(to_label, 265, y_pos + 8);
    lv_obj_set_style_text_font(to_label, &my_font_cn_16, 0);

    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 190, 30);
    lv_obj_set_pos(end_date_container, 300, y_pos + 3);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 1, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 5, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *input_end = lv_textarea_create(end_date_container);
    lv_obj_set_size(input_end, 155, 28);
    lv_obj_set_pos(input_end, 1, 1);
    lv_textarea_set_one_line(input_end, true);
    lv_textarea_set_text(input_end, today_buf);
    lv_obj_set_style_text_font(input_end, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_end, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_end, 0, 0);
    lv_obj_set_style_pad_right(input_end, 0, 0);
    lv_obj_set_style_pad_top(input_end, 4, 0);
    lv_obj_set_style_pad_bottom(input_end, 0, 0);
    lv_obj_set_style_border_width(input_end, 0, 0);
    lv_obj_clear_flag(input_end, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_end_cal = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_end_cal, 28, 28);
    lv_obj_set_pos(btn_end_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_end_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_end_cal, 0, 0);
    lv_obj_set_style_radius(btn_end_cal, 4, 0);
    lv_obj_add_event_cb(btn_end_cal, btn_calendar_end_cb, LV_EVENT_CLICKED, input_end);

    lv_obj_t *icon_end = lv_label_create(btn_end_cal);
    lv_label_set_text(icon_end, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_end, &my_font_cn_16, 0);
    lv_obj_center(icon_end);

    lv_obj_t *query_btn = lv_btn_create(parent);
    lv_obj_set_size(query_btn, 100, 30);
    lv_obj_set_pos(query_btn, 1000, y_pos + 3);
    lv_obj_set_style_bg_color(query_btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(query_btn, 0, 0);
    lv_obj_set_style_radius(query_btn, 5, 0);
    lv_obj_add_event_cb(query_btn, ui_log_rec_operation_query_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *query_label = lv_label_create(query_btn);
    lv_label_set_text(query_label, "查询");
    lv_obj_set_style_text_color(query_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(query_label, &my_font_cn_16, 0);
    lv_obj_center(query_label);

    int table_y = 45;
    int table_height = 555;

    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, table_height);
    lv_obj_set_pos(table_container, 0, table_y);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "记录时间", "日志描述"};
    int header_x[] = {20, 180, 600};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    g_operation_table_area = lv_obj_create(table_container);
    lv_obj_set_size(g_operation_table_area, 1138, table_height - 40);
    lv_obj_set_pos(g_operation_table_area, 0, 40);
    lv_obj_set_style_bg_opa(g_operation_table_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_operation_table_area, 0, 0);
    lv_obj_set_style_radius(g_operation_table_area, 0, 0);
    lv_obj_set_style_pad_all(g_operation_table_area, 0, 0);
    lv_obj_clear_flag(g_operation_table_area, LV_OBJ_FLAG_SCROLLABLE);

    int pagination_y = 610;

    g_operation_btn_first = lv_btn_create(parent);
    lv_obj_set_size(g_operation_btn_first, 70, 35);
    lv_obj_set_pos(g_operation_btn_first, 350, pagination_y);
    lv_obj_set_style_bg_color(g_operation_btn_first, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_operation_btn_first, 0, 0);
    lv_obj_set_style_radius(g_operation_btn_first, 5, 0);

    lv_obj_t *first_label = lv_label_create(g_operation_btn_first);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    g_operation_btn_prev = lv_btn_create(parent);
    lv_obj_set_size(g_operation_btn_prev, 80, 35);
    lv_obj_set_pos(g_operation_btn_prev, 430, pagination_y);
    lv_obj_set_style_bg_color(g_operation_btn_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_operation_btn_prev, 0, 0);
    lv_obj_set_style_radius(g_operation_btn_prev, 5, 0);

    lv_obj_t *prev_label = lv_label_create(g_operation_btn_prev);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    g_operation_page_info = lv_label_create(parent);
    lv_label_set_text(g_operation_page_info, "0/0");
    lv_obj_set_pos(g_operation_page_info, 540, pagination_y + 8);
    lv_obj_set_style_text_font(g_operation_page_info, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(g_operation_page_info, lv_color_hex(0x333333), 0);

    g_operation_btn_next = lv_btn_create(parent);
    lv_obj_set_size(g_operation_btn_next, 80, 35);
    lv_obj_set_pos(g_operation_btn_next, 600, pagination_y);
    lv_obj_set_style_bg_color(g_operation_btn_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_operation_btn_next, 0, 0);
    lv_obj_set_style_radius(g_operation_btn_next, 5, 0);

    lv_obj_t *next_label = lv_label_create(g_operation_btn_next);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    g_operation_btn_last = lv_btn_create(parent);
    lv_obj_set_size(g_operation_btn_last, 70, 35);
    lv_obj_set_pos(g_operation_btn_last, 690, pagination_y);
    lv_obj_set_style_bg_color(g_operation_btn_last, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_operation_btn_last, 0, 0);
    lv_obj_set_style_radius(g_operation_btn_last, 5, 0);

    lv_obj_t *last_label = lv_label_create(g_operation_btn_last);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);

    ui_log_rec_setup_operation(
        input_start,
        input_end,
        g_operation_table_area,
        g_operation_page_info,
        query_btn,
        g_operation_btn_first,
        g_operation_btn_prev,
        g_operation_btn_next,
        g_operation_btn_last
    );
}

/**
 * @brief 创建手灌记录视图
 */
static void create_manual_record_view(lv_obj_t *parent)
{
    int y_pos = 5;
    int pagination_y = 610;
    char today_buf[32];
    get_today_str(today_buf, sizeof(today_buf));

    lv_obj_t *date_label = lv_label_create(parent);
    lv_label_set_text(date_label, "日期:");
    lv_obj_set_pos(date_label, 10, y_pos + 8);
    lv_obj_set_style_text_font(date_label, &my_font_cn_16, 0);

    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 190, 30);
    lv_obj_set_pos(start_date_container, 60, y_pos + 3);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 1, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 5, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *input_start = lv_textarea_create(start_date_container);
    lv_obj_set_size(input_start, 155, 28);
    lv_obj_set_pos(input_start, 1, 1);
    lv_textarea_set_one_line(input_start, true);
    lv_textarea_set_text(input_start, today_buf);
    lv_obj_set_style_text_font(input_start, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_start, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_start, 0, 0);
    lv_obj_set_style_pad_right(input_start, 0, 0);
    lv_obj_set_style_pad_top(input_start, 4, 0);
    lv_obj_set_style_pad_bottom(input_start, 0, 0);
    lv_obj_set_style_border_width(input_start, 0, 0);
    lv_obj_clear_flag(input_start, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_start_cal = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_start_cal, 28, 28);
    lv_obj_set_pos(btn_start_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_start_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_start_cal, 0, 0);
    lv_obj_set_style_radius(btn_start_cal, 4, 0);
    lv_obj_add_event_cb(btn_start_cal, btn_calendar_start_cb, LV_EVENT_CLICKED, input_start);

    lv_obj_t *icon_start = lv_label_create(btn_start_cal);
    lv_label_set_text(icon_start, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_start, &my_font_cn_16, 0);
    lv_obj_center(icon_start);

    lv_obj_t *to_label = lv_label_create(parent);
    lv_label_set_text(to_label, "至");
    lv_obj_set_pos(to_label, 265, y_pos + 8);
    lv_obj_set_style_text_font(to_label, &my_font_cn_16, 0);

    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 190, 30);
    lv_obj_set_pos(end_date_container, 300, y_pos + 3);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 1, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 5, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *input_end = lv_textarea_create(end_date_container);
    lv_obj_set_size(input_end, 155, 28);
    lv_obj_set_pos(input_end, 1, 1);
    lv_textarea_set_one_line(input_end, true);
    lv_textarea_set_text(input_end, today_buf);
    lv_obj_set_style_text_font(input_end, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_end, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_end, 0, 0);
    lv_obj_set_style_pad_right(input_end, 0, 0);
    lv_obj_set_style_pad_top(input_end, 4, 0);
    lv_obj_set_style_pad_bottom(input_end, 0, 0);
    lv_obj_set_style_border_width(input_end, 0, 0);
    lv_obj_clear_flag(input_end, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_end_cal = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_end_cal, 28, 28);
    lv_obj_set_pos(btn_end_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_end_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_end_cal, 0, 0);
    lv_obj_set_style_radius(btn_end_cal, 4, 0);
    lv_obj_add_event_cb(btn_end_cal, btn_calendar_end_cb, LV_EVENT_CLICKED, input_end);

    lv_obj_t *icon_end = lv_label_create(btn_end_cal);
    lv_label_set_text(icon_end, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_end, &my_font_cn_16, 0);
    lv_obj_center(icon_end);

    g_manual_status_dropdown = lv_dropdown_create(parent);
    lv_obj_set_size(g_manual_status_dropdown, 120, 30);
    lv_obj_set_pos(g_manual_status_dropdown, 850, y_pos + 3);
    lv_dropdown_set_options(g_manual_status_dropdown, "全部\n正常\n异常");
    lv_obj_set_style_text_font(g_manual_status_dropdown, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_manual_status_dropdown), &my_font_cn_16, 0);

    lv_obj_t *query_btn = lv_btn_create(parent);
    lv_obj_set_size(query_btn, 100, 30);
    lv_obj_set_pos(query_btn, 1000, y_pos + 3);
    lv_obj_set_style_bg_color(query_btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(query_btn, 0, 0);
    lv_obj_set_style_radius(query_btn, 5, 0);
    lv_obj_add_event_cb(query_btn, ui_log_rec_manual_query_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *query_label = lv_label_create(query_btn);
    lv_label_set_text(query_label, "查询");
    lv_obj_set_style_text_color(query_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(query_label, &my_font_cn_16, 0);
    lv_obj_center(query_label);

    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, 555);
    lv_obj_set_pos(table_container, 0, 45);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "启动时间", "计划运行时长", "实际运行时长", "状态", "详情"};
    int header_x[] = {20, 160, 385, 625, 860, 1010};
    for (int i = 0; i < 6; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    g_manual_table_area = lv_obj_create(table_container);
    lv_obj_set_size(g_manual_table_area, 1138, 515);
    lv_obj_set_pos(g_manual_table_area, 0, 40);
    lv_obj_set_style_bg_opa(g_manual_table_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_manual_table_area, 0, 0);
    lv_obj_set_style_radius(g_manual_table_area, 0, 0);
    lv_obj_set_style_pad_all(g_manual_table_area, 0, 0);
    lv_obj_clear_flag(g_manual_table_area, LV_OBJ_FLAG_SCROLLABLE);

    g_manual_btn_first = lv_btn_create(parent);
    lv_obj_set_size(g_manual_btn_first, 70, 35);
    lv_obj_set_pos(g_manual_btn_first, 350, pagination_y);
    lv_obj_set_style_bg_color(g_manual_btn_first, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_manual_btn_first, 0, 0);
    lv_obj_set_style_radius(g_manual_btn_first, 5, 0);
    lv_obj_t *first_label = lv_label_create(g_manual_btn_first);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    g_manual_btn_prev = lv_btn_create(parent);
    lv_obj_set_size(g_manual_btn_prev, 80, 35);
    lv_obj_set_pos(g_manual_btn_prev, 430, pagination_y);
    lv_obj_set_style_bg_color(g_manual_btn_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_manual_btn_prev, 0, 0);
    lv_obj_set_style_radius(g_manual_btn_prev, 5, 0);
    lv_obj_t *prev_label = lv_label_create(g_manual_btn_prev);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    g_manual_page_info = lv_label_create(parent);
    lv_label_set_text(g_manual_page_info, "0/0");
    lv_obj_set_pos(g_manual_page_info, 540, pagination_y + 8);
    lv_obj_set_style_text_font(g_manual_page_info, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(g_manual_page_info, lv_color_hex(0x333333), 0);

    g_manual_btn_next = lv_btn_create(parent);
    lv_obj_set_size(g_manual_btn_next, 80, 35);
    lv_obj_set_pos(g_manual_btn_next, 600, pagination_y);
    lv_obj_set_style_bg_color(g_manual_btn_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_manual_btn_next, 0, 0);
    lv_obj_set_style_radius(g_manual_btn_next, 5, 0);
    lv_obj_t *next_label = lv_label_create(g_manual_btn_next);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    g_manual_btn_last = lv_btn_create(parent);
    lv_obj_set_size(g_manual_btn_last, 70, 35);
    lv_obj_set_pos(g_manual_btn_last, 690, pagination_y);
    lv_obj_set_style_bg_color(g_manual_btn_last, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_manual_btn_last, 0, 0);
    lv_obj_set_style_radius(g_manual_btn_last, 5, 0);
    lv_obj_t *last_label = lv_label_create(g_manual_btn_last);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);

    ui_log_rec_setup_manual(
        input_start,
        input_end,
        g_manual_status_dropdown,
        g_manual_table_area,
        g_manual_page_info,
        query_btn,
        g_manual_btn_first,
        g_manual_btn_prev,
        g_manual_btn_next,
        g_manual_btn_last
    );
}

/**
 * @brief 创建程序记录视图
 */
static void create_program_record_view(lv_obj_t *parent)
{
    int y_pos = 5;
    int pagination_y = 610;
    char today_buf[32];
    get_today_str(today_buf, sizeof(today_buf));

    lv_obj_t *date_label = lv_label_create(parent);
    lv_label_set_text(date_label, "日期:");
    lv_obj_set_pos(date_label, 10, y_pos + 8);
    lv_obj_set_style_text_font(date_label, &my_font_cn_16, 0);

    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 190, 30);
    lv_obj_set_pos(start_date_container, 60, y_pos + 3);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 1, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 5, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *input_start = lv_textarea_create(start_date_container);
    lv_obj_set_size(input_start, 155, 28);
    lv_obj_set_pos(input_start, 1, 1);
    lv_textarea_set_one_line(input_start, true);
    lv_textarea_set_text(input_start, today_buf);
    lv_obj_set_style_text_font(input_start, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_start, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_start, 0, 0);
    lv_obj_set_style_pad_right(input_start, 0, 0);
    lv_obj_set_style_pad_top(input_start, 4, 0);
    lv_obj_set_style_pad_bottom(input_start, 0, 0);
    lv_obj_set_style_border_width(input_start, 0, 0);
    lv_obj_clear_flag(input_start, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_start_cal = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_start_cal, 28, 28);
    lv_obj_set_pos(btn_start_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_start_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_start_cal, 0, 0);
    lv_obj_set_style_radius(btn_start_cal, 4, 0);
    lv_obj_add_event_cb(btn_start_cal, btn_calendar_start_cb, LV_EVENT_CLICKED, input_start);

    lv_obj_t *icon_start = lv_label_create(btn_start_cal);
    lv_label_set_text(icon_start, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_start, &my_font_cn_16, 0);
    lv_obj_center(icon_start);

    lv_obj_t *to_label = lv_label_create(parent);
    lv_label_set_text(to_label, "至");
    lv_obj_set_pos(to_label, 265, y_pos + 8);
    lv_obj_set_style_text_font(to_label, &my_font_cn_16, 0);

    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 190, 30);
    lv_obj_set_pos(end_date_container, 300, y_pos + 3);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 1, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 5, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *input_end = lv_textarea_create(end_date_container);
    lv_obj_set_size(input_end, 155, 28);
    lv_obj_set_pos(input_end, 1, 1);
    lv_textarea_set_one_line(input_end, true);
    lv_textarea_set_text(input_end, today_buf);
    lv_obj_set_style_text_font(input_end, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_end, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_end, 0, 0);
    lv_obj_set_style_pad_right(input_end, 0, 0);
    lv_obj_set_style_pad_top(input_end, 4, 0);
    lv_obj_set_style_pad_bottom(input_end, 0, 0);
    lv_obj_set_style_border_width(input_end, 0, 0);
    lv_obj_clear_flag(input_end, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_end_cal = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_end_cal, 28, 28);
    lv_obj_set_pos(btn_end_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_end_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_end_cal, 0, 0);
    lv_obj_set_style_radius(btn_end_cal, 4, 0);
    lv_obj_add_event_cb(btn_end_cal, btn_calendar_end_cb, LV_EVENT_CLICKED, input_end);

    lv_obj_t *icon_end = lv_label_create(btn_end_cal);
    lv_label_set_text(icon_end, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_end, &my_font_cn_16, 0);
    lv_obj_center(icon_end);

    g_program_status_dropdown = lv_dropdown_create(parent);
    lv_obj_set_size(g_program_status_dropdown, 120, 30);
    lv_obj_set_pos(g_program_status_dropdown, 850, y_pos + 3);
    lv_dropdown_set_options(g_program_status_dropdown, "全部\n正常\n异常");
    lv_obj_set_style_text_font(g_program_status_dropdown, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_program_status_dropdown), &my_font_cn_16, 0);

    lv_obj_t *query_btn = lv_btn_create(parent);
    lv_obj_set_size(query_btn, 100, 30);
    lv_obj_set_pos(query_btn, 1000, y_pos + 3);
    lv_obj_set_style_bg_color(query_btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(query_btn, 0, 0);
    lv_obj_set_style_radius(query_btn, 5, 0);
    lv_obj_add_event_cb(query_btn, ui_log_rec_program_query_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *query_label = lv_label_create(query_btn);
    lv_label_set_text(query_label, "查询");
    lv_obj_set_style_text_color(query_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(query_label, &my_font_cn_16, 0);
    lv_obj_center(query_label);

    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, 555);
    lv_obj_set_pos(table_container, 0, 45);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "名称", "启动条件", "启动时间", "计划运行时长", "实际运行时长", "状态", "详情"};
    int header_x[] = {15, 105, 245, 400, 585, 770, 955, 1050};
    for (int i = 0; i < 8; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    g_program_table_area = lv_obj_create(table_container);
    lv_obj_set_size(g_program_table_area, 1138, 515);
    lv_obj_set_pos(g_program_table_area, 0, 40);
    lv_obj_set_style_bg_opa(g_program_table_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_program_table_area, 0, 0);
    lv_obj_set_style_radius(g_program_table_area, 0, 0);
    lv_obj_set_style_pad_all(g_program_table_area, 0, 0);
    lv_obj_clear_flag(g_program_table_area, LV_OBJ_FLAG_SCROLLABLE);

    g_program_btn_first = lv_btn_create(parent);
    lv_obj_set_size(g_program_btn_first, 70, 35);
    lv_obj_set_pos(g_program_btn_first, 350, pagination_y);
    lv_obj_set_style_bg_color(g_program_btn_first, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_program_btn_first, 0, 0);
    lv_obj_set_style_radius(g_program_btn_first, 5, 0);
    lv_obj_t *first_label = lv_label_create(g_program_btn_first);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    g_program_btn_prev = lv_btn_create(parent);
    lv_obj_set_size(g_program_btn_prev, 80, 35);
    lv_obj_set_pos(g_program_btn_prev, 430, pagination_y);
    lv_obj_set_style_bg_color(g_program_btn_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_program_btn_prev, 0, 0);
    lv_obj_set_style_radius(g_program_btn_prev, 5, 0);
    lv_obj_t *prev_label = lv_label_create(g_program_btn_prev);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    g_program_page_info = lv_label_create(parent);
    lv_label_set_text(g_program_page_info, "0/0");
    lv_obj_set_pos(g_program_page_info, 540, pagination_y + 8);
    lv_obj_set_style_text_font(g_program_page_info, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(g_program_page_info, lv_color_hex(0x333333), 0);

    g_program_btn_next = lv_btn_create(parent);
    lv_obj_set_size(g_program_btn_next, 80, 35);
    lv_obj_set_pos(g_program_btn_next, 600, pagination_y);
    lv_obj_set_style_bg_color(g_program_btn_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_program_btn_next, 0, 0);
    lv_obj_set_style_radius(g_program_btn_next, 5, 0);
    lv_obj_t *next_label = lv_label_create(g_program_btn_next);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    g_program_btn_last = lv_btn_create(parent);
    lv_obj_set_size(g_program_btn_last, 70, 35);
    lv_obj_set_pos(g_program_btn_last, 690, pagination_y);
    lv_obj_set_style_bg_color(g_program_btn_last, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(g_program_btn_last, 0, 0);
    lv_obj_set_style_radius(g_program_btn_last, 5, 0);
    lv_obj_t *last_label = lv_label_create(g_program_btn_last);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);

    ui_log_rec_setup_program(
        input_start,
        input_end,
        g_program_status_dropdown,
        g_program_table_area,
        g_program_page_info,
        query_btn,
        g_program_btn_first,
        g_program_btn_prev,
        g_program_btn_next,
        g_program_btn_last
    );
}

/**
 * @brief 创建每日流量视图
 */
static void create_daily_flow_view(lv_obj_t *parent)
{
    int y_pos = 5;
    char today_buf[32];
    get_today_str(today_buf, sizeof(today_buf));

    lv_obj_t *date_label = lv_label_create(parent);
    lv_label_set_text(date_label, "日期:");
    lv_obj_set_pos(date_label, 10, y_pos + 8);
    lv_obj_set_style_text_font(date_label, &my_font_cn_16, 0);

    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 190, 30);
    lv_obj_set_pos(start_date_container, 60, y_pos + 3);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 1, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 5, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *input_start = lv_textarea_create(start_date_container);
    lv_obj_set_size(input_start, 155, 28);
    lv_obj_set_pos(input_start, 1, 1);
    lv_textarea_set_one_line(input_start, true);
    lv_textarea_set_text(input_start, today_buf);
    lv_obj_set_style_text_font(input_start, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_start, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_start, 0, 0);
    lv_obj_set_style_pad_right(input_start, 0, 0);
    lv_obj_set_style_pad_top(input_start, 4, 0);
    lv_obj_set_style_pad_bottom(input_start, 0, 0);
    lv_obj_set_style_border_width(input_start, 0, 0);
    lv_obj_clear_flag(input_start, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_start_cal = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_start_cal, 28, 28);
    lv_obj_set_pos(btn_start_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_start_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_start_cal, 0, 0);
    lv_obj_set_style_radius(btn_start_cal, 4, 0);
    lv_obj_add_event_cb(btn_start_cal, btn_calendar_start_cb, LV_EVENT_CLICKED, input_start);

    lv_obj_t *icon_start = lv_label_create(btn_start_cal);
    lv_label_set_text(icon_start, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_start, &my_font_cn_16, 0);
    lv_obj_center(icon_start);

    lv_obj_t *to_label = lv_label_create(parent);
    lv_label_set_text(to_label, "至");
    lv_obj_set_pos(to_label, 265, y_pos + 8);
    lv_obj_set_style_text_font(to_label, &my_font_cn_16, 0);

    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 190, 30);
    lv_obj_set_pos(end_date_container, 300, y_pos + 3);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 1, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 5, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *input_end = lv_textarea_create(end_date_container);
    lv_obj_set_size(input_end, 155, 28);
    lv_obj_set_pos(input_end, 1, 1);
    lv_textarea_set_one_line(input_end, true);
    lv_textarea_set_text(input_end, today_buf);
    lv_obj_set_style_text_font(input_end, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_end, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_end, 0, 0);
    lv_obj_set_style_pad_right(input_end, 0, 0);
    lv_obj_set_style_pad_top(input_end, 4, 0);
    lv_obj_set_style_pad_bottom(input_end, 0, 0);
    lv_obj_set_style_border_width(input_end, 0, 0);
    lv_obj_clear_flag(input_end, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *btn_end_cal = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_end_cal, 28, 28);
    lv_obj_set_pos(btn_end_cal, 161, 1);
    lv_obj_set_style_bg_color(btn_end_cal, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(btn_end_cal, 0, 0);
    lv_obj_set_style_radius(btn_end_cal, 4, 0);
    lv_obj_add_event_cb(btn_end_cal, btn_calendar_end_cb, LV_EVENT_CLICKED, input_end);

    lv_obj_t *icon_end = lv_label_create(btn_end_cal);
    lv_label_set_text(icon_end, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(icon_end, &my_font_cn_16, 0);
    lv_obj_center(icon_end);

    lv_obj_t *query_btn = lv_btn_create(parent);
    lv_obj_set_size(query_btn, 100, 30);
    lv_obj_set_pos(query_btn, 1000, y_pos + 3);
    lv_obj_set_style_bg_color(query_btn, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(query_btn, 0, 0);
    lv_obj_set_style_radius(query_btn, 5, 0);
    lv_obj_add_event_cb(query_btn, query_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *query_label = lv_label_create(query_btn);
    lv_label_set_text(query_label, "查询");
    lv_obj_set_style_text_color(query_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(query_label, &my_font_cn_16, 0);
    lv_obj_center(query_label);

    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, 555);
    lv_obj_set_pos(table_container, 0, 45);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "日期", "灌肥量(L)", "灌水量(m³)"};
    int header_x[] = {20, 250, 550, 850};
    for (int i = 0; i < 4; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    show_placeholder_message(table_container, "当前版本暂未接入每日流量数据");
}


