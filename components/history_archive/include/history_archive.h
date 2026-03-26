#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "event_recorder.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

esp_err_t history_archive_init(void);
bool history_archive_is_available(void);

esp_err_t history_archive_enqueue_basic_record(evt_type_t type, const evt_record_t *record);
esp_err_t history_archive_enqueue_manual_record(const evt_manual_record_t *record);
esp_err_t history_archive_enqueue_program_record(const evt_program_record_t *record);

esp_err_t history_archive_sync_basic_records(evt_type_t type, const evt_record_t *records, size_t count);
esp_err_t history_archive_sync_manual_records(const evt_manual_record_t *records, size_t count);
esp_err_t history_archive_sync_program_records(const evt_program_record_t *records, size_t count);

esp_err_t history_archive_query_basic_records(evt_type_t type, int64_t start_ts, int64_t end_ts,
                                              evt_record_t **records,
                                              size_t *count);
esp_err_t history_archive_query_manual_records(int64_t start_ts, int64_t end_ts,
                                               evt_status_filter_t status_filter,
                                               evt_manual_record_t **records,
                                               size_t *count);
esp_err_t history_archive_query_program_records(int64_t start_ts, int64_t end_ts,
                                                evt_status_filter_t status_filter,
                                                evt_program_record_t **records,
                                                size_t *count);

void history_archive_free_query_result(void *records);

#ifdef __cplusplus
}
#endif
