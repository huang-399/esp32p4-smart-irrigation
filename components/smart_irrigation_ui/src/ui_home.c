/**
 * @file ui_home.c
 * @brief 智慧种植园监控系统 - 首页界面
 * 布局：左右分栏，直接在内容区绘制
 * - 左侧：圆环显示 + 禁用自动化按钮 + 设备状态列表
 * - 右侧：灌溉计划表格 + 按钮 + 状态文字
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include <math.h>  /* 用于cosf和sinf函数 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_mode_circle(lv_obj_t *parent);
static void create_status_list(lv_obj_t *parent);
static void create_schedule_area(lv_obj_t *parent);
static void refresh_schedule_table_display(void);
static void btn_manual_program_cb(lv_event_t *e);
static void btn_manual_irrigation_cb(lv_event_t *e);
static void btn_disable_auto_cb(lv_event_t *e);
static void show_enable_auto_dialog(void);
static void show_warning_dialog(const char *title, const char *message);
static void show_manual_irrigation_dialog(void);
static void show_program_selection_dialog(void);
static void program_start_confirm_cb(lv_event_t *e);
static void program_start_cancel_cb(lv_event_t *e);
static void dialog_confirm_cb(lv_event_t *e);
static void dialog_cancel_cb(lv_event_t *e);
static void dialog_ok_cb(lv_event_t *e);
static void irrigation_confirm_cb(lv_event_t *e);
static void irrigation_cancel_cb(lv_event_t *e);
static void btn_uniform_set_cb(lv_event_t *e);
static void show_uniform_set_confirm_dialog(void);
static void uniform_set_confirm_cb(lv_event_t *e);
static void uniform_set_cancel_cb(lv_event_t *e);
static void enable_auto_mode(void);
static void disable_auto_mode(void);
static void textarea_click_cb(lv_event_t *e);
static void close_home_dialog(void);
static void refresh_runtime_status_display(void);
static void format_runtime_status_text(const ui_irrigation_runtime_status_t *status, char *buf, int buf_size);
static void format_elapsed_text(int elapsed_seconds, char *buf, int buf_size);
static void runtime_status_timer_cb(lv_timer_t *timer);
static void refresh_selected_field_display(void);
static void refresh_zone_dropdown_options(void);
static void apply_zone_selection(int option_index);
static void field_dropdown_changed_cb(lv_event_t *e);

/*********************
 *  STATIC VARIABLES
 *********************/
static lv_obj_t *g_arc = NULL;           /* 圆环对象 */
static lv_obj_t *g_mode_label = NULL;    /* 模式文字 */
static lv_obj_t *g_btn_toggle = NULL;    /* 切换按钮 */
static lv_obj_t *g_btn_label = NULL;     /* 按钮文字 */
static lv_obj_t *g_tick_lines[90];       /* 刻度线数组 */
static int g_tick_count = 0;             /* 实际创建的刻度线数量 */
static bool g_auto_mode_enabled = true;  /* 自动模式状态：true=启用，false=禁用 */
static lv_obj_t *g_dialog = NULL;        /* 确认对话框 */
static lv_obj_t *g_status_text_label = NULL; /* 运行状态文字 */
static lv_obj_t *g_program_checkboxes[100]; /* 程序选择复选框数组 */
static int g_program_checkbox_count = 0;    /* 实际创建的复选框数量 */
static lv_obj_t *g_manual_input_pre_water = NULL;
static lv_obj_t *g_manual_input_post_water = NULL;
static lv_obj_t *g_manual_input_duration = NULL;
static lv_obj_t *g_manual_dropdown_formula = NULL;
static lv_obj_t *g_schedule_card = NULL;   /* 首页计划卡片 */
static lv_timer_t *g_runtime_status_timer = NULL;

static ui_program_auto_mode_set_cb_t g_auto_mode_set_cb = NULL;
static ui_program_auto_mode_get_cb_t g_auto_mode_get_cb = NULL;
static ui_program_start_cb_t g_program_start_cb = NULL;
static ui_manual_irrigation_start_cb_t g_manual_irrigation_start_cb = NULL;
static ui_irrigation_status_get_cb_t g_irrigation_status_get_cb = NULL;
static ui_home_runtime_refresh_cb_t g_runtime_refresh_cb = NULL;
static ui_get_zone_count_cb_t g_home_zone_count_cb = NULL;
static ui_get_zone_list_cb_t g_home_zone_list_cb = NULL;
static ui_home_zone_field_resolve_cb_t g_home_zone_field_resolve_cb = NULL;

static void format_elapsed_text(int elapsed_seconds, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return;
    }

    if (elapsed_seconds < 0) {
        elapsed_seconds = 0;
    }

    int hours = elapsed_seconds / 3600;
    int minutes = (elapsed_seconds % 3600) / 60;
    int seconds = elapsed_seconds % 60;

    if (hours > 0) {
        snprintf(buf, buf_size, "%d时%02d分%02d秒", hours, minutes, seconds);
    } else {
        snprintf(buf, buf_size, "%d分%02d秒", minutes, seconds);
    }
}

static void format_runtime_status_text(const ui_irrigation_runtime_status_t *status, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return;
    }

    if (!status || !status->busy) {
        snprintf(buf, buf_size, "无手动轮灌&无程序运行");
        return;
    }

    char elapsed_buf[32];
    format_elapsed_text(status->elapsed_seconds, elapsed_buf, sizeof(elapsed_buf));

    if (status->program_active) {
        snprintf(buf, buf_size, "程序：%s  合计时长：%d分  运行时长：%s",
                 status->active_name[0] ? status->active_name : "--",
                 status->total_duration,
                 elapsed_buf);
    } else if (status->manual_irrigation_active) {
        snprintf(buf, buf_size, "名称：%s  合计时长：%d分  运行时长：%s",
                 status->active_name[0] ? status->active_name : "手动轮灌",
                 status->total_duration,
                 elapsed_buf);
    } else {
        snprintf(buf, buf_size, "%s", status->status_text[0] ? status->status_text : "无手动轮灌&无程序运行");
    }
}

static void refresh_runtime_status_display(void)
{
    if (!g_irrigation_status_get_cb || !g_status_text_label) {
        return;
    }

    ui_irrigation_runtime_status_t status = {0};
    if (!g_irrigation_status_get_cb(&status)) {
        return;
    }

    char buf[160];
    format_runtime_status_text(&status, buf, sizeof(buf));
    lv_label_set_text(g_status_text_label, buf);
}

static void runtime_status_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    refresh_runtime_status_display();
    refresh_schedule_table_display();
}

static void close_home_dialog(void)
{
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
    g_manual_input_pre_water = NULL;
    g_manual_input_post_water = NULL;
    g_manual_input_duration = NULL;
    g_manual_dropdown_formula = NULL;
    g_program_checkbox_count = 0;
    if (g_runtime_status_timer) {
        lv_timer_del(g_runtime_status_timer);
        g_runtime_status_timer = NULL;
    }
}

/* Zigbee 设备状态标签（首页"当前设备状态"区域） */
#define UI_HOME_MAX_ZONES 16

static lv_obj_t *g_field_val_labels[6] = {NULL};  /* 6 行传感器值 */
static lv_obj_t *g_field_name_labels[6] = {NULL};  /* 6 行名称标签 */
static lv_obj_t *g_field_dropdown = NULL;           /* 灌区下拉框 */
static lv_obj_t *g_dev_status_label = NULL;         /* 右下：设备开关状态标签 */
static lv_obj_t *g_zigbee_status_label = NULL;      /* Zigbee 连接状态标签 */
static int g_selected_field = -1;                   /* 当前灌区映射的田地(0~5, -1=无映射) */
static int g_selected_zone_option = -1;             /* 当前选中的灌区下拉索引 */
static int g_zone_option_count = 0;                 /* 当前灌区选项数量 */
static int g_zone_slot_map[UI_HOME_MAX_ZONES] = {0};
static int g_zone_field_map[UI_HOME_MAX_ZONES] = {0};

/* 首页设备状态摘要缓存 */
static struct {
    bool valid;
    bool water_pump_on;
    bool fert_pump_on;
    bool fert_valve_on;
    bool water_valve_on;
    bool mixer_on;
} s_home_control_cache = {0};

static struct {
    bool valid;
    bool valve_bound;
    bool valve_on;
    bool flow_bound;
    float flow;
    bool pressure_bound;
    float pressure;
} s_home_pipe_cache[7] = {0};

static struct {
    bool valid;
    bool switch_on;
    bool level_bound;
    float level;
} s_home_tank_cache[3] = {0};

/* 田地实时数据缓存，用于田地下拉切换时恢复显示 */
static struct {
    float n, p, k, temp, humi, light;
    uint8_t registered_mask;
    bool  valid;
} s_field_cache[6] = {0};

static void refresh_home_device_status_summary(void)
{
    if (!g_dev_status_label) {
        return;
    }

    lv_label_set_text(g_dev_status_label, "");
    lv_obj_add_flag(g_dev_status_label, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_selected_field_display(void)
{
    if (!g_field_val_labels[0]) {
        return;
    }

    if (g_selected_field < 0 || g_selected_field >= 6) {
        for (int i = 0; i < 6; i++) {
            lv_label_set_text(g_field_val_labels[i], "---");
        }
        return;
    }

    const int field_idx = g_selected_field;
    const float vals[] = {
        s_field_cache[field_idx].n,
        s_field_cache[field_idx].p,
        s_field_cache[field_idx].k,
        s_field_cache[field_idx].temp,
        s_field_cache[field_idx].humi,
        s_field_cache[field_idx].light
    };

    char buf[16];
    for (int i = 0; i < 6; i++) {
        if (s_field_cache[field_idx].valid && (s_field_cache[field_idx].registered_mask & (1U << i))) {
            snprintf(buf, sizeof(buf), "%.1f", vals[i]);
        } else {
            snprintf(buf, sizeof(buf), "---");
        }
        lv_label_set_text(g_field_val_labels[i], buf);
    }
}

static void apply_zone_selection(int option_index)
{
    g_selected_zone_option = option_index;

    if (option_index < 0 || option_index >= g_zone_option_count) {
        g_selected_field = -1;
        refresh_selected_field_display();
        return;
    }

    if (option_index < UI_HOME_MAX_ZONES) {
        g_selected_field = g_zone_field_map[option_index];
    } else {
        g_selected_field = -1;
    }

    refresh_selected_field_display();
}

static void refresh_zone_dropdown_options(void)
{
    if (!g_field_dropdown) {
        return;
    }

    int previous_slot = -1;
    if (g_selected_zone_option >= 0 && g_selected_zone_option < g_zone_option_count) {
        previous_slot = g_zone_slot_map[g_selected_zone_option];
    }

    g_zone_option_count = 0;
    memset(g_zone_slot_map, -1, sizeof(g_zone_slot_map));
    memset(g_zone_field_map, -1, sizeof(g_zone_field_map));

    if (!g_home_zone_count_cb || !g_home_zone_list_cb) {
        lv_dropdown_set_options(g_field_dropdown, "暂无灌区");
        lv_dropdown_set_selected(g_field_dropdown, 0);
        apply_zone_selection(-1);
        return;
    }

    int zone_count = g_home_zone_count_cb();
    if (zone_count <= 0) {
        lv_dropdown_set_options(g_field_dropdown, "暂无灌区");
        lv_dropdown_set_selected(g_field_dropdown, 0);
        apply_zone_selection(-1);
        return;
    }

    if (zone_count > UI_HOME_MAX_ZONES) {
        zone_count = UI_HOME_MAX_ZONES;
    }

    ui_zone_row_t rows[UI_HOME_MAX_ZONES] = {0};
    int filled = g_home_zone_list_cb(rows, zone_count, 0);
    if (filled <= 0) {
        lv_dropdown_set_options(g_field_dropdown, "暂无灌区");
        lv_dropdown_set_selected(g_field_dropdown, 0);
        apply_zone_selection(-1);
        return;
    }

    char options[UI_HOME_MAX_ZONES * 40] = {0};
    size_t pos = 0;
    int selected_option = 0;
    bool selected_kept = false;

    for (int i = 0; i < filled; i++) {
        size_t remain = sizeof(options) - pos;
        int written = snprintf(options + pos, remain, "%s%s",
                               (i > 0) ? "\n" : "",
                               rows[i].name[0] ? rows[i].name : "未命名灌区");
        if (written < 0 || (size_t)written >= remain) {
            break;
        }
        pos += (size_t)written;
        g_zone_slot_map[i] = rows[i].slot_index;
        g_zone_field_map[i] = g_home_zone_field_resolve_cb ?
            g_home_zone_field_resolve_cb(rows[i].slot_index) : -1;
        if (!selected_kept && rows[i].slot_index == previous_slot) {
            selected_option = i;
            selected_kept = true;
        }
        g_zone_option_count++;
    }

    if (g_zone_option_count <= 0) {
        lv_dropdown_set_options(g_field_dropdown, "暂无灌区");
        lv_dropdown_set_selected(g_field_dropdown, 0);
        apply_zone_selection(-1);
        return;
    }

    lv_dropdown_set_options(g_field_dropdown, options);
    lv_dropdown_set_selected(g_field_dropdown, (uint32_t)selected_option);
    apply_zone_selection(selected_option);
}

static void field_dropdown_changed_cb(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    if (!dd) {
        return;
    }

    uint32_t sel = lv_dropdown_get_selected(dd);
    if (sel >= (uint32_t)g_zone_option_count) {
        return;
    }

    apply_zone_selection((int)sel);
}


/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ui_home_register_auto_mode_set_cb(ui_program_auto_mode_set_cb_t cb)
{
    g_auto_mode_set_cb = cb;
}

void ui_home_register_auto_mode_get_cb(ui_program_auto_mode_get_cb_t cb)
{
    g_auto_mode_get_cb = cb;
}

void ui_home_register_program_start_cb(ui_program_start_cb_t cb)
{
    g_program_start_cb = cb;
}

void ui_home_register_manual_irrigation_start_cb(ui_manual_irrigation_start_cb_t cb)
{
    g_manual_irrigation_start_cb = cb;
}

void ui_home_register_irrigation_status_get_cb(ui_irrigation_status_get_cb_t cb)
{
    g_irrigation_status_get_cb = cb;
}

void ui_home_register_runtime_refresh_cb(ui_home_runtime_refresh_cb_t cb)
{
    g_runtime_refresh_cb = cb;
}

void ui_home_register_zone_query_cbs(
    ui_get_zone_count_cb_t count_cb,
    ui_get_zone_list_cb_t list_cb,
    ui_get_zone_detail_cb_t detail_cb)
{
    g_home_zone_count_cb = count_cb;
    g_home_zone_list_cb = list_cb;
    (void)detail_cb;
}

void ui_home_register_zone_field_resolve_cb(ui_home_zone_field_resolve_cb_t cb)
{
    g_home_zone_field_resolve_cb = cb;
}

/**
 * @brief 创建首页界面
 * @param parent 父容器（主内容区）
 */
void ui_home_create(lv_obj_t *parent)
{
    /* 重置静态指针（旧对象已被 ui_switch_nav 中的 lv_obj_clean 销毁） */
    g_arc = NULL;
    g_mode_label = NULL;
    g_btn_toggle = NULL;
    g_btn_label = NULL;
    g_tick_count = 0;
    g_program_checkbox_count = 0;
    g_status_text_label = NULL;
    g_manual_input_pre_water = NULL;
    g_manual_input_post_water = NULL;
    g_manual_input_duration = NULL;
    g_manual_dropdown_formula = NULL;
    g_schedule_card = NULL;
    if (g_runtime_status_timer) {
        lv_timer_del(g_runtime_status_timer);
        g_runtime_status_timer = NULL;
    }
    g_dev_status_label = NULL;
    g_zigbee_status_label = NULL;
    g_field_dropdown = NULL;
    g_selected_field = -1;
    g_selected_zone_option = -1;
    g_zone_option_count = 0;
    memset(g_zone_slot_map, -1, sizeof(g_zone_slot_map));
    memset(g_zone_field_map, -1, sizeof(g_zone_field_map));
    memset(&s_home_control_cache, 0, sizeof(s_home_control_cache));
    memset(s_home_pipe_cache, 0, sizeof(s_home_pipe_cache));
    memset(s_home_tank_cache, 0, sizeof(s_home_tank_cache));
    memset(s_field_cache, 0, sizeof(s_field_cache));
    for (int i = 0; i < 6; i++) {
        g_field_val_labels[i] = NULL;
        g_field_name_labels[i] = NULL;
    }
    /* g_dialog 由 ui_home_close_dialog() 在 ui_switch_nav 中处理 */

    /* 清空父容器 */
    lv_obj_clean(parent);

    /* 创建左侧：圆环模式 */
    create_mode_circle(parent);

    /* 创建左侧：设备状态列表 */
    create_status_list(parent);

    /* 创建右侧：灌溉计划区域 */
    create_schedule_area(parent);

    /* 同步自动模式状态到UI（g_auto_mode_enabled 在页面切换后仍保持） */
    if (g_auto_mode_get_cb) {
        g_auto_mode_enabled = g_auto_mode_get_cb();
    }

    if (!g_auto_mode_enabled) {
        disable_auto_mode();
    }

    refresh_selected_field_display();
    refresh_home_device_status_summary();
    refresh_runtime_status_display();
    refresh_schedule_table_display();
    if (g_runtime_refresh_cb) {
        g_runtime_refresh_cb();
    }
}

/**
 * @brief 创建模式圆环显示（左上角白色卡片）
 */
static void create_mode_circle(lv_obj_t *parent)
{
    /* 创建左上角白色卡片 - 约1/4宽度 */
    /* 内容区宽度1178，1/4约294，减去边距 = 284 */
    lv_obj_t *mode_card = lv_obj_create(parent);
    lv_obj_set_size(mode_card, 284, 362);  /* 高度：735/2-5-5 = 362 */
    lv_obj_set_pos(mode_card, 5, 5);
    lv_obj_set_style_bg_color(mode_card, lv_color_white(), 0);
    lv_obj_set_style_border_width(mode_card, 0, 0);
    lv_obj_set_style_radius(mode_card, 10, 0);
    lv_obj_set_style_pad_all(mode_card, 0, 0);
    lv_obj_clear_flag(mode_card, LV_OBJ_FLAG_SCROLLABLE);

    /* 圆弧（进度环） - 照片中看到是蓝色粗圆环，更大一些 */
    g_arc = lv_arc_create(mode_card);
    lv_obj_set_size(g_arc, 220, 220);
    lv_obj_set_pos(g_arc, 32, 30);  /* 居中：(284-220)/2 = 32 */
    lv_arc_set_range(g_arc, 0, 100);
    lv_arc_set_value(g_arc, 100);  /* 设置为100，让圆环显示满圈 */
    lv_arc_set_bg_angles(g_arc, 0, 360);
    lv_obj_set_style_arc_width(g_arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_arc, lv_color_hex(0xe0e0e0), LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_arc, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_remove_flag(g_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_rounded(g_arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(g_arc, false, LV_PART_INDICATOR);
    /* 隐藏拖动的小球 knob */
    lv_obj_set_style_bg_opa(g_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(g_arc, 0, LV_PART_KNOB);

    /* 在圆环外围添加刻度线 - 像时钟刻度那样 */
    int center_x = 32 + 110;  /* 圆心X: 卡片内的绝对位置 */
    int center_y = 30 + 110;  /* 圆心Y */
    int arc_outer_radius = 110;  /* 圆环外边缘半径 */
    int gap = 8;  /* 刻度线与圆环的间距 */
    int inner_radius = arc_outer_radius + gap;  /* 刻度线起点：110 + 8 = 118 */
    int outer_radius = inner_radius + 6;  /* 刻度线终点：118 + 6 = 124，刻度线长度6px */
    int tick_count = 90;      /* 刻度数量增加到90 */

    /* 为所有刻度线分配静态点数组 */
    static lv_point_precise_t tick_points[90][2];

    g_tick_count = 0;  /* 重置刻度线计数 */

    for (int i = 0; i < tick_count; i++) {
        float angle_deg = i * 360.0f / tick_count;  /* 角度（度数） */

        /* 跳过下方90度区域的刻度线 - LVGL中90度在底部，所以范围是45-135度 */
        if (angle_deg >= 45 && angle_deg <= 135) {
            continue;
        }

        float angle = angle_deg * 3.14159f / 180.0f;  /* 转换为弧度 */

        /* 计算刻度线的起点和终点 */
        int x1 = center_x + (int)(inner_radius * cosf(angle));
        int y1 = center_y + (int)(inner_radius * sinf(angle));
        int x2 = center_x + (int)(outer_radius * cosf(angle));
        int y2 = center_y + (int)(outer_radius * sinf(angle));

        /* 保存到独立的点数组 */
        tick_points[i][0].x = x1;
        tick_points[i][0].y = y1;
        tick_points[i][1].x = x2;
        tick_points[i][1].y = y2;

        /* 创建刻度线并保存引用 */
        g_tick_lines[g_tick_count] = lv_line_create(mode_card);
        lv_line_set_points(g_tick_lines[g_tick_count], tick_points[i], 2);
        lv_obj_set_style_line_width(g_tick_lines[g_tick_count], 2, 0);  /* 线宽2px */
        lv_obj_set_style_line_color(g_tick_lines[g_tick_count], COLOR_PRIMARY, 0);  /* 刻度线颜色与圆环一致 */
        g_tick_count++;
    }

    /* 圆环中心文字 "自动模式" */
    g_mode_label = lv_label_create(mode_card);
    lv_label_set_text(g_mode_label, "自动模式");
    lv_obj_set_style_text_font(g_mode_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_mode_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_mode_label, COLOR_PRIMARY, 0);
    lv_obj_align_to(g_mode_label, g_arc, LV_ALIGN_CENTER, 0, 0);  /* 相对于圆环居中 */

    /* "禁用自动化" 按钮（红色边框，圆角矩形） - 在圆环正下方 */
    g_btn_toggle = lv_btn_create(mode_card);
    lv_obj_set_size(g_btn_toggle, 150, 42);
    lv_obj_set_pos(g_btn_toggle, 67, 270);  /* 居中：(284-150)/2 = 67 */
    lv_obj_set_style_bg_color(g_btn_toggle, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_btn_toggle, 2, 0);
    lv_obj_set_style_border_color(g_btn_toggle, COLOR_ERROR, 0);
    lv_obj_set_style_radius(g_btn_toggle, 19, 0);
    lv_obj_add_event_cb(g_btn_toggle, btn_disable_auto_cb, LV_EVENT_CLICKED, NULL);

    g_btn_label = lv_label_create(g_btn_toggle);
    lv_label_set_text(g_btn_label, "禁用自动化");
    lv_obj_set_style_text_color(g_btn_label, COLOR_ERROR, 0);
    lv_obj_set_style_text_font(g_btn_label, &my_font_cn_16, 0);
    lv_obj_center(g_btn_label);
}

/**
 * @brief 创建设备状态和按钮区域（下半部分的白色卡片）
 */
static void create_status_list(lv_obj_t *parent)
{
    /* 创建白色卡片容器 - 下半部分 */
    /* 内容区高度735，上下对半，每半约367px */
    /* 卡片距离浅灰色边界5px */
    lv_obj_t *status_card = lv_obj_create(parent);
    lv_obj_set_size(status_card, 1168, 358);  /* 高度：735-362-5-5-5 = 358 */
    lv_obj_set_pos(status_card, 5, 372);  /* Y: 5 + 362 + 5 = 372 */
    lv_obj_set_style_bg_color(status_card, lv_color_white(), 0);
    lv_obj_set_style_border_width(status_card, 0, 0);
    lv_obj_set_style_radius(status_card, 10, 0);
    lv_obj_set_style_pad_all(status_card, 0, 0);
    lv_obj_clear_flag(status_card, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧：设备状态 */
    /* 标题 "当前设备状态" + 田地选择按钮 */
    lv_obj_t *title = lv_label_create(status_card);
    lv_label_set_text(title, "当前设备状态");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(title, 20, 20);

    /* Zigbee 连接状态 */
    g_zigbee_status_label = lv_label_create(status_card);
    lv_label_set_text(g_zigbee_status_label, "Zigbee 离线");
    lv_obj_set_style_text_font(g_zigbee_status_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(g_zigbee_status_label, lv_color_hex(0x999999), 0);
    lv_obj_set_pos(g_zigbee_status_label, 340, 20);

    /* 灌区选择下拉框 */
    g_field_dropdown = lv_dropdown_create(status_card);
    lv_dropdown_set_options(g_field_dropdown, "暂无灌区");
    lv_obj_set_size(g_field_dropdown, 140, 35);
    lv_obj_set_pos(g_field_dropdown, 160, 14);
    lv_obj_set_style_text_font(g_field_dropdown, &my_font_cn_16, 0);
    lv_obj_add_event_cb(g_field_dropdown, ui_dropdown_list_font_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_field_dropdown, field_dropdown_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    refresh_zone_dropdown_options();

    /* 6 行传感器数据 */
    static const char *sensor_names[] = {
        "氮(mg/kg):", "磷(mg/kg):", "钾(mg/kg):",
        "温度(°C):", "湿度(%):", "光照(lux):"
    };

    for (int i = 0; i < 6; i++) {
        int y = 65 + i * 45;

        g_field_name_labels[i] = lv_label_create(status_card);
        lv_label_set_text(g_field_name_labels[i], sensor_names[i]);
        lv_obj_set_style_text_font(g_field_name_labels[i], &my_font_cn_16, 0);
        lv_obj_set_pos(g_field_name_labels[i], 30, y);

        g_field_val_labels[i] = lv_label_create(status_card);
        lv_label_set_text(g_field_val_labels[i], "---");
        lv_obj_set_style_text_font(g_field_val_labels[i], &my_font_cn_16, 0);
        lv_obj_set_pos(g_field_val_labels[i], 180, y);
    }

    /* 设备状态摘要（暂不显示） */
    g_dev_status_label = lv_label_create(status_card);
    lv_label_set_text(g_dev_status_label, "");
    lv_obj_set_style_text_color(g_dev_status_label, COLOR_TEXT_MAIN, 0);
    lv_obj_set_style_text_font(g_dev_status_label, &my_font_cn_16, 0);
    lv_obj_set_pos(g_dev_status_label, 260, 115);
    lv_obj_add_flag(g_dev_status_label, LV_OBJ_FLAG_HIDDEN);

    /* 右上角：按钮区域 - 更小的尺寸 */
    lv_obj_t *btn_program = lv_btn_create(status_card);
    lv_obj_set_size(btn_program, 140, 45);
    lv_obj_set_pos(btn_program, 850, 20);  /* 右上角 */
    lv_obj_set_style_bg_color(btn_program, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_program, 22, 0);
    lv_obj_add_event_cb(btn_program, btn_manual_program_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_prog = lv_label_create(btn_program);
    lv_label_set_text(label_prog, "手动运行程序");
    lv_obj_set_style_text_color(label_prog, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_prog, &my_font_cn_16, 0);
    lv_obj_center(label_prog);

    lv_obj_t *btn_irrigation = lv_btn_create(status_card);
    lv_obj_set_size(btn_irrigation, 140, 45);
    lv_obj_set_pos(btn_irrigation, 1000, 20);  /* 右上角，第二个按钮 */
    lv_obj_set_style_bg_color(btn_irrigation, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_irrigation, 22, 0);
    lv_obj_add_event_cb(btn_irrigation, btn_manual_irrigation_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_irr = lv_label_create(btn_irrigation);
    lv_label_set_text(label_irr, "手动进行轮灌");
    lv_obj_set_style_text_color(label_irr, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_irr, &my_font_cn_16, 0);
    lv_obj_center(label_irr);

    /* 中间状态文字 */
    g_status_text_label = lv_label_create(status_card);
    lv_label_set_text(g_status_text_label, "无手动轮灌&无程序运行");
    lv_obj_set_style_text_color(g_status_text_label, lv_color_black(), 0);
    lv_obj_set_style_text_font(g_status_text_label, &my_font_cn_16, 0);
    lv_label_set_long_mode(g_status_text_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_size(g_status_text_label, 620, LV_SIZE_CONTENT);
    lv_obj_set_pos(g_status_text_label, 520, 65);

    if (!g_runtime_status_timer) {
        g_runtime_status_timer = lv_timer_create(runtime_status_timer_cb, 1000, NULL);
    }
}

/**
 * @brief 创建灌溉计划区域（右上角白色卡片）
 */
static void refresh_schedule_table_display(void)
{
    if (!g_schedule_card || !lv_obj_is_valid(g_schedule_card)) {
        return;
    }

    lv_obj_clean(g_schedule_card);

    /* 标题 - 直接显示文字 */
    lv_obj_t *title = lv_label_create(g_schedule_card);
    lv_label_set_text(title, "今日剩余灌溉计划（启动条件：定时）");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(title, 15, 15);

    /* 表格表头区域 */
    lv_obj_t *header_bg = lv_obj_create(g_schedule_card);
    lv_obj_set_size(header_bg, 850, 40);
    lv_obj_set_pos(header_bg, 15, 50);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f8ff), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);
    lv_obj_clear_flag(header_bg, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "程序名称", "关联配方", "启动条件", "启动时间", "合计时长"};
    const int col_widths[] = {60, 180, 140, 140, 140, 140};
    const int col_x[] = {5, 65, 245, 385, 525, 665};
    int x_pos = 5;

    for (int i = 0; i < 6; i++) {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(header_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_pos(header_label, x_pos, 12);
        x_pos += col_widths[i];
    }

    const int max_rows = 4;
    const int row_height = 64;
    const int row_y_start = 90;
    const int row_width = 850;
    const int program_count = ui_program_get_count();
    int display_row = 0;

    for (int i = 0; i < program_count && display_row < max_rows; i++) {
        char condition[32];
        char next_start[40];
        char duration[24];
        const char *name = ui_program_get_name(i);
        const char *formula = ui_program_get_formula(i);

        ui_program_get_condition_text(i, condition, sizeof(condition));
        if (strcmp(condition, "定时") != 0) {
            continue;
        }

        ui_program_get_next_start_text(i, next_start, sizeof(next_start));
        snprintf(duration, sizeof(duration), "%d分钟", ui_program_get_duration(i));

        lv_obj_t *row_bg = lv_obj_create(g_schedule_card);
        lv_obj_set_size(row_bg, row_width, row_height);
        lv_obj_set_pos(row_bg, 15, row_y_start + display_row * row_height);
        lv_obj_set_style_bg_color(row_bg, lv_color_white(), 0);
        lv_obj_set_style_border_width(row_bg, 0, 0);
        lv_obj_set_style_radius(row_bg, 0, 0);
        lv_obj_set_style_pad_all(row_bg, 0, 0);
        lv_obj_clear_flag(row_bg, LV_OBJ_FLAG_SCROLLABLE);

        if ((display_row % 2) == 1) {
            lv_obj_set_style_bg_color(row_bg, lv_color_hex(0xfafcff), 0);
        }

        const char *index_text = "1";
        switch (display_row) {
            case 0: index_text = "1"; break;
            case 1: index_text = "2"; break;
            case 2: index_text = "3"; break;
            default: index_text = "4"; break;
        }

        const char *values[] = {
            index_text,
            name ? name : "--",
            (formula && formula[0]) ? formula : "--",
            condition,
            next_start[0] ? next_start : "--",
            duration,
        };

        for (int col = 0; col < 6; col++) {
            lv_obj_t *label = lv_label_create(row_bg);
            lv_label_set_text(label, values[col]);
            lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
            lv_obj_set_width(label, col_widths[col] - 12);
            lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(label, COLOR_TEXT_MAIN, 0);
            lv_obj_set_pos(label, col_x[col], 22);
        }

        display_row++;
    }

    for (; display_row < max_rows; display_row++) {
        lv_obj_t *row_bg = lv_obj_create(g_schedule_card);
        lv_obj_set_size(row_bg, row_width, row_height);
        lv_obj_set_pos(row_bg, 15, row_y_start + display_row * row_height);
        lv_obj_set_style_bg_color(row_bg, lv_color_white(), 0);
        lv_obj_set_style_border_width(row_bg, 0, 0);
        lv_obj_set_style_radius(row_bg, 0, 0);
        lv_obj_set_style_pad_all(row_bg, 0, 0);
        lv_obj_clear_flag(row_bg, LV_OBJ_FLAG_SCROLLABLE);

        if ((display_row % 2) == 1) {
            lv_obj_set_style_bg_color(row_bg, lv_color_hex(0xfafcff), 0);
        }

        if (display_row == 0) {
            lv_obj_t *empty_label = lv_label_create(row_bg);
            lv_label_set_text(empty_label, "暂无定时灌溉计划");
            lv_obj_set_style_text_font(empty_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(empty_label, COLOR_TEXT_GRAY, 0);
            lv_obj_center(empty_label);
        }
    }
}

/**
 * @brief 创建灌溉计划区域（右上角白色卡片）
 */
static void create_schedule_area(lv_obj_t *parent)
{
    g_schedule_card = lv_obj_create(parent);
    lv_obj_set_size(g_schedule_card, 879, 362);
    lv_obj_set_pos(g_schedule_card, 294, 5);
    lv_obj_set_style_bg_color(g_schedule_card, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_schedule_card, 0, 0);
    lv_obj_set_style_radius(g_schedule_card, 10, 0);
    lv_obj_set_style_pad_all(g_schedule_card, 0, 0);
    lv_obj_clear_flag(g_schedule_card, LV_OBJ_FLAG_SCROLLABLE);

    refresh_schedule_table_display();
}

/**
 * @brief 手动运行程序按钮回调
 */
static void btn_manual_program_cb(lv_event_t *e)
{
    (void)e;

    /* 获取程序数量 */
    int program_count = ui_program_get_count();

    if (program_count == 0) {
        /* 显示告警弹窗 */
        show_warning_dialog("告警提示", "无法启动，当前灌溉程序数量为0，请先\n添加程序！");
    }
    else {
        /* 显示程序选择对话框 */
        show_program_selection_dialog();
    }
}

/**
 * @brief 手动进行轮灌按钮回调
 */
static void btn_manual_irrigation_cb(lv_event_t *e)
{
    (void)e;
    show_manual_irrigation_dialog();
}

/**
 * @brief 禁用自动化按钮回调
 */
static void btn_disable_auto_cb(lv_event_t *e)
{
    (void)e;

    if (g_auto_mode_enabled) {
        /* 当前是启用状态，点击后要禁用 - 直接禁用 */
        disable_auto_mode();
    }
    else {
        /* 当前是禁用状态，点击后要启用 - 显示确认对话框 */
        show_enable_auto_dialog();
    }
}

/**
 * @brief 启用自动模式
 */
static void enable_auto_mode(void)
{
    if (g_auto_mode_set_cb && !g_auto_mode_set_cb(true)) {
        show_warning_dialog("告警提示", "自动模式启用失败，请稍后重试！");
        return;
    }

    g_auto_mode_enabled = true;

    /* 圆环变为蓝色 */
    lv_obj_set_style_arc_color(g_arc, COLOR_PRIMARY, LV_PART_INDICATOR);

    /* 刻度线变为蓝色 */
    for (int i = 0; i < g_tick_count; i++) {
        lv_obj_set_style_line_color(g_tick_lines[i], COLOR_PRIMARY, 0);
    }

    /* 文字变为蓝色 "自动模式" */
    lv_label_set_text(g_mode_label, "自动模式");
    lv_obj_set_style_text_color(g_mode_label, COLOR_PRIMARY, 0);

    /* 按钮变为红色边框，白色背景 */
    lv_obj_set_style_border_color(g_btn_toggle, COLOR_ERROR, 0);
    lv_obj_set_style_bg_color(g_btn_toggle, lv_color_white(), 0);
    lv_label_set_text(g_btn_label, "禁用自动化");
    lv_obj_set_style_text_color(g_btn_label, COLOR_ERROR, 0);
    lv_obj_center(g_btn_label);
}

/**
 * @brief 禁用自动模式
 */
static void disable_auto_mode(void)
{
    if (g_auto_mode_set_cb && !g_auto_mode_set_cb(false)) {
        show_warning_dialog("告警提示", "自动模式禁用失败，请稍后重试！");
        return;
    }

    g_auto_mode_enabled = false;

    /* 圆环变为红色 */
    lv_obj_set_style_arc_color(g_arc, COLOR_ERROR, LV_PART_INDICATOR);

    /* 刻度线变为红色 */
    for (int i = 0; i < g_tick_count; i++) {
        lv_obj_set_style_line_color(g_tick_lines[i], COLOR_ERROR, 0);
    }

    /* 文字变为红色 "自动模式\n已关闭" */
    lv_label_set_text(g_mode_label, "自动模式\n已关闭");
    lv_obj_set_style_text_color(g_mode_label, COLOR_ERROR, 0);

    /* 按钮变为绿色边框，白色背景 */
    lv_obj_set_style_border_color(g_btn_toggle, COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_color(g_btn_toggle, lv_color_white(), 0);
    lv_label_set_text(g_btn_label, "启用自动化");
    lv_obj_set_style_text_color(g_btn_label, COLOR_SUCCESS, 0);
    lv_obj_center(g_btn_label);
}

/**
 * @brief 显示启用自动化确认对话框
 */
static void show_enable_auto_dialog(void)
{
    close_home_dialog();

    /* 创建外层蓝色背景（直角） */
    g_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dialog, 630, 390);
    lv_obj_center(g_dialog);
    lv_obj_set_style_bg_color(g_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_dialog, 0, 0);
    lv_obj_set_style_radius(g_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(g_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_dialog);
    lv_obj_set_size(content, 620, 380);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);  /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "启用全局自动化");
    lv_obj_set_style_text_font(title, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, "是否启用全局自动化");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 取消按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 140, 50);
    lv_obj_set_pos(btn_cancel, 180, 300);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);  /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, dialog_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消");
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_center(label_cancel);

    /* 确认按钮（蓝色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 140, 50);
    lv_obj_set_pos(btn_confirm, 340, 300);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);  /* 蓝色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, dialog_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认");
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 对话框确认按钮回调
 */
static void dialog_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* 启用自动模式 */
    enable_auto_mode();

        close_home_dialog();
}

/**
 * @brief 对话框取消按钮回调
 */
static void dialog_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* 关闭对话框，不做任何改变 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 对话框确定按钮回调（用于告警弹窗）
 */
static void dialog_ok_cb(lv_event_t *e)
{
    (void)e;

        close_home_dialog();
}

/**
 * @brief 显示告警对话框
 * @param title 标题文字
 * @param message 消息文字
 */
static void show_warning_dialog(const char *title, const char *message)
{
    close_home_dialog();

    /* 创建外层蓝色背景（直角） */
    g_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dialog, 630, 390);
    lv_obj_center(g_dialog);
    lv_obj_set_style_bg_color(g_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_dialog, 0, 0);
    lv_obj_set_style_radius(g_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(g_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_dialog);
    lv_obj_set_size(content, 620, 380);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);  /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题（红色） */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, COLOR_ERROR, 0);  /* 红色 */
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 确定按钮（蓝色） */
    lv_obj_t *btn_ok = lv_btn_create(content);
    lv_obj_set_size(btn_ok, 160, 50);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(btn_ok, COLOR_PRIMARY, 0);  /* 蓝色 */
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 25, 0);
    lv_obj_add_event_cb(btn_ok, dialog_ok_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_ok = lv_label_create(btn_ok);
    lv_label_set_text(label_ok, "确定");
    lv_obj_set_style_text_font(label_ok, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_ok, lv_color_white(), 0);
    lv_obj_center(label_ok);
}

/**
 * @brief 轮灌确认启动按钮回调
 */
static void irrigation_confirm_cb(lv_event_t *e)
{
    (void)e;

    ui_manual_irrigation_request_t req = {0};
    if (g_manual_input_pre_water) {
        req.pre_water = atoi(lv_textarea_get_text(g_manual_input_pre_water));
    }
    if (g_manual_input_post_water) {
        req.post_water = atoi(lv_textarea_get_text(g_manual_input_post_water));
    }
    if (g_manual_input_duration) {
        req.total_duration = atoi(lv_textarea_get_text(g_manual_input_duration));
    }
    if (g_manual_dropdown_formula) {
        lv_dropdown_get_selected_str(g_manual_dropdown_formula, req.formula, sizeof(req.formula));
    } else {
        snprintf(req.formula, sizeof(req.formula), "无");
    }

    if (g_manual_irrigation_start_cb && !g_manual_irrigation_start_cb(&req)) {
        show_warning_dialog("告警提示", "手动轮灌启动失败，请检查配置后重试！");
        return;
    }

    refresh_runtime_status_display();

    close_home_dialog();
}

/**
 * @brief 轮灌取消启动按钮回调
 */
static void irrigation_cancel_cb(lv_event_t *e)
{
    (void)e;

    close_home_dialog();
}

/**
 * @brief 程序启动确认按钮回调
 */
static void program_start_confirm_cb(lv_event_t *e)
{
    (void)e;

    int selected_index = -1;
    for (int i = 0; i < g_program_checkbox_count; i++) {
        if (lv_obj_has_state(g_program_checkboxes[i], LV_STATE_CHECKED)) {
            selected_index = i;
            break;
        }
    }

    if (selected_index < 0) {
        show_warning_dialog("告警提示", "请先选择一个程序！");
        return;
    }

    if (g_program_start_cb && !g_program_start_cb(selected_index)) {
        show_warning_dialog("告警提示", "程序启动失败，请稍后重试！");
        return;
    }

    refresh_runtime_status_display();

    close_home_dialog();
}

/**
 * @brief 程序启动取消按钮回调
 */
static void program_start_cancel_cb(lv_event_t *e)
{
    (void)e;

    close_home_dialog();
}

/**
 * @brief 显示程序选择对话框
 */
static void show_program_selection_dialog(void)
{
    close_home_dialog();

    /* 创建外层蓝色背景（直角） */
    g_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dialog, 1050, 650);
    lv_obj_center(g_dialog);
    lv_obj_set_style_bg_color(g_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_dialog, 0, 0);
    lv_obj_set_style_radius(g_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(g_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_dialog);
    lv_obj_set_size(content, 1040, 640);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);  /* 圆角 */
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题（黑色粗体） */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, "今日剩余灌溉计划(启动条件：定时)");
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_pos(title_label, 0, 0);

    /* 表格区域 */
    lv_obj_t *table_bg = lv_obj_create(content);
    lv_obj_set_size(table_bg, 1000, 480);
    lv_obj_set_pos(table_bg, 0, 50);
    lv_obj_set_style_bg_color(table_bg, lv_color_white(), 0);
    lv_obj_set_style_border_width(table_bg, 0, 0);
    lv_obj_set_style_pad_all(table_bg, 0, 0);
    lv_obj_set_style_radius(table_bg, 0, 0);
    lv_obj_clear_flag(table_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头背景 */
    lv_obj_t *header_bg = lv_obj_create(table_bg);
    lv_obj_set_size(header_bg, 1000, 50);
    lv_obj_set_pos(header_bg, 0, 0);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);

    /* 表头列 */
    const char *headers[] = {"选定", "序号", "程序名称", "启动时段", "合计时长", "关联配方"};
    int header_widths[] = {100, 90, 240, 260, 140, 170};
    int x_pos = 0;

    for (int i = 0; i < 6; i++) {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_pos(header_label, x_pos + 20, 17);
        x_pos += header_widths[i];
    }

    /* 获取程序数量并动态添加数据行 */
    int program_count = ui_program_get_count();
    g_program_checkbox_count = 0;

    for (int i = 0; i < program_count && i < 8; i++) {  /* 最多显示8行 */
        int row_y = 60 + i * 50;  /* 表头50px，每行50px */
        x_pos = 0;

        /* 选定 - 复选框 */
        lv_obj_t *checkbox = lv_checkbox_create(table_bg);
        lv_checkbox_set_text(checkbox, "");
        lv_obj_set_pos(checkbox, x_pos + 40, row_y + 10);
        g_program_checkboxes[g_program_checkbox_count++] = checkbox;
        x_pos += header_widths[0];

        /* 序号 */
        lv_obj_t *label_no = lv_label_create(table_bg);
        lv_label_set_text_fmt(label_no, "%d", i + 1);
        lv_obj_set_pos(label_no, x_pos + 50, row_y + 15);
        lv_obj_set_style_text_font(label_no, &my_font_cn_16, 0);
        x_pos += header_widths[1];

        /* 程序名称 */
        lv_obj_t *label_name = lv_label_create(table_bg);
        const char *name = ui_program_get_name(i);
        lv_label_set_text(label_name, name ? name : "");
        lv_obj_set_pos(label_name, x_pos + 20, row_y + 15);
        lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);
        x_pos += header_widths[2];

        /* 启动时段 */
        lv_obj_t *label_period = lv_label_create(table_bg);
        char period_text[96];
        ui_program_get_period_text(i, period_text, sizeof(period_text));
        lv_label_set_text(label_period, period_text);
        lv_obj_set_style_text_font(label_period, &my_font_cn_16, 0);
        lv_obj_set_width(label_period, header_widths[3] - 20);
        lv_label_set_long_mode(label_period, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_pos(label_period, x_pos + 10, row_y + 15);
        x_pos += header_widths[3];

        /* 合计时长 */
        lv_obj_t *label_duration = lv_label_create(table_bg);
        int duration = ui_program_get_duration(i);
        if (duration > 0) {
            lv_label_set_text_fmt(label_duration, "%d分", duration);
        } else {
            lv_label_set_text(label_duration, "--");
        }
        lv_obj_set_pos(label_duration, x_pos + 20, row_y + 15);
        lv_obj_set_style_text_font(label_duration, &my_font_cn_16, 0);
        x_pos += header_widths[4];

        /* 关联配方 */
        lv_obj_t *label_formula = lv_label_create(table_bg);
        const char *formula = ui_program_get_formula(i);
        lv_label_set_text(label_formula, formula ? formula : "");
        lv_obj_set_pos(label_formula, x_pos + 20, row_y + 15);
        lv_obj_set_style_text_font(label_formula, &my_font_cn_16, 0);
    }

    /* 取消启动按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 160, 50);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 320, -10);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);  /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, program_start_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消启动");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认启动按钮（蓝色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 160, 50);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_RIGHT, -320, -10);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);  /* 蓝色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, program_start_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认启动");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 显示手动轮灌设置对话框
 */
static void show_manual_irrigation_dialog(void)
{
    close_home_dialog();

    /* 创建外层蓝色边框 */
    g_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dialog, 1000, 630);
    lv_obj_center(g_dialog);
    lv_obj_set_style_bg_color(g_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_dialog, 3, 0);
    lv_obj_set_style_border_color(g_dialog, COLOR_PRIMARY, 0);  /* 蓝色边框 */
    lv_obj_set_style_radius(g_dialog, 0, 0);
    lv_obj_set_style_pad_all(g_dialog, 20, 0);
    lv_obj_clear_flag(g_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 肥前清水 */
    lv_obj_t *label_pre_water = lv_label_create(g_dialog);
    lv_label_set_text(label_pre_water, "肥前清水(分):");
    lv_obj_set_pos(label_pre_water, 20, 20);
    lv_obj_set_style_text_font(label_pre_water, &my_font_cn_16, 0);

    lv_obj_t *input_pre_water = lv_textarea_create(g_dialog);
    lv_obj_set_size(input_pre_water, 200, 40);
    lv_obj_set_pos(input_pre_water, 180, 15);
    lv_textarea_set_one_line(input_pre_water, true);
    lv_textarea_set_text(input_pre_water, "5");
    lv_obj_set_style_text_font(input_pre_water, &my_font_cn_16, 0);
    g_manual_input_pre_water = input_pre_water;
    lv_obj_add_event_cb(input_pre_water, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 施肥配方 */
    lv_obj_t *label_formula = lv_label_create(g_dialog);
    lv_label_set_text(label_formula, "施肥配方:");
    lv_obj_set_pos(label_formula, 450, 20);
    lv_obj_set_style_text_font(label_formula, &my_font_cn_16, 0);

    lv_obj_t *dropdown_formula = lv_dropdown_create(g_dialog);
    lv_obj_set_size(dropdown_formula, 200, 40);
    lv_obj_set_pos(dropdown_formula, 580, 15);
    lv_dropdown_set_options(dropdown_formula, "无");
    lv_obj_set_style_text_font(dropdown_formula, &my_font_cn_16, 0);
    g_manual_dropdown_formula = dropdown_formula;
    lv_obj_add_event_cb(dropdown_formula, ui_dropdown_list_font_cb, LV_EVENT_READY, NULL);

    /* 选择灌区按钮 */
    lv_obj_t *btn_select_zone = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_select_zone, 120, 40);
    lv_obj_set_pos(btn_select_zone, 850, 15);
    lv_obj_set_style_bg_color(btn_select_zone, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_select_zone, 20, 0);

    lv_obj_t *label_select_zone = lv_label_create(btn_select_zone);
    lv_label_set_text(label_select_zone, "选择灌区");
    lv_obj_set_style_text_color(label_select_zone, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_select_zone, &my_font_cn_16, 0);
    lv_obj_center(label_select_zone);

    /* 肥后清水 */
    lv_obj_t *label_post_water = lv_label_create(g_dialog);
    lv_label_set_text(label_post_water, "肥后清水(分):");
    lv_obj_set_pos(label_post_water, 20, 70);
    lv_obj_set_style_text_font(label_post_water, &my_font_cn_16, 0);

    lv_obj_t *input_post_water = lv_textarea_create(g_dialog);
    lv_obj_set_size(input_post_water, 200, 40);
    lv_obj_set_pos(input_post_water, 180, 65);
    lv_textarea_set_one_line(input_post_water, true);
    lv_textarea_set_text(input_post_water, "5");
    lv_obj_set_style_text_font(input_post_water, &my_font_cn_16, 0);
    lv_obj_add_event_cb(input_post_water, textarea_click_cb, LV_EVENT_CLICKED, NULL);
    g_manual_input_post_water = input_post_water;

    /* 上移、下移按钮 */
    lv_obj_t *btn_up = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_up, 100, 40);
    lv_obj_set_pos(btn_up, 580, 65);
    lv_obj_set_style_bg_color(btn_up, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_up, 20, 0);

    lv_obj_t *label_up = lv_label_create(btn_up);
    lv_label_set_text(label_up, "上移");
    lv_obj_set_style_text_color(label_up, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_up, &my_font_cn_16, 0);
    lv_obj_center(label_up);

    lv_obj_t *btn_down = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_down, 100, 40);
    lv_obj_set_pos(btn_down, 700, 65);
    lv_obj_set_style_bg_color(btn_down, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_down, 20, 0);

    lv_obj_t *label_down = lv_label_create(btn_down);
    lv_label_set_text(label_down, "下移");
    lv_obj_set_style_text_color(label_down, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_down, &my_font_cn_16, 0);
    lv_obj_center(label_down);

    /* 表格区域 */
    lv_obj_t *table_bg = lv_obj_create(g_dialog);
    lv_obj_set_size(table_bg, 960, 370);
    lv_obj_set_pos(table_bg, 20, 120);
    lv_obj_set_style_bg_color(table_bg, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_bg, 0, 0);
    lv_obj_set_style_radius(table_bg, 0, 0);

    /* 表头 */
    const char *headers[] = {"灌溉顺序", "名称", "类型", "运行时长(分)", "操作"};
    int header_widths[] = {180, 240, 180, 240, 120};
    int header_x = 20;

    for (int i = 0; i < 5; i++) {
        lv_obj_t *header_label = lv_label_create(table_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_pos(header_label, header_x, 10);
        header_x += header_widths[i];
    }

    /* 底部运行时长 */
    lv_obj_t *label_duration = lv_label_create(g_dialog);
    lv_label_set_text(label_duration, "运行时长(分):");
    lv_obj_set_pos(label_duration, 20, 520);
    lv_obj_set_style_text_font(label_duration, &my_font_cn_16, 0);

    lv_obj_t *input_duration = lv_textarea_create(g_dialog);
    lv_obj_set_size(input_duration, 150, 40);
    lv_obj_set_pos(input_duration, 180, 515);
    lv_textarea_set_one_line(input_duration, true);
    lv_textarea_set_text(input_duration, "0");
    lv_obj_set_style_text_font(input_duration, &my_font_cn_16, 0);
    lv_obj_add_event_cb(input_duration, textarea_click_cb, LV_EVENT_CLICKED, NULL);
    g_manual_input_duration = input_duration;

    /* 统一设置按钮 */
    lv_obj_t *btn_set_all = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_set_all, 140, 45);
    lv_obj_set_pos(btn_set_all, 450, 515);
    lv_obj_set_style_bg_color(btn_set_all, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_set_all, 22, 0);
    lv_obj_add_event_cb(btn_set_all, btn_uniform_set_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_set_all = lv_label_create(btn_set_all);
    lv_label_set_text(label_set_all, "统一设置");
    lv_obj_set_style_text_color(label_set_all, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_set_all, &my_font_cn_16, 0);
    lv_obj_center(label_set_all);

    /* 取消启动按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_cancel, 140, 45);
    lv_obj_set_pos(btn_cancel, 620, 515);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(btn_cancel, 22, 0);
    lv_obj_add_event_cb(btn_cancel, irrigation_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消启动");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认启动按钮 */
    lv_obj_t *btn_confirm = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_confirm, 140, 45);
    lv_obj_set_pos(btn_confirm, 790, 515);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_confirm, 22, 0);
    lv_obj_add_event_cb(btn_confirm, irrigation_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认启动");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 输入框点击回调 - 显示数字键盘
 */
static void textarea_click_cb(lv_event_t *e)
{
    lv_obj_t *textarea = lv_event_get_target(e);
    ui_main_t *ui_main = ui_get_main();

    if (textarea && ui_main && ui_main->screen) {
        ui_numpad_show(textarea, ui_main->screen);
    }
}

/**
 * @brief 关闭首页弹窗（如果存在）
 */
void ui_home_close_dialog(void)
{
    close_home_dialog();
}

/**
 * @brief 导航离开首页时，置空所有静态对象指针，防止异步回调访问已释放内存
 */
void ui_home_invalidate_objects(void)
{
    close_home_dialog();
    g_arc = NULL;
    g_mode_label = NULL;
    g_btn_toggle = NULL;
    g_btn_label = NULL;
    g_status_text_label = NULL;
    if (g_runtime_status_timer) {
        lv_timer_del(g_runtime_status_timer);
        g_runtime_status_timer = NULL;
    }
    g_dev_status_label = NULL;
    g_zigbee_status_label = NULL;
    g_field_dropdown = NULL;
    g_schedule_card = NULL;
    memset(&s_home_control_cache, 0, sizeof(s_home_control_cache));
    memset(s_home_pipe_cache, 0, sizeof(s_home_pipe_cache));
    memset(s_home_tank_cache, 0, sizeof(s_home_tank_cache));
    memset(s_field_cache, 0, sizeof(s_field_cache));
    for (int i = 0; i < 6; i++) {
        g_field_val_labels[i] = NULL;
        g_field_name_labels[i] = NULL;
    }
}

/**
 * @brief 统一设置按钮回调
 */
static void btn_uniform_set_cb(lv_event_t *e)
{
    (void)e;
    show_uniform_set_confirm_dialog();
}

/**
 * @brief 显示统一设置确认对话框
 */
static void show_uniform_set_confirm_dialog(void)
{
    /* 注意：不删除原对话框，新对话框显示在上层 */

    /* 创建外层蓝色背景（直角） */
    lv_obj_t *confirm_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(confirm_dialog, 630, 390);
    lv_obj_center(confirm_dialog);
    lv_obj_set_style_bg_color(confirm_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(confirm_dialog, 0, 0);
    lv_obj_set_style_radius(confirm_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(confirm_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(confirm_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(confirm_dialog);
    lv_obj_set_size(content, 620, 380);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);  /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "统一修改");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, "是否统一修改运行时长");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 取消修改按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 140, 50);
    lv_obj_set_pos(btn_cancel, 180, 300);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);  /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, uniform_set_cancel_cb, LV_EVENT_CLICKED, confirm_dialog);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消修改");
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_center(label_cancel);

    /* 确认修改按钮（蓝色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 140, 50);
    lv_obj_set_pos(btn_confirm, 340, 300);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);  /* 蓝色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, uniform_set_confirm_cb, LV_EVENT_CLICKED, confirm_dialog);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认修改");
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 统一设置确认按钮回调
 */
static void uniform_set_confirm_cb(lv_event_t *e)
{
    lv_obj_t *confirm_dialog = lv_event_get_user_data(e);

    /* TODO: 执行统一设置运行时长的逻辑 */

    /* 关闭确认对话框 */
    if (confirm_dialog != NULL) {
        lv_obj_del(confirm_dialog);
    }
}

/**
 * @brief 统一设置取消按钮回调
 */
static void uniform_set_cancel_cb(lv_event_t *e)
{
    lv_obj_t *confirm_dialog = lv_event_get_user_data(e);

    /* 关闭确认对话框 */
    if (confirm_dialog != NULL) {
        lv_obj_del(confirm_dialog);
    }
}

/* ---- Zigbee 数据更新接口 ---- */

void ui_home_update_field(int field_id, uint8_t registered_mask,
    float n, float p, float k, float temp, float humi, float light)
{
    if (field_id < 1 || field_id > 6) return;
    int idx = field_id - 1;

    /* 缓存数据 */
    s_field_cache[idx].n = n;
    s_field_cache[idx].p = p;
    s_field_cache[idx].k = k;
    s_field_cache[idx].temp = temp;
    s_field_cache[idx].humi = humi;
    s_field_cache[idx].light = light;
    s_field_cache[idx].registered_mask = registered_mask;
    s_field_cache[idx].valid = true;

    /* 只更新当前选中的灌区映射田地 */
    if (idx != g_selected_field) return;
    refresh_selected_field_display();
}

void ui_home_update_pipe(int pipe_id,
    bool valve_bound, bool valve_on,
    bool flow_bound, float flow,
    bool pressure_bound, float pressure)
{
    if (pipe_id < 0 || pipe_id >= 7) {
        return;
    }

    s_home_pipe_cache[pipe_id].valid = true;
    s_home_pipe_cache[pipe_id].valve_bound = valve_bound;
    s_home_pipe_cache[pipe_id].valve_on = valve_on;
    s_home_pipe_cache[pipe_id].flow_bound = flow_bound;
    s_home_pipe_cache[pipe_id].flow = flow;
    s_home_pipe_cache[pipe_id].pressure_bound = pressure_bound;
    s_home_pipe_cache[pipe_id].pressure = pressure;
    refresh_home_device_status_summary();
}

void ui_home_update_tank(int tank_id, bool switch_on, bool level_bound, float level)
{
    int idx = tank_id - 1;

    if (idx < 0 || idx >= 3) {
        return;
    }

    s_home_tank_cache[idx].valid = true;
    s_home_tank_cache[idx].switch_on = switch_on;
    s_home_tank_cache[idx].level_bound = level_bound;
    s_home_tank_cache[idx].level = level;
    refresh_home_device_status_summary();
}

void ui_home_update_mixer(bool on)
{
    s_home_control_cache.valid = true;
    s_home_control_cache.mixer_on = on;
    refresh_home_device_status_summary();
}

void ui_home_update_control(bool water_pump, bool fert_pump,
    bool fert_valve, bool water_valve, bool mixer)
{
    s_home_control_cache.valid = true;
    s_home_control_cache.water_pump_on = water_pump;
    s_home_control_cache.fert_pump_on = fert_pump;
    s_home_control_cache.fert_valve_on = fert_valve;
    s_home_control_cache.water_valve_on = water_valve;
    s_home_control_cache.mixer_on = mixer;
    refresh_home_device_status_summary();
}

void ui_home_update_zigbee_status(bool online, int frame_count)
{
    if (!g_zigbee_status_label) return;

    if (online) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Zigbee 在线 (%d帧)", frame_count);
        lv_label_set_text(g_zigbee_status_label, buf);
        lv_obj_set_style_text_color(g_zigbee_status_label, lv_color_hex(0x4CAF50), 0);
    } else {
        lv_label_set_text(g_zigbee_status_label, "Zigbee 离线");
        lv_obj_set_style_text_color(g_zigbee_status_label, lv_color_hex(0x999999), 0);
    }
}