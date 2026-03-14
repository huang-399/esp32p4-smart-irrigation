/**
 * @file ui_alarm_records.c
 * @brief 告警管理 - 掉线记录/上电记录 表格渲染与分页逻辑
 */

#include "ui_alarm_records.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/*********************
 *  DEFINES
 *********************/
#define ROW_HEIGHT   50
#define TABLE_START_Y 0

/* Epoch threshold: timestamps before 2024-01-01 are considered unsynchronized */
#define TIME_SYNC_THRESHOLD 1704067200LL

/*********************
 *  TYPEDEFS
 *********************/
typedef struct {
    ui_alarm_rec_type_t type;
    lv_obj_t *input_start;
    lv_obj_t *input_end;
    lv_obj_t *table_area;
    lv_obj_t *page_info;
    lv_obj_t *btn_first;
    lv_obj_t *btn_prev;
    lv_obj_t *btn_next;
    lv_obj_t *btn_last;
    uint16_t  current_page;
    bool      valid;
} rec_tab_ctx_t;

/*********************
 *  STATIC VARIABLES
 *********************/
static ui_alarm_rec_query_fn s_query_fn = NULL;
static rec_tab_ctx_t s_offline_ctx;
static rec_tab_ctx_t s_poweron_ctx;

/*********************
 *  STATIC PROTOTYPES
 *********************/
static int64_t date_str_to_epoch(const char *date_str, bool is_end);
static void render_page(rec_tab_ctx_t *ctx);
static void page_first_cb(lv_event_t *e);
static void page_prev_cb(lv_event_t *e);
static void page_next_cb(lv_event_t *e);
static void page_last_cb(lv_event_t *e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ui_alarm_rec_register_query_cb(ui_alarm_rec_query_fn fn)
{
    s_query_fn = fn;
}

void ui_alarm_rec_setup_offline(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    s_offline_ctx.type = UI_ALARM_REC_OFFLINE;
    s_offline_ctx.input_start = input_start;
    s_offline_ctx.input_end = input_end;
    s_offline_ctx.table_area = table_area;
    s_offline_ctx.page_info = page_info;
    s_offline_ctx.btn_first = btn_first;
    s_offline_ctx.btn_prev = btn_prev;
    s_offline_ctx.btn_next = btn_next;
    s_offline_ctx.btn_last = btn_last;
    s_offline_ctx.current_page = 0;
    s_offline_ctx.valid = true;

    lv_obj_add_event_cb(btn_first, page_first_cb, LV_EVENT_CLICKED, &s_offline_ctx);
    lv_obj_add_event_cb(btn_prev, page_prev_cb, LV_EVENT_CLICKED, &s_offline_ctx);
    lv_obj_add_event_cb(btn_next, page_next_cb, LV_EVENT_CLICKED, &s_offline_ctx);
    lv_obj_add_event_cb(btn_last, page_last_cb, LV_EVENT_CLICKED, &s_offline_ctx);
}

void ui_alarm_rec_setup_poweron(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    s_poweron_ctx.type = UI_ALARM_REC_POWERON;
    s_poweron_ctx.input_start = input_start;
    s_poweron_ctx.input_end = input_end;
    s_poweron_ctx.table_area = table_area;
    s_poweron_ctx.page_info = page_info;
    s_poweron_ctx.btn_first = btn_first;
    s_poweron_ctx.btn_prev = btn_prev;
    s_poweron_ctx.btn_next = btn_next;
    s_poweron_ctx.btn_last = btn_last;
    s_poweron_ctx.current_page = 0;
    s_poweron_ctx.valid = true;

    lv_obj_add_event_cb(btn_first, page_first_cb, LV_EVENT_CLICKED, &s_poweron_ctx);
    lv_obj_add_event_cb(btn_prev, page_prev_cb, LV_EVENT_CLICKED, &s_poweron_ctx);
    lv_obj_add_event_cb(btn_next, page_next_cb, LV_EVENT_CLICKED, &s_poweron_ctx);
    lv_obj_add_event_cb(btn_last, page_last_cb, LV_EVENT_CLICKED, &s_poweron_ctx);
}

void ui_alarm_rec_offline_query_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_offline_ctx.valid) return;
    s_offline_ctx.current_page = 0;
    render_page(&s_offline_ctx);
}

void ui_alarm_rec_poweron_query_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_poweron_ctx.valid) return;
    s_poweron_ctx.current_page = 0;
    render_page(&s_poweron_ctx);
}

void ui_alarm_rec_invalidate(void)
{
    s_offline_ctx.valid = false;
    s_offline_ctx.table_area = NULL;
    s_offline_ctx.page_info = NULL;

    s_poweron_ctx.valid = false;
    s_poweron_ctx.table_area = NULL;
    s_poweron_ctx.page_info = NULL;
}

void ui_alarm_rec_refresh_visible(void)
{
    if (s_offline_ctx.valid && s_offline_ctx.table_area) {
        render_page(&s_offline_ctx);
    }
    if (s_poweron_ctx.valid && s_poweron_ctx.table_area) {
        render_page(&s_poweron_ctx);
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief Parse "YYYY-MM-DD" string to epoch seconds
 * @param date_str Date string in "YYYY-MM-DD" format
 * @param is_end If true, set time to 23:59:59; otherwise 00:00:00
 * @return epoch seconds, or 0 on failure
 */
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

/**
 * @brief Render a page of records into the table area
 */
static void render_page(rec_tab_ctx_t *ctx)
{
    if (!ctx || !ctx->valid || !ctx->table_area || !s_query_fn) return;

    /* Parse date inputs */
    const char *start_str = lv_textarea_get_text(ctx->input_start);
    const char *end_str = lv_textarea_get_text(ctx->input_end);
    int64_t start_ts = date_str_to_epoch(start_str, false);
    int64_t end_ts = date_str_to_epoch(end_str, true);

    /* Query data via callback (static to reduce stack usage in LVGL task) */
    static ui_alarm_rec_result_t result;
    memset(&result, 0, sizeof(result));
    s_query_fn(ctx->type, start_ts, end_ts, ctx->current_page, &result);

    /* Update current page from result (may have been clamped) */
    ctx->current_page = result.current_page;

    /* Clear old rows in table area */
    lv_obj_clean(ctx->table_area);

    /* Column widths: same as header in ui_alarm.c */
    int col_widths[3] = {150, 730, 340};

    if (result.count == 0) {
        /* Show empty message */
        lv_obj_t *empty_label = lv_label_create(ctx->table_area);
        const char *msg = (ctx->type == UI_ALARM_REC_OFFLINE) ?
            "暂无掉线记录" : "暂无上电记录";
        lv_label_set_text(empty_label, msg);
        lv_obj_set_style_text_font(empty_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(empty_label, COLOR_TEXT_GRAY, 0);
        lv_obj_center(empty_label);
    } else {
        /* Render data rows */
        for (int i = 0; i < result.count; i++) {
            int y = TABLE_START_Y + i * ROW_HEIGHT;
            int global_idx = ctx->current_page * UI_ALARM_REC_PAGE_SIZE + i + 1;

            /* Alternating row background */
            if (i % 2 == 1) {
                lv_obj_t *row_bg = lv_obj_create(ctx->table_area);
                lv_obj_set_size(row_bg, 1220, ROW_HEIGHT);
                lv_obj_set_pos(row_bg, 0, y);
                lv_obj_set_style_bg_color(row_bg, lv_color_hex(0xf8f8f8), 0);
                lv_obj_set_style_border_width(row_bg, 0, 0);
                lv_obj_set_style_radius(row_bg, 0, 0);
                lv_obj_set_style_pad_all(row_bg, 0, 0);
                lv_obj_clear_flag(row_bg, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
            }

            int x_pos = 0;

            /* Column 1: 序号 */
            char idx_buf[8];
            snprintf(idx_buf, sizeof(idx_buf), "%d", global_idx);
            lv_obj_t *idx_label = lv_label_create(ctx->table_area);
            lv_label_set_text(idx_label, idx_buf);
            lv_obj_set_style_text_font(idx_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(idx_label, COLOR_TEXT_MAIN, 0);
            lv_obj_set_size(idx_label, col_widths[0], ROW_HEIGHT);
            lv_obj_set_pos(idx_label, x_pos, y);
            lv_obj_set_style_text_align(idx_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_pad_top(idx_label, 17, 0);
            x_pos += col_widths[0];

            /* Column 2: 发生时间 */
            char time_buf[48];
            if (result.records[i].timestamp < TIME_SYNC_THRESHOLD) {
                snprintf(time_buf, sizeof(time_buf), "时间未同步");
            } else {
                time_t ts = (time_t)result.records[i].timestamp;
                struct tm timeinfo;
                localtime_r(&ts, &timeinfo);
                snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d:%02d",
                         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            }
            lv_obj_t *time_label = lv_label_create(ctx->table_area);
            lv_label_set_text(time_label, time_buf);
            lv_obj_set_style_text_font(time_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(time_label, COLOR_TEXT_MAIN, 0);
            lv_obj_set_size(time_label, col_widths[1], ROW_HEIGHT);
            lv_obj_set_pos(time_label, x_pos, y);
            lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_pad_top(time_label, 17, 0);
            x_pos += col_widths[1];

            /* Column 3: 类型/描述 */
            lv_obj_t *desc_label = lv_label_create(ctx->table_area);
            lv_label_set_text(desc_label, result.records[i].desc);
            lv_obj_set_style_text_font(desc_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(desc_label, COLOR_TEXT_MAIN, 0);
            lv_obj_set_size(desc_label, col_widths[2], ROW_HEIGHT);
            lv_obj_set_pos(desc_label, x_pos, y);
            lv_obj_set_style_text_align(desc_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_pad_top(desc_label, 17, 0);
        }
    }

    /* Update page info label */
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
    rec_tab_ctx_t *ctx = (rec_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    ctx->current_page = 0;
    render_page(ctx);
}

static void page_prev_cb(lv_event_t *e)
{
    rec_tab_ctx_t *ctx = (rec_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid || ctx->current_page == 0) return;
    ctx->current_page--;
    render_page(ctx);
}

static void page_next_cb(lv_event_t *e)
{
    rec_tab_ctx_t *ctx = (rec_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    ctx->current_page++;
    render_page(ctx);
}

static void page_last_cb(lv_event_t *e)
{
    rec_tab_ctx_t *ctx = (rec_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    /* Set to a large value; render_page will clamp it */
    ctx->current_page = UINT16_MAX;
    render_page(ctx);
}
