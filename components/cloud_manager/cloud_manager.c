#include "cloud_manager.h"

#include "device_registry.h"
#include "irrigation_scheduler.h"
#include "wifi_manager.h"
#include "zigbee_bridge.h"
#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CLOUD_TELEMETRY_TOPIC           "v1/devices/me/telemetry"
#define CLOUD_ATTRIBUTES_TOPIC          "v1/devices/me/attributes"
#define CLOUD_RPC_REQUEST_TOPIC_FILTER  "v1/devices/me/rpc/request/+"
#define CLOUD_RPC_REQUEST_TOPIC_PREFIX  "v1/devices/me/rpc/request/"
#define CLOUD_RPC_RESPONSE_TOPIC_PREFIX "v1/devices/me/rpc/response/"
#define CLOUD_REPORT_INTERVAL_MS        30000
#define CLOUD_JSON_BUF_SIZE             4096
#define CLOUD_ATTRIBUTES_BUF_SIZE       768
#define CLOUD_RPC_TOPIC_BUF_SIZE        128
#define CLOUD_RPC_DATA_BUF_SIZE         512
#define CLOUD_RPC_RESPONSE_BUF_SIZE     256
#define CLOUD_REPORT_TASK_STACK_SIZE    10240

typedef struct {
    cloud_manager_config_t config;
    char uri[192];
    char telemetry_payload[CLOUD_JSON_BUF_SIZE];
    char attributes_payload[CLOUD_ATTRIBUTES_BUF_SIZE];
    char rpc_response_payload[CLOUD_RPC_RESPONSE_BUF_SIZE];
    esp_mqtt_client_handle_t client;
    TaskHandle_t report_task;
    cloud_manager_rpc_handler_t rpc_handler;
    bool started;
    bool mqtt_connected;
    bool wifi_connected;
    bool time_synced;
    bool attributes_dirty;
} cloud_manager_state_t;

static const char *TAG = "cloud_manager";
static cloud_manager_state_t s_cloud = {0};

static bool json_append(char *buf, size_t size, size_t *used, const char *fmt, ...)
{
    va_list args;
    int written;

    if (!buf || !used || *used >= size) {
        return false;
    }

    va_start(args, fmt);
    written = vsnprintf(buf + *used, size - *used, fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= (size - *used)) {
        return false;
    }

    *used += (size_t)written;
    return true;
}

static bool json_append_escaped(char *buf, size_t size, size_t *used, const char *text)
{
    const char *src = text ? text : "";

    if (!json_append(buf, size, used, "\"")) {
        return false;
    }

    while (*src) {
        unsigned char ch = (unsigned char)(*src++);
        switch (ch) {
            case '\\':
                if (!json_append(buf, size, used, "\\\\")) {
                    return false;
                }
                break;
            case '"':
                if (!json_append(buf, size, used, "\\\"")) {
                    return false;
                }
                break;
            case '\b':
                if (!json_append(buf, size, used, "\\b")) {
                    return false;
                }
                break;
            case '\f':
                if (!json_append(buf, size, used, "\\f")) {
                    return false;
                }
                break;
            case '\n':
                if (!json_append(buf, size, used, "\\n")) {
                    return false;
                }
                break;
            case '\r':
                if (!json_append(buf, size, used, "\\r")) {
                    return false;
                }
                break;
            case '\t':
                if (!json_append(buf, size, used, "\\t")) {
                    return false;
                }
                break;
            default:
                if (ch < 0x20) {
                    if (!json_append(buf, size, used, "\\u%04x", ch)) {
                        return false;
                    }
                } else {
                    if (!json_append(buf, size, used, "%c", ch)) {
                        return false;
                    }
                }
                break;
        }
    }

    return json_append(buf, size, used, "\"");
}

static void rpc_result_set(cloud_rpc_result_t *result, bool success, const char *code, const char *message)
{
    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));
    result->success = success;
    snprintf(result->code, sizeof(result->code), "%s", code ? code : (success ? "OK" : "ERROR"));
    snprintf(result->message, sizeof(result->message), "%s", message ? message : (success ? "操作成功" : "操作失败"));
}

static esp_err_t cloud_publish(const char *topic, const char *payload)
{
    int msg_id;

    if (!s_cloud.started || !s_cloud.client || !s_cloud.mqtt_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    msg_id = esp_mqtt_client_publish(s_cloud.client, topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Failed to publish topic %s", topic);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Published %s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}

static bool extract_request_id(const char *topic, char *out, size_t out_size)
{
    const char *request_id = NULL;

    if (!topic || !out || out_size == 0) {
        return false;
    }

    if (strncmp(topic, CLOUD_RPC_REQUEST_TOPIC_PREFIX, strlen(CLOUD_RPC_REQUEST_TOPIC_PREFIX)) != 0) {
        return false;
    }

    request_id = topic + strlen(CLOUD_RPC_REQUEST_TOPIC_PREFIX);
    if (request_id[0] == '\0' || strlen(request_id) >= out_size) {
        return false;
    }

    snprintf(out, out_size, "%s", request_id);
    return true;
}

static esp_err_t publish_rpc_response(const char *request_id, const cloud_rpc_result_t *result)
{
    char topic[CLOUD_RPC_TOPIC_BUF_SIZE];
    char *payload = s_cloud.rpc_response_payload;
    size_t used = 0;
    const cloud_rpc_result_t fallback = {
        .success = false,
        .code = "ERROR",
        .message = "未知错误",
    };
    const cloud_rpc_result_t *resp = result ? result : &fallback;

    if (!request_id || request_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (snprintf(topic, sizeof(topic), "%s%s", CLOUD_RPC_RESPONSE_TOPIC_PREFIX, request_id) >= (int)sizeof(topic)) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (!json_append(payload, CLOUD_RPC_RESPONSE_BUF_SIZE, &used, "{\"success\":%s,\"code\":",
                     resp->success ? "true" : "false")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_RPC_RESPONSE_BUF_SIZE, &used, resp->code)) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_RPC_RESPONSE_BUF_SIZE, &used, ",\"message\":")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_RPC_RESPONSE_BUF_SIZE, &used, resp->message)) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_RPC_RESPONSE_BUF_SIZE, &used, "}")) {
        return ESP_ERR_NO_MEM;
    }

    return cloud_publish(topic, payload);
}

static bool json_get_bool_param(const cJSON *params, const char *key, bool *out)
{
    cJSON *item = NULL;

    if (!params || !key || !out) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive((cJSON *)params, key);
    if (!cJSON_IsBool(item)) {
        return false;
    }

    *out = cJSON_IsTrue(item);
    return true;
}

static bool json_get_int_param(const cJSON *params, const char *key, int *out)
{
    cJSON *item = NULL;

    if (!params || !key || !out) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive((cJSON *)params, key);
    if (!cJSON_IsNumber(item)) {
        return false;
    }

    *out = item->valueint;
    return true;
}

static bool json_get_uint32_param(const cJSON *params, const char *key, uint32_t *out)
{
    cJSON *item = NULL;

    if (!params || !key || !out) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive((cJSON *)params, key);
    if (!cJSON_IsNumber(item) || item->valuedouble < 0) {
        return false;
    }

    *out = (uint32_t)item->valuedouble;
    return true;
}

static bool json_get_string_param(const cJSON *params, const char *key, char *out, size_t out_size)
{
    cJSON *item = NULL;

    if (!params || !key || !out || out_size == 0) {
        return false;
    }

    item = cJSON_GetObjectItemCaseSensitive((cJSON *)params, key);
    if (!cJSON_IsString(item) || !item->valuestring) {
        return false;
    }

    snprintf(out, out_size, "%s", item->valuestring);
    return true;
}

static bool parse_rpc_command(const char *topic,
                              const char *payload,
                              cloud_rpc_command_t *out_cmd,
                              char *request_id,
                              size_t request_id_size,
                              cloud_rpc_result_t *parse_result)
{
    cJSON *root = NULL;
    cJSON *method = NULL;
    cJSON *params = NULL;
    bool ok = false;

    if (out_cmd) {
        memset(out_cmd, 0, sizeof(*out_cmd));
        out_cmd->type = CLOUD_RPC_UNKNOWN;
    }
    if (request_id && request_id_size > 0) {
        request_id[0] = '\0';
    }
    rpc_result_set(parse_result, false, "INVALID_REQUEST", "RPC请求格式错误");

    if (!topic || !payload || !out_cmd || !request_id || request_id_size == 0) {
        return false;
    }

    if (!extract_request_id(topic, request_id, request_id_size)) {
        rpc_result_set(parse_result, false, "INVALID_REQUEST_ID", "RPC请求ID无效");
        return false;
    }

    root = cJSON_Parse(payload);
    if (!root || !cJSON_IsObject(root)) {
        rpc_result_set(parse_result, false, "INVALID_JSON", "RPC负载不是有效JSON");
        goto cleanup;
    }

    method = cJSON_GetObjectItemCaseSensitive(root, "method");
    if (!cJSON_IsString(method) || !method->valuestring || method->valuestring[0] == '\0') {
        rpc_result_set(parse_result, false, "INVALID_METHOD", "缺少method字段");
        goto cleanup;
    }

    params = cJSON_GetObjectItemCaseSensitive(root, "params");

    if (strcmp(method->valuestring, "controlDevice") == 0) {
        out_cmd->type = CLOUD_RPC_CONTROL_DEVICE;
        if (!cJSON_IsObject(params)
            || !json_get_uint32_param(params, "pointId", &out_cmd->point_id)
            || !json_get_bool_param(params, "on", &out_cmd->on)) {
            rpc_result_set(parse_result, false, "INVALID_PARAMS", "controlDevice参数无效");
            goto cleanup;
        }
    } else if (strcmp(method->valuestring, "controlValve") == 0) {
        out_cmd->type = CLOUD_RPC_CONTROL_VALVE;
        if (!cJSON_IsObject(params)
            || !json_get_uint32_param(params, "pointId", &out_cmd->point_id)
            || !json_get_bool_param(params, "on", &out_cmd->on)) {
            rpc_result_set(parse_result, false, "INVALID_PARAMS", "controlValve参数无效");
            goto cleanup;
        }
    } else if (strcmp(method->valuestring, "setAutoMode") == 0) {
        out_cmd->type = CLOUD_RPC_SET_AUTO_MODE;
        if (!cJSON_IsObject(params) || !json_get_bool_param(params, "enabled", &out_cmd->enabled)) {
            rpc_result_set(parse_result, false, "INVALID_PARAMS", "setAutoMode参数无效");
            goto cleanup;
        }
    } else if (strcmp(method->valuestring, "startProgram") == 0) {
        out_cmd->type = CLOUD_RPC_START_PROGRAM;
        if (!cJSON_IsObject(params) || !json_get_int_param(params, "programIndex", &out_cmd->program_index)) {
            rpc_result_set(parse_result, false, "INVALID_PARAMS", "startProgram参数无效");
            goto cleanup;
        }
    } else if (strcmp(method->valuestring, "startManualIrrigation") == 0) {
        out_cmd->type = CLOUD_RPC_START_MANUAL_IRRIGATION;
        if (!cJSON_IsObject(params)
            || !json_get_string_param(params, "formula", out_cmd->manual.formula, sizeof(out_cmd->manual.formula))
            || !json_get_int_param(params, "preWater", &out_cmd->manual.pre_water)
            || !json_get_int_param(params, "postWater", &out_cmd->manual.post_water)
            || !json_get_int_param(params, "totalDuration", &out_cmd->manual.total_duration)) {
            rpc_result_set(parse_result, false, "INVALID_PARAMS", "startManualIrrigation参数无效");
            goto cleanup;
        }
    } else if (strcmp(method->valuestring, "stopIrrigation") == 0) {
        out_cmd->type = CLOUD_RPC_STOP_IRRIGATION;
    } else if (strcmp(method->valuestring, "reportNow") == 0) {
        out_cmd->type = CLOUD_RPC_REPORT_NOW;
    } else {
        rpc_result_set(parse_result, false, "UNSUPPORTED_METHOD", "不支持的RPC方法");
        goto cleanup;
    }

    ok = true;

cleanup:
    if (!ok && out_cmd) {
        memset(out_cmd, 0, sizeof(*out_cmd));
        out_cmd->type = CLOUD_RPC_UNKNOWN;
    }
    cJSON_Delete(root);
    return ok;
}

static void handle_rpc_request(const char *topic, const char *payload)
{
    cloud_rpc_command_t cmd = {0};
    cloud_rpc_result_t result = {0};
    char request_id[32] = {0};
    bool handler_ok;

    if (!parse_rpc_command(topic, payload, &cmd, request_id, sizeof(request_id), &result)) {
        ESP_LOGW(TAG, "RPC parse failed: topic=%s code=%s", topic ? topic : "<null>", result.code);
        if (request_id[0] != '\0') {
            publish_rpc_response(request_id, &result);
        }
        return;
    }

    if (!s_cloud.rpc_handler) {
        rpc_result_set(&result, false, "RPC_HANDLER_MISSING", "云端控制处理器未注册");
        publish_rpc_response(request_id, &result);
        return;
    }

    memset(&result, 0, sizeof(result));
    handler_ok = s_cloud.rpc_handler(&cmd, &result);
    if (result.code[0] == '\0') {
        rpc_result_set(&result,
                       handler_ok,
                       handler_ok ? "OK" : "EXECUTION_FAILED",
                       handler_ok ? "控制指令已执行" : "控制指令执行失败");
    }

    publish_rpc_response(request_id, &result);
}

static esp_err_t publish_attributes(void)
{
    char *payload = s_cloud.attributes_payload;
    size_t used = 0;
    const esp_app_desc_t *app_desc = esp_app_get_description();

    if (!json_append(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, "{")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, "\"device_name\":")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, "smart_irrigation_gateway")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, ",\"device_model\":")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, "ESP32-P4")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, ",\"fw_version\":")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, app_desc ? app_desc->version : "unknown")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, ",\"hw_version\":")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, "ESP32-P4 + ESP32-C6")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, ",\"wifi_mode\":")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used, "STA")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_ATTRIBUTES_BUF_SIZE, &used,
                     ",\"zone_count\":%d,\"device_count\":%d,\"valve_count\":%d,\"sensor_count\":%d"
                     ",\"supports_rpc\":%s,\"supports_telemetry\":true,\"supports_event_upload\":false}",
                     zone_registry_get_count(),
                     device_registry_get_count(),
                     valve_registry_get_count(),
                     sensor_registry_get_count(),
                     s_cloud.rpc_handler ? "true" : "false")) {
        return ESP_ERR_NO_MEM;
    }

    return cloud_publish(CLOUD_ATTRIBUTES_TOPIC, payload);
}

static esp_err_t publish_telemetry(void)
{
    char *payload = s_cloud.telemetry_payload;
    size_t used = 0;
    irr_runtime_status_t runtime = {0};
    const zb_pipe_data_t *pipe_data = zigbee_bridge_get_pipes();
    const zb_control_data_t *control_data = zigbee_bridge_get_control();

    irrigation_scheduler_get_runtime_status(&runtime);

    if (!json_append(payload, CLOUD_JSON_BUF_SIZE, &used,
                     "{\"wifi_connected\":%s,\"scheduler_busy\":%s,\"program_active\":%s,\"manual_active\":%s,"
                     "\"active_program_index\":%d,\"elapsed_seconds\":%d,\"time_synced\":%s,\"heap_free\":%u,"
                     "\"auto_enabled\":%s,\"total_duration\":%d,\"active_name\":",
                     s_cloud.wifi_connected ? "true" : "false",
                     runtime.busy ? "true" : "false",
                     runtime.program_active ? "true" : "false",
                     runtime.manual_irrigation_active ? "true" : "false",
                     runtime.active_program_index,
                     runtime.elapsed_seconds,
                     s_cloud.time_synced ? "true" : "false",
                     (unsigned)esp_get_free_heap_size(),
                     runtime.auto_enabled ? "true" : "false",
                     runtime.total_duration)) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_JSON_BUF_SIZE, &used, runtime.active_name)) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append(payload, CLOUD_JSON_BUF_SIZE, &used, ",\"status_text\":")) {
        return ESP_ERR_NO_MEM;
    }
    if (!json_append_escaped(payload, CLOUD_JSON_BUF_SIZE, &used, runtime.status_text)) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 1; i <= ZB_MAX_FIELDS; i++) {
        const zb_field_data_t *field = zigbee_bridge_get_field((uint8_t)i);
        if (!field) {
            continue;
        }

        if (!json_append(payload, CLOUD_JSON_BUF_SIZE, &used,
                         ",\"field%d_n\":%.2f,\"field%d_p\":%.2f,\"field%d_k\":%.2f,"
                         "\"field%d_temp\":%.2f,\"field%d_humi\":%.2f,\"field%d_light\":%.2f",
                         i, field->nitrogen,
                         i, field->phosphorus,
                         i, field->potassium,
                         i, field->temperature,
                         i, field->humidity,
                         i, field->light)) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (pipe_data) {
        for (int i = 0; i < ZB_MAX_PIPES; i++) {
            if (!json_append(payload, CLOUD_JSON_BUF_SIZE, &used,
                             ",\"pipe%d_valve_on\":%s,\"pipe%d_flow\":%.2f,\"pipe%d_pressure\":%.2f",
                             i, pipe_data->pipes[i].valve_on ? "true" : "false",
                             i, pipe_data->pipes[i].flow,
                             i, pipe_data->pipes[i].pressure)) {
                return ESP_ERR_NO_MEM;
            }
        }
    }

    if (control_data) {
        if (!json_append(payload, CLOUD_JSON_BUF_SIZE, &used,
                         ",\"water_pump_on\":%s,\"fert_pump_on\":%s,\"fert_valve_on\":%s,\"water_valve_on\":%s,"
                         "\"mixer_on\":%s,\"tank_n_switch_on\":%s,\"tank_p_switch_on\":%s,\"tank_k_switch_on\":%s,"
                         "\"tank_n_level\":%.2f,\"tank_p_level\":%.2f,\"tank_k_level\":%.2f",
                         control_data->water_pump_on ? "true" : "false",
                         control_data->fert_pump_on ? "true" : "false",
                         control_data->fert_valve_on ? "true" : "false",
                         control_data->water_valve_on ? "true" : "false",
                         control_data->mixer_on ? "true" : "false",
                         control_data->tanks[0].switch_on ? "true" : "false",
                         control_data->tanks[1].switch_on ? "true" : "false",
                         control_data->tanks[2].switch_on ? "true" : "false",
                         control_data->tanks[0].level,
                         control_data->tanks[1].level,
                         control_data->tanks[2].level)) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (!json_append(payload, CLOUD_JSON_BUF_SIZE, &used, "}")) {
        return ESP_ERR_NO_MEM;
    }

    return cloud_publish(CLOUD_TELEMETRY_TOPIC, payload);
}

static void trigger_report(void)
{
    if (s_cloud.report_task) {
        xTaskNotifyGive(s_cloud.report_task);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    (void)handler_args;
    (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            int sub_id;

            ESP_LOGI(TAG, "MQTT connected");
            s_cloud.mqtt_connected = true;
            s_cloud.attributes_dirty = true;
            sub_id = esp_mqtt_client_subscribe(s_cloud.client, CLOUD_RPC_REQUEST_TOPIC_FILTER, 1);
            if (sub_id < 0) {
                ESP_LOGW(TAG, "Subscribe RPC topic failed");
            } else {
                ESP_LOGI(TAG, "Subscribed RPC topic, msg_id=%d", sub_id);
            }
            trigger_report();
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            s_cloud.mqtt_connected = false;
            break;
        case MQTT_EVENT_DATA: {
            char topic[CLOUD_RPC_TOPIC_BUF_SIZE];
            char data[CLOUD_RPC_DATA_BUF_SIZE];

            if (!event || !event->topic || !event->data) {
                break;
            }
            if (event->current_data_offset != 0 || event->total_data_len != event->data_len) {
                ESP_LOGW(TAG, "Skip fragmented RPC payload: total=%d chunk=%d offset=%d",
                         event->total_data_len, event->data_len, event->current_data_offset);
                break;
            }
            if (event->topic_len <= 0
                || event->topic_len >= (int)sizeof(topic)
                || event->data_len >= (int)sizeof(data)) {
                ESP_LOGW(TAG, "Skip oversized RPC payload: topic_len=%d data_len=%d",
                         event->topic_len, event->data_len);
                break;
            }

            memcpy(topic, event->topic, (size_t)event->topic_len);
            topic[event->topic_len] = '\0';
            memcpy(data, event->data, (size_t)event->data_len);
            data[event->data_len] = '\0';

            if (strncmp(topic, CLOUD_RPC_REQUEST_TOPIC_PREFIX, strlen(CLOUD_RPC_REQUEST_TOPIC_PREFIX)) == 0) {
                ESP_LOGI(TAG, "Received RPC request: %s %s", topic, data);
                handle_rpc_request(topic, data);
            }
            break;
        }
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT published, msg_id=%d", event ? event->msg_id : -1);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "MQTT error event");
            break;
        default:
            break;
    }
}

static void cloud_report_task(void *arg)
{
    (void)arg;

    while (1) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CLOUD_REPORT_INTERVAL_MS));

        if (!s_cloud.started || !s_cloud.client || !s_cloud.mqtt_connected) {
            continue;
        }

        if (s_cloud.attributes_dirty) {
            if (publish_attributes() == ESP_OK) {
                s_cloud.attributes_dirty = false;
            }
        }

        publish_telemetry();
    }
}

esp_err_t cloud_manager_init(const cloud_manager_config_t *cfg)
{
    if (!cfg) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_cloud, 0, sizeof(s_cloud));
    memcpy(&s_cloud.config, cfg, sizeof(*cfg));
    s_cloud.wifi_connected = wifi_manager_is_connected();
    s_cloud.time_synced = irrigation_scheduler_get_time_valid();
    s_cloud.attributes_dirty = true;

    if (!s_cloud.config.enabled) {
        ESP_LOGI(TAG, "Cloud manager disabled");
        return ESP_OK;
    }

    if (!s_cloud.config.host[0] || !s_cloud.config.access_token[0] || s_cloud.config.port <= 0) {
        ESP_LOGE(TAG, "Cloud config invalid");
        return ESP_ERR_INVALID_ARG;
    }

    if (!json_append(s_cloud.uri, sizeof(s_cloud.uri), &(size_t){0}, "%s://%s:%d",
                     s_cloud.config.enable_tls ? "mqtts" : "mqtt",
                     s_cloud.config.host,
                     s_cloud.config.port)) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Cloud manager configured for %s", s_cloud.uri);
    return ESP_OK;
}

esp_err_t cloud_manager_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {0};

    if (!s_cloud.config.enabled) {
        return ESP_OK;
    }
    if (s_cloud.started) {
        return ESP_OK;
    }

    mqtt_cfg.broker.address.uri = s_cloud.uri;
    mqtt_cfg.credentials.username = s_cloud.config.access_token;

    s_cloud.client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_cloud.client) {
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_cloud.client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    if (xTaskCreate(cloud_report_task, "cloud_report", CLOUD_REPORT_TASK_STACK_SIZE, NULL, 4, &s_cloud.report_task) != pdPASS) {
        esp_mqtt_client_destroy(s_cloud.client);
        s_cloud.client = NULL;
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = esp_mqtt_client_start(s_cloud.client);
    if (ret != ESP_OK) {
        vTaskDelete(s_cloud.report_task);
        s_cloud.report_task = NULL;
        esp_mqtt_client_destroy(s_cloud.client);
        s_cloud.client = NULL;
        return ret;
    }

    s_cloud.started = true;
    trigger_report();
    return ESP_OK;
}

void cloud_manager_register_rpc_handler(cloud_manager_rpc_handler_t handler)
{
    s_cloud.rpc_handler = handler;
    s_cloud.attributes_dirty = true;
    trigger_report();
}

void cloud_manager_notify_wifi(bool connected)
{
    s_cloud.wifi_connected = connected;
    trigger_report();
}

void cloud_manager_notify_time_synced(bool synced)
{
    s_cloud.time_synced = synced;
    trigger_report();
}

bool cloud_manager_is_connected(void)
{
    return s_cloud.mqtt_connected;
}

esp_err_t cloud_manager_request_report_now(void)
{
    trigger_report();
    return ESP_OK;
}
