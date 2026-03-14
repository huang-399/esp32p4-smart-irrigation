#ifndef EVENT_RECORDER_H
#define EVENT_RECORDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include <stdint.h>

#define EVT_RECORD_MAX   20
#define EVT_PAGE_SIZE    10

typedef enum {
    EVT_TYPE_OFFLINE = 0,
    EVT_TYPE_POWERON,
    EVT_TYPE_MAX
} evt_type_t;

typedef struct {
    int64_t timestamp;
    char    desc[64];
} evt_record_t;

typedef struct {
    evt_record_t records[EVT_PAGE_SIZE];
    uint16_t count;
    uint16_t total_matched;
    uint16_t total_pages;
    uint16_t current_page;
} evt_query_result_t;

esp_err_t event_recorder_init(void);
esp_err_t event_recorder_add_offline(const char *desc);
esp_err_t event_recorder_add_poweron(const char *desc);
esp_err_t event_recorder_query(evt_type_t type, int64_t start_ts, int64_t end_ts,
                                uint16_t page, evt_query_result_t *result);

/**
 * @brief Update all records with unsynchronized timestamps to current time.
 *        Call this after NTP time sync completes.
 */
esp_err_t event_recorder_fix_timestamps(void);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_RECORDER_H */
