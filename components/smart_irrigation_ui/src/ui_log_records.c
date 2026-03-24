/**
 * @file ui_log_records.c
 * @brief 日志页面 - 控制日志/操作日志/手灌记录/程序记录 表格渲染与分页逻辑
 */

#include "ui_log_records.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define ROW_HEIGHT    50
#define TABLE_START_Y 0
#define TIME_SYNC_THRESHOLD 1704067200LL

typedef struct {
    ui_log_rec_type_t type;
    lv_obj_t *input_start;
    lv_obj_t *input_end;
    lv_obj_t *status_dropdown;
    lv_obj_t *table_area;
    lv_obj_t *page_info;
    lv_obj_t *btn_first;
    lv_obj_t *btn_prev;
    lv_obj_t *btn_next;
    lv_obj_t *btn_last;
    uint16_t current_page;
    bool valid;
} log_tab_ctx_t;

static ui_log_rec_query_fn s_query_fn = NULL;
static log_tab_ctx_t s_control_ctx;
static log_tab_ctx_t s_operation_ctx;
static log_tab_ctx_t s_manual_ctx;
static log_tab_ctx_t s_program_ctx;

static int64_t date_str_to_epoch(const char *date_str, bool is_end);
static void render_page(log_tab_ctx_t *ctx);
static void page_first_cb(lv_event_t *e);
static void page_prev_cb(lv_event_t *e);
static void page_next_cb(lv_event_t *e);
static void page_last_cb(lv_event_t *e);
static ui_log_rec_status_filter_t get_status_filter(const log_tab_ctx_t *ctx);
static const char *get_empty_message(ui_log_rec_type_t type);
static void format_timestamp(int64_t timestamp, char *buf, size_t buf_size);
static void format_minutes(uint16_t minutes, char *buf, size_t buf_size);
static void add_text_cell(lv_obj_t *parent, const char *text, int x, int y, int width);
static void add_row_background(lv_obj_t *parent, int y);
static void setup_ctx(log_tab_ctx_t *ctx, ui_log_rec_type_t type,
    lv_obj_t *input_start, lv_obj_t *input_end, lv_obj_t *status_dropdown,
    lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);

void ui_log_rec_register_query_cb(ui_log_rec_query_fn fn)
{
    s_query_fn = fn;
}

void ui_log_rec_setup_control(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    setup_ctx(&s_control_ctx, UI_LOG_REC_CONTROL,
        input_start, input_end, NULL,
        table_area, page_info,
        btn_first, btn_prev, btn_next, btn_last);
}

void ui_log_rec_setup_operation(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    setup_ctx(&s_operation_ctx, UI_LOG_REC_OPERATION,
        input_start, input_end, NULL,
        table_area, page_info,
        btn_first, btn_prev, btn_next, btn_last);
}

void ui_log_rec_setup_manual(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *status_dropdown, lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    setup_ctx(&s_manual_ctx, UI_LOG_REC_MANUAL,
        input_start, input_end, status_dropdown,
        table_area, page_info,
        btn_first, btn_prev, btn_next, btn_last);
}

void ui_log_rec_setup_program(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *status_dropdown, lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    setup_ctx(&s_program_ctx, UI_LOG_REC_PROGRAM,
        input_start, input_end, status_dropdown,
        table_area, page_info,
        btn_first, btn_prev, btn_next, btn_last);
}

void ui_log_rec_control_query_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_control_ctx.valid) return;
    s_control_ctx.current_page = 0;
    render_page(&s_control_ctx);
}

void ui_log_rec_operation_query_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_operation_ctx.valid) return;
    s_operation_ctx.current_page = 0;
    render_page(&s_operation_ctx);
}

void ui_log_rec_manual_query_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_manual_ctx.valid) return;
    s_manual_ctx.current_page = 0;
    render_page(&s_manual_ctx);
}

void ui_log_rec_program_query_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_program_ctx.valid) return;
    s_program_ctx.current_page = 0;
    render_page(&s_program_ctx);
}

void ui_log_rec_invalidate(void)
{
    s_control_ctx.valid = false;
    s_control_ctx.table_area = NULL;
    s_control_ctx.page_info = NULL;

    s_operation_ctx.valid = false;
    s_operation_ctx.table_area = NULL;
    s_operation_ctx.page_info = NULL;

    s_manual_ctx.valid = false;
    s_manual_ctx.table_area = NULL;
    s_manual_ctx.page_info = NULL;

    s_program_ctx.valid = false;
    s_program_ctx.table_area = NULL;
    s_program_ctx.page_info = NULL;
}

void ui_log_rec_refresh_visible(void)
{
    if (s_control_ctx.valid && s_control_ctx.table_area) {
        render_page(&s_control_ctx);
    }
    if (s_operation_ctx.valid && s_operation_ctx.table_area) {
        render_page(&s_operation_ctx);
    }
    if (s_manual_ctx.valid && s_manual_ctx.table_area) {
        render_page(&s_manual_ctx);
    }
    if (s_program_ctx.valid && s_program_ctx.table_area) {
        render_page(&s_program_ctx);
    }
}

static void setup_ctx(log_tab_ctx_t *ctx, ui_log_rec_type_t type,
    lv_obj_t *input_start, lv_obj_t *input_end, lv_obj_t *status_dropdown,
    lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    ctx->type = type;
    ctx->input_start = input_start;
    ctx->input_end = input_end;
    ctx->status_dropdown = status_dropdown;
    ctx->table_area = table_area;
    ctx->page_info = page_info;
    ctx->btn_first = btn_first;
    ctx->btn_prev = btn_prev;
    ctx->btn_next = btn_next;
    ctx->btn_last = btn_last;
    ctx->current_page = 0;
    ctx->valid = true;

    lv_obj_add_event_cb(btn_first, page_first_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn_prev, page_prev_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn_next, page_next_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn_last, page_last_cb, LV_EVENT_CLICKED, ctx);
}

static int64_t date_str_to_epoch(const char *date_str, bool is_end)
{
    if (!date_str || strlen(date_str) < 10) return 0;

    int year, month, day;
    if (sscanf(date_str, "%d-%d-%d", &year, &month, &day) != 3) return 0;

    struct tm tm_val = {0};
    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month - 1;
    tm_val.tm_mday = day;

    if (is_end) {
        tm_val.tm_hour = 23;
        tm_val.tm_min = 59;
        tm_val.tm_sec = 59;
    }

    return (int64_t)mktime(&tm_val);
}

static ui_log_rec_status_filter_t get_status_filter(const log_tab_ctx_t *ctx)
{
    if (!ctx || !ctx->status_dropdown) {
        return UI_LOG_REC_STATUS_ALL;
    }

    uint16_t selected = lv_dropdown_get_selected(ctx->status_dropdown);
    if (selected == 1) {
        return UI_LOG_REC_STATUS_NORMAL;
    }
    if (selected == 2) {
        return UI_LOG_REC_STATUS_ABNORMAL;
    }
    return UI_LOG_REC_STATUS_ALL;
}

static const char *get_empty_message(ui_log_rec_type_t type)
{
    switch (type) {
        case UI_LOG_REC_CONTROL:
            return "暂无控制日志";
        case UI_LOG_REC_OPERATION:
            return "暂无操作日志";
        case UI_LOG_REC_MANUAL:
            return "暂无手灌记录";
        case UI_LOG_REC_PROGRAM:
            return "暂无程序记录";
        default:
            return "暂无记录";
    }
}

static void format_timestamp(int64_t timestamp, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
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

static void format_minutes(uint16_t minutes, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) {
        return;
    }
    snprintf(buf, buf_size, "%u分钟", (unsigned)minutes);
}

static void add_row_background(lv_obj_t *parent, int y)
{
    lv_obj_t *row_bg = lv_obj_create(parent);
    lv_obj_set_size(row_bg, 1138, ROW_HEIGHT);
    lv_obj_set_pos(row_bg, 0, y);
    lv_obj_set_style_bg_color(row_bg, lv_color_hex(0xf8f8f8), 0);
    lv_obj_set_style_border_width(row_bg, 0, 0);
    lv_obj_set_style_radius(row_bg, 0, 0);
    lv_obj_set_style_pad_all(row_bg, 0, 0);
    lv_obj_clear_flag(row_bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

static void add_text_cell(lv_obj_t *parent, const char *text, int x, int y, int width)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT_MAIN, 0);
    lv_obj_set_size(label, width, ROW_HEIGHT);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_top(label, 17, 0);
}

static void render_page(log_tab_ctx_t *ctx)
{
    if (!ctx || !ctx->valid || !ctx->table_area || !s_query_fn) return;

    const char *start_str = lv_textarea_get_text(ctx->input_start);
    const char *end_str = lv_textarea_get_text(ctx->input_end);
    int64_t start_ts = date_str_to_epoch(start_str, false);
    int64_t end_ts = date_str_to_epoch(end_str, true);
    ui_log_rec_status_filter_t status_filter = get_status_filter(ctx);
    static ui_log_rec_result_t result;

    memset(&result, 0, sizeof(result));
    s_query_fn(ctx->type, start_ts, end_ts, ctx->current_page, status_filter, &result);

    ctx->current_page = result.current_page;

    lv_obj_clean(ctx->table_area);

    if (result.count == 0) {
        lv_obj_t *empty_label = lv_label_create(ctx->table_area);
        lv_label_set_text(empty_label, get_empty_message(ctx->type));
        lv_obj_set_style_text_font(empty_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(empty_label, COLOR_TEXT_GRAY, 0);
        lv_obj_center(empty_label);
    } else {
        for (int i = 0; i < result.count; i++) {
            int y = TABLE_START_Y + i * ROW_HEIGHT;
            int global_idx = ctx->current_page * UI_LOG_REC_PAGE_SIZE + i + 1;
            char idx_buf[8];
            char time_buf[48];
            char planned_buf[24];
            char actual_buf[24];
            int x_pos = 0;

            if (i % 2 == 1) {
                add_row_background(ctx->table_area, y);
            }

            snprintf(idx_buf, sizeof(idx_buf), "%d", global_idx);
            format_timestamp(result.records[i].timestamp, time_buf, sizeof(time_buf));
            format_minutes(result.records[i].planned_minutes, planned_buf, sizeof(planned_buf));
            format_minutes(result.records[i].actual_minutes, actual_buf, sizeof(actual_buf));

            if (ctx->type == UI_LOG_REC_CONTROL || ctx->type == UI_LOG_REC_OPERATION) {
                static const int col_widths[3] = {150, 380, 608};
                add_text_cell(ctx->table_area, idx_buf, x_pos, y, col_widths[0]);
                x_pos += col_widths[0];
                add_text_cell(ctx->table_area, time_buf, x_pos, y, col_widths[1]);
                x_pos += col_widths[1];
                add_text_cell(ctx->table_area, result.records[i].desc, x_pos, y, col_widths[2]);
            } else if (ctx->type == UI_LOG_REC_MANUAL) {
                static const int col_widths[6] = {100, 220, 180, 180, 120, 338};
                add_text_cell(ctx->table_area, idx_buf, x_pos, y, col_widths[0]);
                x_pos += col_widths[0];
                add_text_cell(ctx->table_area, time_buf, x_pos, y, col_widths[1]);
                x_pos += col_widths[1];
                add_text_cell(ctx->table_area, planned_buf, x_pos, y, col_widths[2]);
                x_pos += col_widths[2];
                add_text_cell(ctx->table_area, actual_buf, x_pos, y, col_widths[3]);
                x_pos += col_widths[3];
                add_text_cell(ctx->table_area, result.records[i].status, x_pos, y, col_widths[4]);
                x_pos += col_widths[4];
                add_text_cell(ctx->table_area, result.records[i].detail, x_pos, y, col_widths[5]);
            } else {
                static const int col_widths[8] = {70, 150, 110, 200, 150, 150, 100, 208};
                add_text_cell(ctx->table_area, idx_buf, x_pos, y, col_widths[0]);
                x_pos += col_widths[0];
                add_text_cell(ctx->table_area, result.records[i].program_name, x_pos, y, col_widths[1]);
                x_pos += col_widths[1];
                add_text_cell(ctx->table_area, result.records[i].trigger, x_pos, y, col_widths[2]);
                x_pos += col_widths[2];
                add_text_cell(ctx->table_area, time_buf, x_pos, y, col_widths[3]);
                x_pos += col_widths[3];
                add_text_cell(ctx->table_area, planned_buf, x_pos, y, col_widths[4]);
                x_pos += col_widths[4];
                add_text_cell(ctx->table_area, actual_buf, x_pos, y, col_widths[5]);
                x_pos += col_widths[5];
                add_text_cell(ctx->table_area, result.records[i].status, x_pos, y, col_widths[6]);
                x_pos += col_widths[6];
                add_text_cell(ctx->table_area, result.records[i].detail, x_pos, y, col_widths[7]);
            }
        }
    }

    if (ctx->page_info) {
        char page_buf[16];
        snprintf(page_buf, sizeof(page_buf), "%d/%d",
                 result.total_matched > 0 ? result.current_page + 1 : 0,
                 result.total_pages > 1 ? result.total_pages :
                 (result.total_matched > 0 ? 1 : 0));
        lv_label_set_text(ctx->page_info, page_buf);
    }
}

static void page_first_cb(lv_event_t *e)
{
    log_tab_ctx_t *ctx = (log_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    ctx->current_page = 0;
    render_page(ctx);
}

static void page_prev_cb(lv_event_t *e)
{
    log_tab_ctx_t *ctx = (log_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid || ctx->current_page == 0) return;
    ctx->current_page--;
    render_page(ctx);
}

static void page_next_cb(lv_event_t *e)
{
    log_tab_ctx_t *ctx = (log_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    ctx->current_page++;
    render_page(ctx);
}

static void page_last_cb(lv_event_t *e)
{
    log_tab_ctx_t *ctx = (log_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    ctx->current_page = UINT16_MAX;
    render_page(ctx);
}
