/**
 * @file ui_alarm_records.c
 * @brief 告警管理 - 历史报警/掉线记录/上电记录 表格渲染与分页逻辑
 */

#include "ui_alarm_records.h"
#include "ui_common.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define ROW_HEIGHT     50
#define TABLE_START_Y  0
#define TIME_SYNC_THRESHOLD 1704067200LL

typedef struct {
    ui_alarm_rec_type_t type;
    lv_obj_t *input_start;
    lv_obj_t *input_end;
    lv_obj_t *table_area;
    lv_obj_t *page_info;
    lv_obj_t *query_btn;
    lv_obj_t *btn_first;
    lv_obj_t *btn_prev;
    lv_obj_t *btn_next;
    lv_obj_t *btn_last;
    lv_obj_t *loading_mask;
    uint16_t current_page;
    uint32_t request_seq;
    uint32_t active_request_id;
    bool has_requested;
    bool valid;
    ui_alarm_rec_query_status_t last_status;
    ui_alarm_rec_result_t last_result;
} rec_tab_ctx_t;

static ui_alarm_rec_query_submit_fn s_query_fn = NULL;
static rec_tab_ctx_t s_history_ctx;
static rec_tab_ctx_t s_offline_ctx;
static rec_tab_ctx_t s_poweron_ctx;

static int64_t date_str_to_epoch(const char *date_str, bool is_end);
static void page_first_cb(lv_event_t *e);
static void page_prev_cb(lv_event_t *e);
static void page_next_cb(lv_event_t *e);
static void page_last_cb(lv_event_t *e);
static const char *get_empty_message(ui_alarm_rec_type_t type);
static const char *get_status_message(ui_alarm_rec_query_status_t status);
static void render_ctx(rec_tab_ctx_t *ctx);
static void setup_ctx(rec_tab_ctx_t *ctx, ui_alarm_rec_type_t type,
    lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info, lv_obj_t *query_btn,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);
static rec_tab_ctx_t *get_ctx(ui_alarm_rec_type_t type);
static void submit_query(rec_tab_ctx_t *ctx, uint16_t page);
static void set_buttons_enabled(rec_tab_ctx_t *ctx, bool enabled);
static void show_loading(rec_tab_ctx_t *ctx);
static void hide_loading(rec_tab_ctx_t *ctx);

void ui_alarm_rec_register_query_cb(ui_alarm_rec_query_submit_fn fn)
{
    s_query_fn = fn;
}

void ui_alarm_rec_apply_query_result(const ui_alarm_rec_response_t *response)
{
    if (!response) {
        return;
    }

    rec_tab_ctx_t *ctx = get_ctx(response->request.type);
    if (!ctx || !ctx->valid || response->request.request_id != ctx->active_request_id) {
        return;
    }

    hide_loading(ctx);
    ctx->last_status = response->status;
    memset(&ctx->last_result, 0, sizeof(ctx->last_result));

    if (response->status == UI_ALARM_REC_QUERY_STATUS_OK) {
        memcpy(&ctx->last_result, &response->result, sizeof(ctx->last_result));
        ctx->current_page = response->result.current_page;
    }

    render_ctx(ctx);
}

void ui_alarm_rec_setup_history_alarm(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info, lv_obj_t *query_btn,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    setup_ctx(&s_history_ctx, UI_ALARM_REC_HISTORY_ALARM,
        input_start, input_end, table_area, page_info, query_btn,
        btn_first, btn_prev, btn_next, btn_last);
}

void ui_alarm_rec_setup_offline(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info, lv_obj_t *query_btn,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    setup_ctx(&s_offline_ctx, UI_ALARM_REC_OFFLINE,
        input_start, input_end, table_area, page_info, query_btn,
        btn_first, btn_prev, btn_next, btn_last);
}

void ui_alarm_rec_setup_poweron(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info, lv_obj_t *query_btn,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    setup_ctx(&s_poweron_ctx, UI_ALARM_REC_POWERON,
        input_start, input_end, table_area, page_info, query_btn,
        btn_first, btn_prev, btn_next, btn_last);
}

void ui_alarm_rec_history_query_btn_cb(lv_event_t *e)
{
    (void)e;
    submit_query(&s_history_ctx, 0);
}

void ui_alarm_rec_offline_query_btn_cb(lv_event_t *e)
{
    (void)e;
    submit_query(&s_offline_ctx, 0);
}

void ui_alarm_rec_poweron_query_btn_cb(lv_event_t *e)
{
    (void)e;
    submit_query(&s_poweron_ctx, 0);
}

void ui_alarm_rec_invalidate(void)
{
    rec_tab_ctx_t *contexts[] = {
        &s_history_ctx,
        &s_offline_ctx,
        &s_poweron_ctx,
    };

    for (size_t i = 0; i < sizeof(contexts) / sizeof(contexts[0]); i++) {
        contexts[i]->valid = false;
        contexts[i]->input_start = NULL;
        contexts[i]->input_end = NULL;
        contexts[i]->table_area = NULL;
        contexts[i]->page_info = NULL;
        contexts[i]->query_btn = NULL;
        contexts[i]->btn_first = NULL;
        contexts[i]->btn_prev = NULL;
        contexts[i]->btn_next = NULL;
        contexts[i]->btn_last = NULL;
        contexts[i]->loading_mask = NULL;
        contexts[i]->active_request_id = 0;
        contexts[i]->has_requested = false;
        memset(&contexts[i]->last_result, 0, sizeof(contexts[i]->last_result));
    }
}

void ui_alarm_rec_refresh_visible(void)
{
    if (s_history_ctx.valid && s_history_ctx.table_area && s_history_ctx.has_requested) {
        submit_query(&s_history_ctx, s_history_ctx.current_page);
    }
    if (s_offline_ctx.valid && s_offline_ctx.table_area && s_offline_ctx.has_requested) {
        submit_query(&s_offline_ctx, s_offline_ctx.current_page);
    }
    if (s_poweron_ctx.valid && s_poweron_ctx.table_area && s_poweron_ctx.has_requested) {
        submit_query(&s_poweron_ctx, s_poweron_ctx.current_page);
    }
}

static void setup_ctx(rec_tab_ctx_t *ctx, ui_alarm_rec_type_t type,
    lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info, lv_obj_t *query_btn,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->type = type;
    ctx->input_start = input_start;
    ctx->input_end = input_end;
    ctx->table_area = table_area;
    ctx->page_info = page_info;
    ctx->query_btn = query_btn;
    ctx->btn_first = btn_first;
    ctx->btn_prev = btn_prev;
    ctx->btn_next = btn_next;
    ctx->btn_last = btn_last;
    ctx->current_page = 0;
    ctx->valid = true;
    ctx->last_status = UI_ALARM_REC_QUERY_STATUS_OK;

    lv_obj_add_event_cb(btn_first, page_first_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn_prev, page_prev_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn_next, page_next_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(btn_last, page_last_cb, LV_EVENT_CLICKED, ctx);
}

static rec_tab_ctx_t *get_ctx(ui_alarm_rec_type_t type)
{
    switch (type) {
        case UI_ALARM_REC_HISTORY_ALARM:
            return &s_history_ctx;
        case UI_ALARM_REC_OFFLINE:
            return &s_offline_ctx;
        case UI_ALARM_REC_POWERON:
            return &s_poweron_ctx;
        default:
            return NULL;
    }
}

static void submit_query(rec_tab_ctx_t *ctx, uint16_t page)
{
    ui_alarm_rec_request_t request;

    if (!ctx || !ctx->valid || !ctx->table_area || !s_query_fn) {
        return;
    }

    memset(&request, 0, sizeof(request));
    request.type = ctx->type;
    request.start_ts = date_str_to_epoch(lv_textarea_get_text(ctx->input_start), false);
    request.end_ts = date_str_to_epoch(lv_textarea_get_text(ctx->input_end), true);
    request.page = page;
    request.request_id = ++ctx->request_seq;

    ctx->has_requested = true;
    ctx->active_request_id = request.request_id;
    show_loading(ctx);
    s_query_fn(&request);
}

static void set_buttons_enabled(rec_tab_ctx_t *ctx, bool enabled)
{
    lv_obj_t *buttons[] = {
        ctx ? ctx->query_btn : NULL,
        ctx ? ctx->btn_first : NULL,
        ctx ? ctx->btn_prev : NULL,
        ctx ? ctx->btn_next : NULL,
        ctx ? ctx->btn_last : NULL,
    };

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
        if (!buttons[i]) {
            continue;
        }
        if (enabled) {
            lv_obj_clear_state(buttons[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(buttons[i], LV_STATE_DISABLED);
        }
    }
}

static void show_loading(rec_tab_ctx_t *ctx)
{
    lv_obj_t *parent;
    lv_obj_t *frame;
    lv_obj_t *content;
    lv_obj_t *label;

    if (!ctx || !ctx->table_area) {
        return;
    }

    hide_loading(ctx);
    set_buttons_enabled(ctx, false);

    parent = lv_obj_get_parent(ctx->table_area);
    if (!parent) {
        return;
    }

    ctx->loading_mask = lv_obj_create(parent);
    lv_obj_remove_style_all(ctx->loading_mask);
    lv_obj_set_size(ctx->loading_mask, lv_obj_get_width(parent), lv_obj_get_height(parent));
    lv_obj_set_pos(ctx->loading_mask, 0, 0);
    lv_obj_set_style_bg_opa(ctx->loading_mask, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(ctx->loading_mask, lv_color_black(), 0);
    lv_obj_add_flag(ctx->loading_mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(ctx->loading_mask, LV_OBJ_FLAG_SCROLLABLE);

    frame = lv_obj_create(ctx->loading_mask);
    lv_obj_set_size(frame, 400, 200);
    lv_obj_center(frame);
    lv_obj_set_style_bg_color(frame, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(frame, 0, 0);
    lv_obj_set_style_radius(frame, 0, 0);
    lv_obj_set_style_pad_all(frame, 5, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    content = lv_obj_create(frame);
    lv_obj_set_size(content, 390, 190);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    label = lv_label_create(content);
    lv_label_set_text(label, "正在查询中...");
    lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT_MAIN, 0);
    lv_obj_center(label);
}

static void hide_loading(rec_tab_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->loading_mask) {
        lv_obj_del(ctx->loading_mask);
        ctx->loading_mask = NULL;
    }

    set_buttons_enabled(ctx, true);
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

static const char *get_empty_message(ui_alarm_rec_type_t type)
{
    switch (type) {
        case UI_ALARM_REC_OFFLINE:
            return "暂无掉线记录";
        case UI_ALARM_REC_POWERON:
            return "暂无上电记录";
        case UI_ALARM_REC_HISTORY_ALARM:
            return "暂无历史报警记录";
        default:
            return "暂无记录";
    }
}

static const char *get_status_message(ui_alarm_rec_query_status_t status)
{
    switch (status) {
        case UI_ALARM_REC_QUERY_STATUS_TF_UNAVAILABLE:
            return "TF卡不可用，无法查询历史记录";
        case UI_ALARM_REC_QUERY_STATUS_FAILED:
            return "历史记录读取失败，请稍后重试";
        case UI_ALARM_REC_QUERY_STATUS_OK:
        default:
            return NULL;
    }
}

static void render_ctx(rec_tab_ctx_t *ctx)
{
    const char *message;
    const ui_alarm_rec_result_t *result;
    int col_widths[3] = {150, 730, 340};

    if (!ctx || !ctx->valid || !ctx->table_area) {
        return;
    }

    lv_obj_clean(ctx->table_area);
    result = &ctx->last_result;
    message = get_status_message(ctx->last_status);
    if (!message && result->count == 0) {
        message = get_empty_message(ctx->type);
    }

    if (message) {
        lv_obj_t *empty_label = lv_label_create(ctx->table_area);
        lv_label_set_text(empty_label, message);
        lv_obj_set_style_text_font(empty_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(empty_label, COLOR_TEXT_GRAY, 0);
        lv_obj_center(empty_label);
    } else {
        for (int i = 0; i < result->count; i++) {
            int y = TABLE_START_Y + i * ROW_HEIGHT;
            int global_idx = result->current_page * UI_ALARM_REC_PAGE_SIZE + i + 1;
            int x_pos = 0;
            char idx_buf[8];
            char time_buf[48];

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

            snprintf(idx_buf, sizeof(idx_buf), "%d", global_idx);
            if (result->records[i].timestamp < TIME_SYNC_THRESHOLD) {
                snprintf(time_buf, sizeof(time_buf), "时间未同步");
            } else {
                time_t ts = (time_t)result->records[i].timestamp;
                struct tm timeinfo;
                localtime_r(&ts, &timeinfo);
                snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d:%02d",
                         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            }

            lv_obj_t *idx_label = lv_label_create(ctx->table_area);
            lv_label_set_text(idx_label, idx_buf);
            lv_obj_set_style_text_font(idx_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(idx_label, COLOR_TEXT_MAIN, 0);
            lv_obj_set_size(idx_label, col_widths[0], ROW_HEIGHT);
            lv_obj_set_pos(idx_label, x_pos, y);
            lv_obj_set_style_text_align(idx_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_pad_top(idx_label, 17, 0);
            x_pos += col_widths[0];

            lv_obj_t *time_label = lv_label_create(ctx->table_area);
            lv_label_set_text(time_label, time_buf);
            lv_obj_set_style_text_font(time_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(time_label, COLOR_TEXT_MAIN, 0);
            lv_obj_set_size(time_label, col_widths[1], ROW_HEIGHT);
            lv_obj_set_pos(time_label, x_pos, y);
            lv_obj_set_style_text_align(time_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_pad_top(time_label, 17, 0);
            x_pos += col_widths[1];

            lv_obj_t *desc_label = lv_label_create(ctx->table_area);
            lv_label_set_text(desc_label, result->records[i].desc);
            lv_obj_set_style_text_font(desc_label, &my_font_cn_16, 0);
            lv_obj_set_style_text_color(desc_label, COLOR_TEXT_MAIN, 0);
            lv_obj_set_size(desc_label, col_widths[2], ROW_HEIGHT);
            lv_obj_set_pos(desc_label, x_pos, y);
            lv_obj_set_style_text_align(desc_label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_pad_top(desc_label, 17, 0);
        }
    }

    if (ctx->page_info) {
        char page_buf[16];
        snprintf(page_buf, sizeof(page_buf), "%d/%d",
                 result->total_matched > 0 ? result->current_page + 1 : 0,
                 result->total_pages > 0 ? result->total_pages : 0);
        lv_label_set_text(ctx->page_info, page_buf);
    }
}

static void page_first_cb(lv_event_t *e)
{
    rec_tab_ctx_t *ctx = (rec_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    submit_query(ctx, 0);
}

static void page_prev_cb(lv_event_t *e)
{
    rec_tab_ctx_t *ctx = (rec_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid || ctx->current_page == 0) return;
    submit_query(ctx, ctx->current_page - 1);
}

static void page_next_cb(lv_event_t *e)
{
    rec_tab_ctx_t *ctx = (rec_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    submit_query(ctx, ctx->current_page + 1);
}

static void page_last_cb(lv_event_t *e)
{
    rec_tab_ctx_t *ctx = (rec_tab_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !ctx->valid) return;
    submit_query(ctx, UINT16_MAX);
}
