#ifndef UI_LOG_RECORDS_H
#define UI_LOG_RECORDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdint.h>

#define UI_LOG_REC_PAGE_SIZE 10

typedef enum {
    UI_LOG_REC_CONTROL = 0,
    UI_LOG_REC_OPERATION,
    UI_LOG_REC_MANUAL,
    UI_LOG_REC_PROGRAM
} ui_log_rec_type_t;

typedef enum {
    UI_LOG_REC_STATUS_ALL = 0,
    UI_LOG_REC_STATUS_NORMAL,
    UI_LOG_REC_STATUS_ABNORMAL
} ui_log_rec_status_filter_t;

typedef struct {
    int64_t timestamp;
    char desc[64];
    char program_name[32];
    char trigger[16];
    uint16_t planned_minutes;
    uint16_t actual_minutes;
    char status[16];
    char detail[64];
} ui_log_rec_item_t;

typedef struct {
    ui_log_rec_item_t records[UI_LOG_REC_PAGE_SIZE];
    uint16_t count;
    uint16_t total_matched;
    uint16_t total_pages;
    uint16_t current_page;
} ui_log_rec_result_t;

typedef void (*ui_log_rec_query_fn)(ui_log_rec_type_t type,
    int64_t start_ts, int64_t end_ts, uint16_t page,
    ui_log_rec_status_filter_t status_filter,
    ui_log_rec_result_t *result);

void ui_log_rec_register_query_cb(ui_log_rec_query_fn fn);

void ui_log_rec_setup_control(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);

void ui_log_rec_setup_operation(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);

void ui_log_rec_setup_manual(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *status_dropdown, lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);

void ui_log_rec_setup_program(lv_obj_t *input_start, lv_obj_t *input_end,
    lv_obj_t *status_dropdown, lv_obj_t *table_area, lv_obj_t *page_info,
    lv_obj_t *btn_first, lv_obj_t *btn_prev,
    lv_obj_t *btn_next, lv_obj_t *btn_last);

void ui_log_rec_control_query_btn_cb(lv_event_t *e);
void ui_log_rec_operation_query_btn_cb(lv_event_t *e);
void ui_log_rec_manual_query_btn_cb(lv_event_t *e);
void ui_log_rec_program_query_btn_cb(lv_event_t *e);

void ui_log_rec_invalidate(void);
void ui_log_rec_refresh_visible(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_LOG_RECORDS_H */
