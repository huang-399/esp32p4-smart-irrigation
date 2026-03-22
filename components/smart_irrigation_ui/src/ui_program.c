/**
 * @file ui_program.c
 * @brief 智慧种植园监控系统 - 程序管理界面
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include "ui_keyboard.h"
#include "irrigation_scheduler.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*********************
 *      TYPEDEFS
 *********************/
/* 灌溉时段数据结构 */
typedef struct
{
    bool enabled;  /* 是否开启 */
    char time[16]; /* 时间 */
} irrigation_period_t;

/* 程序数据结构 */
typedef struct
{
    char name[32];       /* 程序名称 */
    bool auto_enabled;   /* 启用自动 */
    char start_date[16]; /* 开始日期 */
    char end_date[16];   /* 结束日期 */
    char condition[16];  /* 启动条件 */
    char formula[32];    /* 关联配方 */
    int pre_water;       /* 肥前清水(min) */
    int post_water;      /* 肥后清水(min) */
    char mode[32];       /* 灌溉模式 */
    char next_start[32]; /* 下次启动时段 */
    int total_duration;  /* 合计时长(分) */
    int period_count;    /* 灌溉时段数量 */
    irrigation_period_t periods[10]; /* 灌溉时段数据 */
    bool selected_valves[10]; /* 选中的阀门（最多10个） */
    bool selected_zones[10];  /* 选中的灌区（最多10个） */
} program_data_t;

/* 配方数据结构 */
typedef struct
{
    char name[32];       /* 配方名称 */
    int method;          /* 配肥方式: 0=比例稀释, 1=EC调配, 2=固定流速 */
    int dilution;        /* 稀释倍数 */
    float ec;            /* EC目标值 */
    float ph;            /* PH目标值 */
    float valve_opening; /* 阀门固定开度(%) */
    int stir_time;       /* 搅拌时间(S) */
    int channel_count;   /* 通道数量 */
} formula_data_t;

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void clear_program_form_widget_refs(void);
static void create_tab_buttons(lv_obj_t *parent);
static void create_table_area(lv_obj_t *parent);
static void create_formula_table_area(lv_obj_t *parent);
static void create_pagination(lv_obj_t *parent);
static void tab_program_cb(lv_event_t *e);
static void tab_formula_cb(lv_event_t *e);
static void btn_add_program_cb(lv_event_t *e);
static void btn_add_formula_cb(lv_event_t *e);
static void create_add_program_dialog(void);
static void create_add_formula_dialog(void);
static void btn_cancel_add_cb(lv_event_t *e);
static void btn_confirm_add_cb(lv_event_t *e);
static void btn_cancel_add_formula_cb(lv_event_t *e);
static void btn_confirm_add_formula_cb(lv_event_t *e);
static void btn_calendar_start_cb(lv_event_t *e);
static void btn_calendar_end_cb(lv_event_t *e);
static void calendar_event_cb(lv_event_t *e);
static void btn_calendar_close_cb(lv_event_t *e);
static void btn_year_prev_cb(lv_event_t *e);
static void btn_year_next_cb(lv_event_t *e);
static void btn_month_prev_cb(lv_event_t *e);
static void btn_month_next_cb(lv_event_t *e);
static void menu_irrigation_date_cb(lv_event_t *e);
static void menu_irrigation_period_cb(lv_event_t *e);
static void menu_irrigation_zone_cb(lv_event_t *e);
static void create_irrigation_date_panel(lv_obj_t *parent);
static void create_irrigation_period_panel(lv_obj_t *parent);
static void create_irrigation_zone_panel(lv_obj_t *parent);
static void textarea_click_cb(lv_event_t *e);
static void name_input_click_cb(lv_event_t *e);
static void btn_select_zone_cb(lv_event_t *e);
static void create_zone_selection_dialog(void);
static void btn_zone_cancel_cb(lv_event_t *e);
static void btn_zone_confirm_cb(lv_event_t *e);
static void tab_valve_cb(lv_event_t *e);
static void tab_zone_cb(lv_event_t *e);
static void btn_uniform_set_cb(lv_event_t *e);
static void create_uniform_set_dialog(void);
static void btn_uniform_cancel_cb(lv_event_t *e);
static void btn_uniform_confirm_cb(lv_event_t *e);
static void period_checkbox_cb(lv_event_t *e);
static void refresh_program_table(void);
static void refresh_formula_table(void);
static void refresh_irrigation_zone_table(void);
static void btn_edit_program_cb(lv_event_t *e);
static void btn_delete_program_cb(lv_event_t *e);
static void btn_edit_formula_cb(lv_event_t *e);
static void btn_delete_formula_cb(lv_event_t *e);
static void save_current_form_data(void);
static void reset_temp_form_data(void);
static void show_warning_dialog(const char *message);
static void btn_warning_close_cb(lv_event_t *e);
static void dropdown_method_change_cb(lv_event_t *e);
static void show_delete_formula_confirm_dialog(int index);
static void btn_delete_formula_cancel_cb(lv_event_t *e);
static void btn_delete_formula_confirm_cb(lv_event_t *e);
static void show_delete_program_confirm_dialog(int index);
static void btn_delete_program_cancel_cb(lv_event_t *e);
static void btn_delete_program_confirm_cb(lv_event_t *e);
static void zone_selection_checkbox_cb(lv_event_t *e);
static void btn_delete_selected_target_cb(lv_event_t *e);
static void show_delete_selected_target_confirm_dialog(bool is_zone, int index);
static void btn_delete_selected_target_cancel_cb(lv_event_t *e);
static void btn_delete_selected_target_confirm_cb(lv_event_t *e);
static void ensure_program_selection_cache_loaded(void);
static void find_zone_name_for_valve(uint16_t valve_id, char *buf, int buf_size);

/*********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t *g_tab_program = NULL;                   /* 程序管理标签 */
static lv_obj_t *g_tab_formula = NULL;                   /* 配方管理标签 */
static bool g_add_dialog = false;                        /* 是否处于添加程序页面 */
static lv_obj_t *g_add_formula_dialog = NULL;            /* 添加配方卡片引用（内容区对象） */
static lv_obj_t *g_parent_container = NULL;              /* 父容器引用 */
static lv_obj_t *g_table_area = NULL;                    /* 程序管理表格区域 */
static lv_obj_t *g_formula_table_area = NULL;            /* 配方管理表格区域 */
static lv_obj_t *g_btn_add_program = NULL;               /* 添加程序按钮 */
static lv_obj_t *g_btn_add_formula = NULL;               /* 添加配方按钮 */
static lv_obj_t *g_pagination = NULL;                    /* 分页控件 */
static lv_obj_t *g_input_start_date = NULL;              /* 开始日期输入框 */
static lv_obj_t *g_input_end_date = NULL;                /* 结束日期输入框 */
static lv_obj_t *g_calendar_popup = NULL;                /* 日历弹窗 */
static lv_obj_t *g_current_date_input = NULL;            /* 当前选择日期的输入框 */
static lv_obj_t *g_calendar_widget = NULL;               /* 日历控件引用 */
static lv_obj_t *g_year_month_label = NULL;              /* 年月显示标签 */
static lv_obj_t *g_form_area = NULL;                     /* 表单区域引用 */
static lv_obj_t *g_menu_buttons[3] = {NULL, NULL, NULL}; /* 三个菜单按钮的引用 */
static lv_obj_t *g_zone_dialog = NULL;                   /* 灌区选择对话框 */
static lv_obj_t *g_zone_tab_valve = NULL;                /* 阀门标签 */
static lv_obj_t *g_zone_tab_zone = NULL;                 /* 灌区标签 */
static lv_obj_t *g_zone_content = NULL;                  /* 灌区对话框内容区域 */
static lv_obj_t *g_uniform_dialog = NULL;                /* 统一设置对话框 */
static lv_obj_t *g_table_bg = NULL;                      /* 表格背景引用 */
static lv_obj_t *g_zone_table_bg = NULL;                 /* 灌区选择表格背景引用 */
static lv_obj_t *g_warning_dialog = NULL;                /* 警告对话框 */
static lv_obj_t *g_delete_confirm_dialog = NULL;         /* 删除确认对话框 */
static int g_delete_formula_index = -1;                  /* 待删除的配方索引 */
static int g_delete_program_index = -1;                  /* 待删除的程序索引 */
static bool g_delete_target_is_zone = false;             /* 待删除目标类型 */
static int g_delete_target_index = -1;                   /* 待删除目标索引 */
static bool g_is_editing_program = false;                /* 是否处于编辑程序模式 */
static int g_editing_program_index = -1;                 /* 正在编辑的程序索引 */
static program_data_t g_temp_program;                    /* 临时程序数据（用于编辑） */
static bool g_is_editing_formula = false;                /* 是否处于编辑配方模式 */
static int g_editing_formula_index = -1;                 /* 正在编辑的配方索引 */
static formula_data_t g_temp_formula_data;               /* 临时配方数据（用于编辑） */

/* 程序数据存储 */
#define MAX_PROGRAMS 15
static program_data_t g_programs[MAX_PROGRAMS];
static int g_program_count = 0;

/* 配方数据存储 */
#define MAX_FORMULAS 15
static formula_data_t g_formulas[MAX_FORMULAS];
static int g_formula_count = 0;
static lv_obj_t *g_formula_table_bg = NULL; /* 配方表格背景引用 */

/* 当前编辑的表单控件引用 */
static lv_obj_t *g_input_program_name = NULL;
static lv_obj_t *g_checkbox_auto = NULL;

/* 临时保存当前正在编辑的程序数据 */
static program_data_t g_current_editing_program = {0};

/* 临时保存灌溉时段数据 */
#define MAX_PERIODS 10
static irrigation_period_t g_temp_periods[MAX_PERIODS] = {0};

/* 临时保存其他表单数据 */
static bool g_temp_auto_enabled = false;
static char g_temp_start_date[16] = "2026-01-01";
static char g_temp_end_date[16] = "2026-01-01";
static bool g_temp_selected_valves[10] = {false}; /* 临时保存选中的阀门 */
static bool g_temp_selected_zones[10] = {false};  /* 临时保存选中的灌区 */
static int g_temp_pre_water = 0;                  /* 临时保存肥前清水 */
static int g_temp_post_water = 0;                 /* 临时保存肥后清水 */
static char g_temp_formula[32] = "无";            /* 临时保存关联配方 */
static char g_temp_mode[32] = "每天执行";         /* 临时保存灌溉模式 */
static char g_temp_condition[16] = "定时";        /* 临时保存启动条件 */

static lv_obj_t *g_zone_valve_checkboxes[10] = {NULL};
static lv_obj_t *g_zone_zone_checkboxes[10] = {NULL};

/* 程序页真实数据查询回调 */
static ui_get_valve_count_cb_t g_program_valve_count_cb = NULL;
static ui_get_valve_list_cb_t  g_program_valve_list_cb = NULL;
static ui_get_zone_count_cb_t  g_program_zone_count_cb = NULL;
static ui_get_zone_list_cb_t   g_program_zone_list_cb = NULL;
static ui_get_zone_detail_cb_t g_program_zone_detail_cb = NULL;

static int g_program_cached_valve_count = 0;
static ui_valve_row_t g_program_cached_valves[10];
static int g_program_cached_zone_count = 0;
static ui_zone_row_t g_program_cached_zones[10];

/* 临时保存当前正在编辑的配方数据（完整数据） */
static formula_data_t g_current_editing_formula = {0};

/* 配方表单输入框引用 */
static lv_obj_t *g_formula_name_input = NULL;
static lv_obj_t *g_formula_method_dropdown = NULL;
static lv_obj_t *g_formula_dilution_input = NULL;
static lv_obj_t *g_formula_ec_input = NULL;
static lv_obj_t *g_formula_ph_input = NULL;
static lv_obj_t *g_formula_valve_input = NULL;
static lv_obj_t *g_formula_stir_input = NULL;

/* 灌溉日期面板控件引用 */
static lv_obj_t *g_input_pre_water = NULL;
static lv_obj_t *g_input_post_water = NULL;
static lv_obj_t *g_dropdown_condition = NULL;
static lv_obj_t *g_dropdown_formula = NULL;
static lv_obj_t *g_dropdown_mode = NULL;

/* ---- Backend persistence bridge for programs & formulas ---- */

static const char *PROG_TAG = "prog_store";

static void copy_program_to_backend(const program_data_t *src, irr_program_t *dst)
{
    if (!src || !dst) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    memcpy(dst->name, src->name, sizeof(dst->name));
    dst->name[sizeof(dst->name) - 1] = '\0';
    dst->auto_enabled = src->auto_enabled;
    memcpy(dst->start_date, src->start_date, sizeof(dst->start_date));
    dst->start_date[sizeof(dst->start_date) - 1] = '\0';
    memcpy(dst->end_date, src->end_date, sizeof(dst->end_date));
    dst->end_date[sizeof(dst->end_date) - 1] = '\0';
    memcpy(dst->condition, src->condition, sizeof(dst->condition));
    dst->condition[sizeof(dst->condition) - 1] = '\0';
    memcpy(dst->formula, src->formula, sizeof(dst->formula));
    dst->formula[sizeof(dst->formula) - 1] = '\0';
    dst->pre_water = src->pre_water;
    dst->post_water = src->post_water;
    memcpy(dst->mode, src->mode, sizeof(dst->mode));
    dst->mode[sizeof(dst->mode) - 1] = '\0';
    memcpy(dst->next_start, src->next_start, sizeof(dst->next_start));
    dst->next_start[sizeof(dst->next_start) - 1] = '\0';
    dst->total_duration = src->total_duration;
    dst->period_count = src->period_count;

    if (dst->period_count > IRR_MAX_PERIODS) {
        dst->period_count = IRR_MAX_PERIODS;
    }

    for (int i = 0; i < dst->period_count; i++) {
        dst->periods[i].enabled = src->periods[i].enabled;
        memcpy(dst->periods[i].time, src->periods[i].time, sizeof(dst->periods[i].time));
        dst->periods[i].time[sizeof(dst->periods[i].time) - 1] = '\0';
    }

    for (int i = 0; i < 10; i++) {
        dst->selected_valves[i] = src->selected_valves[i];
        dst->selected_zones[i] = src->selected_zones[i];
    }
}

static void copy_program_from_backend(const irr_program_t *src, program_data_t *dst)
{
    if (!src || !dst) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    memcpy(dst->name, src->name, sizeof(dst->name));
    dst->name[sizeof(dst->name) - 1] = '\0';
    dst->auto_enabled = src->auto_enabled;
    memcpy(dst->start_date, src->start_date, sizeof(dst->start_date));
    dst->start_date[sizeof(dst->start_date) - 1] = '\0';
    memcpy(dst->end_date, src->end_date, sizeof(dst->end_date));
    dst->end_date[sizeof(dst->end_date) - 1] = '\0';
    memcpy(dst->condition, src->condition, sizeof(dst->condition));
    dst->condition[sizeof(dst->condition) - 1] = '\0';
    memcpy(dst->formula, src->formula, sizeof(dst->formula));
    dst->formula[sizeof(dst->formula) - 1] = '\0';
    dst->pre_water = src->pre_water;
    dst->post_water = src->post_water;
    memcpy(dst->mode, src->mode, sizeof(dst->mode));
    dst->mode[sizeof(dst->mode) - 1] = '\0';
    memcpy(dst->next_start, src->next_start, sizeof(dst->next_start));
    dst->next_start[sizeof(dst->next_start) - 1] = '\0';
    dst->total_duration = src->total_duration;
    dst->period_count = src->period_count;

    if (dst->period_count > MAX_PERIODS) {
        dst->period_count = MAX_PERIODS;
    }

    for (int i = 0; i < dst->period_count; i++) {
        dst->periods[i].enabled = src->periods[i].enabled;
        memcpy(dst->periods[i].time, src->periods[i].time, sizeof(dst->periods[i].time));
        dst->periods[i].time[sizeof(dst->periods[i].time) - 1] = '\0';
    }

    for (int i = 0; i < 10; i++) {
        dst->selected_valves[i] = src->selected_valves[i];
        dst->selected_zones[i] = src->selected_zones[i];
    }
}

static void copy_formula_to_backend(const formula_data_t *src, irr_formula_t *dst)
{
    if (!src || !dst) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    memcpy(dst->name, src->name, sizeof(dst->name));
    dst->name[sizeof(dst->name) - 1] = '\0';
    dst->method = src->method;
    dst->dilution = src->dilution;
    dst->ec = src->ec;
    dst->ph = src->ph;
    dst->valve_opening = src->valve_opening;
    dst->stir_time = src->stir_time;
    dst->channel_count = src->channel_count;
}

static void copy_formula_from_backend(const irr_formula_t *src, formula_data_t *dst)
{
    if (!src || !dst) {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    memcpy(dst->name, src->name, sizeof(dst->name));
    dst->name[sizeof(dst->name) - 1] = '\0';
    dst->method = src->method;
    dst->dilution = src->dilution;
    dst->ec = src->ec;
    dst->ph = src->ph;
    dst->valve_opening = src->valve_opening;
    dst->stir_time = src->stir_time;
    dst->channel_count = src->channel_count;
}

static esp_err_t persist_all_programs_to_backend(void)
{
    if (g_program_count < 0 || g_program_count > IRR_MAX_PROGRAMS) {
        ESP_LOGE(PROG_TAG, "Program count out of range: %d", g_program_count);
        return ESP_ERR_INVALID_ARG;
    }

    irr_program_t backend_programs[IRR_MAX_PROGRAMS] = {0};
    for (int i = 0; i < g_program_count; i++) {
        copy_program_to_backend(&g_programs[i], &backend_programs[i]);
    }

    esp_err_t ret = irrigation_scheduler_replace_programs(backend_programs, g_program_count);
    if (ret != ESP_OK) {
        ESP_LOGE(PROG_TAG, "Persist programs failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static esp_err_t persist_all_formulas_to_backend(void)
{
    if (g_formula_count < 0 || g_formula_count > IRR_MAX_FORMULAS) {
        ESP_LOGE(PROG_TAG, "Formula count out of range: %d", g_formula_count);
        return ESP_ERR_INVALID_ARG;
    }

    irr_formula_t backend_formulas[IRR_MAX_FORMULAS] = {0};
    for (int i = 0; i < g_formula_count; i++) {
        copy_formula_to_backend(&g_formulas[i], &backend_formulas[i]);
    }

    esp_err_t ret = irrigation_scheduler_replace_formulas(backend_formulas, g_formula_count);
    if (ret != ESP_OK) {
        ESP_LOGE(PROG_TAG, "Persist formulas failed: %s", esp_err_to_name(ret));
    }
    return ret;
}

static void nvs_save_program(int index)
{
    (void)index;
    persist_all_programs_to_backend();
}

static void nvs_save_all_programs(void)
{
    persist_all_programs_to_backend();
}

static void nvs_save_formula(int index)
{
    (void)index;
    persist_all_formulas_to_backend();
}

static void nvs_save_all_formulas(void)
{
    persist_all_formulas_to_backend();
}

static void reload_programs_from_backend(void)
{
    g_program_count = irrigation_scheduler_get_program_count();
    if (g_program_count > MAX_PROGRAMS) {
        g_program_count = MAX_PROGRAMS;
    }

    memset(g_programs, 0, sizeof(g_programs));
    for (int i = 0; i < g_program_count; i++) {
        irr_program_t backend_program = {0};
        if (irrigation_scheduler_get_program(i, &backend_program)) {
            copy_program_from_backend(&backend_program, &g_programs[i]);
        }
    }
}

void ui_program_store_init(void)
{
    reload_programs_from_backend();
    ESP_LOGI(PROG_TAG, "Loaded %d programs from backend store", g_program_count);

    g_formula_count = irrigation_scheduler_get_formula_count();
    if (g_formula_count > MAX_FORMULAS) {
        g_formula_count = MAX_FORMULAS;
    }

    memset(g_formulas, 0, sizeof(g_formulas));
    for (int i = 0; i < g_formula_count; i++) {
        irr_formula_t backend_formula = {0};
        if (irrigation_scheduler_get_formula(i, &backend_formula)) {
            copy_formula_from_backend(&backend_formula, &g_formulas[i]);
        }
    }
    ESP_LOGI(PROG_TAG, "Loaded %d formulas from backend store", g_formula_count);
}

/* ---- Date helpers (same as ui_alarm.c) ---- */

static void clear_program_form_widget_refs(void)
{
    g_checkbox_auto = NULL;
    g_input_start_date = NULL;
    g_input_end_date = NULL;
    g_input_pre_water = NULL;
    g_input_post_water = NULL;
    g_dropdown_condition = NULL;
    g_dropdown_formula = NULL;
    g_dropdown_mode = NULL;
}

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

static int program_get_display_duration(const program_data_t *prog)
{
    if (!prog) {
        return 0;
    }

    if (prog->total_duration > 0) {
        return prog->total_duration;
    }

    return prog->pre_water + prog->post_water;
}

static void format_program_period_text(const program_data_t *prog, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return;
    }

    buf[0] = '\0';

    if (!prog || prog->period_count <= 0) {
        snprintf(buf, buf_size, "--");
        return;
    }

    for (int i = 0; i < prog->period_count; i++) {
        if (prog->periods[i].time[0] == '\0') {
            continue;
        }

        size_t used = strlen(buf);
        if (used >= (size_t)(buf_size - 1)) {
            break;
        }

        snprintf(buf + used, buf_size - (int)used, "%s%s",
                 used > 0 ? " / " : "", prog->periods[i].time);
    }

    if (buf[0] == '\0') {
        snprintf(buf, buf_size, "--");
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
void ui_program_close_calendar(void)
{
    if (g_calendar_popup)
    {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
        g_current_date_input = NULL;
        g_calendar_widget = NULL;
        g_year_month_label = NULL;
    }
}

/**
 * @brief 关闭灌区选择对话框（如果存在）
 */
void ui_program_close_zone_dialog(void)
{
    if (g_zone_dialog)
    {
        lv_obj_del(g_zone_dialog);
        g_zone_dialog = NULL;
        g_zone_tab_valve = NULL;
        g_zone_tab_zone = NULL;
        g_zone_content = NULL;
    }
}

void ui_program_close_overlays(void)
{
    /* g_add_dialog 和 g_add_formula_dialog 属于内容区对象，
       由 ui_switch_nav() 中的 lv_obj_clean(g_ui_main.content) 统一销毁 */
    g_add_dialog = false;
    g_add_formula_dialog = NULL;

    if (g_uniform_dialog) {
        lv_obj_del(g_uniform_dialog);
        g_uniform_dialog = NULL;
    }
    if (g_warning_dialog) {
        lv_obj_t *bg = lv_obj_get_parent(g_warning_dialog);
        g_warning_dialog = NULL;
        if (bg) {
            lv_obj_del(bg);
        }
    }
    if (g_delete_confirm_dialog) {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }
}

void ui_program_register_selection_query_cbs(
    ui_get_valve_count_cb_t valve_count_cb,
    ui_get_valve_list_cb_t  valve_list_cb,
    ui_get_zone_count_cb_t  zone_count_cb,
    ui_get_zone_list_cb_t   zone_list_cb,
    ui_get_zone_detail_cb_t zone_detail_cb)
{
    g_program_valve_count_cb = valve_count_cb;
    g_program_valve_list_cb = valve_list_cb;
    g_program_zone_count_cb = zone_count_cb;
    g_program_zone_list_cb = zone_list_cb;
    g_program_zone_detail_cb = zone_detail_cb;
}

/**
 * @brief 创建程序管理界面
 * @param parent 父容器（主内容区）
 */
void ui_program_create(lv_obj_t *parent)
{
    /* 一次性初始化灌溉时段时间(只在第一次调用时执行) */
    static bool periods_time_initialized = false;
    if (!periods_time_initialized)
    {
        for (int i = 0; i < MAX_PERIODS; i++)
        {
            snprintf(g_temp_periods[i].time, sizeof(g_temp_periods[i].time), "%02d:00:00", 6 + i);
        }
        periods_time_initialized = true;
    }

    /* 重置静态指针（旧对象已被 ui_switch_nav 中的 lv_obj_clean 销毁） */
    g_tab_program = NULL;
    g_tab_formula = NULL;
    g_add_dialog = false;
    g_add_formula_dialog = NULL;
    g_parent_container = NULL;
    g_table_area = NULL;
    g_formula_table_area = NULL;
    g_btn_add_program = NULL;
    g_btn_add_formula = NULL;
    g_pagination = NULL;
    g_input_start_date = NULL;
    g_input_end_date = NULL;
    /* g_calendar_popup 由 ui_program_close_calendar() 在 ui_switch_nav 中处理 */
    g_current_date_input = NULL;
    g_calendar_widget = NULL;
    g_year_month_label = NULL;
    g_form_area = NULL;
    for (int i = 0; i < 3; i++) g_menu_buttons[i] = NULL;
    /* g_zone_dialog 由 ui_program_close_zone_dialog() 在 ui_switch_nav 中处理 */
    g_zone_tab_valve = NULL;
    g_zone_tab_zone = NULL;
    g_zone_content = NULL;
    g_uniform_dialog = NULL;
    g_table_bg = NULL;
    g_warning_dialog = NULL;
    g_delete_confirm_dialog = NULL;
    g_zone_table_bg = NULL;
    memset(g_zone_valve_checkboxes, 0, sizeof(g_zone_valve_checkboxes));
    memset(g_zone_zone_checkboxes, 0, sizeof(g_zone_zone_checkboxes));
    g_input_program_name = NULL;
    g_checkbox_auto = NULL;
    g_formula_name_input = NULL;
    g_formula_method_dropdown = NULL;
    g_formula_dilution_input = NULL;
    g_formula_ec_input = NULL;
    g_formula_ph_input = NULL;
    g_formula_valve_input = NULL;
    g_formula_stir_input = NULL;
    g_formula_table_bg = NULL;
    g_input_pre_water = NULL;
    g_input_post_water = NULL;
    g_dropdown_condition = NULL;
    g_dropdown_formula = NULL;
    g_dropdown_mode = NULL;

    /* 保存父容器引用 */
    g_parent_container = parent;

    /* 清空父容器 */
    lv_obj_clean(parent);

    /* 创建顶部标签页 */
    create_tab_buttons(parent);

    /* 创建白色圆角卡片容器 */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, 1168, 660); /* 1178-5-5 = 1168 */
    lv_obj_set_pos(card, 5, 70);      /* 标签页下方5px */
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 10, 0); /* 圆角 */
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建程序管理表格区域 - 现在在卡片内 */
    g_table_area = lv_obj_create(card);
    lv_obj_set_size(g_table_area, 1168, 660);
    lv_obj_set_pos(g_table_area, 0, 0);
    lv_obj_set_style_bg_opa(g_table_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_table_area, 0, 0);
    lv_obj_set_style_pad_all(g_table_area, 0, 0);
    lv_obj_clear_flag(g_table_area, LV_OBJ_FLAG_SCROLLABLE);
    create_table_area(g_table_area);

    /* 创建配方管理表格区域 - 初始隐藏 */
    g_formula_table_area = lv_obj_create(card);
    lv_obj_set_size(g_formula_table_area, 1168, 660);
    lv_obj_set_pos(g_formula_table_area, 0, 0);
    lv_obj_set_style_bg_opa(g_formula_table_area, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_formula_table_area, 0, 0);
    lv_obj_set_style_pad_all(g_formula_table_area, 0, 0);
    lv_obj_clear_flag(g_formula_table_area, LV_OBJ_FLAG_SCROLLABLE);
    create_formula_table_area(g_formula_table_area);
    lv_obj_add_flag(g_formula_table_area, LV_OBJ_FLAG_HIDDEN); /* 初始隐藏 */

    /* 创建分页控件 - 现在在卡片内 */
    g_pagination = lv_obj_create(card);
    lv_obj_set_size(g_pagination, 1168, 80);
    lv_obj_set_pos(g_pagination, 0, 580);
    lv_obj_set_style_bg_opa(g_pagination, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_pagination, 0, 0);
    lv_obj_set_style_pad_all(g_pagination, 0, 0);
    lv_obj_clear_flag(g_pagination, LV_OBJ_FLAG_SCROLLABLE);
    create_pagination(g_pagination);

    /* 创建添加程序按钮 - 现在在卡片内，与分页按钮同一水平线 */
    g_btn_add_program = lv_btn_create(card);
    lv_obj_set_size(g_btn_add_program, 100, 40);                      /* 再缩小按钮 */
    lv_obj_align(g_btn_add_program, LV_ALIGN_BOTTOM_RIGHT, -10, -10); /* 右下角对齐 */
    lv_obj_set_style_bg_color(g_btn_add_program, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(g_btn_add_program, 20, 0);
    lv_obj_add_event_cb(g_btn_add_program, btn_add_program_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_add = lv_label_create(g_btn_add_program);
    lv_label_set_text(label_add, "添加程序");
    lv_obj_set_style_text_color(label_add, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_add, &my_font_cn_16, 0); /* 缩小字体 */
    lv_obj_center(label_add);

    /* 创建添加配方按钮 - 初始隐藏 */
    g_btn_add_formula = lv_btn_create(card);
    lv_obj_set_size(g_btn_add_formula, 100, 40);
    lv_obj_align(g_btn_add_formula, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_set_style_bg_color(g_btn_add_formula, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(g_btn_add_formula, 20, 0);
    lv_obj_add_event_cb(g_btn_add_formula, btn_add_formula_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_add_formula = lv_label_create(g_btn_add_formula);
    lv_label_set_text(label_add_formula, "添加配方");
    lv_obj_set_style_text_color(label_add_formula, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_add_formula, &my_font_cn_16, 0);
    lv_obj_center(label_add_formula);
    lv_obj_add_flag(g_btn_add_formula, LV_OBJ_FLAG_HIDDEN); /* 初始隐藏 */
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 创建顶部标签页按钮
 */
static void create_tab_buttons(lv_obj_t *parent)
{
    /* 程序管理标签 */
    g_tab_program = lv_btn_create(parent);
    lv_obj_set_size(g_tab_program, 150, 50);
    lv_obj_set_pos(g_tab_program, 15, 10);
    lv_obj_set_style_bg_color(g_tab_program, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(g_tab_program, 10, 0);
    lv_obj_set_style_border_width(g_tab_program, 0, 0); /* 选中时无边框 */
    lv_obj_set_style_pad_all(g_tab_program, 0, 0);      /* 移除内边距 */
    lv_obj_add_event_cb(g_tab_program, tab_program_cb, LV_EVENT_CLICKED, NULL);

    /* 程序管理图标 */
    lv_obj_t *icon_program = lv_label_create(g_tab_program);
    lv_label_set_text(icon_program, LV_SYMBOL_LIST);
    lv_obj_set_style_text_color(icon_program, lv_color_white(), 0);
    lv_obj_set_style_text_font(icon_program, &my_font_cn_16, 0);
    lv_obj_align(icon_program, LV_ALIGN_CENTER, -28, 0);

    /* 程序管理文字 */
    lv_obj_t *text_program = lv_label_create(g_tab_program);
    lv_label_set_text(text_program, "程序管理");
    lv_obj_set_style_text_color(text_program, lv_color_white(), 0);
    lv_obj_set_style_text_font(text_program, &my_font_cn_16, 0);
    lv_obj_align(text_program, LV_ALIGN_CENTER, 20, 0);

    /* 配方管理标签 */
    g_tab_formula = lv_btn_create(parent);
    lv_obj_set_size(g_tab_formula, 150, 50);
    lv_obj_set_pos(g_tab_formula, 180, 10);
    lv_obj_set_style_bg_color(g_tab_formula, lv_color_white(), 0); /* 未选中白色 */
    lv_obj_set_style_radius(g_tab_formula, 10, 0);
    lv_obj_set_style_border_width(g_tab_formula, 1, 0);                      /* 添加边框 */
    lv_obj_set_style_border_color(g_tab_formula, lv_color_hex(0xcccccc), 0); /* 淡灰色边框 */
    lv_obj_set_style_pad_all(g_tab_formula, 0, 0);                           /* 移除内边距 */
    lv_obj_add_event_cb(g_tab_formula, tab_formula_cb, LV_EVENT_CLICKED, NULL);

    /* 配方管理图标 */
    lv_obj_t *icon_formula = lv_label_create(g_tab_formula);
    lv_label_set_text(icon_formula, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(icon_formula, COLOR_PRIMARY, 0); /* 蓝色图标 */
    lv_obj_set_style_text_font(icon_formula, &my_font_cn_16, 0);
    lv_obj_align(icon_formula, LV_ALIGN_CENTER, -28, 0);

    /* 配方管理文字 */
    lv_obj_t *text_formula = lv_label_create(g_tab_formula);
    lv_label_set_text(text_formula, "配方管理");
    lv_obj_set_style_text_color(text_formula, lv_color_black(), 0); /* 黑色文字 */
    lv_obj_set_style_text_font(text_formula, &my_font_cn_16, 0);
    lv_obj_align(text_formula, LV_ALIGN_CENTER, 20, 0);
}

/**
 * @brief 创建表格区域
 */
static void create_table_area(lv_obj_t *parent)
{
    /* 表格背景 */
    g_table_bg = lv_obj_create(parent);
    lv_obj_set_size(g_table_bg, 1148, 580); /* 增加高度 */
    lv_obj_set_pos(g_table_bg, 10, 10);     /* 卡片内部位置 */
    lv_obj_set_style_bg_color(g_table_bg, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_table_bg, 0, 0); /* 移除边框 */
    lv_obj_set_style_radius(g_table_bg, 0, 0);
    lv_obj_set_style_pad_all(g_table_bg, 0, 0);
    lv_obj_clear_flag(g_table_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头背景 */
    lv_obj_t *header_bg = lv_obj_create(g_table_bg);
    lv_obj_set_size(header_bg, 1148, 50); /* 无边框，占满宽度 */
    lv_obj_set_pos(header_bg, 0, 0);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0); /* 移除内边距 */

    /* 表头列 */
    const char *headers[] = {"序号", "启用自动", "程序名称", "下次启动时段", "合计时长", "关联配方", "启动条件", "操作"};
    int header_widths[] = {80, 120, 180, 180, 120, 150, 150, 166};
    int x_pos = 0;

    for (int i = 0; i < 8; i++)
    {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0); /* 缩小字体 */
        lv_obj_set_pos(header_label, x_pos + 10, 17);                /* 调整垂直居中 */
        x_pos += header_widths[i];
    }

    /* 动态添加程序数据行 */
    refresh_program_table();
}

/**
 * @brief 创建分页控件
 */
static void create_pagination(lv_obj_t *parent)
{
    /* 分页控件容器 - 底部居中对齐 */
    lv_obj_t *pagination_bg = lv_obj_create(parent);
    lv_obj_set_size(pagination_bg, 450, 50);                  /* 容器宽度和高度 */
    lv_obj_align(pagination_bg, LV_ALIGN_BOTTOM_MID, 0, -20); /* 底部居中，上移一点 */
    lv_obj_set_style_bg_opa(pagination_bg, LV_OPA_TRANSP, 0); /* 透明背景 */
    lv_obj_set_style_border_width(pagination_bg, 0, 0);
    lv_obj_set_style_radius(pagination_bg, 0, 0);
    lv_obj_set_style_pad_all(pagination_bg, 0, 0); /* 移除所有内边距 */
    lv_obj_clear_flag(pagination_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 首页按钮 */
    lv_obj_t *btn_first = lv_btn_create(pagination_bg);
    lv_obj_set_size(btn_first, 80, 40);
    lv_obj_set_pos(btn_first, 5, 5);
    lv_obj_set_style_bg_color(btn_first, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_radius(btn_first, 5, 0);
    lv_obj_set_style_pad_all(btn_first, 0, 0);

    lv_obj_t *label_first = lv_label_create(btn_first);
    lv_label_set_text(label_first, "首页");
    lv_obj_set_style_text_font(label_first, &my_font_cn_16, 0);
    lv_obj_center(label_first);

    /* 上一页按钮 */
    lv_obj_t *btn_prev = lv_btn_create(pagination_bg);
    lv_obj_set_size(btn_prev, 80, 40);
    lv_obj_set_pos(btn_prev, 90, 5);
    lv_obj_set_style_bg_color(btn_prev, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_radius(btn_prev, 5, 0);
    lv_obj_set_style_pad_all(btn_prev, 0, 0);

    lv_obj_t *label_prev = lv_label_create(btn_prev);
    lv_label_set_text(label_prev, "上一页");
    lv_obj_set_style_text_font(label_prev, &my_font_cn_16, 0);
    lv_obj_center(label_prev);

    /* 页码显示 */
    lv_obj_t *label_page = lv_label_create(pagination_bg);
    lv_label_set_text(label_page, "0/0");
    lv_obj_set_style_text_font(label_page, &my_font_cn_16, 0);
    lv_obj_set_pos(label_page, 200, 13);

    /* 下一页按钮 */
    lv_obj_t *btn_next = lv_btn_create(pagination_bg);
    lv_obj_set_size(btn_next, 80, 40);
    lv_obj_set_pos(btn_next, 280, 5);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_radius(btn_next, 5, 0);
    lv_obj_set_style_pad_all(btn_next, 0, 0);

    lv_obj_t *label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, "下一页");
    lv_obj_set_style_text_font(label_next, &my_font_cn_16, 0);
    lv_obj_center(label_next);

    /* 尾页按钮 */
    lv_obj_t *btn_last = lv_btn_create(pagination_bg);
    lv_obj_set_size(btn_last, 80, 40);
    lv_obj_set_pos(btn_last, 365, 5);
    lv_obj_set_style_bg_color(btn_last, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_radius(btn_last, 5, 0);
    lv_obj_set_style_pad_all(btn_last, 0, 0);

    lv_obj_t *label_last = lv_label_create(btn_last);
    lv_label_set_text(label_last, "尾页");
    lv_obj_set_style_text_font(label_last, &my_font_cn_16, 0);
    lv_obj_center(label_last);
}

/**
 * @brief 程序管理标签点击回调
 */
static void tab_program_cb(lv_event_t *e)
{
    (void)e;
    /* 切换到程序管理标签 */
    lv_obj_set_style_bg_color(g_tab_program, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(g_tab_formula, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_tab_program, 0, 0); /* 选中时无边框 */
    lv_obj_set_style_border_width(g_tab_formula, 1, 0); /* 未选中时有边框 */
    lv_obj_set_style_border_color(g_tab_formula, lv_color_hex(0xcccccc), 0);

    /* 更新程序管理标签颜色（选中）：图标和文字都是白色 */
    lv_obj_t *icon_program = lv_obj_get_child(g_tab_program, 0);
    lv_obj_t *text_program = lv_obj_get_child(g_tab_program, 1);
    if (icon_program)
    {
        lv_obj_set_style_text_color(icon_program, lv_color_white(), 0);
    }
    if (text_program)
    {
        lv_obj_set_style_text_color(text_program, lv_color_white(), 0);
    }

    /* 更新配方管理标签颜色（未选中）：图标蓝色，文字黑色 */
    lv_obj_t *icon_formula = lv_obj_get_child(g_tab_formula, 0);
    lv_obj_t *text_formula = lv_obj_get_child(g_tab_formula, 1);
    if (icon_formula)
    {
        lv_obj_set_style_text_color(icon_formula, COLOR_PRIMARY, 0);
    }
    if (text_formula)
    {
        lv_obj_set_style_text_color(text_formula, lv_color_black(), 0);
    }

    /* 显示程序管理区域，隐藏配方管理区域 */
    if (g_table_area)
    {
        lv_obj_clear_flag(g_table_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_formula_table_area)
    {
        lv_obj_add_flag(g_formula_table_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_btn_add_program)
    {
        lv_obj_clear_flag(g_btn_add_program, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_btn_add_formula)
    {
        lv_obj_add_flag(g_btn_add_formula, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 配方管理标签点击回调
 */
static void tab_formula_cb(lv_event_t *e)
{
    (void)e;
    /* 切换到配方管理标签 */
    lv_obj_set_style_bg_color(g_tab_formula, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(g_tab_program, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_tab_formula, 0, 0); /* 选中时无边框 */
    lv_obj_set_style_border_width(g_tab_program, 1, 0); /* 未选中时有边框 */
    lv_obj_set_style_border_color(g_tab_program, lv_color_hex(0xcccccc), 0);

    /* 更新配方管理标签颜色（选中）：图标和文字都是白色 */
    lv_obj_t *icon_formula = lv_obj_get_child(g_tab_formula, 0);
    lv_obj_t *text_formula = lv_obj_get_child(g_tab_formula, 1);
    if (icon_formula)
    {
        lv_obj_set_style_text_color(icon_formula, lv_color_white(), 0);
    }
    if (text_formula)
    {
        lv_obj_set_style_text_color(text_formula, lv_color_white(), 0);
    }

    /* 更新程序管理标签颜色（未选中）：图标蓝色，文字黑色 */
    lv_obj_t *icon_program = lv_obj_get_child(g_tab_program, 0);
    lv_obj_t *text_program = lv_obj_get_child(g_tab_program, 1);
    if (icon_program)
    {
        lv_obj_set_style_text_color(icon_program, COLOR_PRIMARY, 0);
    }
    if (text_program)
    {
        lv_obj_set_style_text_color(text_program, lv_color_black(), 0);
    }

    /* 显示配方管理区域，隐藏程序管理区域 */
    if (g_formula_table_area)
    {
        lv_obj_clear_flag(g_formula_table_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_table_area)
    {
        lv_obj_add_flag(g_table_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_btn_add_formula)
    {
        lv_obj_clear_flag(g_btn_add_formula, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_btn_add_program)
    {
        lv_obj_add_flag(g_btn_add_program, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 添加程序按钮回调
 */
static void btn_add_program_cb(lv_event_t *e)
{
    (void)e;
    create_add_program_dialog();
}

/**
 * @brief 创建添加程序对话框
 */
static void create_add_program_dialog(void)
{
    /* 如果不是编辑模式，重置临时表单数据为默认值 */
    if (!g_is_editing_program)
    {
        reset_temp_form_data();
    }
    else
    {
        /* 编辑模式：加载程序数据到临时变量 */
        g_current_editing_program = g_temp_program;
        g_temp_auto_enabled = g_temp_program.auto_enabled;
        snprintf(g_temp_start_date, sizeof(g_temp_start_date), "%s", g_temp_program.start_date);
        snprintf(g_temp_end_date, sizeof(g_temp_end_date), "%s", g_temp_program.end_date);

        /* 恢复肥前清水、肥后清水 */
        g_temp_pre_water = g_temp_program.pre_water;
        g_temp_post_water = g_temp_program.post_water;

        /* 恢复关联配方、灌溉模式、启动条件 */
        snprintf(g_temp_formula, sizeof(g_temp_formula), "%s", g_temp_program.formula);
        snprintf(g_temp_mode, sizeof(g_temp_mode), "%s", g_temp_program.mode);
        snprintf(g_temp_condition, sizeof(g_temp_condition), "%s", g_temp_program.condition);

        /* 恢复灌溉时段数据 */
        for (int i = 0; i < MAX_PERIODS && i < g_temp_program.period_count; i++)
        {
            g_temp_periods[i] = g_temp_program.periods[i];
        }

        /* 恢复阀门和灌区选择 */
        for (int i = 0; i < 10; i++)
        {
            g_temp_selected_valves[i] = g_temp_program.selected_valves[i];
            g_temp_selected_zones[i] = g_temp_program.selected_zones[i];
        }
    }

    /* 添加程序页面属于内容区重建流程，不单独删除内容区对象 */
    g_add_dialog = false;

    /* 清空父容器 */
    lv_obj_clean(g_parent_container);

    /* 标记当前处于添加程序页面 */
    g_add_dialog = true;

    /* 顶部元素 - 程序名称标签 */
    lv_obj_t *label_name_title = lv_label_create(g_parent_container);
    lv_label_set_text(label_name_title, "程序名称");
    lv_obj_set_pos(label_name_title, 10, 25);
    lv_obj_set_style_text_font(label_name_title, &my_fontbd_16, 0);

    /* 程序名称输入框 */
    g_input_program_name = lv_textarea_create(g_parent_container);
    lv_obj_set_size(g_input_program_name, 200, 40);
    lv_obj_set_pos(g_input_program_name, 110, 20);
    /* 从临时变量恢复程序名称,如果为空则使用默认值 */
    if (strlen(g_current_editing_program.name) > 0)
    {
        lv_textarea_set_text(g_input_program_name, g_current_editing_program.name);
    }
    else
    {
        lv_textarea_set_text(g_input_program_name, "程序1");
    }
    lv_textarea_set_one_line(g_input_program_name, true);
    lv_obj_set_style_bg_color(g_input_program_name, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_input_program_name, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_input_program_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_input_program_name, 0, 0);
    lv_obj_set_style_pad_right(g_input_program_name, 0, 0);
    lv_obj_set_style_pad_top(g_input_program_name, 4, 0);
    lv_obj_set_style_pad_bottom(g_input_program_name, 0, 0);
    lv_obj_add_event_cb(g_input_program_name, name_input_click_cb, LV_EVENT_CLICKED, NULL);

    /* 取消按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(g_parent_container);
    lv_obj_set_size(btn_cancel, 120, 45);
    lv_obj_set_pos(btn_cancel, 900, 18);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_radius(btn_cancel, 22, 0);
    lv_obj_add_event_cb(btn_cancel, btn_cancel_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, g_is_editing_program ? "取消编辑" : "取消添加");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认按钮 */
    lv_obj_t *btn_confirm = lv_btn_create(g_parent_container);
    lv_obj_set_size(btn_confirm, 120, 45);
    lv_obj_set_pos(btn_confirm, 1035, 18);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_confirm, 22, 0);
    lv_obj_add_event_cb(btn_confirm, btn_confirm_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, g_is_editing_program ? "确认编辑" : "确认添加");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);

    /* 左侧浅蓝色菜单区域 - 有圆角，上下左各5px边距 */
    lv_obj_t *left_menu = lv_obj_create(g_parent_container);
    lv_obj_set_size(left_menu, 250, 658);                            /* 高度: 738 - 70(顶部) - 5(底部) - 5(调整) = 658 */
    lv_obj_set_pos(left_menu, 5, 70);                                /* x: 左边距5px, y: 70 */
    lv_obj_set_style_bg_color(left_menu, lv_color_hex(0xa8d8ea), 0); /* 浅蓝色 */
    lv_obj_set_style_border_width(left_menu, 0, 0);
    lv_obj_set_style_radius(left_menu, 10, 0); /* 圆角 */
    lv_obj_set_style_pad_all(left_menu, 0, 0);
    lv_obj_clear_flag(left_menu, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧菜单项 */
    const char *menu_items[] = {"灌溉日期", "灌溉时段", "灌区选择"};
    lv_event_cb_t menu_callbacks[] = {menu_irrigation_date_cb, menu_irrigation_period_cb, menu_irrigation_zone_cb};

    for (int i = 0; i < 3; i++)
    {
        lv_obj_t *menu_btn = lv_btn_create(left_menu);
        lv_obj_set_size(menu_btn, 230, 50);
        lv_obj_set_pos(menu_btn, 10, 20 + i * 60);
        /* 第一个按钮(灌溉日期)默认选中为深蓝色，其他为浅蓝色 */
        if (i == 0)
        {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x70c1d8), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0xa8d8ea), 0);
        }
        lv_obj_set_style_radius(menu_btn, 5, 0);
        lv_obj_add_event_cb(menu_btn, menu_callbacks[i], LV_EVENT_CLICKED, NULL);

        /* 保存按钮引用 */
        g_menu_buttons[i] = menu_btn;

        lv_obj_t *menu_label = lv_label_create(menu_btn);
        lv_label_set_text_fmt(menu_label, "%s         >", menu_items[i]);
        lv_obj_set_style_text_font(menu_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(menu_label, lv_color_black(), 0);
        lv_obj_align(menu_label, LV_ALIGN_LEFT_MID, 20, 0);
        lv_obj_set_style_text_font(menu_label, &my_font_cn_16, 0);
    }

    /* 右侧白色表单区域 - 有圆角，上下右各5px边距 */
    g_form_area = lv_obj_create(g_parent_container);
    lv_obj_set_size(g_form_area, 913, 658);                      /* 宽度: 1178 - 5(左边距) - 250(左侧) - 5(间距) - 5(右边距) = 913, 高度: 738 - 70(顶部) - 5(底部) - 5 = 658 */
    lv_obj_set_pos(g_form_area, 260, 70);                        /* x: 5(左边距) + 250(左侧) + 5(间距), y: 70 */
    lv_obj_set_style_bg_color(g_form_area, lv_color_white(), 0); /* 白色背景 */
    lv_obj_set_style_border_width(g_form_area, 0, 0);
    lv_obj_set_style_radius(g_form_area, 10, 0); /* 圆角 */
    lv_obj_set_style_pad_all(g_form_area, 0, 0);
    lv_obj_clear_flag(g_form_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 默认显示灌溉日期界面 */
    create_irrigation_date_panel(g_form_area);
}

/**
 * @brief 取消添加按钮回调
 */
static void btn_cancel_add_cb(lv_event_t *e)
{
    (void)e;
    /* 重置临时表单数据 */
    reset_temp_form_data();
    /* 重置编辑模式标志 */
    g_is_editing_program = false;
    g_editing_program_index = -1;
    /* 恢复显示程序列表页面 */
    ui_program_create(g_parent_container);
}

/**
 * @brief 确认添加按钮回调
 */
static void btn_confirm_add_cb(lv_event_t *e)
{
    (void)e;

    /* 先保存当前表单数据到临时变量 */
    save_current_form_data();

    /* 检查程序名是否为空 */
    if (strlen(g_current_editing_program.name) == 0)
    {
        show_warning_dialog("程序名称不能为空!");
        return;
    }

    /* 检查程序名是否重复 */
    for (int i = 0; i < g_program_count; i++)
    {
        /* 如果是编辑模式，跳过自己 */
        if (g_is_editing_program && i == g_editing_program_index)
            continue;

        if (strcmp(g_programs[i].name, g_current_editing_program.name) == 0)
        {
            show_warning_dialog("程序名称已存在,请使用不同的名称!");
            return;
        }
    }

    program_data_t *prog;

    if (g_is_editing_program)
    {
        /* 编辑模式：更新现有程序 */
        if (g_editing_program_index < 0 || g_editing_program_index >= g_program_count)
        {
            show_warning_dialog("编辑失败：程序索引无效!");
            return;
        }
        prog = &g_programs[g_editing_program_index];
    }
    else
    {
        /* 添加模式：检查是否超过最大程序数 */
        if (g_program_count >= MAX_PROGRAMS)
        {
            show_warning_dialog("超过存储上限，请先删减");
            return;
        }
        prog = &g_programs[g_program_count];
        g_program_count++;
    }

    /* 程序名称 - 从临时变量读取 */
    snprintf(prog->name, sizeof(prog->name), "%s", g_current_editing_program.name);

    /* 启用自动 - 从临时变量读取 */
    prog->auto_enabled = g_temp_auto_enabled;

    /* 开始日期 - 从临时变量读取 */
    snprintf(prog->start_date, sizeof(prog->start_date), "%s", g_temp_start_date);

    /* 结束日期 - 从临时变量读取 */
    snprintf(prog->end_date, sizeof(prog->end_date), "%s", g_temp_end_date);

    /* 保存灌溉时段数据 */
    int period_count = 0;
    for (int i = 0; i < MAX_PERIODS; i++)
    {
        if (g_temp_periods[i].enabled)
        {
            prog->periods[period_count] = g_temp_periods[i];
            period_count++;
        }
    }
    prog->period_count = period_count;

    /* 保存阀门和灌区选择 */
    for (int i = 0; i < 10; i++)
    {
        prog->selected_valves[i] = g_temp_selected_valves[i];
        prog->selected_zones[i] = g_temp_selected_zones[i];
    }

    /* 保存肥前清水、肥后清水 - 从临时变量读取 */
    prog->pre_water = g_temp_pre_water;
    prog->post_water = g_temp_post_water;

    /* 保存关联配方、灌溉模式、启动条件 - 从临时变量读取 */
    snprintf(prog->formula, sizeof(prog->formula), "%s", g_temp_formula);
    snprintf(prog->mode, sizeof(prog->mode), "%s", g_temp_mode);
    snprintf(prog->condition, sizeof(prog->condition), "%s", g_temp_condition);

    /* 设置其他默认值 */
    snprintf(prog->next_start, sizeof(prog->next_start), "--");
    int min_total_duration = prog->pre_water + prog->post_water;
    if (g_is_editing_program && prog->total_duration > min_total_duration) {
        min_total_duration = prog->total_duration;
    }
    prog->total_duration = min_total_duration;

    /* 保存到NVS */
    int save_idx = g_is_editing_program ? g_editing_program_index : (g_program_count - 1);
    nvs_save_program(save_idx);

    /* 重置临时表单数据和编辑模式标志 */
    reset_temp_form_data();
    g_is_editing_program = false;
    g_editing_program_index = -1;

    /* 恢复显示程序列表页面 */
    ui_program_create(g_parent_container);
}

/**
 * @brief 开始日期日历按钮回调
 */
static void btn_calendar_start_cb(lv_event_t *e)
{
    (void)e;

    /* 如果已经有弹窗,先删除 */
    if (g_calendar_popup)
    {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
    }

    g_current_date_input = g_input_start_date;

    /* 创建日历弹窗 */
    g_calendar_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_calendar_popup, 465, 390); /* 宽度465px, 高度390px */
    lv_obj_center(g_calendar_popup);
    lv_obj_set_style_bg_color(g_calendar_popup, lv_color_white(), 0);
    lv_obj_set_style_radius(g_calendar_popup, 10, 0);
    lv_obj_clear_flag(g_calendar_popup, LV_OBJ_FLAG_SCROLLABLE); /* 禁止滚动 */

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
    lv_obj_clear_flag(g_calendar_widget, LV_OBJ_FLAG_SCROLLABLE); /* 禁止滚动拖动 */
    lv_obj_set_style_text_font(g_calendar_widget, &my_font_cn_16, 0);

    /* 从输入框解析日期并设置日历显示 */
    calendar_set_from_input(g_calendar_widget, g_current_date_input, g_year_month_label);

    /* 关闭按钮 */
    lv_obj_t *btn_close = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_close, 100, 40);
    lv_obj_set_pos(btn_close, (465 - 100) / 2 - 20, 325); /* 放在日历控件下方,向左偏移20px */
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
    (void)e;

    /* 如果已经有弹窗,先删除 */
    if (g_calendar_popup)
    {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
    }

    g_current_date_input = g_input_end_date;

    /* 创建日历弹窗 */
    g_calendar_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_calendar_popup, 465, 390); /* 宽度465px, 高度390px */
    lv_obj_center(g_calendar_popup);
    lv_obj_set_style_bg_color(g_calendar_popup, lv_color_white(), 0);
    lv_obj_set_style_radius(g_calendar_popup, 10, 0);
    lv_obj_clear_flag(g_calendar_popup, LV_OBJ_FLAG_SCROLLABLE); /* 禁止滚动 */

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
    lv_obj_clear_flag(g_calendar_widget, LV_OBJ_FLAG_SCROLLABLE); /* 禁止滚动拖动 */
    lv_obj_set_style_text_font(g_calendar_widget, &my_font_cn_16, 0);

    /* 从输入框解析日期并设置日历显示 */
    calendar_set_from_input(g_calendar_widget, g_current_date_input, g_year_month_label);

    /* 关闭按钮 */
    lv_obj_t *btn_close = lv_btn_create(g_calendar_popup);
    lv_obj_set_size(btn_close, 100, 40);
    lv_obj_set_pos(btn_close, (465 - 100) / 2 - 20, 325); /* 放在日历控件下方,向左偏移20px */
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
    if (g_calendar_popup)
    {
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
    lv_obj_t *calendar = lv_event_get_current_target(e); /* 使用 current_target 而不是 target */

    /* 只处理 VALUE_CHANGED 事件 */
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        lv_calendar_date_t date;

        if (lv_calendar_get_pressed_date(calendar, &date) == LV_RESULT_OK)
        {
            if (g_current_date_input)
            {
                char date_str[16];
                snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d",
                         date.year, date.month, date.day);
                lv_textarea_set_text(g_current_date_input, date_str);

                /* 选择日期后异步关闭弹窗（避免在回调中删除导致崩溃） */
                if (g_calendar_popup)
                {
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
    if (g_calendar_widget)
    {
        const lv_calendar_date_t *date_ptr = lv_calendar_get_showed_date(g_calendar_widget);
        lv_calendar_date_t date = *date_ptr; /* 复制结构体 */
        date.year--;
        lv_calendar_set_showed_date(g_calendar_widget, date.year, date.month);

        /* 更新年月显示 */
        if (g_year_month_label)
        {
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
    if (g_calendar_widget)
    {
        const lv_calendar_date_t *date_ptr = lv_calendar_get_showed_date(g_calendar_widget);
        lv_calendar_date_t date = *date_ptr; /* 复制结构体 */
        date.year++;
        lv_calendar_set_showed_date(g_calendar_widget, date.year, date.month);

        /* 更新年月显示 */
        if (g_year_month_label)
        {
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
    if (g_calendar_widget)
    {
        const lv_calendar_date_t *date_ptr = lv_calendar_get_showed_date(g_calendar_widget);
        lv_calendar_date_t date = *date_ptr; /* 复制结构体 */

        /* 处理月份边界 */
        if (date.month == 1)
        {
            date.month = 12;
            date.year--;
        }
        else
        {
            date.month--;
        }

        lv_calendar_set_showed_date(g_calendar_widget, date.year, date.month);

        /* 更新年月显示 */
        if (g_year_month_label)
        {
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
    if (g_calendar_widget)
    {
        const lv_calendar_date_t *date_ptr = lv_calendar_get_showed_date(g_calendar_widget);
        lv_calendar_date_t date = *date_ptr; /* 复制结构体 */

        /* 处理月份边界 */
        if (date.month == 12)
        {
            date.month = 1;
            date.year++;
        }
        else
        {
            date.month++;
        }

        lv_calendar_set_showed_date(g_calendar_widget, date.year, date.month);

        /* 更新年月显示 */
        if (g_year_month_label)
        {
            char date_str[16];
            snprintf(date_str, sizeof(date_str), "%04d-%02d", date.year, date.month);
            lv_label_set_text(g_year_month_label, date_str);
        }
    }
}
/**
 * @brief 灌溉日期菜单回调
 */
static void menu_irrigation_date_cb(lv_event_t *e)
{
    (void)e;

    /* 保存当前表单数据 */
    save_current_form_data();

    /* 关闭日历弹窗（如果存在） */
    if (g_calendar_popup)
    {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
        g_current_date_input = NULL;
        g_calendar_widget = NULL;
        g_year_month_label = NULL;
    }

    /* 更新菜单按钮状态 - 选中第0个 */
    for (int i = 0; i < 3; i++)
    {
        if (g_menu_buttons[i])
        {
            if (i == 0)
            {
                lv_obj_set_style_bg_color(g_menu_buttons[i], lv_color_hex(0x70c1d8), 0);
            }
            else
            {
                lv_obj_set_style_bg_color(g_menu_buttons[i], lv_color_hex(0xa8d8ea), 0);
            }
        }
    }

    /* 清空表单区域并显示灌溉日期界面 */
    if (g_form_area)
    {
        lv_obj_clean(g_form_area);
        clear_program_form_widget_refs();
        create_irrigation_date_panel(g_form_area);
    }
}

/**
 * @brief 灌溉时段菜单回调
 */
static void menu_irrigation_period_cb(lv_event_t *e)
{
    (void)e;

    /* 保存当前表单数据 */
    save_current_form_data();

    /* 关闭日历弹窗（如果存在） */
    if (g_calendar_popup)
    {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
        g_current_date_input = NULL;
        g_calendar_widget = NULL;
        g_year_month_label = NULL;
    }

    /* 更新菜单按钮状态 - 选中第1个 */
    for (int i = 0; i < 3; i++)
    {
        if (g_menu_buttons[i])
        {
            if (i == 1)
            {
                lv_obj_set_style_bg_color(g_menu_buttons[i], lv_color_hex(0x70c1d8), 0);
            }
            else
            {
                lv_obj_set_style_bg_color(g_menu_buttons[i], lv_color_hex(0xa8d8ea), 0);
            }
        }
    }

    /* 清空表单区域并显示灌溉时段界面 */
    if (g_form_area)
    {
        lv_obj_clean(g_form_area);
        clear_program_form_widget_refs();
        create_irrigation_period_panel(g_form_area);
    }
}

/**
 * @brief 灌区选择菜单回调
 */
static void menu_irrigation_zone_cb(lv_event_t *e)
{
    (void)e;

    /* 保存当前表单数据 */
    save_current_form_data();

    /* 关闭日历弹窗（如果存在） */
    if (g_calendar_popup)
    {
        lv_obj_del(g_calendar_popup);
        g_calendar_popup = NULL;
        g_current_date_input = NULL;
        g_calendar_widget = NULL;
        g_year_month_label = NULL;
    }

    /* 更新菜单按钮状态 - 选中第2个 */
    for (int i = 0; i < 3; i++)
    {
        if (g_menu_buttons[i])
        {
            if (i == 2)
            {
                lv_obj_set_style_bg_color(g_menu_buttons[i], lv_color_hex(0x70c1d8), 0);
            }
            else
            {
                lv_obj_set_style_bg_color(g_menu_buttons[i], lv_color_hex(0xa8d8ea), 0);
            }
        }
    }

    /* 清空表单区域并显示灌区选择界面 */
    if (g_form_area)
    {
        lv_obj_clean(g_form_area);
        clear_program_form_widget_refs();
        create_irrigation_zone_panel(g_form_area);
    }
}

/**
 * @brief 创建灌溉时段界面
 */
static void create_irrigation_period_panel(lv_obj_t *parent)
{
    /* 创建可滚动容器 */
    lv_obj_t *scroll_container = lv_obj_create(parent);
    lv_obj_set_size(scroll_container, 883, 628);
    lv_obj_set_pos(scroll_container, 10, 10);
    lv_obj_set_style_bg_color(scroll_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(scroll_container, 0, 0);
    lv_obj_set_style_pad_all(scroll_container, 0, 0);
    lv_obj_set_style_radius(scroll_container, 0, 0);
    lv_obj_set_scroll_dir(scroll_container, LV_DIR_VER);

    int y_pos = 10;

    /* 创建10个启动时段 */
    for (int i = 0; i < 10; i++)
    {
        /* 时段标签 */
        lv_obj_t *label_period = lv_label_create(scroll_container);
        lv_label_set_text_fmt(label_period, "启动时段%d", i + 1);
        lv_obj_set_pos(label_period, 20, y_pos + 8);
        lv_obj_set_style_text_font(label_period, &my_font_cn_16, 0);
        lv_obj_set_style_text_font(label_period, &my_font_cn_16, 0);

        /* 开启复选框 */
        lv_obj_t *checkbox = lv_checkbox_create(scroll_container);
        lv_checkbox_set_text(checkbox, "开启");
        lv_obj_set_pos(checkbox, 200, y_pos + 5);
        lv_obj_set_style_text_font(checkbox, &my_font_cn_16, 0);

        /* 从临时变量恢复复选框状态 */
        if (g_temp_periods[i].enabled)
        {
            lv_obj_add_state(checkbox, LV_STATE_CHECKED);
        }

        /* 时间输入框容器 */
        lv_obj_t *time_container = lv_obj_create(scroll_container);
        lv_obj_set_size(time_container, 245, 40);
        lv_obj_set_pos(time_container, 350, y_pos);
        /* 根据复选框状态设置背景颜色 */
        if (g_temp_periods[i].enabled)
        {
            lv_obj_set_style_bg_color(time_container, lv_color_white(), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(time_container, lv_color_hex(0xf5f5f5), 0);
        }
        lv_obj_set_style_border_width(time_container, 2, 0);
        lv_obj_set_style_border_color(time_container, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_radius(time_container, 0, 0);
        lv_obj_set_style_pad_all(time_container, 0, 0);
        lv_obj_clear_flag(time_container, LV_OBJ_FLAG_SCROLLABLE);

        /* 时间输入框 - 只读 */
        lv_obj_t *time_input = lv_textarea_create(time_container);
        lv_obj_set_size(time_input, 205, 40);
        lv_obj_set_pos(time_input, 0, 0);
        /* 从临时变量加载时间 */
        lv_textarea_set_text(time_input, g_temp_periods[i].time);
        lv_textarea_set_one_line(time_input, true);
        /* 根据复选框状态设置背景颜色 */
        if (g_temp_periods[i].enabled)
        {
            lv_obj_set_style_bg_color(time_input, lv_color_white(), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(time_input, lv_color_hex(0xf5f5f5), 0);
        }
        lv_obj_set_style_bg_opa(time_input, LV_OPA_COVER, 0); /* 确保背景不透明 */
        lv_obj_set_style_border_width(time_input, 0, 0);
        lv_obj_set_style_radius(time_input, 0, 0);
        lv_obj_set_style_text_font(time_input, &my_font_cn_16, 0);
        lv_textarea_set_accepted_chars(time_input, "");       /* 设为只读,不接受任何字符 */
        lv_obj_clear_flag(time_input, LV_OBJ_FLAG_CLICKABLE); /* 不可点击 */

        /* 将时间输入框作为用户数据传递给复选框 */
        lv_obj_add_event_cb(checkbox, period_checkbox_cb, LV_EVENT_VALUE_CHANGED, time_input);

        /* 时间选择按钮（日历图标） */
        lv_obj_t *btn_time = lv_btn_create(time_container);
        lv_obj_set_size(btn_time, 40, 40);
        lv_obj_set_pos(btn_time, 205, 0);
        lv_obj_set_style_bg_color(btn_time, lv_color_hex(0xf0f0f0), 0);
        lv_obj_set_style_border_width(btn_time, 0, 0);
        lv_obj_set_style_radius(btn_time, 0, 0);
        /* TODO: 添加时间选择回调 */

        lv_obj_t *label_time_icon = lv_label_create(btn_time);
        lv_label_set_text(label_time_icon, LV_SYMBOL_LIST);
        lv_obj_set_style_text_font(label_time_icon, &my_font_cn_16, 0);
        lv_obj_center(label_time_icon);

        y_pos += 60;
    }
}

/**
 * @brief 创建灌溉日期界面
 */
static void create_irrigation_date_panel(lv_obj_t *parent)
{
    int y_pos = 20;

    /* 启用自动控制 复选框 */
    g_checkbox_auto = lv_checkbox_create(parent);
    lv_checkbox_set_text(g_checkbox_auto, "启用自动控制");
    lv_obj_set_style_text_font(g_checkbox_auto, &my_font_cn_16, 0);
    lv_obj_set_pos(g_checkbox_auto, 20, y_pos);

    /* 从临时变量恢复复选框状态 */
    if (g_temp_auto_enabled)
    {
        lv_obj_add_state(g_checkbox_auto, LV_STATE_CHECKED);
    }

    y_pos += 60;

    /* 生效日期行 */
    lv_obj_t *label_date = lv_label_create(parent);
    lv_label_set_text(label_date, "生效日期:");
    lv_obj_set_style_text_font(label_date, &my_font_cn_16, 0);
    lv_obj_set_pos(label_date, 20, y_pos);

    /* 开始日期输入框容器 */
    lv_obj_t *start_date_container = lv_obj_create(parent);
    lv_obj_set_size(start_date_container, 245, 40);
    lv_obj_set_pos(start_date_container, 140, y_pos - 5);
    lv_obj_set_style_bg_color(start_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(start_date_container, 2, 0);
    lv_obj_set_style_border_color(start_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(start_date_container, 0, 0);
    lv_obj_set_style_pad_all(start_date_container, 0, 0);
    lv_obj_clear_flag(start_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 开始日期输入框 - 只读 */
    g_input_start_date = lv_textarea_create(start_date_container);
    lv_obj_set_size(g_input_start_date, 205, 40);
    lv_obj_set_pos(g_input_start_date, 0, 0);
    lv_textarea_set_text(g_input_start_date, g_temp_start_date); /* 从临时变量加载 */
    lv_textarea_set_one_line(g_input_start_date, true);
    lv_obj_set_style_bg_color(g_input_start_date, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_input_start_date, 0, 0);
    lv_obj_set_style_radius(g_input_start_date, 0, 0);
    lv_obj_clear_flag(g_input_start_date, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_state(g_input_start_date, LV_STATE_DISABLED);

    /* 开始日期日历图标按钮 - 在容器内 */
    lv_obj_t *btn_cal_start = lv_btn_create(start_date_container);
    lv_obj_set_size(btn_cal_start, 40, 40);
    lv_obj_set_pos(btn_cal_start, 205, 0);
    lv_obj_set_style_bg_color(btn_cal_start, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(btn_cal_start, 0, 0);
    lv_obj_set_style_radius(btn_cal_start, 0, 0);
    lv_obj_add_event_cb(btn_cal_start, btn_calendar_start_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cal_start = lv_label_create(btn_cal_start);
    lv_label_set_text(label_cal_start, LV_SYMBOL_LIST);
    lv_obj_set_style_text_font(label_cal_start, &my_font_cn_16, 0);
    lv_obj_center(label_cal_start);

    lv_obj_t *label_to = lv_label_create(parent);
    lv_label_set_text(label_to, "至");
    lv_obj_set_style_text_font(label_to, &my_font_cn_16, 0);
    lv_obj_set_pos(label_to, 400, y_pos);

    /* 结束日期输入框容器 */
    lv_obj_t *end_date_container = lv_obj_create(parent);
    lv_obj_set_size(end_date_container, 245, 40);
    lv_obj_set_pos(end_date_container, 440, y_pos - 5);
    lv_obj_set_style_bg_color(end_date_container, lv_color_white(), 0);
    lv_obj_set_style_border_width(end_date_container, 2, 0);
    lv_obj_set_style_border_color(end_date_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(end_date_container, 0, 0);
    lv_obj_set_style_pad_all(end_date_container, 0, 0);
    lv_obj_clear_flag(end_date_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 结束日期输入框 - 只读 */
    g_input_end_date = lv_textarea_create(end_date_container);
    lv_obj_set_size(g_input_end_date, 205, 40);
    lv_obj_set_pos(g_input_end_date, 0, 0);
    lv_textarea_set_text(g_input_end_date, g_temp_end_date); /* 从临时变量加载 */
    lv_textarea_set_one_line(g_input_end_date, true);
    lv_obj_set_style_bg_color(g_input_end_date, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_input_end_date, 0, 0);
    lv_obj_set_style_radius(g_input_end_date, 0, 0);
    lv_obj_clear_flag(g_input_end_date, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_state(g_input_end_date, LV_STATE_DISABLED);

    /* 结束日期日历图标按钮 - 在容器内 */
    lv_obj_t *btn_cal_end = lv_btn_create(end_date_container);
    lv_obj_set_size(btn_cal_end, 40, 40);
    lv_obj_set_pos(btn_cal_end, 205, 0);
    lv_obj_set_style_bg_color(btn_cal_end, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(btn_cal_end, 0, 0);
    lv_obj_set_style_radius(btn_cal_end, 0, 0);
    lv_obj_add_event_cb(btn_cal_end, btn_calendar_end_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cal_end = lv_label_create(btn_cal_end);
    lv_label_set_text(label_cal_end, LV_SYMBOL_LIST);
    lv_obj_set_style_text_font(label_cal_end, &my_font_cn_16, 0);
    lv_obj_center(label_cal_end);

    y_pos += 70;

    /* 第一行：启动条件 和 关联配方 */
    lv_obj_t *label_condition = lv_label_create(parent);
    lv_label_set_text(label_condition, "启动条件:");
    lv_obj_set_style_text_font(label_condition, &my_font_cn_16, 0);
    lv_obj_set_pos(label_condition, 20, y_pos);

    g_dropdown_condition = lv_dropdown_create(parent);
    lv_dropdown_set_options(g_dropdown_condition, "定时");
    lv_obj_set_size(g_dropdown_condition, 230, 40);
    lv_obj_set_style_text_font(g_dropdown_condition, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_dropdown_condition), &my_font_cn_16, 0);
    lv_obj_set_pos(g_dropdown_condition, 180, y_pos - 5);
    lv_obj_set_style_bg_color(g_dropdown_condition, lv_color_white(), 0);

    lv_obj_t *label_formula = lv_label_create(parent);
    lv_label_set_text(label_formula, "关联配方:");
    lv_obj_set_style_text_font(label_formula, &my_font_cn_16, 0);
    lv_obj_set_pos(label_formula, 480, y_pos);

    g_dropdown_formula = lv_dropdown_create(parent);

    /* 动态构建配方选项列表 */
    char formula_options[512] = "无"; /* 默认总是有"无"选项 */
    if (g_formula_count > 0)
    {
        /* 如果有配方,添加所有配方名称 */
        for (int i = 0; i < g_formula_count; i++)
        {
            strcat(formula_options, "\n");
            strcat(formula_options, g_formulas[i].name);
        }
    }
    lv_dropdown_set_options(g_dropdown_formula, formula_options);

    lv_obj_set_size(g_dropdown_formula, 230, 40);
    lv_obj_set_style_text_font(g_dropdown_formula, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_dropdown_formula), &my_font_cn_16, 0);
    lv_obj_set_pos(g_dropdown_formula, 640, y_pos - 5);
    lv_obj_set_style_bg_color(g_dropdown_formula, lv_color_white(), 0);
    y_pos += 70;

    /* 第二行：程序时间冲突 和 肥后清水 */
    lv_obj_t *label_conflict = lv_label_create(parent);
    lv_label_set_text(label_conflict, "程序时间冲突:");
    lv_obj_set_style_text_font(label_conflict, &my_font_cn_16, 0);
    lv_obj_set_pos(label_conflict, 20, y_pos);

    lv_obj_t *dd_conflict = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd_conflict, "谁先执行谁");
    lv_obj_set_size(dd_conflict, 230, 40);
    lv_obj_set_style_text_font(dd_conflict, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dd_conflict), &my_font_cn_16, 0);
    lv_obj_set_pos(dd_conflict, 180, y_pos - 5);
    lv_obj_set_style_bg_color(dd_conflict, lv_color_white(), 0);

    lv_obj_t *label_post_water = lv_label_create(parent);
    lv_label_set_text(label_post_water, "肥后清水(min):");
    lv_obj_set_style_text_font(label_post_water, &my_font_cn_16, 0);
    lv_obj_set_pos(label_post_water, 480, y_pos);

    g_input_post_water = lv_textarea_create(parent);
    lv_obj_set_size(g_input_post_water, 230, 40);
    lv_obj_set_pos(g_input_post_water, 640, y_pos - 5);
    char post_water_str[16];
    snprintf(post_water_str, sizeof(post_water_str), "%d", g_temp_post_water);
    lv_textarea_set_text(g_input_post_water, post_water_str);
    lv_textarea_set_one_line(g_input_post_water, true);
    lv_obj_set_style_bg_color(g_input_post_water, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_input_post_water, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_input_post_water, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_input_post_water, 0, 0);
    lv_obj_set_style_pad_right(g_input_post_water, 0, 0);
    lv_obj_set_style_pad_top(g_input_post_water, 4, 0);
    lv_obj_set_style_pad_bottom(g_input_post_water, 0, 0);
    lv_obj_add_event_cb(g_input_post_water, textarea_click_cb, LV_EVENT_CLICKED, NULL);
    y_pos += 70;

    /* 第三行：肥前清水 */
    lv_obj_t *label_pre_water = lv_label_create(parent);
    lv_label_set_text(label_pre_water, "肥前清水(min):");
    lv_obj_set_style_text_font(label_pre_water, &my_font_cn_16, 0);
    lv_obj_set_pos(label_pre_water, 20, y_pos);

    g_input_pre_water = lv_textarea_create(parent);
    lv_obj_set_size(g_input_pre_water, 230, 40);
    lv_obj_set_pos(g_input_pre_water, 180, y_pos - 5);
    char pre_water_str[16];
    snprintf(pre_water_str, sizeof(pre_water_str), "%d", g_temp_pre_water);
    lv_textarea_set_text(g_input_pre_water, pre_water_str);
    lv_textarea_set_one_line(g_input_pre_water, true);
    lv_obj_set_style_bg_color(g_input_pre_water, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_input_pre_water, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_input_pre_water, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_input_pre_water, 0, 0);
    lv_obj_set_style_pad_right(g_input_pre_water, 0, 0);
    lv_obj_set_style_pad_top(g_input_pre_water, 4, 0);
    lv_obj_set_style_pad_bottom(g_input_pre_water, 0, 0);
    lv_obj_add_event_cb(g_input_pre_water, textarea_click_cb, LV_EVENT_CLICKED, NULL);
    y_pos += 70;

    /* 第四行：灌溉模式 */
    lv_obj_t *label_mode = lv_label_create(parent);
    lv_label_set_text(label_mode, "灌溉模式:");
    lv_obj_set_style_text_font(label_mode, &my_font_cn_16, 0);
    lv_obj_set_pos(label_mode, 20, y_pos);

    g_dropdown_mode = lv_dropdown_create(parent);
    lv_dropdown_set_options(g_dropdown_mode, "每天执行\n间隔天数\n按照星期\n每月奇数日\n每月偶数日");
    lv_obj_set_size(g_dropdown_mode, 230, 40);
    lv_obj_set_style_text_font(g_dropdown_mode, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_dropdown_mode), &my_font_cn_16, 0);
    lv_obj_set_pos(g_dropdown_mode, 180, y_pos - 5);
    lv_obj_set_style_bg_color(g_dropdown_mode, lv_color_white(), 0);

    /* 根据临时变量设置下拉框选中项 */
    /* 设置灌溉模式 */
    const char *mode_options[] = {"每天执行", "间隔天数", "按照星期", "每月奇数日", "每月偶数日"};
    for (int i = 0; i < 5; i++)
    {
        if (strcmp(g_temp_mode, mode_options[i]) == 0)
        {
            lv_dropdown_set_selected(g_dropdown_mode, i);
            break;
        }
    }

    /* 设置关联配方 - 需要从选项中查找匹配的项 */
    char formula_buf[32];
    uint16_t formula_count = lv_dropdown_get_option_cnt(g_dropdown_formula);
    if (formula_count > 0)
    {
        bool found = false;
        for (uint16_t i = 0; i < formula_count; i++)
        {
            lv_dropdown_set_selected(g_dropdown_formula, i);
            lv_dropdown_get_selected_str(g_dropdown_formula, formula_buf, sizeof(formula_buf));
            if (strcmp(g_temp_formula, formula_buf) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            lv_dropdown_set_selected(g_dropdown_formula, 0);
        }
    }

    /* 启动条件通常只有一个选项"定时",所以默认选中即可 */
    lv_dropdown_set_selected(g_dropdown_condition, 0);
}

/**
 * @brief 创建灌区选择界面
 */
static void create_irrigation_zone_panel(lv_obj_t *parent)
{
    /* 创建表格区域 */
    g_zone_table_bg = lv_obj_create(parent);
    lv_obj_set_size(g_zone_table_bg, 883, 520);
    lv_obj_set_pos(g_zone_table_bg, 10, 10);
    lv_obj_set_style_bg_color(g_zone_table_bg, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_zone_table_bg, 0, 0);
    lv_obj_set_style_pad_all(g_zone_table_bg, 0, 0);
    lv_obj_set_style_radius(g_zone_table_bg, 0, 0);
    lv_obj_clear_flag(g_zone_table_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头背景 */
    lv_obj_t *header_bg = lv_obj_create(g_zone_table_bg);
    lv_obj_set_size(header_bg, 883, 50);
    lv_obj_set_pos(header_bg, 0, 0);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);

    /* 表头列 */
    const char *headers[] = {"序号", "名称", "类型", "运行时长(分)", "操作"};
    int header_widths[] = {100, 250, 180, 200, 153};
    int x_pos = 0;

    for (int i = 0; i < 5; i++)
    {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_pos(header_label, x_pos + 20, 17);
        x_pos += header_widths[i];
    }

    refresh_irrigation_zone_table();

    /* 底部按钮区域 - 距离白色底边20px */
    int btn_height = 45;               /* 按钮高度 */
    int btn_y = 648 - 20 - btn_height; /* 648(parent高度) - 20(底部间距) - 按钮高度 */
    int gap = 10;                      /* 按钮间隔 */
    int x_start = 50;                  /* 从左侧开始 */

    /* 上移按钮 */
    lv_obj_t *btn_up = lv_btn_create(parent);
    lv_obj_set_size(btn_up, 100, btn_height);
    lv_obj_set_pos(btn_up, x_start, btn_y);
    lv_obj_set_style_bg_color(btn_up, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_up, 22, 0);

    lv_obj_t *label_up = lv_label_create(btn_up);
    lv_label_set_text(label_up, "上移");
    lv_obj_set_style_text_color(label_up, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_up, &my_font_cn_16, 0);
    lv_obj_center(label_up);

    /* 下移按钮 */
    x_pos = x_start + 100 + gap;
    lv_obj_t *btn_down = lv_btn_create(parent);
    lv_obj_set_size(btn_down, 100, btn_height);
    lv_obj_set_pos(btn_down, x_pos, btn_y);
    lv_obj_set_style_bg_color(btn_down, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_down, 22, 0);

    lv_obj_t *label_down = lv_label_create(btn_down);
    lv_label_set_text(label_down, "下移");
    lv_obj_set_style_text_color(label_down, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_down, &my_font_cn_16, 0);
    lv_obj_center(label_down);

    /* 倒序按钮 */
    x_pos += 100 + gap;
    lv_obj_t *btn_reverse = lv_btn_create(parent);
    lv_obj_set_size(btn_reverse, 100, btn_height);
    lv_obj_set_pos(btn_reverse, x_pos, btn_y);
    lv_obj_set_style_bg_color(btn_reverse, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_reverse, 22, 0);

    lv_obj_t *label_reverse = lv_label_create(btn_reverse);
    lv_label_set_text(label_reverse, "倒序");
    lv_obj_set_style_text_color(label_reverse, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_reverse, &my_font_cn_16, 0);
    lv_obj_center(label_reverse);

    /* 运行时长标签 */
    x_pos += 100 + 20; /* 增加间距 */
    lv_obj_t *label_duration = lv_label_create(parent);
    lv_label_set_text(label_duration, "运行时长(分)");
    lv_obj_set_pos(label_duration, x_pos, btn_y + 12);
    lv_obj_set_style_text_font(label_duration, &my_font_cn_16, 0);

    /* 运行时长输入框 - 白色背景 */
    x_pos += 105; /* 标签后 */
    lv_obj_t *input_bg = lv_obj_create(parent);
    lv_obj_set_size(input_bg, 75, btn_height);
    lv_obj_set_pos(input_bg, x_pos, btn_y);
    lv_obj_set_style_bg_color(input_bg, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_bg, 1, 0);
    lv_obj_set_style_border_color(input_bg, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(input_bg, 5, 0);
    lv_obj_set_style_pad_all(input_bg, 0, 0);
    lv_obj_clear_flag(input_bg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *input_duration = lv_textarea_create(input_bg);
    lv_obj_set_size(input_duration, 73, 43);
    lv_obj_set_pos(input_duration, 1, 1);
    lv_textarea_set_text(input_duration, "0");
    lv_textarea_set_one_line(input_duration, true);
    lv_obj_set_style_bg_color(input_duration, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_duration, 0, 0);
    lv_obj_set_style_radius(input_duration, 0, 0);
    lv_obj_set_style_text_align(input_duration, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(input_duration, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_duration, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_duration, 0, 0);
    lv_obj_set_style_pad_right(input_duration, 0, 0);
    lv_obj_set_style_pad_top(input_duration, 4, 0);
    lv_obj_set_style_pad_bottom(input_duration, 0, 0);

    /* 统一设置按钮 */
    x_pos += 75 + gap;
    lv_obj_t *btn_apply = lv_btn_create(parent);
    lv_obj_set_size(btn_apply, 110, btn_height);
    lv_obj_set_pos(btn_apply, x_pos, btn_y);
    lv_obj_set_style_bg_color(btn_apply, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_apply, 22, 0);
    lv_obj_add_event_cb(btn_apply, btn_uniform_set_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_apply = lv_label_create(btn_apply);
    lv_label_set_text(label_apply, "统一设置");
    lv_obj_set_style_text_color(label_apply, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_apply, &my_font_cn_16, 0);
    lv_obj_center(label_apply);

    /* 选择灌区按钮 */
    x_pos += 110 + gap;
    lv_obj_t *btn_select = lv_btn_create(parent);
    lv_obj_set_size(btn_select, 120, btn_height);
    lv_obj_set_pos(btn_select, x_pos, btn_y);
    lv_obj_set_style_bg_color(btn_select, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_select, 22, 0);
    lv_obj_add_event_cb(btn_select, btn_select_zone_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_select = lv_label_create(btn_select);
    lv_label_set_text(label_select, "选择灌区");
    lv_obj_set_style_text_color(label_select, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_select, &my_font_cn_16, 0);
    lv_obj_center(label_select);
}

/**
 * @brief 创建配方管理表格区域
 */
static void create_formula_table_area(lv_obj_t *parent)
{
    /* 表格背景 */
    g_formula_table_bg = lv_obj_create(parent);
    lv_obj_set_size(g_formula_table_bg, 1148, 580);
    lv_obj_set_pos(g_formula_table_bg, 10, 10);
    lv_obj_set_style_bg_color(g_formula_table_bg, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_formula_table_bg, 0, 0);
    lv_obj_set_style_radius(g_formula_table_bg, 0, 0);
    lv_obj_set_style_pad_all(g_formula_table_bg, 0, 0);
    lv_obj_clear_flag(g_formula_table_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头背景 */
    lv_obj_t *header_bg = lv_obj_create(g_formula_table_bg);
    lv_obj_set_size(header_bg, 1148, 50);
    lv_obj_set_pos(header_bg, 0, 0);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);

    /* 表头列 - 配方管理：序号、配方名称、配方详情、操作 */
    const char *headers[] = {"序号", "配方名称", "配方详情", "操作"};
    int header_widths[] = {100, 250, 558, 240};  /* 调整配方详情和操作列宽度 */
    int x_pos = 0;

    for (int i = 0; i < 4; i++)
    {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_pos(header_label, x_pos + 20, 17);
        x_pos += header_widths[i];
    }

    /* 刷新配方数据 */
    refresh_formula_table();
}

/**
 * @brief 添加配方按钮回调
 */
static void btn_add_formula_cb(lv_event_t *e)
{
    (void)e;
    create_add_formula_dialog();
}

/**
 * @brief 创建添加配方对话框
 */
static void create_add_formula_dialog(void)
{
    /* 如果是编辑模式，加载配方数据到临时变量 */
    if (g_is_editing_formula)
    {
        g_current_editing_formula = g_temp_formula_data;
    }
    else
    {
        /* 添加模式：重置临时配方数据为默认值 */
        memset(&g_current_editing_formula, 0, sizeof(g_current_editing_formula));
        g_current_editing_formula.method = 0;           /* 默认比例稀释 */
        g_current_editing_formula.dilution = 100;       /* 默认稀释倍数 */
        g_current_editing_formula.ec = 1.32f;           /* 默认EC值 */
        g_current_editing_formula.ph = 7.6f;            /* 默认PH值 */
        g_current_editing_formula.valve_opening = 100.0f; /* 默认阀门开度 */
        g_current_editing_formula.stir_time = 30;       /* 默认搅拌时间 */
        g_current_editing_formula.channel_count = 0;    /* 默认无通道 */
    }

    /* 清空父容器 */
    lv_obj_clean(g_parent_container);

    /* 添加配方页面属于内容区重建流程，不纳入 screen 级 overlay 删除链路 */
    g_add_formula_dialog = NULL;

    /* 配方名称标签 - 在父容器顶部 */
    lv_obj_t *label_name_title = lv_label_create(g_parent_container);
    lv_label_set_text(label_name_title, "配方名称");
    lv_obj_set_pos(label_name_title, 10, 25);
    lv_obj_set_style_text_font(label_name_title, &my_fontbd_16, 0);

    /* 配方名称输入框 - 在父容器顶部 */
    g_formula_name_input = lv_textarea_create(g_parent_container);
    lv_obj_set_size(g_formula_name_input, 250, 40);
    lv_obj_set_pos(g_formula_name_input, 110, 20);
    /* 根据编辑模式加载配方名称 */
    if (g_is_editing_formula && strlen(g_current_editing_formula.name) > 0)
    {
        lv_textarea_set_text(g_formula_name_input, g_current_editing_formula.name);
    }
    else
    {
        lv_textarea_set_text(g_formula_name_input, "配方1");
    }
    lv_textarea_set_one_line(g_formula_name_input, true);
    lv_obj_set_style_bg_color(g_formula_name_input, lv_color_hex(0xfafafa), 0);
    lv_obj_set_style_border_width(g_formula_name_input, 1, 0);
    lv_obj_set_style_border_color(g_formula_name_input, lv_color_hex(0xd0d0d0), 0);
    lv_obj_set_style_radius(g_formula_name_input, 5, 0);
    lv_obj_set_style_pad_all(g_formula_name_input, 8, 0);
    lv_obj_set_style_text_font(g_formula_name_input, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_formula_name_input, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_formula_name_input, 0, 0);
    lv_obj_set_style_pad_right(g_formula_name_input, 0, 0);
    lv_obj_set_style_pad_top(g_formula_name_input, 4, 0);
    lv_obj_set_style_pad_bottom(g_formula_name_input, 0, 0);
    lv_obj_add_event_cb(g_formula_name_input, name_input_click_cb, LV_EVENT_CLICKED, NULL);

    /* 取消添加按钮 - 在父容器顶部右侧 */
    lv_obj_t *btn_cancel = lv_btn_create(g_parent_container);
    lv_obj_set_size(btn_cancel, 120, 40);
    lv_obj_set_pos(btn_cancel, 920, 20);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_radius(btn_cancel, 20, 0);
    lv_obj_add_event_cb(btn_cancel, btn_cancel_add_formula_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, g_is_editing_formula ? "取消编辑" : "取消添加");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认添加按钮 - 在父容器顶部右侧 */
    lv_obj_t *btn_confirm = lv_btn_create(g_parent_container);
    lv_obj_set_size(btn_confirm, 120, 40);
    lv_obj_set_pos(btn_confirm, 1048, 20);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_confirm, 20, 0);
    lv_obj_add_event_cb(btn_confirm, btn_confirm_add_formula_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, g_is_editing_formula ? "确认编辑" : "确认添加");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);

    /* 创建白色圆角卡片容器 - 在配方名称下方 */
    lv_obj_t *card = lv_obj_create(g_parent_container);
    lv_obj_set_size(card, 1168, 660);
    lv_obj_set_pos(card, 5, 70);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 将对话框容器设置为白色卡片 */
    g_add_formula_dialog = card;

    /* 白色卡片内的第一行：配肥方式 + 稀释倍数 */
    int y_pos = 0;     /* 由于有padding，从0开始 */
    int input_x = 210; /* 统一输入框的x位置，让它们对齐 */

    /* 配肥方式标签 */
    lv_obj_t *label_method = lv_label_create(card);
    lv_label_set_text(label_method, "配肥方式:");
    lv_obj_set_pos(label_method, 0, y_pos + 10);
    lv_obj_set_style_text_font(label_method, &my_font_cn_16, 0);

    /* 配肥方式下拉框 */
    g_formula_method_dropdown = lv_dropdown_create(card);
    lv_dropdown_set_options(g_formula_method_dropdown, "比例稀释\nEC调配\n固定流速");
    lv_obj_set_size(g_formula_method_dropdown, 220, 40);
    lv_obj_set_style_text_font(g_formula_method_dropdown, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_formula_method_dropdown), &my_font_cn_16, 0);
    lv_obj_set_pos(g_formula_method_dropdown, input_x, y_pos);
    lv_obj_set_style_bg_color(g_formula_method_dropdown, lv_color_hex(0xfafafa), 0);
    lv_obj_set_style_border_width(g_formula_method_dropdown, 1, 0);
    lv_obj_set_style_border_color(g_formula_method_dropdown, lv_color_hex(0xd0d0d0), 0);
    lv_obj_set_style_radius(g_formula_method_dropdown, 5, 0);
    lv_obj_set_style_pad_left(g_formula_method_dropdown, 8, 0);
    /* 根据临时配方数据设置配肥方式 */
    lv_dropdown_set_selected(g_formula_method_dropdown, g_current_editing_formula.method);
    lv_obj_add_event_cb(g_formula_method_dropdown, dropdown_method_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 稀释倍数标签 */
    lv_obj_t *label_dilution = lv_label_create(card);
    lv_label_set_text(label_dilution, "稀释倍数(清水/母液):");
    lv_obj_set_pos(label_dilution, 500, y_pos + 10);
    lv_obj_set_style_text_font(label_dilution, &my_font_cn_16, 0);

    /* 稀释倍数输入框 */
    g_formula_dilution_input = lv_textarea_create(card);
    lv_obj_set_size(g_formula_dilution_input, 100, 40);
    lv_obj_set_pos(g_formula_dilution_input, 690, y_pos);
    /* 根据临时配方数据设置稀释倍数 */
    char dilution_str[16];
    snprintf(dilution_str, sizeof(dilution_str), "%d", g_current_editing_formula.dilution);
    lv_textarea_set_text(g_formula_dilution_input, dilution_str);
    lv_textarea_set_one_line(g_formula_dilution_input, true);
    lv_obj_set_style_bg_color(g_formula_dilution_input, lv_color_hex(0xfafafa), 0);
    lv_obj_set_style_border_width(g_formula_dilution_input, 1, 0);
    lv_obj_set_style_border_color(g_formula_dilution_input, lv_color_hex(0xd0d0d0), 0);
    lv_obj_set_style_radius(g_formula_dilution_input, 5, 0);
    lv_obj_set_style_pad_all(g_formula_dilution_input, 8, 0);
    lv_obj_set_style_text_font(g_formula_dilution_input, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_formula_dilution_input, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_formula_dilution_input, 0, 0);
    lv_obj_set_style_pad_right(g_formula_dilution_input, 0, 0);
    lv_obj_set_style_pad_top(g_formula_dilution_input, 4, 0);
    lv_obj_set_style_pad_bottom(g_formula_dilution_input, 0, 0);
    lv_obj_add_event_cb(g_formula_dilution_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 第二行：EC目标值 + PH目标值 */
    y_pos += 60;

    /* EC目标值 */
    lv_obj_t *label_ec = lv_label_create(card);
    lv_label_set_text(label_ec, "EC目标值(ms/cm):");
    lv_obj_set_pos(label_ec, 0, y_pos + 10);
    lv_obj_set_style_text_font(label_ec, &my_font_cn_16, 0);

    g_formula_ec_input = lv_textarea_create(card);
    lv_obj_set_size(g_formula_ec_input, 220, 40);
    lv_obj_set_pos(g_formula_ec_input, input_x, y_pos);
    /* 根据临时配方数据设置EC值 */
    char ec_str[16];
    snprintf(ec_str, sizeof(ec_str), "%.2f", g_current_editing_formula.ec);
    lv_textarea_set_text(g_formula_ec_input, ec_str);
    lv_textarea_set_one_line(g_formula_ec_input, true);
    lv_obj_set_style_bg_color(g_formula_ec_input, lv_color_hex(0xfafafa), 0);
    lv_obj_set_style_border_width(g_formula_ec_input, 1, 0);
    lv_obj_set_style_border_color(g_formula_ec_input, lv_color_hex(0xd0d0d0), 0);
    lv_obj_set_style_radius(g_formula_ec_input, 5, 0);
    lv_obj_set_style_pad_all(g_formula_ec_input, 8, 0);
    lv_obj_set_style_text_font(g_formula_ec_input, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_formula_ec_input, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_formula_ec_input, 0, 0);
    lv_obj_set_style_pad_right(g_formula_ec_input, 0, 0);
    lv_obj_set_style_pad_top(g_formula_ec_input, 4, 0);
    lv_obj_set_style_pad_bottom(g_formula_ec_input, 0, 0);
    lv_obj_add_event_cb(g_formula_ec_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* PH目标值 */
    lv_obj_t *label_ph = lv_label_create(card);
    lv_label_set_text(label_ph, "PH目标值:");
    lv_obj_set_pos(label_ph, 500, y_pos + 10);
    lv_obj_set_style_text_font(label_ph, &my_font_cn_16, 0);

    g_formula_ph_input = lv_textarea_create(card);
    lv_obj_set_size(g_formula_ph_input, 100, 40);
    lv_obj_set_pos(g_formula_ph_input, 690, y_pos);
    /* 根据临时配方数据设置PH值 */
    char ph_str[16];
    snprintf(ph_str, sizeof(ph_str), "%.1f", g_current_editing_formula.ph);
    lv_textarea_set_text(g_formula_ph_input, ph_str);
    lv_textarea_set_one_line(g_formula_ph_input, true);
    lv_obj_set_style_bg_color(g_formula_ph_input, lv_color_hex(0xfafafa), 0);
    lv_obj_set_style_border_width(g_formula_ph_input, 1, 0);
    lv_obj_set_style_border_color(g_formula_ph_input, lv_color_hex(0xd0d0d0), 0);
    lv_obj_set_style_radius(g_formula_ph_input, 5, 0);
    lv_obj_set_style_pad_all(g_formula_ph_input, 8, 0);
    lv_obj_set_style_text_font(g_formula_ph_input, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_formula_ph_input, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_formula_ph_input, 0, 0);
    lv_obj_set_style_pad_right(g_formula_ph_input, 0, 0);
    lv_obj_set_style_pad_top(g_formula_ph_input, 4, 0);
    lv_obj_set_style_pad_bottom(g_formula_ph_input, 0, 0);
    lv_obj_add_event_cb(g_formula_ph_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 第三行：搅拌时间 + 阀门固定开度 */
    y_pos += 60;

    /* 搅拌时间 */
    lv_obj_t *label_stir = lv_label_create(card);
    lv_label_set_text(label_stir, "搅拌时间(S):");
    lv_obj_set_pos(label_stir, 0, y_pos + 10);
    lv_obj_set_style_text_font(label_stir, &my_font_cn_16, 0);

    g_formula_stir_input = lv_textarea_create(card);
    lv_obj_set_size(g_formula_stir_input, 220, 40);
    lv_obj_set_pos(g_formula_stir_input, input_x, y_pos);
    /* 根据临时配方数据设置搅拌时间 */
    char stir_str[16];
    snprintf(stir_str, sizeof(stir_str), "%d", g_current_editing_formula.stir_time);
    lv_textarea_set_text(g_formula_stir_input, stir_str);
    lv_textarea_set_one_line(g_formula_stir_input, true);
    lv_obj_set_style_bg_color(g_formula_stir_input, lv_color_hex(0xfafafa), 0);
    lv_obj_set_style_border_width(g_formula_stir_input, 1, 0);
    lv_obj_set_style_border_color(g_formula_stir_input, lv_color_hex(0xd0d0d0), 0);
    lv_obj_set_style_radius(g_formula_stir_input, 5, 0);
    lv_obj_set_style_pad_all(g_formula_stir_input, 8, 0);
    lv_obj_set_style_text_font(g_formula_stir_input, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_formula_stir_input, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_formula_stir_input, 0, 0);
    lv_obj_set_style_pad_right(g_formula_stir_input, 0, 0);
    lv_obj_set_style_pad_top(g_formula_stir_input, 4, 0);
    lv_obj_set_style_pad_bottom(g_formula_stir_input, 0, 0);
    lv_obj_add_event_cb(g_formula_stir_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 阀门固定开度 */
    lv_obj_t *label_valve = lv_label_create(card);
    lv_label_set_text(label_valve, "阀门固定开度(%):");
    lv_obj_set_pos(label_valve, 500, y_pos + 10);
    lv_obj_set_style_text_font(label_valve, &my_font_cn_16, 0);

    g_formula_valve_input = lv_textarea_create(card);
    lv_obj_set_size(g_formula_valve_input, 100, 40);
    lv_obj_set_pos(g_formula_valve_input, 690, y_pos);
    /* 根据临时配方数据设置阀门开度 */
    char valve_str[16];
    snprintf(valve_str, sizeof(valve_str), "%.1f", g_current_editing_formula.valve_opening);
    lv_textarea_set_text(g_formula_valve_input, valve_str);
    lv_textarea_set_one_line(g_formula_valve_input, true);
    lv_obj_set_style_bg_color(g_formula_valve_input, lv_color_hex(0xfafafa), 0);
    lv_obj_set_style_border_width(g_formula_valve_input, 1, 0);
    lv_obj_set_style_border_color(g_formula_valve_input, lv_color_hex(0xd0d0d0), 0);
    lv_obj_set_style_radius(g_formula_valve_input, 5, 0);
    lv_obj_set_style_pad_all(g_formula_valve_input, 8, 0);
    lv_obj_set_style_text_font(g_formula_valve_input, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_formula_valve_input, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_formula_valve_input, 0, 0);
    lv_obj_set_style_pad_right(g_formula_valve_input, 0, 0);
    lv_obj_set_style_pad_top(g_formula_valve_input, 4, 0);
    lv_obj_set_style_pad_bottom(g_formula_valve_input, 0, 0);
    lv_obj_add_event_cb(g_formula_valve_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 已选通道号区域 */
    y_pos += 70;

    lv_obj_t *label_channels = lv_label_create(card);
    lv_label_set_text(label_channels, "已选通道号:");
    lv_obj_set_pos(label_channels, 0, y_pos);
    lv_obj_set_style_text_font(label_channels, &my_font_cn_16, 0);

    /* 创建通道表格区域 - 宽度改为3/5 */
    y_pos += 35;
    int table_width = (int)(1128 * 0.6); /* 3/5宽度 */
    lv_obj_t *channel_table = lv_obj_create(card);
    lv_obj_set_size(channel_table, table_width, 280);
    lv_obj_set_pos(channel_table, 0, y_pos);
    lv_obj_set_style_bg_color(channel_table, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(channel_table, 1, 0);
    lv_obj_set_style_border_color(channel_table, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(channel_table, 5, 0);
    lv_obj_set_style_pad_all(channel_table, 0, 0);
    lv_obj_clear_flag(channel_table, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头 */
    lv_obj_t *table_header = lv_obj_create(channel_table);
    lv_obj_set_size(table_header, table_width, 45);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);

    const char *headers[] = {"勾选", "注肥通道", "注肥比例", "母液罐"};
    int header_x[] = {30, 180, 360, 510}; /* 按3/5宽度调整列位置 */

    for (int i = 0; i < 4; i++)
    {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 15);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
    }

    /* 数据行 - 1号肥通道 */
    int row_y = 50;

    /* 复选框 */
    lv_obj_t *checkbox = lv_checkbox_create(channel_table);
    lv_obj_set_pos(checkbox, 30, row_y + 5);
    lv_checkbox_set_text(checkbox, "");

    /* 注肥通道 */
    lv_obj_t *label_channel1 = lv_label_create(channel_table);
    lv_label_set_text(label_channel1, "1号肥通道");
    lv_obj_set_pos(label_channel1, 180, row_y + 10);
    lv_obj_set_style_text_font(label_channel1, &my_font_cn_16, 0);

    /* 注肥比例 */
    lv_obj_t *input_ratio = lv_textarea_create(channel_table);
    lv_obj_set_size(input_ratio, 100, 35);
    lv_obj_set_pos(input_ratio, 360, row_y);
    lv_textarea_set_text(input_ratio, "1.00");
    lv_textarea_set_one_line(input_ratio, true);
    lv_obj_set_style_bg_color(input_ratio, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_ratio, 1, 0);
    lv_obj_set_style_border_color(input_ratio, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(input_ratio, 5, 0);
    lv_obj_set_style_text_font(input_ratio, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_ratio, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_ratio, 0, 0);
    lv_obj_set_style_pad_right(input_ratio, 0, 0);
    lv_obj_set_style_pad_top(input_ratio, 4, 0);
    lv_obj_set_style_pad_bottom(input_ratio, 0, 0);
    lv_obj_add_event_cb(input_ratio, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 母液罐 */
    lv_obj_t *label_tank = lv_label_create(channel_table);
    lv_label_set_text(label_tank, "1号");
    lv_obj_set_pos(label_tank, 510, row_y + 10);
    lv_obj_set_style_text_font(label_tank, &my_font_cn_16, 0);

    /* 初始化输入框状态 - 根据配肥方式和编辑模式设置 */
    int current_method = 0; /* 默认比例稀释 */
    if (g_is_editing_formula)
    {
        current_method = g_temp_formula_data.method;
    }

    /* 根据配肥方式设置输入框状态 */
    switch (current_method)
    {
    case 0: /* 比例稀释 */
        /* 稀释倍数可编辑，EC、PH和阀门固定开度禁用 */
        if (g_formula_dilution_input)
        {
            lv_obj_clear_state(g_formula_dilution_input, LV_STATE_DISABLED);
            lv_obj_add_flag(g_formula_dilution_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_dilution_input, lv_color_black(), 0);
            lv_textarea_set_accepted_chars(g_formula_dilution_input, "0123456789");
        }
        if (g_formula_ec_input)
        {
            lv_obj_add_state(g_formula_ec_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ec_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ec_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ec_input, "");
        }
        if (g_formula_ph_input)
        {
            lv_obj_add_state(g_formula_ph_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ph_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ph_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ph_input, "");
        }
        if (g_formula_valve_input)
        {
            lv_obj_add_state(g_formula_valve_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_valve_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_valve_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_valve_input, "");
        }
        break;

    case 1: /* EC调配 */
        /* 只有EC可编辑，稀释倍数、PH和阀门固定开度禁用 */
        if (g_formula_dilution_input)
        {
            lv_obj_add_state(g_formula_dilution_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_dilution_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_dilution_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_dilution_input, "");
        }
        if (g_formula_ec_input)
        {
            lv_obj_clear_state(g_formula_ec_input, LV_STATE_DISABLED);
            lv_obj_add_flag(g_formula_ec_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ec_input, lv_color_black(), 0);
            lv_textarea_set_accepted_chars(g_formula_ec_input, "0123456789.");
        }
        if (g_formula_ph_input)
        {
            lv_obj_add_state(g_formula_ph_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ph_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ph_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ph_input, "");
        }
        if (g_formula_valve_input)
        {
            lv_obj_add_state(g_formula_valve_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_valve_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_valve_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_valve_input, "");
        }
        break;

    case 2: /* 固定流速 */
        /* 稀释倍数、EC和PH禁用，阀门固定开度可编辑 */
        if (g_formula_dilution_input)
        {
            lv_obj_add_state(g_formula_dilution_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_dilution_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_dilution_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_dilution_input, "");
        }
        if (g_formula_ec_input)
        {
            lv_obj_add_state(g_formula_ec_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ec_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ec_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ec_input, "");
        }
        if (g_formula_ph_input)
        {
            lv_obj_add_state(g_formula_ph_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ph_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ph_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ph_input, "");
        }
        if (g_formula_valve_input)
        {
            lv_obj_clear_state(g_formula_valve_input, LV_STATE_DISABLED);
            lv_obj_add_flag(g_formula_valve_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_valve_input, lv_color_black(), 0);
            lv_textarea_set_accepted_chars(g_formula_valve_input, "0123456789.");
        }
        break;

    default:
        break;
    }
}

/**
 * @brief 取消添加配方按钮回调
 */
static void btn_cancel_add_formula_cb(lv_event_t *e)
{
    (void)e;
    /* 重置编辑模式标志 */
    g_is_editing_formula = false;
    g_editing_formula_index = -1;
    /* 恢复显示配方列表页面 */
    ui_program_create(g_parent_container);
    /* 切换到配方管理标签 */
    lv_obj_set_style_bg_color(g_tab_formula, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(g_tab_program, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_tab_formula, 0, 0); /* 选中时无边框 */
    lv_obj_set_style_border_width(g_tab_program, 1, 0); /* 未选中时有边框 */
    lv_obj_set_style_border_color(g_tab_program, lv_color_hex(0xcccccc), 0);

    /* 更新配方管理标签颜色（选中）：图标和文字都是白色 */
    lv_obj_t *icon_formula = lv_obj_get_child(g_tab_formula, 0);
    lv_obj_t *text_formula = lv_obj_get_child(g_tab_formula, 1);
    if (icon_formula)
    {
        lv_obj_set_style_text_color(icon_formula, lv_color_white(), 0);
    }
    if (text_formula)
    {
        lv_obj_set_style_text_color(text_formula, lv_color_white(), 0);
    }

    /* 更新程序管理标签颜色（未选中）：图标蓝色，文字黑色 */
    lv_obj_t *icon_program = lv_obj_get_child(g_tab_program, 0);
    lv_obj_t *text_program = lv_obj_get_child(g_tab_program, 1);
    if (icon_program)
    {
        lv_obj_set_style_text_color(icon_program, COLOR_PRIMARY, 0);
    }
    if (text_program)
    {
        lv_obj_set_style_text_color(text_program, lv_color_black(), 0);
    }

    /* 显示配方管理区域 */
    if (g_formula_table_area)
    {
        lv_obj_clear_flag(g_formula_table_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_table_area)
    {
        lv_obj_add_flag(g_table_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_btn_add_formula)
    {
        lv_obj_clear_flag(g_btn_add_formula, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_btn_add_program)
    {
        lv_obj_add_flag(g_btn_add_program, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 确认添加配方按钮回调
 */
static void btn_confirm_add_formula_cb(lv_event_t *e)
{
    (void)e;

    /* 获取配方名称输入框 - 它是第2个子对象(第0个是标签) */
    lv_obj_t *input_name = lv_obj_get_child(g_parent_container, 1);
    if (!input_name)
    {
        show_warning_dialog("无法获取配方名称!");
        return;
    }

    const char *name = lv_textarea_get_text(input_name);
    if (!name || strlen(name) == 0)
    {
        show_warning_dialog("配方名称不能为空!");
        return;
    }

    /* 检查配方名是否重复 */
    for (int i = 0; i < g_formula_count; i++)
    {
        /* 如果是编辑模式，跳过自己 */
        if (g_is_editing_formula && i == g_editing_formula_index)
            continue;

        if (strcmp(g_formulas[i].name, name) == 0)
        {
            show_warning_dialog("配方名称已存在,请使用不同的名称!");
            return;
        }
    }

    formula_data_t *formula;

    if (g_is_editing_formula)
    {
        /* 编辑模式：更新现有配方 */
        if (g_editing_formula_index < 0 || g_editing_formula_index >= g_formula_count)
        {
            show_warning_dialog("编辑失败：配方索引无效!");
            return;
        }
        formula = &g_formulas[g_editing_formula_index];
    }
    else
    {
        /* 添加模式：检查是否超过最大配方数 */
        if (g_formula_count >= MAX_FORMULAS)
        {
            show_warning_dialog("超过存储上限，请先删减");
            return;
        }
        formula = &g_formulas[g_formula_count];
        g_formula_count++;
    }

    /* 查找通道表格卡片并统计勾选的通道数 */
    int checked_channels = 0;
    lv_obj_t *channel_card = NULL;
    lv_obj_t *dropdown_method = NULL;

    /* 遍历查找白色卡片和通道表格 */
    uint32_t child_count = lv_obj_get_child_count(g_parent_container);
    for (uint32_t i = 0; i < child_count; i++)
    {
        lv_obj_t *child = lv_obj_get_child(g_parent_container, i);
        /* 查找白色卡片容器 - 大小为 1168 x 660 */
        if (lv_obj_get_width(child) == 1168 && lv_obj_get_height(child) == 660)
        {
            /* 在白色卡片内查找配肥方式下拉框 */
            uint32_t card_child_count = lv_obj_get_child_count(child);
            for (uint32_t j = 0; j < card_child_count; j++)
            {
                lv_obj_t *card_child = lv_obj_get_child(child, j);
                if (lv_obj_check_type(card_child, &lv_dropdown_class))
                {
                    dropdown_method = card_child;
                    break;
                }
            }
        }
        /* 查找通道表格容器 - 大小为 (1128 * 0.6) x 280 */
        if (lv_obj_get_width(child) == (int)(1128 * 0.6) && lv_obj_get_height(child) == 280)
        {
            channel_card = child;
        }
    }

    /* 如果找到通道表格，统计勾选的复选框数量 */
    if (channel_card)
    {
        uint32_t card_child_count = lv_obj_get_child_count(channel_card);
        for (uint32_t i = 0; i < card_child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(channel_card, i);
            if (lv_obj_check_type(child, &lv_checkbox_class))
            {
                if (lv_obj_get_state(child) & LV_STATE_CHECKED)
                {
                    checked_channels++;
                }
            }
        }
    }

    /* 获取配肥方式 */
    int method = 0; /* 默认比例稀释 */
    if (dropdown_method)
    {
        method = lv_dropdown_get_selected(dropdown_method);
    }

    /* 保存配方数据 */
    snprintf(formula->name, sizeof(formula->name), "%s", name);
    formula->method = method;

    /* 获取输入框的值 */
    if (g_formula_dilution_input)
    {
        const char *text = lv_textarea_get_text(g_formula_dilution_input);
        formula->dilution = text ? atoi(text) : 100;
    }
    else
    {
        formula->dilution = 100;
    }

    if (g_formula_ec_input)
    {
        const char *text = lv_textarea_get_text(g_formula_ec_input);
        formula->ec = text ? atof(text) : 1.32f;
    }
    else
    {
        formula->ec = 1.32f;
    }

    if (g_formula_ph_input)
    {
        const char *text = lv_textarea_get_text(g_formula_ph_input);
        formula->ph = text ? atof(text) : 7.6f;
    }
    else
    {
        formula->ph = 7.6f;
    }

    if (g_formula_valve_input)
    {
        const char *text = lv_textarea_get_text(g_formula_valve_input);
        formula->valve_opening = text ? atof(text) : 100.0f;
    }
    else
    {
        formula->valve_opening = 100.0f;
    }

    if (g_formula_stir_input)
    {
        const char *text = lv_textarea_get_text(g_formula_stir_input);
        formula->stir_time = text ? atoi(text) : 30;
    }
    else
    {
        formula->stir_time = 30;
    }

    formula->channel_count = checked_channels; /* 勾选的通道数 */

    /* 保存到NVS */
    {
        int save_idx = g_is_editing_formula ? g_editing_formula_index : (g_formula_count - 1);
        nvs_save_formula(save_idx);
    }

    /* 重置编辑模式标志 */
    g_is_editing_formula = false;
    g_editing_formula_index = -1;

    /* 恢复显示配方列表页面 */
    ui_program_create(g_parent_container);
    /* 切换到配方管理标签 */
    lv_obj_set_style_bg_color(g_tab_formula, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(g_tab_program, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_tab_formula, 0, 0); /* 选中时无边框 */
    lv_obj_set_style_border_width(g_tab_program, 1, 0); /* 未选中时有边框 */
    lv_obj_set_style_border_color(g_tab_program, lv_color_hex(0xcccccc), 0);

    /* 更新配方管理标签颜色（选中）：图标和文字都是白色 */
    lv_obj_t *icon_formula = lv_obj_get_child(g_tab_formula, 0);
    lv_obj_t *text_formula = lv_obj_get_child(g_tab_formula, 1);
    if (icon_formula)
    {
        lv_obj_set_style_text_color(icon_formula, lv_color_white(), 0);
    }
    if (text_formula)
    {
        lv_obj_set_style_text_color(text_formula, lv_color_white(), 0);
    }

    /* 更新程序管理标签颜色（未选中）：图标蓝色，文字黑色 */
    lv_obj_t *icon_program = lv_obj_get_child(g_tab_program, 0);
    lv_obj_t *text_program = lv_obj_get_child(g_tab_program, 1);
    if (icon_program)
    {
        lv_obj_set_style_text_color(icon_program, COLOR_PRIMARY, 0);
    }
    if (text_program)
    {
        lv_obj_set_style_text_color(text_program, lv_color_black(), 0);
    }

    /* 显示配方管理区域 */
    if (g_formula_table_area)
    {
        lv_obj_clear_flag(g_formula_table_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_table_area)
    {
        lv_obj_add_flag(g_table_area, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_btn_add_formula)
    {
        lv_obj_clear_flag(g_btn_add_formula, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_btn_add_program)
    {
        lv_obj_add_flag(g_btn_add_program, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 输入框点击回调 - 显示数字键盘
 */
static void textarea_click_cb(lv_event_t *e)
{
    lv_obj_t *textarea = lv_event_get_target(e);
    ui_main_t *ui_main = ui_get_main();

    if (textarea && ui_main && ui_main->screen)
    {
        ui_numpad_show(textarea, ui_main->screen);
    }
}

/**
 * @brief 程序/配方名称输入框点击回调 - 显示26键中英文键盘
 */
static void name_input_click_cb(lv_event_t *e)
{
    lv_obj_t *textarea = lv_event_get_target(e);
    ui_main_t *ui_main = ui_get_main();

    if (textarea && ui_main && ui_main->screen)
    {
        ui_keyboard_show(textarea, ui_main->screen);
    }
}

/**
 * @brief 选择灌区按钮回调
 */
static void btn_select_zone_cb(lv_event_t *e)
{
    (void)e;
    create_zone_selection_dialog();
}

/**
 * @brief 创建灌区选择对话框
 */
static void create_zone_selection_dialog(void)
{
    ui_main_t *ui_main = ui_get_main();
    if (!ui_main || !ui_main->screen)
        return;

    /* 如果已存在对话框，先删除 */
    if (g_zone_dialog)
    {
        lv_obj_del(g_zone_dialog);
        g_zone_dialog = NULL;
        g_zone_tab_valve = NULL;
        g_zone_tab_zone = NULL;
        g_zone_content = NULL;
    }

    /* 创建对话框背景 - 只覆盖内容区域，不覆盖左侧菜单栏和底部状态栏 */
    g_zone_dialog = lv_obj_create(ui_main->screen);
    lv_obj_set_size(g_zone_dialog, 1178, 735); /* 内容区域大小 */
    lv_obj_set_pos(g_zone_dialog, 97, 5);      /* 内容区域位置 */
    lv_obj_set_style_bg_color(g_zone_dialog, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(g_zone_dialog, 0, 0);
    lv_obj_set_style_radius(g_zone_dialog, 10, 0); /* 圆角与内容区一致 */
    lv_obj_set_style_pad_all(g_zone_dialog, 0, 0);
    lv_obj_clear_flag(g_zone_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶部标签页区域 */
    /* 阀门标签 */
    g_zone_tab_valve = lv_btn_create(g_zone_dialog);
    lv_obj_set_size(g_zone_tab_valve, 150, 50);
    lv_obj_set_pos(g_zone_tab_valve, 10, 10);
    lv_obj_set_style_bg_color(g_zone_tab_valve, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(g_zone_tab_valve, 10, 0);
    lv_obj_set_style_border_width(g_zone_tab_valve, 0, 0);
    lv_obj_add_event_cb(g_zone_tab_valve, tab_valve_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_valve = lv_label_create(g_zone_tab_valve);
    lv_label_set_text(label_valve, "阀门");
    lv_obj_set_style_text_color(label_valve, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_valve, &my_font_cn_16, 0);
    lv_obj_center(label_valve);

    /* 灌区标签 */
    g_zone_tab_zone = lv_btn_create(g_zone_dialog);
    lv_obj_set_size(g_zone_tab_zone, 150, 50);
    lv_obj_set_pos(g_zone_tab_zone, 175, 10);
    lv_obj_set_style_bg_color(g_zone_tab_zone, lv_color_white(), 0);
    lv_obj_set_style_radius(g_zone_tab_zone, 10, 0);
    lv_obj_set_style_border_width(g_zone_tab_zone, 1, 0);
    lv_obj_set_style_border_color(g_zone_tab_zone, lv_color_hex(0xcccccc), 0);
    lv_obj_add_event_cb(g_zone_tab_zone, tab_zone_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_zone = lv_label_create(g_zone_tab_zone);
    lv_label_set_text(label_zone, "灌区");
    lv_obj_set_style_text_color(label_zone, lv_color_black(), 0);
    lv_obj_set_style_text_font(label_zone, &my_font_cn_16, 0);
    lv_obj_center(label_zone);

    /* 内容区域 - 白色卡片 */
    g_zone_content = lv_obj_create(g_zone_dialog);
    lv_obj_set_size(g_zone_content, 1168, 595); /* 高度调整以适应按钮 */
    lv_obj_set_pos(g_zone_content, 5, 70);
    lv_obj_set_style_bg_color(g_zone_content, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_zone_content, 0, 0);
    lv_obj_set_style_radius(g_zone_content, 10, 0);
    lv_obj_set_style_pad_all(g_zone_content, 10, 0);
    lv_obj_clear_flag(g_zone_content, LV_OBJ_FLAG_SCROLLABLE);

    /* 默认显示阀门列表 - 调用阀门标签回调来初始化内容 */
    tab_valve_cb(NULL);

    /* 底部按钮区域 - 在对话框内，距底部10px */
    int btn_y = 735 - 45 - 10; /* 对话框高度 - 按钮高度 - 底部边距 */

    /* 取消选择按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(g_zone_dialog);
    lv_obj_set_size(btn_cancel, 120, 45);
    lv_obj_set_pos(btn_cancel, 1178 - 120 - 135 - 10, btn_y); /* 距右边距留出确认按钮的空间 */
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_radius(btn_cancel, 22, 0);
    lv_obj_add_event_cb(btn_cancel, btn_zone_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消选择");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认选择按钮 */
    lv_obj_t *btn_confirm = lv_btn_create(g_zone_dialog);
    lv_obj_set_size(btn_confirm, 120, 45);
    lv_obj_set_pos(btn_confirm, 1178 - 120 - 10, btn_y); /* 靠右对齐，距右边距10px */
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_confirm, 22, 0);
    lv_obj_add_event_cb(btn_confirm, btn_zone_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认选择");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 取消选择按钮回调
 */
static void btn_zone_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (g_zone_dialog)
    {
        lv_obj_del(g_zone_dialog);
        g_zone_dialog = NULL;
        g_zone_tab_valve = NULL;
        g_zone_tab_zone = NULL;
        g_zone_content = NULL;
    }
}

/**
 * @brief 确认选择按钮回调
 */
static void btn_zone_confirm_cb(lv_event_t *e)
{
    (void)e;
    refresh_irrigation_zone_table();
    if (g_zone_dialog)
    {
        lv_obj_del(g_zone_dialog);
        g_zone_dialog = NULL;
        g_zone_tab_valve = NULL;
        g_zone_tab_zone = NULL;
        g_zone_content = NULL;
    }
}

/**
 * @brief 阀门标签点击回调
 */
static void tab_valve_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_set_style_bg_color(g_zone_tab_valve, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(g_zone_tab_zone, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_zone_tab_valve, 0, 0);
    lv_obj_set_style_border_width(g_zone_tab_zone, 1, 0);

    lv_obj_t *label_valve = lv_obj_get_child(g_zone_tab_valve, 0);
    lv_obj_t *label_zone = lv_obj_get_child(g_zone_tab_zone, 0);
    if (label_valve) {
        lv_obj_set_style_text_color(label_valve, lv_color_white(), 0);
    }
    if (label_zone) {
        lv_obj_set_style_text_color(label_zone, lv_color_black(), 0);
    }

    memset(g_zone_valve_checkboxes, 0, sizeof(g_zone_valve_checkboxes));
    ensure_program_selection_cache_loaded();

    if (g_zone_content)
    {
        lv_obj_clean(g_zone_content);

        lv_obj_t *table_bg = lv_obj_create(g_zone_content);
        lv_obj_set_size(table_bg, 1148, 575);
        lv_obj_set_pos(table_bg, 0, 0);
        lv_obj_set_style_bg_color(table_bg, lv_color_white(), 0);
        lv_obj_set_style_border_width(table_bg, 0, 0);
        lv_obj_set_style_pad_all(table_bg, 0, 0);
        lv_obj_set_style_radius(table_bg, 0, 0);
        lv_obj_clear_flag(table_bg, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *header_bg = lv_obj_create(table_bg);
        lv_obj_set_size(header_bg, 1148, 50);
        lv_obj_set_pos(header_bg, 0, 0);
        lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f0f0), 0);
        lv_obj_set_style_border_width(header_bg, 0, 0);
        lv_obj_set_style_radius(header_bg, 0, 0);
        lv_obj_set_style_pad_all(header_bg, 0, 0);

        const char *headers[] = {"勾选", "序号", "阀门名称", "所属灌区"};
        int header_widths[] = {100, 100, 400, 548};
        int x_pos = 0;

        for (int i = 0; i < 4; i++)
        {
            lv_obj_t *header_label = lv_label_create(header_bg);
            lv_label_set_text(header_label, headers[i]);
            lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
            lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
            lv_obj_set_pos(header_label, x_pos + 20, 17);
            x_pos += header_widths[i];
        }

        if (g_program_cached_valve_count == 0) {
            lv_obj_t *empty_label = lv_label_create(table_bg);
            lv_label_set_text(empty_label, "暂无阀门数据");
            lv_obj_set_style_text_font(empty_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(empty_label, COLOR_TEXT_GRAY, 0);
            lv_obj_set_pos(empty_label, 20, 70);
            return;
        }

        for (int i = 0; i < g_program_cached_valve_count && i < 10; i++) {
            int row_y = 60 + i * 50;
            char zone_name[32];
            find_zone_name_for_valve(g_program_cached_valves[i].id, zone_name, sizeof(zone_name));

            lv_obj_t *checkbox = lv_checkbox_create(table_bg);
            lv_obj_set_pos(checkbox, 30, row_y);
            lv_checkbox_set_text(checkbox, "");
            if (g_temp_selected_valves[i]) {
                lv_obj_add_state(checkbox, LV_STATE_CHECKED);
            }
            lv_obj_add_event_cb(checkbox, zone_selection_checkbox_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)i);
            g_zone_valve_checkboxes[i] = checkbox;

            lv_obj_t *label_no = lv_label_create(table_bg);
            lv_label_set_text_fmt(label_no, "%d", i + 1);
            lv_obj_set_pos(label_no, 120, row_y + 5);
            lv_obj_set_style_text_font(label_no, &my_font_cn_16, 0);

            lv_obj_t *label_name = lv_label_create(table_bg);
            lv_label_set_text(label_name, g_program_cached_valves[i].name[0] ? g_program_cached_valves[i].name : "--");
            lv_obj_set_pos(label_name, 220, row_y + 5);
            lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);

            lv_obj_t *label_zone_name = lv_label_create(table_bg);
            lv_label_set_text(label_zone_name, zone_name);
            lv_obj_set_pos(label_zone_name, 620, row_y + 5);
            lv_obj_set_style_text_font(label_zone_name, &my_font_cn_16, 0);
        }
    }
}

/**
 * @brief 灌区标签点击回调
 */
static void tab_zone_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_set_style_bg_color(g_zone_tab_zone, COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_color(g_zone_tab_valve, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_zone_tab_zone, 0, 0);
    lv_obj_set_style_border_width(g_zone_tab_valve, 1, 0);

    lv_obj_t *label_zone = lv_obj_get_child(g_zone_tab_zone, 0);
    lv_obj_t *label_valve = lv_obj_get_child(g_zone_tab_valve, 0);
    if (label_zone) {
        lv_obj_set_style_text_color(label_zone, lv_color_white(), 0);
    }
    if (label_valve) {
        lv_obj_set_style_text_color(label_valve, lv_color_black(), 0);
    }

    memset(g_zone_zone_checkboxes, 0, sizeof(g_zone_zone_checkboxes));
    ensure_program_selection_cache_loaded();

    if (g_zone_content)
    {
        lv_obj_clean(g_zone_content);

        lv_obj_t *table_bg = lv_obj_create(g_zone_content);
        lv_obj_set_size(table_bg, 1148, 575);
        lv_obj_set_pos(table_bg, 0, 0);
        lv_obj_set_style_bg_color(table_bg, lv_color_white(), 0);
        lv_obj_set_style_border_width(table_bg, 0, 0);
        lv_obj_set_style_pad_all(table_bg, 0, 0);
        lv_obj_set_style_radius(table_bg, 0, 0);
        lv_obj_clear_flag(table_bg, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *header_bg = lv_obj_create(table_bg);
        lv_obj_set_size(header_bg, 1148, 50);
        lv_obj_set_pos(header_bg, 0, 0);
        lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f0f0), 0);
        lv_obj_set_style_border_width(header_bg, 0, 0);
        lv_obj_set_style_radius(header_bg, 0, 0);
        lv_obj_set_style_pad_all(header_bg, 0, 0);

        const char *headers[] = {"勾选", "序号", "灌区名称", "包含阀门"};
        int header_widths[] = {100, 100, 400, 548};
        int x_pos = 0;

        for (int i = 0; i < 4; i++)
        {
            lv_obj_t *header_label = lv_label_create(header_bg);
            lv_label_set_text(header_label, headers[i]);
            lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
            lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
            lv_obj_set_pos(header_label, x_pos + 20, 17);
            x_pos += header_widths[i];
        }

        if (g_program_cached_zone_count == 0) {
            lv_obj_t *empty_label = lv_label_create(table_bg);
            lv_label_set_text(empty_label, "暂无灌区数据");
            lv_obj_set_style_text_font(empty_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(empty_label, COLOR_TEXT_GRAY, 0);
            lv_obj_set_pos(empty_label, 20, 70);
            return;
        }

        for (int i = 0; i < g_program_cached_zone_count && i < 10; i++) {
            int row_y = 60 + i * 50;
            const char *valve_names = g_program_cached_zones[i].valve_names[0] ? g_program_cached_zones[i].valve_names : "--";

            lv_obj_t *checkbox = lv_checkbox_create(table_bg);
            lv_obj_set_pos(checkbox, 30, row_y);
            lv_checkbox_set_text(checkbox, "");
            if (g_temp_selected_zones[i]) {
                lv_obj_add_state(checkbox, LV_STATE_CHECKED);
            }
            lv_obj_add_event_cb(checkbox, zone_selection_checkbox_cb, LV_EVENT_VALUE_CHANGED, (void *)(intptr_t)(0x100 + i));
            g_zone_zone_checkboxes[i] = checkbox;

            lv_obj_t *label_no = lv_label_create(table_bg);
            lv_label_set_text_fmt(label_no, "%d", i + 1);
            lv_obj_set_pos(label_no, 120, row_y + 5);
            lv_obj_set_style_text_font(label_no, &my_font_cn_16, 0);

            lv_obj_t *label_name = lv_label_create(table_bg);
            lv_label_set_text(label_name, g_program_cached_zones[i].name[0] ? g_program_cached_zones[i].name : "--");
            lv_obj_set_pos(label_name, 220, row_y + 5);
            lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);

            lv_obj_t *label_valves = lv_label_create(table_bg);
            lv_label_set_text(label_valves, valve_names);
            lv_obj_set_pos(label_valves, 620, row_y + 5);
            lv_obj_set_style_text_font(label_valves, &my_font_cn_16, 0);
        }
    }
}

/**
 * @brief 统一设置按钮回调
 */
static void btn_uniform_set_cb(lv_event_t *e)
{
    (void)e;

    /* 检查程序数量 */
    if (g_program_count == 0) {
        /* 显示告警弹窗 */
        show_warning_dialog("无法启动，当前灌溉程序数量为0，请先\n添加程序！");
    }
    else {
        /* 显示统一设置对话框 */
        create_uniform_set_dialog();
    }
}

/**
 * @brief 创建统一设置确认对话框
 */
static void create_uniform_set_dialog(void)
{
    /* 如果已存在对话框，先删除 */
    if (g_uniform_dialog)
    {
        lv_obj_del(g_uniform_dialog);
        g_uniform_dialog = NULL;
    }

    /* 创建外层蓝色背景（直角） */
    g_uniform_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_uniform_dialog, 630, 390);
    lv_obj_center(g_uniform_dialog);
    lv_obj_set_style_bg_color(g_uniform_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_uniform_dialog, 0, 0);
    lv_obj_set_style_radius(g_uniform_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_uniform_dialog, 5, 0); /* 5px内边距 */
    lv_obj_clear_flag(g_uniform_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_uniform_dialog);
    lv_obj_set_size(content, 620, 380); /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0); /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题（黑色粗体） */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, "统一修改");
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0); /* 黑色 */
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, "是否统一修改运行时长");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 取消按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 160, 50);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 100, -30);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xa0a0a0), 0); /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, btn_uniform_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消修改");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认按钮（蓝色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 160, 50);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_RIGHT, -100, -30);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0); /* 蓝色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, btn_uniform_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认修改");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 取消统一设置按钮回调
 */
static void btn_uniform_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (g_uniform_dialog)
    {
        lv_obj_del(g_uniform_dialog);
        g_uniform_dialog = NULL;
    }
}

/**
 * @brief 确认统一设置按钮回调
 */
static void btn_uniform_confirm_cb(lv_event_t *e)
{
    (void)e;
    /* TODO: 实现统一设置运行时长的逻辑 */
    if (g_uniform_dialog)
    {
        lv_obj_del(g_uniform_dialog);
        g_uniform_dialog = NULL;
    }
}

/**
 * @brief 灌溉时段复选框状态改变回调
 */
static void period_checkbox_cb(lv_event_t *e)
{
    lv_obj_t *checkbox = lv_event_get_target(e);
    lv_obj_t *time_input = (lv_obj_t *)lv_event_get_user_data(e);

    if (time_input)
    {
        /* 获取时间输入框的父容器 */
        lv_obj_t *time_container = lv_obj_get_parent(time_input);

        if (lv_obj_get_state(checkbox) & LV_STATE_CHECKED)
        {
            /* 复选框被勾选 - 输入框和容器背景变白色 */
            if (time_container)
            {
                lv_obj_set_style_bg_color(time_container, lv_color_white(), 0);
                lv_obj_set_style_bg_opa(time_container, LV_OPA_COVER, 0);
            }
            lv_obj_set_style_bg_color(time_input, lv_color_white(), 0);
            lv_obj_set_style_bg_opa(time_input, LV_OPA_COVER, 0);
            /* 强制刷新样式 */
            lv_obj_invalidate(time_input);
            if (time_container)
            {
                lv_obj_invalidate(time_container);
            }
        }
        else
        {
            /* 复选框未勾选 - 输入框和容器背景变灰色 */
            if (time_container)
            {
                lv_obj_set_style_bg_color(time_container, lv_color_hex(0xf5f5f5), 0);
                lv_obj_set_style_bg_opa(time_container, LV_OPA_COVER, 0);
            }
            lv_obj_set_style_bg_color(time_input, lv_color_hex(0xf5f5f5), 0);
            lv_obj_set_style_bg_opa(time_input, LV_OPA_COVER, 0);
            /* 强制刷新样式 */
            lv_obj_invalidate(time_input);
            if (time_container)
            {
                lv_obj_invalidate(time_container);
            }
        }
    }

    /* 保存灌溉时段的选中状态 */
    /* 从checkbox的user_data获取时段索引 - 这里需要通过checkbox在父容器中的位置来判断 */
    lv_obj_t *parent = lv_obj_get_parent(checkbox);
    if (parent)
    {
        /* 遍历所有时段复选框,找到当前checkbox的索引 */
        int period_index = -1;
        uint32_t child_count = lv_obj_get_child_count(parent);
        int checkbox_count = 0;

        for (uint32_t i = 0; i < child_count; i++)
        {
            lv_obj_t *child = lv_obj_get_child(parent, i);
            if (lv_obj_check_type(child, &lv_checkbox_class))
            {
                if (child == checkbox)
                {
                    period_index = checkbox_count;
                    break;
                }
                checkbox_count++;
            }
        }

        /* 保存到全局变量 */
        if (period_index >= 0 && period_index < MAX_PERIODS)
        {
            g_temp_periods[period_index].enabled = (lv_obj_get_state(checkbox) & LV_STATE_CHECKED) ? true : false;
        }
    }
}

/**
 * @brief 保存当前表单数据到临时变量
 */
static void save_current_form_data(void)
{
    /* 保存程序名称 */
    if (g_input_program_name && lv_obj_is_valid(g_input_program_name))
    {
        const char *name = lv_textarea_get_text(g_input_program_name);
        if (name)
        {
            snprintf(g_current_editing_program.name, sizeof(g_current_editing_program.name), "%s", name);
        }
    }

    /* 保存自动控制复选框状态 */
    if (g_checkbox_auto && lv_obj_is_valid(g_checkbox_auto))
    {
        g_temp_auto_enabled = (lv_obj_get_state(g_checkbox_auto) & LV_STATE_CHECKED) ? true : false;
    }

    /* 保存日期输入框内容 */
    if (g_input_start_date && lv_obj_is_valid(g_input_start_date))
    {
        const char *start = lv_textarea_get_text(g_input_start_date);
        if (start)
        {
            snprintf(g_temp_start_date, sizeof(g_temp_start_date), "%s", start);
        }
    }

    if (g_input_end_date && lv_obj_is_valid(g_input_end_date))
    {
        const char *end = lv_textarea_get_text(g_input_end_date);
        if (end)
        {
            snprintf(g_temp_end_date, sizeof(g_temp_end_date), "%s", end);
        }
    }

    /* 保存肥前清水、肥后清水 */
    if (g_input_pre_water && lv_obj_is_valid(g_input_pre_water))
    {
        const char *pre_text = lv_textarea_get_text(g_input_pre_water);
        if (pre_text)
        {
            g_temp_pre_water = atoi(pre_text);
        }
    }

    if (g_input_post_water && lv_obj_is_valid(g_input_post_water))
    {
        const char *post_text = lv_textarea_get_text(g_input_post_water);
        if (post_text)
        {
            g_temp_post_water = atoi(post_text);
        }
    }

    /* 保存启动条件下拉框 */
    if (g_dropdown_condition && lv_obj_is_valid(g_dropdown_condition))
    {
        char buf[16];
        lv_dropdown_get_selected_str(g_dropdown_condition, buf, sizeof(buf));
        snprintf(g_temp_condition, sizeof(g_temp_condition), "%s", buf);
    }

    /* 保存关联配方下拉框 */
    if (g_dropdown_formula && lv_obj_is_valid(g_dropdown_formula))
    {
        char buf[32];
        lv_dropdown_get_selected_str(g_dropdown_formula, buf, sizeof(buf));
        snprintf(g_temp_formula, sizeof(g_temp_formula), "%s", buf);
    }

    /* 保存灌溉模式下拉框 */
    if (g_dropdown_mode && lv_obj_is_valid(g_dropdown_mode))
    {
        char buf[32];
        lv_dropdown_get_selected_str(g_dropdown_mode, buf, sizeof(buf));
        snprintf(g_temp_mode, sizeof(g_temp_mode), "%s", buf);
    }
}

/**
 * @brief 重置临时表单数据为默认值
 */
static void reset_temp_form_data(void)
{
    /* 重置程序名称 */
    memset(g_current_editing_program.name, 0, sizeof(g_current_editing_program.name));

    /* 重置自动控制复选框状态 */
    g_temp_auto_enabled = false;

    /* 重置日期为默认值 */
    {
        char today_buf[32];
        get_today_str(today_buf, sizeof(today_buf));
        strncpy(g_temp_start_date, today_buf, sizeof(g_temp_start_date) - 1);
        g_temp_start_date[sizeof(g_temp_start_date) - 1] = '\0';
        strncpy(g_temp_end_date, today_buf, sizeof(g_temp_end_date) - 1);
        g_temp_end_date[sizeof(g_temp_end_date) - 1] = '\0';
    }

    /* 重置灌溉时段数据 - 全部默认不勾选 */
    for (int i = 0; i < MAX_PERIODS; i++)
    {
        g_temp_periods[i].enabled = false; /* 默认全部不开启 */
        snprintf(g_temp_periods[i].time, sizeof(g_temp_periods[i].time), "%02d:00:00", 6 + i);
    }

    /* 重置阀门和灌区选择 */
    for (int i = 0; i < 10; i++)
    {
        g_temp_selected_valves[i] = false;
        g_temp_selected_zones[i] = false;
    }

    /* 重置肥前清水、肥后清水 */
    g_temp_pre_water = 0;
    g_temp_post_water = 0;

    /* 重置关联配方、灌溉模式、启动条件 */
    snprintf(g_temp_formula, sizeof(g_temp_formula), "无");
    snprintf(g_temp_mode, sizeof(g_temp_mode), "每天执行");
    snprintf(g_temp_condition, sizeof(g_temp_condition), "定时");

    /* 重置控件指针 */
    g_input_program_name = NULL;
    g_checkbox_auto = NULL;
    g_input_start_date = NULL;
    g_input_end_date = NULL;
    g_input_pre_water = NULL;
    g_input_post_water = NULL;
    g_dropdown_condition = NULL;
    g_dropdown_formula = NULL;
    g_dropdown_mode = NULL;
}

static void ensure_program_selection_cache_loaded(void)
{
    g_program_cached_valve_count = 0;
    memset(g_program_cached_valves, 0, sizeof(g_program_cached_valves));
    if (g_program_valve_count_cb && g_program_valve_list_cb) {
        int total = g_program_valve_count_cb();
        if (total > 10) {
            total = 10;
        }
        if (total > 0) {
            g_program_cached_valve_count = g_program_valve_list_cb(g_program_cached_valves, total, 0);
            if (g_program_cached_valve_count < 0) {
                g_program_cached_valve_count = 0;
            } else if (g_program_cached_valve_count > 10) {
                g_program_cached_valve_count = 10;
            }
        }
    }

    g_program_cached_zone_count = 0;
    memset(g_program_cached_zones, 0, sizeof(g_program_cached_zones));
    if (g_program_zone_count_cb && g_program_zone_list_cb) {
        int total = g_program_zone_count_cb();
        if (total > 10) {
            total = 10;
        }
        if (total > 0) {
            g_program_cached_zone_count = g_program_zone_list_cb(g_program_cached_zones, total, 0);
            if (g_program_cached_zone_count < 0) {
                g_program_cached_zone_count = 0;
            } else if (g_program_cached_zone_count > 10) {
                g_program_cached_zone_count = 10;
            }
        }
    }
}

/**
 * @brief 刷新灌区选择表格数据
 */
static void refresh_irrigation_zone_table(void)
{
    ensure_program_selection_cache_loaded();

    if (!g_zone_table_bg) {
        return;
    }

    uint32_t child_count = lv_obj_get_child_count(g_zone_table_bg);
    for (uint32_t i = child_count; i > 1; i--) {
        lv_obj_t *child = lv_obj_get_child(g_zone_table_bg, i - 1);
        lv_obj_del(child);
    }

    int header_widths[] = {100, 250, 180, 200, 153};
    int row_index = 0;

    for (int i = 0; i < g_program_cached_valve_count && row_index < 10; i++) {
        if (!g_temp_selected_valves[i]) {
            continue;
        }

        int row_y = 60 + row_index * 50;
        int x_pos = 0;

        lv_obj_t *label_no = lv_label_create(g_zone_table_bg);
        lv_label_set_text_fmt(label_no, "%d", row_index + 1);
        lv_obj_set_pos(label_no, x_pos + 30, row_y + 15);
        lv_obj_set_style_text_font(label_no, &my_font_cn_16, 0);
        x_pos += header_widths[0];

        lv_obj_t *label_name = lv_label_create(g_zone_table_bg);
        lv_label_set_text(label_name, g_program_cached_valves[i].name[0] ? g_program_cached_valves[i].name : "--");
        lv_obj_set_pos(label_name, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);
        x_pos += header_widths[1];

        lv_obj_t *label_type = lv_label_create(g_zone_table_bg);
        lv_label_set_text(label_type, "阀门");
        lv_obj_set_pos(label_type, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_type, &my_font_cn_16, 0);
        x_pos += header_widths[2];

        lv_obj_t *label_duration = lv_label_create(g_zone_table_bg);
        lv_label_set_text(label_duration, "--");
        lv_obj_set_pos(label_duration, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_duration, &my_font_cn_16, 0);
        x_pos += header_widths[3];

        lv_obj_t *btn_del = lv_btn_create(g_zone_table_bg);
        lv_obj_set_size(btn_del, 80, 30);
        lv_obj_set_pos(btn_del, x_pos + 20, row_y + 8);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xe74c3c), 0);
        lv_obj_set_style_radius(btn_del, 5, 0);
        lv_obj_add_event_cb(btn_del, btn_delete_selected_target_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *label_del = lv_label_create(btn_del);
        lv_label_set_text(label_del, "删除");
        lv_obj_set_style_text_color(label_del, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_del, &my_font_cn_16, 0);
        lv_obj_center(label_del);

        row_index++;
    }

    for (int i = 0; i < g_program_cached_zone_count && row_index < 10; i++) {
        if (!g_temp_selected_zones[i]) {
            continue;
        }

        int row_y = 60 + row_index * 50;
        int x_pos = 0;

        lv_obj_t *label_no = lv_label_create(g_zone_table_bg);
        lv_label_set_text_fmt(label_no, "%d", row_index + 1);
        lv_obj_set_pos(label_no, x_pos + 30, row_y + 15);
        lv_obj_set_style_text_font(label_no, &my_font_cn_16, 0);
        x_pos += header_widths[0];

        lv_obj_t *label_name = lv_label_create(g_zone_table_bg);
        lv_label_set_text(label_name, g_program_cached_zones[i].name[0] ? g_program_cached_zones[i].name : "--");
        lv_obj_set_pos(label_name, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);
        x_pos += header_widths[1];

        lv_obj_t *label_type = lv_label_create(g_zone_table_bg);
        lv_label_set_text(label_type, "灌区");
        lv_obj_set_pos(label_type, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_type, &my_font_cn_16, 0);
        x_pos += header_widths[2];

        lv_obj_t *label_duration = lv_label_create(g_zone_table_bg);
        lv_label_set_text(label_duration, "--");
        lv_obj_set_pos(label_duration, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_duration, &my_font_cn_16, 0);
        x_pos += header_widths[3];

        lv_obj_t *btn_del = lv_btn_create(g_zone_table_bg);
        lv_obj_set_size(btn_del, 80, 30);
        lv_obj_set_pos(btn_del, x_pos + 20, row_y + 8);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xe74c3c), 0);
        lv_obj_set_style_radius(btn_del, 5, 0);
        lv_obj_add_event_cb(btn_del, btn_delete_selected_target_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(0x100 + i));

        lv_obj_t *label_del = lv_label_create(btn_del);
        lv_label_set_text(label_del, "删除");
        lv_obj_set_style_text_color(label_del, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_del, &my_font_cn_16, 0);
        lv_obj_center(label_del);

        row_index++;
    }
}

static void zone_selection_checkbox_cb(lv_event_t *e)
{
    lv_obj_t *checkbox = lv_event_get_target(e);
    intptr_t raw = (intptr_t)lv_event_get_user_data(e);
    bool is_zone = (raw & 0x100) != 0;
    int index = (int)(raw & 0xFF);
    bool checked = (lv_obj_get_state(checkbox) & LV_STATE_CHECKED) ? true : false;

    if (index < 0 || index >= 10) {
        return;
    }

    if (is_zone) {
        g_temp_selected_zones[index] = checked;
    } else {
        g_temp_selected_valves[index] = checked;
    }
}

static void btn_delete_selected_target_cb(lv_event_t *e)
{
    intptr_t raw = (intptr_t)lv_event_get_user_data(e);
    bool is_zone = (raw & 0x100) != 0;
    int index = (int)(raw & 0xFF);
    show_delete_selected_target_confirm_dialog(is_zone, index);
}

static void show_delete_selected_target_confirm_dialog(bool is_zone, int index)
{
    g_delete_target_is_zone = is_zone;
    g_delete_target_index = index;

    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }

    g_delete_confirm_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_delete_confirm_dialog, 630, 390);
    lv_obj_center(g_delete_confirm_dialog);
    lv_obj_set_style_bg_color(g_delete_confirm_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_delete_confirm_dialog, 0, 0);
    lv_obj_set_style_radius(g_delete_confirm_dialog, 0, 0);
    lv_obj_set_style_pad_all(g_delete_confirm_dialog, 5, 0);
    lv_obj_clear_flag(g_delete_confirm_dialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *content = lv_obj_create(g_delete_confirm_dialog);
    lv_obj_set_size(content, 620, 380);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, is_zone ? "删除灌区" : "删除阀门");
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, is_zone ? "是否从当前程序中删除该灌区" : "是否从当前程序中删除该阀门");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 160, 50);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 100, -30);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, btn_delete_selected_target_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消删除");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 160, 50);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_RIGHT, -100, -30);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xe74c3c), 0);
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, btn_delete_selected_target_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认删除");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
}

static void btn_delete_selected_target_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }
    g_delete_target_is_zone = false;
    g_delete_target_index = -1;
}

static void btn_delete_selected_target_confirm_cb(lv_event_t *e)
{
    (void)e;

    if (g_delete_target_index >= 0 && g_delete_target_index < 10) {
        if (g_delete_target_is_zone) {
            g_temp_selected_zones[g_delete_target_index] = false;
        } else {
            g_temp_selected_valves[g_delete_target_index] = false;
        }
    }

    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }
    g_delete_target_is_zone = false;
    g_delete_target_index = -1;
    refresh_irrigation_zone_table();
}

static void find_zone_name_for_valve(uint16_t valve_id, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return;
    }

    snprintf(buf, buf_size, "--");

    if (!g_program_zone_detail_cb) {
        return;
    }

    for (int i = 0; i < g_program_cached_zone_count; i++) {
        ui_zone_add_params_t detail = {0};
        if (!g_program_zone_detail_cb(g_program_cached_zones[i].slot_index, &detail)) {
            continue;
        }
        for (int j = 0; j < detail.valve_count; j++) {
            if (detail.valve_ids[j] == valve_id) {
                snprintf(buf, buf_size, "%s", g_program_cached_zones[i].name[0] ? g_program_cached_zones[i].name : "--");
                return;
            }
        }
    }
}

/**
 * @brief 刷新程序管理表格数据
 */
static void refresh_program_table(void)
{
    reload_programs_from_backend();

    /* 删除所有现有数据行（保留表头，表头是第一个子对象） */
    uint32_t child_count = lv_obj_get_child_count(g_table_bg);
    for (uint32_t i = child_count; i > 1; i--)
    {
        lv_obj_t *child = lv_obj_get_child(g_table_bg, i - 1);
        lv_obj_del(child);
    }

    /* 列宽度定义 */
    int header_widths[] = {80, 120, 180, 180, 120, 150, 150, 166};

    /* 添加每个程序的数据行 */
    for (int i = 0; i < g_program_count; i++)
    {
        program_data_t *prog = &g_programs[i];
        int row_y = 60 + i * 50; /* 表头50px，每行50px */
        int x_pos = 0;

        /* 序号 */
        lv_obj_t *label_no = lv_label_create(g_table_bg);
        lv_label_set_text_fmt(label_no, "%d", i + 1);
        lv_obj_set_pos(label_no, x_pos + 30, row_y + 15);
        lv_obj_set_style_text_font(label_no, &my_font_cn_16, 0);
        x_pos += header_widths[0];

        /* 启用自动 - 复选框(只读,不可修改) */
        lv_obj_t *checkbox = lv_checkbox_create(g_table_bg);
        lv_checkbox_set_text(checkbox, "");
        lv_obj_set_pos(checkbox, x_pos + 40, row_y + 10);
        if (prog->auto_enabled)
        {
            lv_obj_add_state(checkbox, LV_STATE_CHECKED);
        }
        /* 禁用复选框,使其不可点击修改 */
        lv_obj_add_state(checkbox, LV_STATE_DISABLED);
        lv_obj_clear_flag(checkbox, LV_OBJ_FLAG_CLICKABLE);
        x_pos += header_widths[1];

        /* 程序名称 */
        lv_obj_t *label_name = lv_label_create(g_table_bg);
        lv_label_set_text(label_name, prog->name);
        lv_obj_set_pos(label_name, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);
        x_pos += header_widths[2];

        /* 下次启动时段 */
        lv_obj_t *label_next = lv_label_create(g_table_bg);
        lv_label_set_text(label_next, prog->next_start);
        lv_obj_set_pos(label_next, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_next, &my_font_cn_16, 0);
        x_pos += header_widths[3];

        /* 合计时长 */
        lv_obj_t *label_duration = lv_label_create(g_table_bg);
        if (prog->total_duration > 0)
        {
            lv_label_set_text_fmt(label_duration, "%d分", prog->total_duration);
        }
        else
        {
            lv_label_set_text(label_duration, "--");
        }
        lv_obj_set_pos(label_duration, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_duration, &my_font_cn_16, 0);
        x_pos += header_widths[4];

        /* 关联配方 */
        lv_obj_t *label_formula = lv_label_create(g_table_bg);
        lv_label_set_text(label_formula, prog->formula);
        lv_obj_set_pos(label_formula, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_formula, &my_font_cn_16, 0);
        x_pos += header_widths[5];

        /* 启动条件 */
        lv_obj_t *label_condition = lv_label_create(g_table_bg);
        lv_label_set_text(label_condition, prog->condition);
        lv_obj_set_pos(label_condition, x_pos + 10, row_y + 15);
        lv_obj_set_style_text_font(label_condition, &my_font_cn_16, 0);
        x_pos += header_widths[6];

        /* 操作按钮 */
        /* 编辑按钮 */
        lv_obj_t *btn_edit = lv_btn_create(g_table_bg);
        lv_obj_set_size(btn_edit, 60, 30);
        lv_obj_set_pos(btn_edit, x_pos + 5, row_y + 8);
        lv_obj_set_style_bg_color(btn_edit, COLOR_PRIMARY, 0);
        lv_obj_set_style_radius(btn_edit, 5, 0);
        lv_obj_add_event_cb(btn_edit, btn_edit_program_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *label_edit = lv_label_create(btn_edit);
        lv_label_set_text(label_edit, "编辑");
        lv_obj_set_style_text_color(label_edit, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_edit, &my_font_cn_16, 0);
        lv_obj_center(label_edit);

        /* 删除按钮 */
        lv_obj_t *btn_del = lv_btn_create(g_table_bg);
        lv_obj_set_size(btn_del, 60, 30);
        lv_obj_set_pos(btn_del, x_pos + 70, row_y + 8);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xe74c3c), 0); /* 红色 */
        lv_obj_set_style_radius(btn_del, 5, 0);
        lv_obj_add_event_cb(btn_del, btn_delete_program_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *label_del = lv_label_create(btn_del);
        lv_label_set_text(label_del, "删除");
        lv_obj_set_style_text_color(label_del, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_del, &my_font_cn_16, 0);
        lv_obj_center(label_del);
    }
}

/**
 * @brief 编辑程序按钮回调
 */
static void btn_edit_program_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    if (index < 0 || index >= g_program_count)
        return;

    /* 设置为编辑模式 */
    g_is_editing_program = true;
    g_editing_program_index = index;

    /* 加载程序数据到临时存储 */
    g_temp_program = g_programs[index];

    /* 显示添加程序对话框（会根据编辑模式调整界面） */
    create_add_program_dialog();
}

/**
 * @brief 删除程序按钮回调
 */
static void btn_delete_program_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    if (index < 0 || index >= g_program_count)
        return;

    /* 显示删除确认对话框 */
    show_delete_program_confirm_dialog(index);
}

/**
 * @brief 显示警告对话框
 * @param message 警告消息文本
 */
static void show_warning_dialog(const char *message)
{
    /* 如果对话框已存在,先删除 */
    if (g_warning_dialog != NULL)
    {
        lv_obj_del(g_warning_dialog);
        g_warning_dialog = NULL;
    }

    /* 创建模态背景层 */
    lv_obj_t *bg = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bg, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(bg, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(bg, LV_OPA_50, 0);
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建警告对话框 */
    g_warning_dialog = lv_obj_create(bg);
    lv_obj_set_size(g_warning_dialog, 500, 250);
    lv_obj_center(g_warning_dialog);
    lv_obj_set_style_bg_color(g_warning_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_warning_dialog, 3, 0);
    lv_obj_set_style_border_color(g_warning_dialog, COLOR_WARNING, 0); /* 橙色边框 */
    lv_obj_set_style_radius(g_warning_dialog, 10, 0);
    lv_obj_set_style_pad_all(g_warning_dialog, 20, 0);
    lv_obj_clear_flag(g_warning_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 警告图标和标题 */
    lv_obj_t *label_title = lv_label_create(g_warning_dialog);
    lv_label_set_text(label_title, LV_SYMBOL_WARNING " 警告");
    lv_obj_set_pos(label_title, 0, 10);
    lv_obj_set_style_text_font(label_title, &my_font_cn_16, 0); /* 使用内置字体显示符号 */
    lv_obj_set_style_text_color(label_title, COLOR_WARNING, 0);

    /* 警告消息 */
    lv_obj_t *label_msg = lv_label_create(g_warning_dialog);
    lv_label_set_text(label_msg, message);
    lv_obj_set_pos(label_msg, 0, 80);
    lv_obj_set_style_text_font(label_msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_msg, COLOR_TEXT_MAIN, 0);
    lv_obj_set_width(label_msg, 460);
    lv_label_set_long_mode(label_msg, LV_LABEL_LONG_WRAP);

    /* 确定按钮 */
    lv_obj_t *btn_ok = lv_btn_create(g_warning_dialog);
    lv_obj_set_size(btn_ok, 120, 45);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(btn_ok, COLOR_WARNING, 0);
    lv_obj_set_style_radius(btn_ok, 20, 0);
    lv_obj_add_event_cb(btn_ok, btn_warning_close_cb, LV_EVENT_CLICKED, bg);

    lv_obj_t *label_ok = lv_label_create(btn_ok);
    lv_label_set_text(label_ok, "确定");
    lv_obj_set_style_text_color(label_ok, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_ok, &my_fontbd_16, 0);
    lv_obj_center(label_ok);
}

/**
 * @brief 警告对话框关闭按钮回调
 */
static void btn_warning_close_cb(lv_event_t *e)
{
    lv_obj_t *bg = (lv_obj_t *)lv_event_get_user_data(e);

    /* 删除对话框 */
    if (g_warning_dialog != NULL)
    {
        g_warning_dialog = NULL;
    }

    /* 删除背景层(会自动删除子对象) */
    if (bg)
    {
        lv_obj_del(bg);
    }
}

/**
 * @brief 刷新配方管理表格数据
 */
static void refresh_formula_table(void)
{
    if (!g_formula_table_bg)
        return;

    /* 删除所有现有数据行（保留表头，表头是第一个子对象） */
    uint32_t child_count = lv_obj_get_child_count(g_formula_table_bg);
    for (uint32_t i = child_count; i > 1; i--)
    {
        lv_obj_t *child = lv_obj_get_child(g_formula_table_bg, i - 1);
        lv_obj_del(child);
    }

    /* 列宽度定义 */
    int header_widths[] = {100, 250, 558, 240};  /* 与表头宽度保持一致 */

    /* 添加每个配方的数据行 */
    for (int i = 0; i < g_formula_count; i++)
    {
        formula_data_t *formula = &g_formulas[i];
        int row_y = 60 + i * 50; /* 表头50px，每行50px */
        int x_pos = 0;

        /* 序号 */
        lv_obj_t *label_no = lv_label_create(g_formula_table_bg);
        lv_label_set_text_fmt(label_no, "%d", i + 1);
        lv_obj_set_pos(label_no, x_pos + 30, row_y + 15);
        lv_obj_set_style_text_font(label_no, &my_font_cn_16, 0);
        x_pos += header_widths[0];

        /* 配方名称 */
        lv_obj_t *label_name = lv_label_create(g_formula_table_bg);
        lv_label_set_text(label_name, formula->name);
        lv_obj_set_pos(label_name, x_pos + 20, row_y + 15);
        lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);
        x_pos += header_widths[1];

        /* 配方详情 */
        lv_obj_t *label_detail = lv_label_create(g_formula_table_bg);
        char detail_text[256];

        /* 根据配肥方式显示不同的详情 */
        const char *method_names[] = {"比例稀释", "EC调配", "固定流速"};
        const char *method_name = (formula->method >= 0 && formula->method <= 2) ? method_names[formula->method] : "未知";

        switch (formula->method)
        {
        case 0: /* 比例稀释 */
            snprintf(detail_text, sizeof(detail_text),
                     "%s, 稀释倍数: %d, 通道: %d",
                     method_name, formula->dilution, formula->channel_count);
            break;
        case 1: /* EC调配 */
            snprintf(detail_text, sizeof(detail_text),
                     "%s, EC: %.2f ms/cm, 通道: %d",
                     method_name, formula->ec, formula->channel_count);
            break;
        case 2: /* 固定流速 */
            snprintf(detail_text, sizeof(detail_text),
                     "%s, 阀门开度: %.1f%%, 通道: %d",
                     method_name, formula->valve_opening, formula->channel_count);
            break;
        default:
            snprintf(detail_text, sizeof(detail_text),
                     "%s, 通道: %d",
                     method_name, formula->channel_count);
            break;
        }

        lv_label_set_text(label_detail, detail_text);
        lv_obj_set_pos(label_detail, x_pos + 20, row_y + 15);
        lv_obj_set_style_text_font(label_detail, &my_font_cn_16, 0);
        x_pos += header_widths[2];

        /* 操作按钮容器 */
        /* 编辑按钮 */
        lv_obj_t *btn_edit = lv_btn_create(g_formula_table_bg);
        lv_obj_set_size(btn_edit, 80, 30);
        lv_obj_set_pos(btn_edit, x_pos + 20, row_y + 8);
        lv_obj_set_style_bg_color(btn_edit, COLOR_PRIMARY, 0);
        lv_obj_set_style_radius(btn_edit, 5, 0);
        lv_obj_add_event_cb(btn_edit, btn_edit_formula_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *label_edit = lv_label_create(btn_edit);
        lv_label_set_text(label_edit, "编辑");
        lv_obj_set_style_text_color(label_edit, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_edit, &my_font_cn_16, 0);
        lv_obj_center(label_edit);

        /* 删除按钮 */
        lv_obj_t *btn_del = lv_btn_create(g_formula_table_bg);
        lv_obj_set_size(btn_del, 80, 30);
        lv_obj_set_pos(btn_del, x_pos + 115, row_y + 8);
        lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xe74c3c), 0); /* 红色 */
        lv_obj_set_style_radius(btn_del, 5, 0);
        lv_obj_add_event_cb(btn_del, btn_delete_formula_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        lv_obj_t *label_del = lv_label_create(btn_del);
        lv_label_set_text(label_del, "删除");
        lv_obj_set_style_text_color(label_del, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_del, &my_font_cn_16, 0);
        lv_obj_center(label_del);
    }
}

/**
 * @brief 编辑配方按钮回调
 */
static void btn_edit_formula_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    if (index < 0 || index >= g_formula_count)
        return;

    /* 设置为编辑模式 */
    g_is_editing_formula = true;
    g_editing_formula_index = index;

    /* 加载配方数据到临时存储 */
    g_temp_formula_data = g_formulas[index];

    /* 显示添加配方对话框（会根据编辑模式调整界面） */
    create_add_formula_dialog();
}

/**
 * @brief 配肥方式下拉框变化回调
 */
static void dropdown_method_change_cb(lv_event_t *e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);

    /* 先恢复所有输入框为默认值（除了配肥方式和通道号） */
    if (g_formula_dilution_input)
    {
        lv_textarea_set_text(g_formula_dilution_input, "100");
    }
    if (g_formula_ec_input)
    {
        lv_textarea_set_text(g_formula_ec_input, "1.32");
    }
    if (g_formula_ph_input)
    {
        lv_textarea_set_text(g_formula_ph_input, "7.6");
    }
    if (g_formula_valve_input)
    {
        lv_textarea_set_text(g_formula_valve_input, "100.0");
    }
    if (g_formula_stir_input)
    {
        lv_textarea_set_text(g_formula_stir_input, "30");
    }

    /* 根据选择的配肥方式，启用/禁用对应的输入框 */
    switch (selected)
    {
    case 0: /* 比例稀释 */
        /* 稀释倍数可编辑，EC、PH和阀门固定开度禁用 */
        if (g_formula_dilution_input)
        {
            lv_obj_clear_state(g_formula_dilution_input, LV_STATE_DISABLED);
            lv_obj_add_flag(g_formula_dilution_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_dilution_input, lv_color_black(), 0);
            lv_textarea_set_accepted_chars(g_formula_dilution_input, "0123456789");
        }
        if (g_formula_ec_input)
        {
            lv_obj_add_state(g_formula_ec_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ec_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ec_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ec_input, "");
        }
        if (g_formula_ph_input)
        {
            lv_obj_add_state(g_formula_ph_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ph_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ph_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ph_input, "");
        }
        if (g_formula_valve_input)
        {
            lv_obj_add_state(g_formula_valve_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_valve_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_valve_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_valve_input, "");
        }
        break;

    case 1: /* EC调配 */
        /* 只有EC可编辑，稀释倍数、PH和阀门固定开度禁用 */
        if (g_formula_dilution_input)
        {
            lv_obj_add_state(g_formula_dilution_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_dilution_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_dilution_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_dilution_input, "");
        }
        if (g_formula_ec_input)
        {
            lv_obj_clear_state(g_formula_ec_input, LV_STATE_DISABLED);
            lv_obj_add_flag(g_formula_ec_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ec_input, lv_color_black(), 0);
            lv_textarea_set_accepted_chars(g_formula_ec_input, "0123456789.");
        }
        if (g_formula_ph_input)
        {
            lv_obj_add_state(g_formula_ph_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ph_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ph_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ph_input, "");
        }
        if (g_formula_valve_input)
        {
            lv_obj_add_state(g_formula_valve_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_valve_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_valve_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_valve_input, "");
        }
        break;

    case 2: /* 固定流速 */
        /* 稀释倍数、EC和PH禁用，阀门固定开度可编辑 */
        if (g_formula_dilution_input)
        {
            lv_obj_add_state(g_formula_dilution_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_dilution_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_dilution_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_dilution_input, "");
        }
        if (g_formula_ec_input)
        {
            lv_obj_add_state(g_formula_ec_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ec_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ec_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ec_input, "");
        }
        if (g_formula_ph_input)
        {
            lv_obj_add_state(g_formula_ph_input, LV_STATE_DISABLED);
            lv_obj_clear_flag(g_formula_ph_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_ph_input, lv_color_hex(0xa0a0a0), 0);
            lv_textarea_set_accepted_chars(g_formula_ph_input, "");
        }
        if (g_formula_valve_input)
        {
            lv_obj_clear_state(g_formula_valve_input, LV_STATE_DISABLED);
            lv_obj_add_flag(g_formula_valve_input, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_text_color(g_formula_valve_input, lv_color_black(), 0);
            lv_textarea_set_accepted_chars(g_formula_valve_input, "0123456789.");
        }
        break;

    default:
        break;
    }
}

/**
 * @brief 删除配方按钮回调
 */
static void btn_delete_formula_cb(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);

    if (index < 0 || index >= g_formula_count)
        return;

    /* 显示删除确认对话框 */
    show_delete_formula_confirm_dialog(index);
}

/**
 * @brief 显示删除配方确认对话框
 * @param index 要删除的配方索引
 */
static void show_delete_formula_confirm_dialog(int index)
{
    /* 保存要删除的配方索引 */
    g_delete_formula_index = index;

    /* 如果对话框已存在，先删除 */
    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }

    /* 创建外层蓝色背景（直角） */
    g_delete_confirm_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_delete_confirm_dialog, 630, 390);
    lv_obj_center(g_delete_confirm_dialog);
    lv_obj_set_style_bg_color(g_delete_confirm_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_delete_confirm_dialog, 0, 0);
    lv_obj_set_style_radius(g_delete_confirm_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_delete_confirm_dialog, 5, 0); /* 5px内边距 */
    lv_obj_clear_flag(g_delete_confirm_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_delete_confirm_dialog);
    lv_obj_set_size(content, 620, 380); /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0); /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题（黑色粗体） */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, "删除配方");
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0); /* 黑色 */
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, "是否永久删除该配方");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 取消删除按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 160, 50);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 100, -30);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0); /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, btn_delete_formula_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消删除");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认删除按钮（红色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 160, 50);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_RIGHT, -100, -30);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xe74c3c), 0); /* 红色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, btn_delete_formula_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认删除");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 删除配方取消按钮回调
 */
static void btn_delete_formula_cancel_cb(lv_event_t *e)
{
    (void)e;
    /* 关闭对话框 */
    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }
    g_delete_formula_index = -1;
}

/**
 * @brief 删除配方确认按钮回调
 */
static void btn_delete_formula_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* 检查索引是否有效 */
    if (g_delete_formula_index < 0 || g_delete_formula_index >= g_formula_count)
    {
        /* 关闭对话框 */
        if (g_delete_confirm_dialog != NULL)
        {
            lv_obj_del(g_delete_confirm_dialog);
            g_delete_confirm_dialog = NULL;
        }
        g_delete_formula_index = -1;
        return;
    }

    /* 删除配方 - 将后面的配方向前移动 */
    for (int i = g_delete_formula_index; i < g_formula_count - 1; i++)
    {
        g_formulas[i] = g_formulas[i + 1];
    }
    g_formula_count--;
    nvs_save_all_formulas();

    /* 关闭对话框 */
    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }
    g_delete_formula_index = -1;

    /* 刷新表格显示 */
    refresh_formula_table();
}

/**
 * @brief 显示删除程序确认对话框
 * @param index 要删除的程序索引
 */
static void show_delete_program_confirm_dialog(int index)
{
    /* 保存要删除的程序索引 */
    g_delete_program_index = index;

    /* 如果对话框已存在，先删除 */
    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }

    /* 创建外层蓝色背景（直角） */
    g_delete_confirm_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_delete_confirm_dialog, 630, 390);
    lv_obj_center(g_delete_confirm_dialog);
    lv_obj_set_style_bg_color(g_delete_confirm_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_delete_confirm_dialog, 0, 0);
    lv_obj_set_style_radius(g_delete_confirm_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_delete_confirm_dialog, 5, 0); /* 5px内边距 */
    lv_obj_clear_flag(g_delete_confirm_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_delete_confirm_dialog);
    lv_obj_set_size(content, 620, 380); /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0); /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题（黑色粗体） */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, "删除程序");
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0); /* 黑色 */
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, "是否永久删除该程序");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 取消删除按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 160, 50);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 100, -30);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0); /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, btn_delete_program_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消删除");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认删除按钮（红色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 160, 50);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_RIGHT, -100, -30);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xe74c3c), 0); /* 红色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, btn_delete_program_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认删除");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 删除程序取消按钮回调
 */
static void btn_delete_program_cancel_cb(lv_event_t *e)
{
    (void)e;
    /* 关闭对话框 */
    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }
    g_delete_program_index = -1;
}

/**
 * @brief 删除程序确认按钮回调
 */
static void btn_delete_program_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* 检查索引是否有效 */
    if (g_delete_program_index < 0 || g_delete_program_index >= g_program_count)
    {
        /* 关闭对话框 */
        if (g_delete_confirm_dialog != NULL)
        {
            lv_obj_del(g_delete_confirm_dialog);
            g_delete_confirm_dialog = NULL;
        }
        g_delete_program_index = -1;
        return;
    }

    /* 删除程序 - 将后面的程序向前移动 */
    for (int i = g_delete_program_index; i < g_program_count - 1; i++)
    {
        g_programs[i] = g_programs[i + 1];
    }
    g_program_count--;
    nvs_save_all_programs();

    /* 关闭对话框 */
    if (g_delete_confirm_dialog != NULL)
    {
        lv_obj_del(g_delete_confirm_dialog);
        g_delete_confirm_dialog = NULL;
    }
    g_delete_program_index = -1;

    /* 刷新表格显示 */
    refresh_program_table();
}

/**
 * @brief 获取程序数量
 * @return 程序数量
 */
int ui_program_get_count(void)
{
    return g_program_count;
}

/**
 * @brief 获取指定索引的程序名称
 * @param index 程序索引
 * @return 程序名称指针（如果索引无效返回NULL）
 */
const char* ui_program_get_name(int index)
{
    if (index < 0 || index >= g_program_count)
        return NULL;
    return g_programs[index].name;
}

/**
 * @brief 获取指定索引的程序合计时长
 * @param index 程序索引
 * @return 合计时长（分钟）
 */
int ui_program_get_duration(int index)
{
    if (index < 0 || index >= g_program_count)
        return 0;
    return program_get_display_duration(&g_programs[index]);
}

/**
 * @brief 获取指定索引的程序启动时段摘要
 * @param index 程序索引
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void ui_program_get_period_text(int index, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return;
    }

    if (index < 0 || index >= g_program_count) {
        snprintf(buf, buf_size, "--");
        return;
    }

    format_program_period_text(&g_programs[index], buf, buf_size);
}

/**
 * @brief 获取指定索引的程序关联配方
 * @param index 程序索引
 * @return 关联配方名称指针（如果索引无效返回NULL）
 */
const char* ui_program_get_formula(int index)
{
    if (index < 0 || index >= g_program_count)
        return NULL;
    return g_programs[index].formula;
}

/**
 * @brief 获取指定索引的程序启动条件文本
 * @param index 程序索引
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void ui_program_get_condition_text(int index, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return;
    }

    if (index < 0 || index >= g_program_count) {
        snprintf(buf, buf_size, "--");
        return;
    }

    snprintf(buf, buf_size, "%s", g_programs[index].condition[0] ? g_programs[index].condition : "--");
}

/**
 * @brief 获取指定索引的程序下次启动文本
 * @param index 程序索引
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void ui_program_get_next_start_text(int index, char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) {
        return;
    }

    if (index < 0 || index >= g_program_count) {
        snprintf(buf, buf_size, "--");
        return;
    }

    snprintf(buf, buf_size, "%s", g_programs[index].next_start[0] ? g_programs[index].next_start : "--");
}
