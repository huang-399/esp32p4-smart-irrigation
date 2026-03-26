#include "history_archive.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_x.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "history_archive";

#define HISTORY_ROOT_DIR        "history"
#define HISTORY_MANUAL_DIR      "manual"
#define HISTORY_PROGRAM_DIR     "program"
#define HISTORY_OFFLINE_DIR     "offline"
#define HISTORY_POWERON_DIR     "poweron"
#define HISTORY_ALARM_DIR       "alarm"
#define HISTORY_CONTROL_DIR     "control"
#define HISTORY_OPERATION_DIR   "operation"
#define HISTORY_QUEUE_LEN       16
#define HISTORY_TASK_STACK_SIZE 6144

#define CSV_BASIC_FIELD_COUNT   2
#define CSV_MANUAL_FIELD_COUNT  5
#define CSV_PROGRAM_FIELD_COUNT 7
#define CSV_FIELD_BUF_SIZE      160
#define CSV_LINE_BUF_SIZE       512

typedef enum {
    ARCHIVE_MSG_BASIC = 0,
    ARCHIVE_MSG_MANUAL,
    ARCHIVE_MSG_PROGRAM
} archive_msg_type_t;

typedef struct {
    archive_msg_type_t type;
    evt_type_t basic_type;
    union {
        evt_record_t basic;
        evt_manual_record_t manual;
        evt_program_record_t program;
    } data;
} archive_msg_t;

static QueueHandle_t s_archive_queue = NULL;
static SemaphoreHandle_t s_archive_mutex = NULL;
static bool s_initialized = false;
static bool s_archive_available = false;

static esp_err_t ensure_archive_mounted_locked(void);
static esp_err_t ensure_directory_locked(const char *path);
static esp_err_t ensure_parent_directories_locked(const char *subdir);
static esp_err_t build_daily_file_path(const char *subdir, int64_t ts, char *path, size_t path_size);
static esp_err_t append_basic_record_locked(evt_type_t type, const evt_record_t *record);
static esp_err_t append_manual_record_locked(const evt_manual_record_t *record);
static esp_err_t append_program_record_locked(const evt_program_record_t *record);
static esp_err_t query_basic_records_locked(evt_type_t type,
                                            int64_t start_ts,
                                            int64_t end_ts,
                                            evt_record_t **records,
                                            size_t *count);
static esp_err_t query_manual_records_locked(int64_t start_ts, int64_t end_ts,
                                             evt_status_filter_t status_filter,
                                             evt_manual_record_t **records,
                                             size_t *count);
static esp_err_t query_program_records_locked(int64_t start_ts, int64_t end_ts,
                                              evt_status_filter_t status_filter,
                                              evt_program_record_t **records,
                                              size_t *count);

static bool is_archive_basic_type(evt_type_t type)
{
    return type >= EVT_TYPE_OFFLINE && type <= EVT_TYPE_ALARM;
}

static const char *basic_type_subdir(evt_type_t type)
{
    switch (type) {
        case EVT_TYPE_OFFLINE:
            return HISTORY_OFFLINE_DIR;
        case EVT_TYPE_POWERON:
            return HISTORY_POWERON_DIR;
        case EVT_TYPE_CONTROL:
            return HISTORY_CONTROL_DIR;
        case EVT_TYPE_OPERATION:
            return HISTORY_OPERATION_DIR;
        case EVT_TYPE_ALARM:
            return HISTORY_ALARM_DIR;
        default:
            return NULL;
    }
}

static bool status_matches_filter(const char *status, evt_status_filter_t filter)
{
    if (filter == EVT_STATUS_FILTER_ALL) {
        return true;
    }

    if (filter == EVT_STATUS_FILTER_NORMAL) {
        return strcmp(status ? status : "", "正常") == 0;
    }

    if (filter == EVT_STATUS_FILTER_ABNORMAL) {
        return status && status[0] != '\0' && strcmp(status, "正常") != 0;
    }

    return true;
}

static bool timestamp_matches_range(int64_t ts, int64_t start_ts, int64_t end_ts)
{
    if (start_ts > 0 && ts < start_ts) {
        return false;
    }
    if (end_ts > 0 && ts > end_ts) {
        return false;
    }
    return true;
}

static int compare_basic_records_desc(const void *lhs, const void *rhs)
{
    const evt_record_t *a = (const evt_record_t *)lhs;
    const evt_record_t *b = (const evt_record_t *)rhs;

    if (a->timestamp < b->timestamp) {
        return 1;
    }
    if (a->timestamp > b->timestamp) {
        return -1;
    }
    return 0;
}

static int compare_manual_records_desc(const void *lhs, const void *rhs)
{
    const evt_manual_record_t *a = (const evt_manual_record_t *)lhs;
    const evt_manual_record_t *b = (const evt_manual_record_t *)rhs;

    if (a->start_ts < b->start_ts) {
        return 1;
    }
    if (a->start_ts > b->start_ts) {
        return -1;
    }
    return 0;
}

static int compare_program_records_desc(const void *lhs, const void *rhs)
{
    const evt_program_record_t *a = (const evt_program_record_t *)lhs;
    const evt_program_record_t *b = (const evt_program_record_t *)rhs;

    if (a->start_ts < b->start_ts) {
        return 1;
    }
    if (a->start_ts > b->start_ts) {
        return -1;
    }
    return 0;
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    size_t len;

    if (!dst || dst_size == 0) {
        return;
    }

    dst[0] = '\0';
    if (!src) {
        return;
    }

    len = strnlen(src, dst_size - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static bool has_csv_suffix(const char *name)
{
    size_t len;

    if (!name) {
        return false;
    }

    len = strlen(name);
    return len >= 4 && strcmp(name + len - 4, ".csv") == 0;
}

static esp_err_t csv_escape_field(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;

    if (!dst || dst_size < 3) {
        return ESP_ERR_INVALID_ARG;
    }

    dst[out++] = '"';
    src = src ? src : "";

    while (*src) {
        if (out + 2 >= dst_size) {
            return ESP_FAIL;
        }

        if (*src == '"') {
            dst[out++] = '"';
            dst[out++] = '"';
        } else {
            dst[out++] = *src;
        }
        src++;
    }

    if (out + 2 > dst_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    dst[out++] = '"';
    dst[out] = '\0';
    return ESP_OK;
}

static int parse_csv_fields(const char *line, char fields[][CSV_FIELD_BUF_SIZE], int max_fields)
{
    int field_index = 0;
    int out = 0;
    bool in_quotes = false;
    const char *p = line;

    if (!line || !fields || max_fields <= 0) {
        return 0;
    }

    memset(fields, 0, (size_t)max_fields * CSV_FIELD_BUF_SIZE);

    while (*p && field_index < max_fields) {
        char c = *p++;

        if (!in_quotes && (c == '\r' || c == '\n')) {
            break;
        }

        if (c == '"') {
            if (in_quotes && *p == '"') {
                if (out < CSV_FIELD_BUF_SIZE - 1) {
                    fields[field_index][out++] = '"';
                }
                p++;
            } else {
                in_quotes = !in_quotes;
            }
            continue;
        }

        if (!in_quotes && c == ',') {
            fields[field_index][out] = '\0';
            field_index++;
            out = 0;
            continue;
        }

        if (out < CSV_FIELD_BUF_SIZE - 1) {
            fields[field_index][out++] = c;
        }
    }

    if (field_index < max_fields) {
        fields[field_index][out] = '\0';
        field_index++;
    }

    return field_index;
}

static esp_err_t append_unique_basic_record(evt_record_t **records,
                                            size_t *count,
                                            size_t *capacity,
                                            const evt_record_t *record)
{
    if (!records || !count || !capacity || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < *count; i++) {
        if ((*records)[i].timestamp == record->timestamp &&
            strcmp((*records)[i].desc, record->desc) == 0) {
            return ESP_OK;
        }
    }

    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        evt_record_t *new_records = realloc(*records, new_capacity * sizeof(evt_record_t));
        if (!new_records) {
            return ESP_ERR_NO_MEM;
        }
        *records = new_records;
        *capacity = new_capacity;
    }

    (*records)[*count] = *record;
    (*count)++;
    return ESP_OK;
}

static esp_err_t append_unique_manual_record(evt_manual_record_t **records,
                                             size_t *count,
                                             size_t *capacity,
                                             const evt_manual_record_t *record)
{
    if (!records || !count || !capacity || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < *count; i++) {
        if (event_recorder_manual_record_equals(&(*records)[i], record)) {
            return ESP_OK;
        }
    }

    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        evt_manual_record_t *new_records = realloc(*records, new_capacity * sizeof(evt_manual_record_t));
        if (!new_records) {
            return ESP_ERR_NO_MEM;
        }
        *records = new_records;
        *capacity = new_capacity;
    }

    (*records)[*count] = *record;
    (*count)++;
    return ESP_OK;
}

static esp_err_t append_unique_program_record(evt_program_record_t **records,
                                              size_t *count,
                                              size_t *capacity,
                                              const evt_program_record_t *record)
{
    if (!records || !count || !capacity || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    for (size_t i = 0; i < *count; i++) {
        if (event_recorder_program_record_equals(&(*records)[i], record)) {
            return ESP_OK;
        }
    }

    if (*count >= *capacity) {
        size_t new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
        evt_program_record_t *new_records = realloc(*records, new_capacity * sizeof(evt_program_record_t));
        if (!new_records) {
            return ESP_ERR_NO_MEM;
        }
        *records = new_records;
        *capacity = new_capacity;
    }

    (*records)[*count] = *record;
    (*count)++;
    return ESP_OK;
}

static esp_err_t parse_basic_record_line(const char *line, evt_record_t *record)
{
    char fields[CSV_BASIC_FIELD_COUNT][CSV_FIELD_BUF_SIZE];
    int field_count;

    if (!line || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    field_count = parse_csv_fields(line, fields, CSV_BASIC_FIELD_COUNT);
    if (field_count != CSV_BASIC_FIELD_COUNT) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    memset(record, 0, sizeof(*record));
    record->timestamp = strtoll(fields[0], NULL, 10);
    copy_text(record->desc, sizeof(record->desc), fields[1]);
    return ESP_OK;
}

static esp_err_t parse_manual_record_line(const char *line, evt_manual_record_t *record)
{
    char fields[CSV_MANUAL_FIELD_COUNT][CSV_FIELD_BUF_SIZE];
    int field_count;

    if (!line || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    field_count = parse_csv_fields(line, fields, CSV_MANUAL_FIELD_COUNT);
    if (field_count != CSV_MANUAL_FIELD_COUNT) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    memset(record, 0, sizeof(*record));
    record->start_ts = strtoll(fields[0], NULL, 10);
    record->planned_minutes = (uint16_t)strtoul(fields[1], NULL, 10);
    record->actual_minutes = (uint16_t)strtoul(fields[2], NULL, 10);
    copy_text(record->status, sizeof(record->status), fields[3]);
    copy_text(record->detail, sizeof(record->detail), fields[4]);
    return ESP_OK;
}

static esp_err_t parse_program_record_line(const char *line, evt_program_record_t *record)
{
    char fields[CSV_PROGRAM_FIELD_COUNT][CSV_FIELD_BUF_SIZE];
    int field_count;

    if (!line || !record) {
        return ESP_ERR_INVALID_ARG;
    }

    field_count = parse_csv_fields(line, fields, CSV_PROGRAM_FIELD_COUNT);
    if (field_count != CSV_PROGRAM_FIELD_COUNT) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    memset(record, 0, sizeof(*record));
    record->start_ts = strtoll(fields[0], NULL, 10);
    record->planned_minutes = (uint16_t)strtoul(fields[1], NULL, 10);
    record->actual_minutes = (uint16_t)strtoul(fields[2], NULL, 10);
    copy_text(record->program_name, sizeof(record->program_name), fields[3]);
    copy_text(record->trigger, sizeof(record->trigger), fields[4]);
    copy_text(record->status, sizeof(record->status), fields[5]);
    copy_text(record->detail, sizeof(record->detail), fields[6]);
    return ESP_OK;
}

static esp_err_t ensure_archive_mounted_locked(void)
{
    esp_err_t ret;

    if (s_archive_available) {
        return ESP_OK;
    }

    ret = bsp_sdcard_mount();
    if (ret == ESP_OK) {
        s_archive_available = true;
        ESP_LOGI(TAG, "TF archive mounted at %s", BSP_SD_MOUNT_POINT);
        return ESP_OK;
    }

    s_archive_available = false;
    ESP_LOGW(TAG, "TF archive mount failed: %s", esp_err_to_name(ret));
    return ret;
}

static esp_err_t ensure_directory_locked(const char *path)
{
    struct stat st = {0};

    if (!path || path[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? ESP_OK : ESP_FAIL;
    }

    if (mkdir(path, 0775) == 0) {
        return ESP_OK;
    }

    if (errno == EEXIST) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "mkdir failed for %s: errno=%d", path, errno);
    return ESP_FAIL;
}

static esp_err_t ensure_parent_directories_locked(const char *subdir)
{
    char path[160];
    esp_err_t ret;

    ret = ensure_directory_locked(BSP_SD_MOUNT_POINT "/" HISTORY_ROOT_DIR);
    if (ret != ESP_OK) {
        return ret;
    }

    snprintf(path, sizeof(path), "%s/%s/%s", BSP_SD_MOUNT_POINT, HISTORY_ROOT_DIR, subdir);
    return ensure_directory_locked(path);
}

static esp_err_t build_daily_file_path(const char *subdir, int64_t ts, char *path, size_t path_size)
{
    struct tm timeinfo = {0};
    time_t raw_time;

    if (!subdir || !path || path_size == 0 || ts <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    raw_time = (time_t)ts;
    if (localtime_r(&raw_time, &timeinfo) == NULL) {
        return ESP_FAIL;
    }

    snprintf(path, path_size,
             "%s/%s/%s/%04d-%02d-%02d.csv",
             BSP_SD_MOUNT_POINT,
             HISTORY_ROOT_DIR,
             subdir,
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday);
    return ESP_OK;
}

static bool basic_record_exists_in_file_locked(const char *path, const evt_record_t *record)
{
    FILE *fp;
    char line[CSV_LINE_BUF_SIZE];

    if (!path || !record) {
        return false;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        evt_record_t existing = {0};

        if (strncmp(line, "timestamp,", 10) == 0) {
            continue;
        }

        if (parse_basic_record_line(line, &existing) != ESP_OK) {
            continue;
        }

        if (existing.timestamp == record->timestamp &&
            strcmp(existing.desc, record->desc) == 0) {
            fclose(fp);
            return true;
        }
    }

    fclose(fp);
    return false;
}

static bool manual_record_exists_in_file_locked(const char *path, const evt_manual_record_t *record)
{
    FILE *fp;
    char line[CSV_LINE_BUF_SIZE];

    if (!path || !record) {
        return false;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        evt_manual_record_t existing = {0};

        if (strncmp(line, "start_ts,", 9) == 0) {
            continue;
        }

        if (parse_manual_record_line(line, &existing) != ESP_OK) {
            continue;
        }

        if (event_recorder_manual_record_equals(&existing, record)) {
            fclose(fp);
            return true;
        }
    }

    fclose(fp);
    return false;
}

static bool program_record_exists_in_file_locked(const char *path, const evt_program_record_t *record)
{
    FILE *fp;
    char line[CSV_LINE_BUF_SIZE];

    if (!path || !record) {
        return false;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return false;
    }

    while (fgets(line, sizeof(line), fp)) {
        evt_program_record_t existing = {0};

        if (strncmp(line, "start_ts,", 9) == 0) {
            continue;
        }

        if (parse_program_record_line(line, &existing) != ESP_OK) {
            continue;
        }

        if (event_recorder_program_record_equals(&existing, record)) {
            fclose(fp);
            return true;
        }
    }

    fclose(fp);
    return false;
}

static esp_err_t append_basic_record_locked(evt_type_t type, const evt_record_t *record)
{
    char path[192];
    char desc_escaped[132];
    struct stat st = {0};
    bool new_file;
    FILE *fp;
    int written;
    esp_err_t ret;
    const char *subdir = basic_type_subdir(type);

    if (!is_archive_basic_type(type) || !record || record->timestamp <= 0 || !subdir) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = ensure_archive_mounted_locked();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = ensure_parent_directories_locked(subdir);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = build_daily_file_path(subdir, record->timestamp, path, sizeof(path));
    if (ret != ESP_OK) {
        return ret;
    }

    if (basic_record_exists_in_file_locked(path, record)) {
        return ESP_OK;
    }

    new_file = (stat(path, &st) != 0);
    ret = csv_escape_field(record->desc, desc_escaped, sizeof(desc_escaped));
    if (ret != ESP_OK) {
        return ret;
    }

    fp = fopen(path, "a");
    if (!fp) {
        ESP_LOGW(TAG, "Failed to open %s for append: errno=%d", path, errno);
        return ESP_FAIL;
    }

    if (new_file || st.st_size == 0) {
        written = fprintf(fp, "timestamp,desc\n");
        if (written < 0) {
            fclose(fp);
            return ESP_FAIL;
        }
    }

    written = fprintf(fp, "%lld,%s\n", (long long)record->timestamp, desc_escaped);
    if (written < 0) {
        fclose(fp);
        return ESP_FAIL;
    }

    fclose(fp);
    return ESP_OK;
}

static esp_err_t append_manual_record_locked(const evt_manual_record_t *record)
{
    char path[192];
    char status_escaped[40];
    char detail_escaped[132];
    struct stat st = {0};
    bool new_file;
    FILE *fp;
    int written;
    esp_err_t ret;

    if (!record || record->start_ts <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = ensure_archive_mounted_locked();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = ensure_parent_directories_locked(HISTORY_MANUAL_DIR);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = build_daily_file_path(HISTORY_MANUAL_DIR, record->start_ts, path, sizeof(path));
    if (ret != ESP_OK) {
        return ret;
    }

    if (manual_record_exists_in_file_locked(path, record)) {
        return ESP_OK;
    }

    new_file = (stat(path, &st) != 0);
    ret = csv_escape_field(record->status, status_escaped, sizeof(status_escaped));
    if (ret != ESP_OK) {
        return ret;
    }
    ret = csv_escape_field(record->detail, detail_escaped, sizeof(detail_escaped));
    if (ret != ESP_OK) {
        return ret;
    }

    fp = fopen(path, "a");
    if (!fp) {
        ESP_LOGW(TAG, "Failed to open %s for append: errno=%d", path, errno);
        return ESP_FAIL;
    }

    if (new_file || st.st_size == 0) {
        written = fprintf(fp, "start_ts,planned_minutes,actual_minutes,status,detail\n");
        if (written < 0) {
            fclose(fp);
            return ESP_FAIL;
        }
    }

    written = fprintf(fp, "%lld,%u,%u,%s,%s\n",
                      (long long)record->start_ts,
                      record->planned_minutes,
                      record->actual_minutes,
                      status_escaped,
                      detail_escaped);
    if (written < 0) {
        fclose(fp);
        return ESP_FAIL;
    }

    fclose(fp);
    return ESP_OK;
}

static esp_err_t append_program_record_locked(const evt_program_record_t *record)
{
    char path[192];
    char program_name_escaped[68];
    char trigger_escaped[40];
    char status_escaped[40];
    char detail_escaped[132];
    struct stat st = {0};
    bool new_file;
    FILE *fp;
    int written;
    esp_err_t ret;

    if (!record || record->start_ts <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = ensure_archive_mounted_locked();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = ensure_parent_directories_locked(HISTORY_PROGRAM_DIR);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = build_daily_file_path(HISTORY_PROGRAM_DIR, record->start_ts, path, sizeof(path));
    if (ret != ESP_OK) {
        return ret;
    }

    if (program_record_exists_in_file_locked(path, record)) {
        return ESP_OK;
    }

    new_file = (stat(path, &st) != 0);
    ret = csv_escape_field(record->program_name, program_name_escaped, sizeof(program_name_escaped));
    if (ret != ESP_OK) {
        return ret;
    }
    ret = csv_escape_field(record->trigger, trigger_escaped, sizeof(trigger_escaped));
    if (ret != ESP_OK) {
        return ret;
    }
    ret = csv_escape_field(record->status, status_escaped, sizeof(status_escaped));
    if (ret != ESP_OK) {
        return ret;
    }
    ret = csv_escape_field(record->detail, detail_escaped, sizeof(detail_escaped));
    if (ret != ESP_OK) {
        return ret;
    }

    fp = fopen(path, "a");
    if (!fp) {
        ESP_LOGW(TAG, "Failed to open %s for append: errno=%d", path, errno);
        return ESP_FAIL;
    }

    if (new_file || st.st_size == 0) {
        written = fprintf(fp, "start_ts,planned_minutes,actual_minutes,program_name,trigger,status,detail\n");
        if (written < 0) {
            fclose(fp);
            return ESP_FAIL;
        }
    }

    written = fprintf(fp, "%lld,%u,%u,%s,%s,%s,%s\n",
                      (long long)record->start_ts,
                      record->planned_minutes,
                      record->actual_minutes,
                      program_name_escaped,
                      trigger_escaped,
                      status_escaped,
                      detail_escaped);
    if (written < 0) {
        fclose(fp);
        return ESP_FAIL;
    }

    fclose(fp);
    return ESP_OK;
}

static esp_err_t query_basic_file_locked(const char *path,
                                         int64_t start_ts,
                                         int64_t end_ts,
                                         evt_record_t **records,
                                         size_t *count,
                                         size_t *capacity)
{
    FILE *fp;
    char line[CSV_LINE_BUF_SIZE];

    fp = fopen(path, "r");
    if (!fp) {
        return ESP_FAIL;
    }

    while (fgets(line, sizeof(line), fp)) {
        evt_record_t record = {0};
        esp_err_t ret;

        if (strncmp(line, "timestamp,", 10) == 0) {
            continue;
        }

        ret = parse_basic_record_line(line, &record);
        if (ret != ESP_OK) {
            continue;
        }

        if (!timestamp_matches_range(record.timestamp, start_ts, end_ts)) {
            continue;
        }

        ret = append_unique_basic_record(records, count, capacity, &record);
        if (ret != ESP_OK) {
            fclose(fp);
            return ret;
        }
    }

    fclose(fp);
    return ESP_OK;
}

static esp_err_t query_manual_file_locked(const char *path,
                                          int64_t start_ts,
                                          int64_t end_ts,
                                          evt_status_filter_t status_filter,
                                          evt_manual_record_t **records,
                                          size_t *count,
                                          size_t *capacity)
{
    FILE *fp;
    char line[CSV_LINE_BUF_SIZE];

    fp = fopen(path, "r");
    if (!fp) {
        return ESP_FAIL;
    }

    while (fgets(line, sizeof(line), fp)) {
        evt_manual_record_t record = {0};
        esp_err_t ret;

        if (strncmp(line, "start_ts,", 9) == 0) {
            continue;
        }

        ret = parse_manual_record_line(line, &record);
        if (ret != ESP_OK) {
            continue;
        }

        if (!timestamp_matches_range(record.start_ts, start_ts, end_ts)) {
            continue;
        }
        if (!status_matches_filter(record.status, status_filter)) {
            continue;
        }

        ret = append_unique_manual_record(records, count, capacity, &record);
        if (ret != ESP_OK) {
            fclose(fp);
            return ret;
        }
    }

    fclose(fp);
    return ESP_OK;
}

static esp_err_t query_program_file_locked(const char *path,
                                           int64_t start_ts,
                                           int64_t end_ts,
                                           evt_status_filter_t status_filter,
                                           evt_program_record_t **records,
                                           size_t *count,
                                           size_t *capacity)
{
    FILE *fp;
    char line[CSV_LINE_BUF_SIZE];

    fp = fopen(path, "r");
    if (!fp) {
        return ESP_FAIL;
    }

    while (fgets(line, sizeof(line), fp)) {
        evt_program_record_t record = {0};
        esp_err_t ret;

        if (strncmp(line, "start_ts,", 9) == 0) {
            continue;
        }

        ret = parse_program_record_line(line, &record);
        if (ret != ESP_OK) {
            continue;
        }

        if (!timestamp_matches_range(record.start_ts, start_ts, end_ts)) {
            continue;
        }
        if (!status_matches_filter(record.status, status_filter)) {
            continue;
        }

        ret = append_unique_program_record(records, count, capacity, &record);
        if (ret != ESP_OK) {
            fclose(fp);
            return ret;
        }
    }

    fclose(fp);
    return ESP_OK;
}

static esp_err_t query_basic_records_locked(evt_type_t type,
                                            int64_t start_ts,
                                            int64_t end_ts,
                                            evt_record_t **records,
                                            size_t *count)
{
    char dir_path[160];
    DIR *dir;
    struct dirent *entry;
    size_t capacity = 0;
    const char *subdir = basic_type_subdir(type);

    if (!is_archive_basic_type(type) || !subdir) {
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(dir_path, sizeof(dir_path), "%s/%s/%s", BSP_SD_MOUNT_POINT, HISTORY_ROOT_DIR, subdir);

    dir = opendir(dir_path);
    if (!dir) {
        return ESP_OK;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[192];
        esp_err_t ret;

        if (!has_csv_suffix(entry->d_name)) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        ret = query_basic_file_locked(path, start_ts, end_ts, records, count, &capacity);
        if (ret != ESP_OK) {
            closedir(dir);
            free(*records);
            *records = NULL;
            *count = 0;
            return ret;
        }
    }

    closedir(dir);

    if (*count > 1) {
        qsort(*records, *count, sizeof(evt_record_t), compare_basic_records_desc);
    }

    return ESP_OK;
}

static esp_err_t query_manual_records_locked(int64_t start_ts, int64_t end_ts,
                                             evt_status_filter_t status_filter,
                                             evt_manual_record_t **records,
                                             size_t *count)
{
    char dir_path[160];
    DIR *dir;
    struct dirent *entry;
    size_t capacity = 0;

    snprintf(dir_path, sizeof(dir_path), "%s/%s/%s", BSP_SD_MOUNT_POINT, HISTORY_ROOT_DIR, HISTORY_MANUAL_DIR);

    dir = opendir(dir_path);
    if (!dir) {
        return ESP_OK;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[192];
        esp_err_t ret;

        if (!has_csv_suffix(entry->d_name)) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        ret = query_manual_file_locked(path, start_ts, end_ts, status_filter, records, count, &capacity);
        if (ret != ESP_OK) {
            closedir(dir);
            free(*records);
            *records = NULL;
            *count = 0;
            return ret;
        }
    }

    closedir(dir);

    if (*count > 1) {
        qsort(*records, *count, sizeof(evt_manual_record_t), compare_manual_records_desc);
    }

    return ESP_OK;
}

static esp_err_t query_program_records_locked(int64_t start_ts, int64_t end_ts,
                                              evt_status_filter_t status_filter,
                                              evt_program_record_t **records,
                                              size_t *count)
{
    char dir_path[160];
    DIR *dir;
    struct dirent *entry;
    size_t capacity = 0;

    snprintf(dir_path, sizeof(dir_path), "%s/%s/%s", BSP_SD_MOUNT_POINT, HISTORY_ROOT_DIR, HISTORY_PROGRAM_DIR);

    dir = opendir(dir_path);
    if (!dir) {
        return ESP_OK;
    }

    while ((entry = readdir(dir)) != NULL) {
        char path[192];
        esp_err_t ret;

        if (!has_csv_suffix(entry->d_name)) {
            continue;
        }

        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        ret = query_program_file_locked(path, start_ts, end_ts, status_filter, records, count, &capacity);
        if (ret != ESP_OK) {
            closedir(dir);
            free(*records);
            *records = NULL;
            *count = 0;
            return ret;
        }
    }

    closedir(dir);

    if (*count > 1) {
        qsort(*records, *count, sizeof(evt_program_record_t), compare_program_records_desc);
    }

    return ESP_OK;
}

static void archive_task(void *param)
{
    archive_msg_t msg;

    (void)param;

    while (xQueueReceive(s_archive_queue, &msg, portMAX_DELAY) == pdTRUE) {
        esp_err_t ret;

        xSemaphoreTake(s_archive_mutex, portMAX_DELAY);
        if (msg.type == ARCHIVE_MSG_BASIC) {
            ret = append_basic_record_locked(msg.basic_type, &msg.data.basic);
        } else if (msg.type == ARCHIVE_MSG_MANUAL) {
            ret = append_manual_record_locked(&msg.data.manual);
        } else {
            ret = append_program_record_locked(&msg.data.program);
        }
        xSemaphoreGive(s_archive_mutex);

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Archive write failed: %s", esp_err_to_name(ret));
        }
    }

    vTaskDelete(NULL);
}

esp_err_t history_archive_init(void)
{
    BaseType_t task_ok;

    if (s_initialized) {
        return ESP_OK;
    }

    s_archive_mutex = xSemaphoreCreateMutex();
    if (!s_archive_mutex) {
        return ESP_ERR_NO_MEM;
    }

    s_archive_queue = xQueueCreate(HISTORY_QUEUE_LEN, sizeof(archive_msg_t));
    if (!s_archive_queue) {
        vSemaphoreDelete(s_archive_mutex);
        s_archive_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    xSemaphoreTake(s_archive_mutex, portMAX_DELAY);
    (void)ensure_archive_mounted_locked();
    xSemaphoreGive(s_archive_mutex);

    task_ok = xTaskCreate(archive_task,
                          "hist_archive",
                          HISTORY_TASK_STACK_SIZE,
                          NULL,
                          3,
                          NULL);
    if (task_ok != pdPASS) {
        vQueueDelete(s_archive_queue);
        s_archive_queue = NULL;
        vSemaphoreDelete(s_archive_mutex);
        s_archive_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    return ESP_OK;
}

bool history_archive_is_available(void)
{
    bool available;

    if (!s_initialized || !s_archive_mutex) {
        return false;
    }

    xSemaphoreTake(s_archive_mutex, portMAX_DELAY);
    available = s_archive_available;
    xSemaphoreGive(s_archive_mutex);
    return available;
}

esp_err_t history_archive_enqueue_basic_record(evt_type_t type, const evt_record_t *record)
{
    archive_msg_t msg = {0};

    if (!s_initialized || !s_archive_queue || !is_archive_basic_type(type) || !record) {
        return ESP_ERR_INVALID_STATE;
    }

    msg.type = ARCHIVE_MSG_BASIC;
    msg.basic_type = type;
    msg.data.basic = *record;

    if (xQueueSend(s_archive_queue, &msg, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t history_archive_enqueue_manual_record(const evt_manual_record_t *record)
{
    archive_msg_t msg = {0};

    if (!s_initialized || !s_archive_queue || !record) {
        return ESP_ERR_INVALID_STATE;
    }

    msg.type = ARCHIVE_MSG_MANUAL;
    msg.data.manual = *record;

    if (xQueueSend(s_archive_queue, &msg, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t history_archive_enqueue_program_record(const evt_program_record_t *record)
{
    archive_msg_t msg = {0};

    if (!s_initialized || !s_archive_queue || !record) {
        return ESP_ERR_INVALID_STATE;
    }

    msg.type = ARCHIVE_MSG_PROGRAM;
    msg.data.program = *record;

    if (xQueueSend(s_archive_queue, &msg, 0) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t history_archive_sync_basic_records(evt_type_t type, const evt_record_t *records, size_t count)
{
    esp_err_t ret = ESP_OK;

    if (!s_initialized || !is_archive_basic_type(type)) {
        return ESP_ERR_INVALID_STATE;
    }
    if (count > 0 && !records) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_archive_mutex, portMAX_DELAY);

    for (size_t i = 0; i < count; i++) {
        if (records[i].timestamp <= 0) {
            continue;
        }
        ret = append_basic_record_locked(type, &records[i]);
        if (ret != ESP_OK) {
            break;
        }
    }

    xSemaphoreGive(s_archive_mutex);
    return ret;
}

esp_err_t history_archive_sync_manual_records(const evt_manual_record_t *records, size_t count)
{
    esp_err_t ret = ESP_OK;

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (count > 0 && !records) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_archive_mutex, portMAX_DELAY);

    for (size_t i = 0; i < count; i++) {
        if (records[i].start_ts <= 0) {
            continue;
        }
        ret = append_manual_record_locked(&records[i]);
        if (ret != ESP_OK) {
            break;
        }
    }

    xSemaphoreGive(s_archive_mutex);
    return ret;
}

esp_err_t history_archive_sync_program_records(const evt_program_record_t *records, size_t count)
{
    esp_err_t ret = ESP_OK;

    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (count > 0 && !records) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_archive_mutex, portMAX_DELAY);

    for (size_t i = 0; i < count; i++) {
        if (records[i].start_ts <= 0) {
            continue;
        }
        ret = append_program_record_locked(&records[i]);
        if (ret != ESP_OK) {
            break;
        }
    }

    xSemaphoreGive(s_archive_mutex);
    return ret;
}

esp_err_t history_archive_query_basic_records(evt_type_t type, int64_t start_ts, int64_t end_ts,
                                              evt_record_t **records, size_t *count)
{
    esp_err_t ret;

    if (!s_initialized || !records || !count || !is_archive_basic_type(type)) {
        return ESP_ERR_INVALID_ARG;
    }

    *records = NULL;
    *count = 0;

    xSemaphoreTake(s_archive_mutex, portMAX_DELAY);
    ret = ensure_archive_mounted_locked();
    if (ret == ESP_OK) {
        ret = query_basic_records_locked(type, start_ts, end_ts, records, count);
    }
    xSemaphoreGive(s_archive_mutex);

    if (ret != ESP_OK) {
        free(*records);
        *records = NULL;
        *count = 0;
        return ret;
    }

    return ESP_OK;
}

esp_err_t history_archive_query_manual_records(int64_t start_ts, int64_t end_ts,
                                               evt_status_filter_t status_filter,
                                               evt_manual_record_t **records,
                                               size_t *count)
{
    esp_err_t ret;

    if (!s_initialized || !records || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    *records = NULL;
    *count = 0;

    xSemaphoreTake(s_archive_mutex, portMAX_DELAY);
    ret = ensure_archive_mounted_locked();
    if (ret == ESP_OK) {
        ret = query_manual_records_locked(start_ts, end_ts, status_filter, records, count);
    }
    xSemaphoreGive(s_archive_mutex);

    if (ret != ESP_OK) {
        free(*records);
        *records = NULL;
        *count = 0;
        return ret;
    }

    return ESP_OK;
}

esp_err_t history_archive_query_program_records(int64_t start_ts, int64_t end_ts,
                                                evt_status_filter_t status_filter,
                                                evt_program_record_t **records,
                                                size_t *count)
{
    esp_err_t ret;

    if (!s_initialized || !records || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    *records = NULL;
    *count = 0;

    xSemaphoreTake(s_archive_mutex, portMAX_DELAY);
    ret = ensure_archive_mounted_locked();
    if (ret == ESP_OK) {
        ret = query_program_records_locked(start_ts, end_ts, status_filter, records, count);
    }
    xSemaphoreGive(s_archive_mutex);

    if (ret != ESP_OK) {
        free(*records);
        *records = NULL;
        *count = 0;
        return ret;
    }

    return ESP_OK;
}

void history_archive_free_query_result(void *records)
{
    free(records);
}
