#ifndef EVENT_RECORDER_H
#define EVENT_RECORDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EVT_RECORD_MAX   20
#define EVT_PAGE_SIZE    10

typedef enum {
    EVT_TYPE_OFFLINE = 0,
    EVT_TYPE_POWERON,
    EVT_TYPE_CONTROL,
    EVT_TYPE_OPERATION,
    EVT_TYPE_ALARM,
    EVT_TYPE_MANUAL_RECORD,
    EVT_TYPE_PROGRAM_RECORD,
    EVT_TYPE_MAX
} evt_type_t;

typedef enum {
    EVT_STATUS_FILTER_ALL = 0,
    EVT_STATUS_FILTER_NORMAL,
    EVT_STATUS_FILTER_ABNORMAL
} evt_status_filter_t;

typedef struct {
    int64_t timestamp;
    char    desc[64];
} evt_record_t;

typedef struct {
    int64_t start_ts;
    uint16_t planned_minutes;
    uint16_t actual_minutes;
    char status[16];
    char detail[64];
} evt_manual_record_t;

typedef struct {
    char program_name[32];
    char trigger[16];
    int64_t start_ts;
    uint16_t planned_minutes;
    uint16_t actual_minutes;
    char status[16];
    char detail[64];
} evt_program_record_t;

typedef struct {
    evt_record_t records[EVT_PAGE_SIZE];
    uint16_t count;
    uint16_t total_matched;
    uint16_t total_pages;
    uint16_t current_page;
} evt_query_result_t;

typedef struct {
    evt_manual_record_t records[EVT_PAGE_SIZE];
    uint16_t count;
    uint16_t total_matched;
    uint16_t total_pages;
    uint16_t current_page;
} evt_manual_query_result_t;

typedef struct {
    evt_program_record_t records[EVT_PAGE_SIZE];
    uint16_t count;
    uint16_t total_matched;
    uint16_t total_pages;
    uint16_t current_page;
} evt_program_query_result_t;

esp_err_t event_recorder_init(void);
esp_err_t event_recorder_add_basic_record(evt_type_t type, const evt_record_t *record);
esp_err_t event_recorder_add_offline(const char *desc);
esp_err_t event_recorder_add_poweron(const char *desc);
esp_err_t event_recorder_add_control(const char *desc);
esp_err_t event_recorder_add_operation(const char *desc);
esp_err_t event_recorder_add_alarm(const char *desc);
esp_err_t event_recorder_add_manual_record(const evt_manual_record_t *record);
esp_err_t event_recorder_add_program_record(const evt_program_record_t *record);
esp_err_t event_recorder_query(evt_type_t type, int64_t start_ts, int64_t end_ts,
                                uint16_t page, evt_query_result_t *result);
esp_err_t event_recorder_query_manual_records(int64_t start_ts, int64_t end_ts,
                                               evt_status_filter_t status_filter,
                                               uint16_t page,
                                               evt_manual_query_result_t *result);
esp_err_t event_recorder_query_program_records(int64_t start_ts, int64_t end_ts,
                                                evt_status_filter_t status_filter,
                                                uint16_t page,
                                                evt_program_query_result_t *result);

esp_err_t event_recorder_get_basic_records_snapshot(evt_type_t type,
                                                    evt_record_t *records,
                                                    size_t max_records,
                                                    size_t *out_count);
esp_err_t event_recorder_get_manual_records_snapshot(evt_manual_record_t *records,
                                                     size_t max_records,
                                                     size_t *out_count);
esp_err_t event_recorder_get_program_records_snapshot(evt_program_record_t *records,
                                                      size_t max_records,
                                                      size_t *out_count);
esp_err_t event_recorder_clear_legacy_storage(void);
bool event_recorder_manual_record_equals(const evt_manual_record_t *a, const evt_manual_record_t *b);
bool event_recorder_program_record_equals(const evt_program_record_t *a, const evt_program_record_t *b);

/**
 * @brief Update all records with unsynchronized timestamps to current time.
 *        Call this after NTP time sync completes.
 */
esp_err_t event_recorder_fix_timestamps(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_RECORDER_H */
