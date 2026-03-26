#ifndef UI_ALARM_RECORDS_H
#define UI_ALARM_RECORDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdint.h>

#define UI_ALARM_REC_PAGE_SIZE 10

typedef enum {
    UI_ALARM_REC_OFFLINE = 0,
    UI_ALARM_REC_POWERON,
    UI_ALARM_REC_HISTORY_ALARM
} ui_alarm_rec_type_t;

typedef enum {
    UI_ALARM_REC_QUERY_STATUS_OK = 0,
    UI_ALARM_REC_QUERY_STATUS_TF_UNAVAILABLE,
    UI_ALARM_REC_QUERY_STATUS_FAILED
} ui_alarm_rec_query_status_t;

typedef struct {
    int64_t timestamp;
    char    desc[64];
} ui_alarm_rec_item_t;

typedef struct {
    ui_alarm_rec_item_t records[UI_ALARM_REC_PAGE_SIZE];
    uint16_t count;
    uint16_t total_matched;
    uint16_t total_pages;
    uint16_t current_page;
} ui_alarm_rec_result_t;

typedef struct {
    ui_alarm_rec_type_t type;
    int64_t start_ts;
    int64_t end_ts;
    uint16_t page;
    uint32_t request_id;
} ui_alarm_rec_request_t;

typedef struct {
    ui_alarm_rec_request_t request;
    ui_alarm_rec_query_status_t status;
    ui_alarm_rec_result_t result;
} ui_alarm_rec_response_t;

typedef void (*ui_alarm_rec_query_submit_fn)(const ui_alarm_rec_request_t *request);

void ui_alarm_rec_register_query_cb(ui_alarm_rec_query_submit_fn fn);
void ui_alarm_rec_apply_query_result(const ui_alarm_rec_response_t *response);

void ui_alarm_rec_setup_history_alarm(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info, lv_obj_t *query_btn,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);

void ui_alarm_rec_setup_offline(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info, lv_obj_t *query_btn,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);

void ui_alarm_rec_setup_poweron(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info, lv_obj_t *query_btn,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);

void ui_alarm_rec_history_query_btn_cb(lv_event_t *e);
void ui_alarm_rec_offline_query_btn_cb(lv_event_t *e);
void ui_alarm_rec_poweron_query_btn_cb(lv_event_t *e);

void ui_alarm_rec_invalidate(void);
void ui_alarm_rec_refresh_visible(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_ALARM_RECORDS_H */
