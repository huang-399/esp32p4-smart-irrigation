#include "irrigation_scheduler.h"

#include "device_registry.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "zigbee_bridge.h"
#include "event_recorder.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

esp_err_t irrigation_store_init(void);

static const char *TAG = "irr_sched";

static irr_runtime_status_t s_runtime = {
    .auto_enabled = true,
    .busy = false,
    .program_active = false,
    .manual_irrigation_active = false,
    .active_program_index = -1,
    .active_name = "",
    .status_text = "无手动轮灌&无程序运行",
    .total_duration = 0,
    .elapsed_seconds = 0,
};

static bool s_time_valid = false;
static bool s_task_started = false;
static TaskHandle_t s_scheduler_task = NULL;
static time_t s_runtime_started_at = 0;
static time_t s_runtime_deadline_at = 0;
static int s_last_fire_yyyymmdd[IRR_MAX_PROGRAMS][IRR_MAX_PERIODS];
static uint16_t s_active_valve_ids[DEV_REG_MAX_VALVES];
static int s_active_valve_count = 0;

typedef struct {
    int program_index;
    int period_index;
    int fire_yyyymmdd;
} irr_queue_item_t;

static irr_queue_item_t s_queue[IRR_SCHED_QUEUE_LEN];
static int s_queue_count = 0;

typedef struct {
    bool active;
    int64_t start_ts;
    uint16_t planned_minutes;
    int pre_water;
    int post_water;
    char formula[32];
} manual_run_ctx_t;

typedef struct {
    bool active;
    int64_t start_ts;
    uint16_t planned_minutes;
    int valve_count;
    char program_name[32];
    char trigger[16];
    char source[16];
} program_run_ctx_t;

static manual_run_ctx_t s_manual_run_ctx;
static program_run_ctx_t s_program_run_ctx;
static bool s_finishing_normally = false;

static void update_elapsed_seconds(void);

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
}

static const char *map_trigger_text(const char *source)
{
    if (!source) {
        return "未知";
    }
    if (strcmp(source, "manual") == 0) {
        return "手动";
    }
    if (strcmp(source, "auto") == 0) {
        return "自动";
    }
    if (strcmp(source, "queued") == 0) {
        return "排队";
    }
    return "未知";
}

static uint16_t elapsed_seconds_to_minutes(int elapsed_seconds)
{
    if (elapsed_seconds <= 0) {
        return 0;
    }
    return (uint16_t)((elapsed_seconds + 59) / 60);
}

static void clear_run_contexts(void)
{
    memset(&s_manual_run_ctx, 0, sizeof(s_manual_run_ctx));
    memset(&s_program_run_ctx, 0, sizeof(s_program_run_ctx));
}

static void persist_abnormal_manual_record(int64_t start_ts,
                                          uint16_t planned_minutes,
                                          uint16_t actual_minutes,
                                          int pre_water,
                                          int post_water,
                                          const char *formula,
                                          const char *status,
                                          const char *detail)
{
    evt_manual_record_t record = {0};

    record.start_ts = start_ts;
    record.planned_minutes = planned_minutes;
    record.actual_minutes = actual_minutes;
    copy_text(record.status, sizeof(record.status), status);
    snprintf(record.detail, sizeof(record.detail),
             "前清水%d分 后清水%d分",
             pre_water,
             post_water);
    if (formula && formula[0] != '\0') {
        size_t len = strlen(record.detail);
        if (len < sizeof(record.detail) - 1) {
            copy_text(record.detail + len,
                      sizeof(record.detail) - len,
                      " 配方:");
            len = strlen(record.detail);
        }
        if (len < sizeof(record.detail) - 1) {
            copy_text(record.detail + len,
                      sizeof(record.detail) - len,
                      formula);
            len = strlen(record.detail);
        }
    }
    if (detail && detail[0] != '\0') {
        size_t len = strlen(record.detail);
        if (len < sizeof(record.detail) - 1) {
            copy_text(record.detail + len,
                      sizeof(record.detail) - len,
                      " ");
            len = strlen(record.detail);
        }
        if (len < sizeof(record.detail) - 1) {
            copy_text(record.detail + len,
                      sizeof(record.detail) - len,
                      detail);
        }
    }
    event_recorder_add_manual_record(&record);
}

static void persist_abnormal_program_record(const char *program_name,
                                           const char *trigger,
                                           int64_t start_ts,
                                           uint16_t planned_minutes,
                                           uint16_t actual_minutes,
                                           int valve_count,
                                           const char *source,
                                           const char *status,
                                           const char *detail)
{
    evt_program_record_t record = {0};

    record.start_ts = start_ts;
    record.planned_minutes = planned_minutes;
    record.actual_minutes = actual_minutes;
    copy_text(record.program_name, sizeof(record.program_name), program_name);
    copy_text(record.trigger, sizeof(record.trigger), trigger);
    copy_text(record.status, sizeof(record.status), status);
    snprintf(record.detail, sizeof(record.detail), "阀门数:%d 来源:", valve_count);
    {
        size_t len = strlen(record.detail);
        if (len < sizeof(record.detail) - 1) {
            copy_text(record.detail + len,
                      sizeof(record.detail) - len,
                      (source && source[0] != '\0') ? source : "unknown");
            len = strlen(record.detail);
        }
        if (detail && detail[0] != '\0' && len < sizeof(record.detail) - 1) {
            copy_text(record.detail + len,
                      sizeof(record.detail) - len,
                      " ");
            len = strlen(record.detail);
        }
        if (detail && detail[0] != '\0' && len < sizeof(record.detail) - 1) {
            copy_text(record.detail + len,
                      sizeof(record.detail) - len,
                      detail);
        }
    }
    event_recorder_add_program_record(&record);
}

static void persist_interrupted_run_if_needed(void)
{
    if (!s_runtime.busy || s_finishing_normally) {
        return;
    }

    update_elapsed_seconds();

    if (s_runtime.manual_irrigation_active && s_manual_run_ctx.active) {
        persist_abnormal_manual_record(s_manual_run_ctx.start_ts,
                                       s_manual_run_ctx.planned_minutes,
                                       elapsed_seconds_to_minutes(s_runtime.elapsed_seconds),
                                       s_manual_run_ctx.pre_water,
                                       s_manual_run_ctx.post_water,
                                       s_manual_run_ctx.formula,
                                       "中断结束",
                                       "运行提前结束");
        return;
    }

    if (s_runtime.program_active && s_program_run_ctx.active) {
        persist_abnormal_program_record(s_program_run_ctx.program_name,
                                        s_program_run_ctx.trigger,
                                        s_program_run_ctx.start_ts,
                                        s_program_run_ctx.planned_minutes,
                                        elapsed_seconds_to_minutes(s_runtime.elapsed_seconds),
                                        s_program_run_ctx.valve_count,
                                        s_program_run_ctx.source,
                                        "中断结束",
                                        "运行提前结束");
    }
}

static void persist_completed_run(void)
{
    if (s_runtime.manual_irrigation_active && s_manual_run_ctx.active) {
        evt_manual_record_t record = {0};

        record.start_ts = s_manual_run_ctx.start_ts;
        record.planned_minutes = s_manual_run_ctx.planned_minutes;
        record.actual_minutes = elapsed_seconds_to_minutes(s_runtime.elapsed_seconds);
        copy_text(record.status, sizeof(record.status), "正常");
        snprintf(record.detail, sizeof(record.detail),
                 "前清水%d分 后清水%d分",
                 s_manual_run_ctx.pre_water,
                 s_manual_run_ctx.post_water);
        if (s_manual_run_ctx.formula[0] != '\0') {
            size_t len = strlen(record.detail);
            if (len < sizeof(record.detail) - 1) {
                copy_text(record.detail + len,
                          sizeof(record.detail) - len,
                          " 配方:");
                len = strlen(record.detail);
            }
            if (len < sizeof(record.detail) - 1) {
                copy_text(record.detail + len,
                          sizeof(record.detail) - len,
                          s_manual_run_ctx.formula);
            }
        }
        event_recorder_add_manual_record(&record);
        return;
    }

    if (s_runtime.program_active && s_program_run_ctx.active) {
        evt_program_record_t record = {0};

        record.start_ts = s_program_run_ctx.start_ts;
        record.planned_minutes = s_program_run_ctx.planned_minutes;
        record.actual_minutes = elapsed_seconds_to_minutes(s_runtime.elapsed_seconds);
        copy_text(record.program_name, sizeof(record.program_name), s_program_run_ctx.program_name);
        copy_text(record.trigger, sizeof(record.trigger), s_program_run_ctx.trigger);
        copy_text(record.status, sizeof(record.status), "正常");
        snprintf(record.detail, sizeof(record.detail), "阀门数:%d 来源:",
                 s_program_run_ctx.valve_count);
        {
            size_t len = strlen(record.detail);
            if (len < sizeof(record.detail) - 1) {
                copy_text(record.detail + len,
                          sizeof(record.detail) - len,
                          s_program_run_ctx.source[0] != '\0' ? s_program_run_ctx.source : "unknown");
            }
        }
        event_recorder_add_program_record(&record);
    }
}

static void update_elapsed_seconds(void)
{
    if (!s_runtime.busy || s_runtime_started_at == 0) {
        s_runtime.elapsed_seconds = 0;
        return;
    }

    time_t now = time(NULL);
    if (now <= s_runtime_started_at) {
        s_runtime.elapsed_seconds = 0;
        return;
    }

    s_runtime.elapsed_seconds = (int)(now - s_runtime_started_at);
}

static bool valve_id_exists(const uint16_t *ids, int count, uint16_t valve_id)
{
    for (int i = 0; i < count; i++) {
        if (ids[i] == valve_id) {
            return true;
        }
    }

    return false;
}

static int collect_program_valve_ids(const irr_program_t *program, uint16_t *out_ids, int max_ids)
{
    int count = 0;
    const dev_valve_info_t *valves = valve_registry_get_all();
    const dev_zone_info_t *zones = zone_registry_get_all();

    if (!program || !out_ids || max_ids <= 0) {
        return 0;
    }

    for (int i = 0; i < 10 && count < max_ids; i++) {
        if (!program->selected_valves[i]) {
            continue;
        }
        if (i >= DEV_REG_MAX_VALVES || !valves[i].valid) {
            continue;
        }
        if (valves[i].id == 0 || valve_id_exists(out_ids, count, valves[i].id)) {
            continue;
        }
        out_ids[count++] = valves[i].id;
    }

    for (int i = 0; i < 10 && count < max_ids; i++) {
        if (!program->selected_zones[i]) {
            continue;
        }
        if (i >= DEV_REG_MAX_ZONES || !zones[i].valid) {
            continue;
        }

        for (int j = 0; j < zones[i].valve_count && count < max_ids; j++) {
            uint16_t valve_id = zones[i].valve_ids[j];
            if (valve_id == 0 || valve_id_exists(out_ids, count, valve_id)) {
                continue;
            }
            out_ids[count++] = valve_id;
        }
    }

    return count;
}

static esp_err_t control_valve_id(uint16_t valve_id, bool on)
{
    const dev_valve_info_t *valves = valve_registry_get_all();

    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        zb_control_target_t target;
        uint32_t point_id;

        if (!valves[i].valid || valves[i].id != valve_id) {
            continue;
        }

        point_id = (2000U + (uint32_t)valves[i].channel) * 100U + 1U;
        if (!zigbee_bridge_resolve_control_target(point_id, &target)) {
            ESP_LOGW(TAG, "Skip valve control, unsupported point_id=%lu valve_id=%u channel=%u",
                     (unsigned long)point_id, valve_id, valves[i].channel);
            return ESP_ERR_NOT_SUPPORTED;
        }

        return zigbee_bridge_send_control(target.dev_type, target.dev_id, on);
    }

    ESP_LOGW(TAG, "Valve not found: id=%u", valve_id);
    return ESP_ERR_NOT_FOUND;
}

static void stop_active_valves(void)
{
    for (int i = 0; i < s_active_valve_count; i++) {
        esp_err_t ret = control_valve_id(s_active_valve_ids[i], false);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Close valve failed: id=%u err=%s",
                     s_active_valve_ids[i], esp_err_to_name(ret));
        }
    }

    memset(s_active_valve_ids, 0, sizeof(s_active_valve_ids));
    s_active_valve_count = 0;
}

static bool apply_program_targets(const irr_program_t *program)
{
    uint16_t valve_ids[DEV_REG_MAX_VALVES] = {0};
    int valve_count = collect_program_valve_ids(program, valve_ids, DEV_REG_MAX_VALVES);
    bool any_success = false;

    if (valve_count <= 0) {
        ESP_LOGW(TAG, "No real valves resolved for program");
        return false;
    }

    memset(s_active_valve_ids, 0, sizeof(s_active_valve_ids));
    s_active_valve_count = 0;

    for (int i = 0; i < valve_count; i++) {
        esp_err_t ret = control_valve_id(valve_ids[i], true);
        if (ret == ESP_OK) {
            s_active_valve_ids[s_active_valve_count++] = valve_ids[i];
            any_success = true;
        } else {
            ESP_LOGW(TAG, "Open valve failed: id=%u err=%s",
                     valve_ids[i], esp_err_to_name(ret));
        }
    }

    if (!any_success) {
        stop_active_valves();
        return false;
    }

    return true;
}

static void set_idle_status(void)
{
    persist_interrupted_run_if_needed();
    stop_active_valves();
    s_runtime.busy = false;
    s_runtime.program_active = false;
    s_runtime.manual_irrigation_active = false;
    s_runtime.active_program_index = -1;
    s_runtime.active_name[0] = '\0';
    s_runtime.total_duration = 0;
    s_runtime.elapsed_seconds = 0;
    s_runtime_started_at = 0;
    s_runtime_deadline_at = 0;
    clear_run_contexts();
    copy_text(s_runtime.status_text, sizeof(s_runtime.status_text), "无手动轮灌&无程序运行");
}

static int make_yyyymmdd(const struct tm *timeinfo)
{
    if (!timeinfo) {
        return 0;
    }

    return (timeinfo->tm_year + 1900) * 10000
         + (timeinfo->tm_mon + 1) * 100
         + timeinfo->tm_mday;
}

static bool parse_date_yyyymmdd(const char *text, int *out)
{
    int year = 0;
    int month = 0;
    int day = 0;

    if (!text || text[0] == '\0' || !out) {
        return false;
    }

    if (sscanf(text, "%d-%d-%d", &year, &month, &day) != 3) {
        return false;
    }

    if (year < 2024 || month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }

    *out = year * 10000 + month * 100 + day;
    return true;
}

static bool parse_period_time(const char *text, int *hour, int *minute)
{
    int h = 0;
    int m = 0;
    int s = 0;
    int matched = 0;

    if (!text || text[0] == '\0' || !hour || !minute) {
        return false;
    }

    matched = sscanf(text, "%d:%d:%d", &h, &m, &s);
    if (matched < 2) {
        return false;
    }

    if (h < 0 || h > 23 || m < 0 || m > 59) {
        return false;
    }

    *hour = h;
    *minute = m;
    return true;
}

static bool program_has_targets(const irr_program_t *program)
{
    if (!program) {
        return false;
    }

    for (int i = 0; i < 10; i++) {
        if (program->selected_zones[i] || program->selected_valves[i]) {
            return true;
        }
    }

    return false;
}

static int get_effective_duration_minutes(const irr_program_t *program)
{
    int duration = 0;

    if (!program) {
        return 1;
    }

    duration = program->total_duration;
    if (duration <= 0) {
        duration = program->pre_water + program->post_water;
    }
    if (duration <= 0) {
        duration = 1;
    }

    return duration;
}

static bool is_program_timed_mode(const irr_program_t *program)
{
    if (!program) {
        return false;
    }

    if (program->condition[0] == '\0') {
        return true;
    }

    return strcmp(program->condition, "定时") == 0;
}

static bool is_program_in_date_range(const irr_program_t *program, int yyyymmdd)
{
    int start_date = 0;
    int end_date = 0;
    bool has_start = false;
    bool has_end = false;

    if (!program) {
        return false;
    }

    has_start = parse_date_yyyymmdd(program->start_date, &start_date);
    has_end = parse_date_yyyymmdd(program->end_date, &end_date);

    if (has_start && yyyymmdd < start_date) {
        return false;
    }
    if (has_end && yyyymmdd > end_date) {
        return false;
    }

    return true;
}

static bool program_can_auto_run_now(const irr_program_t *program, int yyyymmdd)
{
    if (!program) {
        return false;
    }

    if (!program->auto_enabled) {
        return false;
    }

    if (!is_program_timed_mode(program)) {
        return false;
    }

    if (!program_has_targets(program)) {
        return false;
    }

    if (program->period_count <= 0) {
        return false;
    }

    return is_program_in_date_range(program, yyyymmdd);
}

static bool queue_contains(int program_index, int period_index, int fire_yyyymmdd)
{
    for (int i = 0; i < s_queue_count; i++) {
        if (s_queue[i].program_index == program_index
            && s_queue[i].period_index == period_index
            && s_queue[i].fire_yyyymmdd == fire_yyyymmdd) {
            return true;
        }
    }

    return false;
}

static bool enqueue_program(int program_index, int period_index, int fire_yyyymmdd)
{
    if (queue_contains(program_index, period_index, fire_yyyymmdd)) {
        return true;
    }

    if (s_queue_count >= IRR_SCHED_QUEUE_LEN) {
        ESP_LOGW(TAG, "Queue full, drop program index=%d period=%d day=%d",
                 program_index, period_index, fire_yyyymmdd);
        return false;
    }

    s_queue[s_queue_count].program_index = program_index;
    s_queue[s_queue_count].period_index = period_index;
    s_queue[s_queue_count].fire_yyyymmdd = fire_yyyymmdd;
    s_queue_count++;
    ESP_LOGI(TAG, "Program queued: index=%d period=%d queue=%d",
             program_index, period_index, s_queue_count);
    return true;
}

static bool dequeue_program(irr_queue_item_t *out)
{
    if (!out || s_queue_count <= 0) {
        return false;
    }

    *out = s_queue[0];
    if (s_queue_count > 1) {
        memmove(&s_queue[0], &s_queue[1], sizeof(s_queue[0]) * (s_queue_count - 1));
    }
    s_queue_count--;
    return true;
}

static void clear_queued_programs(void)
{
    memset(s_queue, 0, sizeof(s_queue));
    s_queue_count = 0;
}

static bool start_program_internal(int index, const char *source)
{
    irr_program_t program = {0};
    int duration_minutes = 0;
    time_t now = 0;
    const char *trigger = map_trigger_text(source);

    if (!irrigation_scheduler_get_program(index, &program)) {
        ESP_LOGW(TAG, "Start program failed, invalid index=%d", index);
        persist_abnormal_program_record("未知程序",
                                        trigger,
                                        (int64_t)time(NULL),
                                        0,
                                        0,
                                        0,
                                        source,
                                        "启动失败",
                                        "程序索引无效");
        return false;
    }

    duration_minutes = get_effective_duration_minutes(&program);
    now = time(NULL);

    if (!program_has_targets(&program)) {
        ESP_LOGW(TAG, "Start program failed, no targets selected: index=%d", index);
        persist_abnormal_program_record(program.name,
                                        trigger,
                                        (int64_t)now,
                                        (uint16_t)duration_minutes,
                                        0,
                                        0,
                                        source,
                                        "启动失败",
                                        "程序无可执行目标");
        return false;
    }

    if (!apply_program_targets(&program)) {
        ESP_LOGW(TAG, "Start program failed, target actuation failed: index=%d", index);
        persist_abnormal_program_record(program.name,
                                        trigger,
                                        (int64_t)now,
                                        (uint16_t)duration_minutes,
                                        0,
                                        0,
                                        source,
                                        "启动失败",
                                        "未打开任何阀门");
        return false;
    }

    s_runtime.busy = true;
    s_runtime.program_active = true;
    s_runtime.manual_irrigation_active = false;
    s_runtime.active_program_index = index;
    s_runtime.total_duration = duration_minutes;
    s_runtime.elapsed_seconds = 0;
    s_runtime_started_at = now;
    s_runtime_deadline_at = now + (time_t)duration_minutes * 60;
    copy_text(s_runtime.active_name, sizeof(s_runtime.active_name), program.name);
    snprintf(s_runtime.status_text, sizeof(s_runtime.status_text), "程序运行中：%s", program.name);

    memset(&s_manual_run_ctx, 0, sizeof(s_manual_run_ctx));
    memset(&s_program_run_ctx, 0, sizeof(s_program_run_ctx));
    s_program_run_ctx.active = true;
    s_program_run_ctx.start_ts = (int64_t)now;
    s_program_run_ctx.planned_minutes = (uint16_t)duration_minutes;
    s_program_run_ctx.valve_count = s_active_valve_count;
    copy_text(s_program_run_ctx.program_name, sizeof(s_program_run_ctx.program_name), program.name);
    copy_text(s_program_run_ctx.trigger, sizeof(s_program_run_ctx.trigger), trigger);
    copy_text(s_program_run_ctx.source, sizeof(s_program_run_ctx.source), source ? source : "unknown");

    ESP_LOGI(TAG, "Program started[%s]: index=%d name=%s total=%d valves=%d",
             source ? source : "unknown", index, program.name, duration_minutes, s_active_valve_count);
    return true;
}

static bool start_manual_irrigation_internal(const irr_manual_irrigation_request_t *req)
{
    int duration_minutes = 0;
    time_t now = 0;

    if (!req) {
        return false;
    }

    duration_minutes = req->total_duration;
    if (duration_minutes <= 0) {
        duration_minutes = req->pre_water + req->post_water;
    }
    if (duration_minutes <= 0) {
        duration_minutes = 1;
    }

    now = time(NULL);

    if (req->pre_water <= 0 && req->post_water <= 0 && req->total_duration <= 0) {
        persist_abnormal_manual_record((int64_t)now,
                                       (uint16_t)duration_minutes,
                                       0,
                                       req->pre_water,
                                       req->post_water,
                                       req->formula,
                                       "启动失败",
                                       "手灌时长无效");
        return false;
    }

    s_runtime.busy = true;
    s_runtime.program_active = false;
    s_runtime.manual_irrigation_active = true;
    s_runtime.active_program_index = -1;
    s_runtime.total_duration = duration_minutes;
    s_runtime.elapsed_seconds = 0;
    s_runtime_started_at = now;
    s_runtime_deadline_at = now + (time_t)duration_minutes * 60;
    copy_text(s_runtime.active_name, sizeof(s_runtime.active_name), "手动轮灌");
    snprintf(s_runtime.status_text, sizeof(s_runtime.status_text),
             "手动轮灌中：前清水%d分 后清水%d分", req->pre_water, req->post_water);

    memset(&s_program_run_ctx, 0, sizeof(s_program_run_ctx));
    memset(&s_manual_run_ctx, 0, sizeof(s_manual_run_ctx));
    s_manual_run_ctx.active = true;
    s_manual_run_ctx.start_ts = (int64_t)now;
    s_manual_run_ctx.planned_minutes = (uint16_t)duration_minutes;
    s_manual_run_ctx.pre_water = req->pre_water;
    s_manual_run_ctx.post_water = req->post_water;
    copy_text(s_manual_run_ctx.formula, sizeof(s_manual_run_ctx.formula), req->formula);

    ESP_LOGI(TAG, "Manual irrigation started: pre=%d post=%d total=%d formula=%s",
             req->pre_water, req->post_water, duration_minutes, req->formula);
    return true;
}

static void try_start_next_queued_program(void)
{
    irr_queue_item_t item = {0};

    while (!s_runtime.busy && dequeue_program(&item)) {
        if (start_program_internal(item.program_index, "queued")) {
            return;
        }
    }
}

static void finish_current_run_if_needed(void)
{
    time_t now = 0;

    if (!s_runtime.busy || s_runtime_deadline_at == 0) {
        return;
    }

    now = time(NULL);
    if (now < s_runtime_deadline_at) {
        return;
    }

    update_elapsed_seconds();
    ESP_LOGI(TAG, "Irrigation run completed: name=%s elapsed=%d valves=%d",
             s_runtime.active_name, s_runtime.elapsed_seconds, s_active_valve_count);
    s_finishing_normally = true;
    persist_completed_run();
    set_idle_status();
    s_finishing_normally = false;
    try_start_next_queued_program();
}

static bool compute_next_start_text(const irr_program_t *program, char *buf, int buf_size)
{
    time_t now = 0;
    time_t best_time = 0;
    bool found = false;
    int period_count = 0;

    if (!buf || buf_size <= 0) {
        return false;
    }

    copy_text(buf, (size_t)buf_size, "--");

    if (!program) {
        return false;
    }

    if (!program->auto_enabled) {
        copy_text(buf, (size_t)buf_size, "自动关闭");
        return true;
    }

    if (!is_program_timed_mode(program)) {
        copy_text(buf, (size_t)buf_size, "条件触发");
        return true;
    }

    if (!program_has_targets(program)) {
        copy_text(buf, (size_t)buf_size, "未选对象");
        return true;
    }

    if (!s_time_valid) {
        copy_text(buf, (size_t)buf_size, "等待校时");
        return true;
    }

    now = time(NULL);
    period_count = program->period_count;
    if (period_count > IRR_MAX_PERIODS) {
        period_count = IRR_MAX_PERIODS;
    }

    for (int day_offset = 0; day_offset < 366; day_offset++) {
        time_t day_time = now + (time_t)day_offset * 24 * 60 * 60;
        struct tm day_tm = {0};
        localtime_r(&day_time, &day_tm);

        if (!is_program_in_date_range(program, make_yyyymmdd(&day_tm))) {
            continue;
        }

        for (int i = 0; i < period_count; i++) {
            int hour = 0;
            int minute = 0;
            struct tm candidate_tm = day_tm;
            time_t candidate_time = 0;

            if (!program->periods[i].enabled) {
                continue;
            }
            if (!parse_period_time(program->periods[i].time, &hour, &minute)) {
                continue;
            }

            candidate_tm.tm_hour = hour;
            candidate_tm.tm_min = minute;
            candidate_tm.tm_sec = 0;
            candidate_tm.tm_isdst = -1;
            candidate_time = mktime(&candidate_tm);
            if (candidate_time == (time_t)-1 || candidate_time <= now) {
                continue;
            }

            if (!found || candidate_time < best_time) {
                best_time = candidate_time;
                found = true;
            }
        }
    }

    if (!found) {
        copy_text(buf, (size_t)buf_size, "--");
        return true;
    }

    {
        struct tm best_tm = {0};
        localtime_r(&best_time, &best_tm);
        snprintf(buf, buf_size, "%02d-%02d %02d:%02d",
                 best_tm.tm_mon + 1, best_tm.tm_mday,
                 best_tm.tm_hour, best_tm.tm_min);
    }

    return true;
}

static void refresh_program_next_start_cache(void)
{
    int count = irrigation_scheduler_get_program_count();

    if (count > IRR_MAX_PROGRAMS) {
        count = IRR_MAX_PROGRAMS;
    }

    for (int i = 0; i < count; i++) {
        irr_program_t program = {0};
        char next_start[32] = "--";

        if (!irrigation_scheduler_get_program(i, &program)) {
            continue;
        }

        compute_next_start_text(&program, next_start, sizeof(next_start));
        irrigation_scheduler_set_program_next_start(i, next_start);
    }
}

static void scan_auto_triggers(void)
{
    time_t now = 0;
    struct tm now_tm = {0};
    int today = 0;
    int count = 0;

    if (!s_time_valid || !s_runtime.auto_enabled) {
        return;
    }

    now = time(NULL);
    localtime_r(&now, &now_tm);
    today = make_yyyymmdd(&now_tm);
    count = irrigation_scheduler_get_program_count();
    if (count > IRR_MAX_PROGRAMS) {
        count = IRR_MAX_PROGRAMS;
    }

    for (int i = 0; i < count; i++) {
        irr_program_t program = {0};
        int period_count = 0;

        if (!irrigation_scheduler_get_program(i, &program)) {
            continue;
        }
        if (!program_can_auto_run_now(&program, today)) {
            continue;
        }

        period_count = program.period_count;
        if (period_count > IRR_MAX_PERIODS) {
            period_count = IRR_MAX_PERIODS;
        }

        for (int j = 0; j < period_count; j++) {
            int hour = 0;
            int minute = 0;

            if (!program.periods[j].enabled) {
                continue;
            }
            if (!parse_period_time(program.periods[j].time, &hour, &minute)) {
                continue;
            }
            if (s_last_fire_yyyymmdd[i][j] == today) {
                continue;
            }
            if (hour != now_tm.tm_hour || minute != now_tm.tm_min) {
                continue;
            }

            s_last_fire_yyyymmdd[i][j] = today;
            if (!s_runtime.busy) {
                start_program_internal(i, "auto");
            } else {
                enqueue_program(i, j, today);
            }
        }
    }
}

static void irrigation_scheduler_task(void *arg)
{
    (void)arg;

    while (1) {
        update_elapsed_seconds();
        finish_current_run_if_needed();
        if (!s_runtime.busy) {
            try_start_next_queued_program();
        }
        scan_auto_triggers();
        refresh_program_next_start_cache();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t irrigation_scheduler_init(void)
{
    esp_err_t ret = irrigation_store_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Irrigation store init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    if (!s_task_started) {
        memset(s_last_fire_yyyymmdd, 0, sizeof(s_last_fire_yyyymmdd));
        memset(s_queue, 0, sizeof(s_queue));
        memset(s_active_valve_ids, 0, sizeof(s_active_valve_ids));
        s_active_valve_count = 0;
        s_queue_count = 0;
        set_idle_status();
        s_runtime.auto_enabled = true;

        BaseType_t ok = xTaskCreate(
            irrigation_scheduler_task,
            "irr_sched",
            4096,
            NULL,
            4,
            &s_scheduler_task);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "Create scheduler task failed");
            return ESP_FAIL;
        }
        s_task_started = true;
    }

    refresh_program_next_start_cache();
    ESP_LOGI(TAG, "Irrigation scheduler initialized");
    return ESP_OK;
}

void irrigation_scheduler_set_time_valid(bool valid)
{
    s_time_valid = valid;
    ESP_LOGI(TAG, "Time valid state: %s", valid ? "true" : "false");
}

bool irrigation_scheduler_get_time_valid(void)
{
    return s_time_valid;
}

bool irrigation_scheduler_set_auto_enabled(bool enabled)
{
    s_runtime.auto_enabled = enabled;
    if (!enabled) {
        clear_queued_programs();
    }
    ESP_LOGI(TAG, "Auto mode %s", enabled ? "enabled" : "disabled");
    return true;
}

bool irrigation_scheduler_get_auto_enabled(void)
{
    return s_runtime.auto_enabled;
}

bool irrigation_scheduler_start_program(int index)
{
    if (s_runtime.busy) {
        ESP_LOGW(TAG, "Manual program start rejected, scheduler busy");
        persist_abnormal_program_record("未知程序",
                                        "手动",
                                        (int64_t)time(NULL),
                                        0,
                                        0,
                                        0,
                                        "manual",
                                        "启动失败",
                                        "调度器忙碌");
        return false;
    }

    return start_program_internal(index, "manual");
}

bool irrigation_scheduler_start_manual_irrigation(const irr_manual_irrigation_request_t *req)
{
    if (!req) {
        return false;
    }

    if (s_runtime.busy) {
        ESP_LOGW(TAG, "Manual irrigation start rejected, scheduler busy");
        persist_abnormal_manual_record((int64_t)time(NULL),
                                       (uint16_t)0,
                                       (uint16_t)0,
                                       req->pre_water,
                                       req->post_water,
                                       req->formula,
                                       "启动失败",
                                       "调度器忙碌");
        return false;
    }

    return start_manual_irrigation_internal(req);
}

void irrigation_scheduler_get_runtime_status(irr_runtime_status_t *out)
{
    if (!out) {
        return;
    }

    update_elapsed_seconds();
    *out = s_runtime;
}
