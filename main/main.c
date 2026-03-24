#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_sntp.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "ui_common.h"
#include "ui_wifi.h"
#include "ui_display.h"
#include "ui_network.h"
#include "ui_alarm.h"
#include "ui_alarm_records.h"
#include "ui_log_records.h"
#include "wifi_manager.h"
#include "display_manager.h"
#include "alarm_manager.h"
#include "event_recorder.h"
#include "zigbee_bridge.h"
#include "device_registry.h"
#include "irrigation_scheduler.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
static const char *TAG = "smart_irrigation";

static bool on_get_zone_detail(int slot_index, ui_zone_add_params_t *out);

/* NTP time sync state */
static bool s_time_synced = false;
static bool s_ntp_started = false;
static bool s_wifi_connected = false;
static bool s_event_fix_scheduled = false;
static bool s_event_fix_completed = false;

static bool is_nav_visible(nav_item_t nav)
{
    ui_main_t *ui = ui_get_main();
    return ui && ui->current_nav == nav;
}

/* ---- Time update task ---- */

static const char *get_weekday_cn(int wday)
{
    static const char *days[] = {"周日","周一","周二","周三","周四","周五","周六"};
    if (wday >= 0 && wday <= 6) return days[wday];
    return "--";
}

static void time_update_task(void *pvParameters)
{
    (void)pvParameters;
    while (1) {
        if (s_time_synced) {
            time_t now;
            struct tm timeinfo;
            time(&now);
            localtime_r(&now, &timeinfo);

            char time_buf[48];
            snprintf(time_buf, sizeof(time_buf), "%02d:%02d:%02d\n%04d/%02d/%02d %s",
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     get_weekday_cn(timeinfo.tm_wday));

            bsp_display_lock(-1);
            ui_statusbar_set_time(time_buf);
            bsp_display_unlock();
        } else if (s_wifi_connected) {
            bsp_display_lock(-1);
            ui_statusbar_set_time("时间同步中...\n请稍候");
            bsp_display_unlock();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void refresh_log_records_async(void *arg)
{
    (void)arg;
    if (is_nav_visible(NAV_LOG)) {
        ui_log_rec_refresh_visible();
    }
}

static void refresh_alarm_records_async(void *arg)
{
    (void)arg;
    if (is_nav_visible(NAV_LOG)) {
        ui_alarm_rec_refresh_visible();
    }
}

static void fix_event_timestamps_task(void *arg)
{
    (void)arg;

    vTaskDelay(pdMS_TO_TICKS(2000));

    esp_err_t ret = event_recorder_fix_timestamps();
    if (ret == ESP_OK) {
        s_event_fix_completed = true;
        lv_async_call(refresh_alarm_records_async, NULL);
        lv_async_call(refresh_log_records_async, NULL);
    } else {
        ESP_LOGW(TAG, "Deferred event timestamp repair failed: %s", esp_err_to_name(ret));
    }

    s_event_fix_scheduled = false;
    vTaskDelete(NULL);
}

static void schedule_event_timestamp_fix(void)
{
    if (s_event_fix_completed || s_event_fix_scheduled) {
        return;
    }

    BaseType_t ok = xTaskCreate(fix_event_timestamps_task,
                                "evt_ts_fix",
                                4096,
                                NULL,
                                4,
                                NULL);
    if (ok == pdPASS) {
        s_event_fix_scheduled = true;
    } else {
        ESP_LOGW(TAG, "Failed to create deferred timestamp repair task");
    }
}

static void sntp_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP time synchronized");
    s_time_synced = true;
    irrigation_scheduler_set_time_valid(true);
    schedule_event_timestamp_fix();
}

static void start_ntp(void)
{
    if (s_ntp_started) return;
    s_ntp_started = true;

    ESP_LOGI(TAG, "Starting SNTP time sync");
    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.aliyun.com");
    esp_sntp_setservername(1, "cn.pool.ntp.org");
    esp_sntp_setservername(2, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(sntp_sync_cb);
    esp_sntp_init();
}

static void stop_ntp(void)
{
    if (!s_ntp_started) return;
    esp_sntp_stop();
    s_ntp_started = false;
    /* Keep s_time_synced true so the system clock continues displaying.
       NTP will re-sync when WiFi reconnects. */
}

/* ---- WiFi scan callback ---- */

static void update_ui_with_scan_results(void *data)
{
    ui_wifi_scan_result_t *result = (ui_wifi_scan_result_t *)data;
    ui_wifi_update_scan_results(result);
    free(result);
}

static void on_wifi_scan_done(const wifi_mgr_scan_result_t *result, void *user_data)
{
    (void)user_data;

    ui_wifi_scan_result_t *ui_result = malloc(sizeof(ui_wifi_scan_result_t));
    if (!ui_result) {
        ESP_LOGE(TAG, "Failed to allocate scan result buffer");
        return;
    }

    ui_result->ap_count = result->ap_count;
    for (int i = 0; i < result->ap_count; i++) {
        strncpy(ui_result->ap_list[i].ssid, result->ap_list[i].ssid, 33);
        ui_result->ap_list[i].rssi = result->ap_list[i].rssi;
        ui_result->ap_list[i].authmode = result->ap_list[i].authmode;
        ui_result->ap_list[i].is_connected = result->ap_list[i].is_connected;
        ui_result->ap_list[i].has_saved_password = result->ap_list[i].has_saved_password;
    }

    lv_async_call(update_ui_with_scan_results, ui_result);
}

/* ---- WiFi connection status callback ---- */

typedef struct {
    wifi_mgr_conn_status_t status;
    char ssid[33];
} conn_status_msg_t;

static void update_ui_conn_status(void *data)
{
    conn_status_msg_t *msg = (conn_status_msg_t *)data;
    if (msg->status == WIFI_MGR_CONNECTED) {
        ui_wifi_set_connected(true, msg->ssid);
    } else if (msg->status == WIFI_MGR_CONNECT_FAILED) {
        ui_wifi_set_connected(false, msg->ssid);
        ui_wifi_show_connect_failed(msg->ssid);
    } else {
        ui_wifi_set_connected(false, msg->ssid);
    }
    free(msg);
}

static void on_wifi_conn_status(wifi_mgr_conn_status_t status, const char *ssid, void *user_data)
{
    (void)user_data;

    conn_status_msg_t *msg = malloc(sizeof(conn_status_msg_t));
    if (!msg) return;

    msg->status = status;
    strncpy(msg->ssid, ssid ? ssid : "", 32);
    msg->ssid[32] = '\0';

    if (status == WIFI_MGR_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected: %s", msg->ssid);
        s_wifi_connected = true;
        start_ntp();
        if (msg->ssid[0] != '\0') {
            char desc[64];
            snprintf(desc, sizeof(desc), "WiFi连接: %s", msg->ssid);
            event_recorder_add_operation(desc);
        }
    } else if (status == WIFI_MGR_CONNECT_FAILED) {
        ESP_LOGW(TAG, "WiFi connect failed: %s", msg->ssid);
    } else {
        ESP_LOGI(TAG, "WiFi disconnected: %s", msg->ssid);
        s_wifi_connected = false;
        stop_ntp();
        /* Record disconnect event */
        char desc[64];
        snprintf(desc, sizeof(desc), "WiFi断开: %s", msg->ssid);
        event_recorder_add_offline(desc);
    }

    lv_async_call(update_ui_conn_status, msg);
}

/* ---- UI callbacks ---- */

static void request_wifi_scan(void)
{
    ESP_LOGI(TAG, "WiFi scan requested by UI");
    wifi_manager_start_scan(on_wifi_scan_done, NULL);
}

static void request_wifi_connect(const char *ssid, const char *password)
{
    ESP_LOGI(TAG, "WiFi connect requested: SSID=%s", ssid);
    esp_err_t ret = wifi_manager_connect(ssid, password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed: %s", esp_err_to_name(ret));
    } else {
        char desc[64];
        snprintf(desc, sizeof(desc), "WiFi操作: 连接 %.24s", ssid ? ssid : "");
        event_recorder_add_operation(desc);
    }
}

static void request_wifi_disconnect(void)
{
    ESP_LOGI(TAG, "WiFi disconnect requested by UI");
    esp_err_t ret = wifi_manager_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi disconnect failed: %s", esp_err_to_name(ret));
    } else {
        event_recorder_add_operation("WiFi操作: 主动断开连接");
    }
}

static void request_wifi_connect_saved(const char *ssid)
{
    ESP_LOGI(TAG, "WiFi connect (saved) requested: SSID=%s", ssid);
    esp_err_t ret = wifi_manager_connect_saved(ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect saved failed: %s", esp_err_to_name(ret));
    } else {
        char desc[64];
        snprintf(desc, sizeof(desc), "WiFi操作: 连接已保存网络 %.20s", ssid ? ssid : "");
        event_recorder_add_operation(desc);
    }
}

/* ---- Display settings bridge callbacks ---- */

static void on_preview_brightness(int percent)
{
    display_manager_preview_brightness(percent);
}

static void on_save_brightness(int percent)
{
    display_manager_set_brightness(percent);

    char desc[64];
    snprintf(desc, sizeof(desc), "显示设置: 亮度 %d%%", percent);
    event_recorder_add_operation(desc);
}

static void on_timeout_change(int index)
{
    display_manager_set_timeout_index(index);

    char desc[64];
    snprintf(desc, sizeof(desc), "显示设置: 熄屏时间索引 %d", index);
    event_recorder_add_operation(desc);
}

/* ---- Network settings bridge callbacks ---- */

static ui_net_result_t on_apply_static_ip(const char *ip, const char *mask,
                                           const char *gateway, const char *dns)
{
    ESP_LOGI(TAG, "Static IP requested: %s/%s gw:%s dns:%s", ip, mask, gateway, dns);

    if (!wifi_manager_is_connected()) {
        return UI_NET_RESULT_NOT_CONNECTED;
    }

    esp_err_t ret = wifi_manager_set_static_ip(ip, mask, gateway, dns);
    if (ret == ESP_OK) {
        char desc[64];
        snprintf(desc, sizeof(desc), "网络设置: 应用静态IP %s/%s", ip, mask);
        event_recorder_add_operation(desc);
    }
    return (ret == ESP_OK) ? UI_NET_RESULT_OK : UI_NET_RESULT_FAIL;
}

static ui_net_result_t on_restore_dhcp(void)
{
    ESP_LOGI(TAG, "DHCP restore requested");
    esp_err_t ret = wifi_manager_restore_dhcp();
    if (ret == ESP_OK) {
        event_recorder_add_operation("网络设置: 恢复DHCP");
    }
    return (ret == ESP_OK) ? UI_NET_RESULT_OK : UI_NET_RESULT_FAIL;
}

static void on_get_network_info(char *buf, int buf_size)
{
    int pos = 0;

    /* WiFi connection status */
    bool connected = wifi_manager_is_connected();
    pos += snprintf(buf + pos, buf_size - pos, "WiFi状态: %s\n",
                    connected ? "已连接" : "未连接");

    if (connected) {
        pos += snprintf(buf + pos, buf_size - pos, "SSID: %s\n",
                        wifi_manager_get_connected_ssid());
    }

    /* IP info */
    esp_netif_t *netif = wifi_manager_get_sta_netif();
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            pos += snprintf(buf + pos, buf_size - pos,
                            "IP地址: " IPSTR "\n"
                            "子网掩码: " IPSTR "\n"
                            "默认网关: " IPSTR "\n",
                            IP2STR(&ip_info.ip),
                            IP2STR(&ip_info.netmask),
                            IP2STR(&ip_info.gw));
        }

        /* DNS */
        esp_netif_dns_info_t dns_info;
        if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
            pos += snprintf(buf + pos, buf_size - pos,
                            "DNS: " IPSTR "\n",
                            IP2STR(&dns_info.ip.u_addr.ip4));
        }

        /* MAC address */
        uint8_t mac[6];
        if (esp_netif_get_mac(netif, mac) == ESP_OK) {
            pos += snprintf(buf + pos, buf_size - pos,
                            "MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }

        /* Hostname */
        const char *hostname = NULL;
        if (esp_netif_get_hostname(netif, &hostname) == ESP_OK && hostname) {
            pos += snprintf(buf + pos, buf_size - pos, "主机名: %s\n", hostname);
        }
    } else {
        pos += snprintf(buf + pos, buf_size - pos, "网络接口未初始化\n");
    }
    (void)pos;
}

/* ---- Event records bridge callback ---- */

static void on_query_records(ui_alarm_rec_type_t type, int64_t start_ts,
    int64_t end_ts, uint16_t page, ui_alarm_rec_result_t *result)
{
    static evt_query_result_t r;
    evt_type_t evt_type;

    if (!result) {
        return;
    }

    memset(result, 0, sizeof(*result));

    switch (type) {
        case UI_ALARM_REC_OFFLINE:
            evt_type = EVT_TYPE_OFFLINE;
            break;
        case UI_ALARM_REC_POWERON:
            evt_type = EVT_TYPE_POWERON;
            break;
        case UI_ALARM_REC_HISTORY_ALARM:
            evt_type = EVT_TYPE_ALARM;
            break;
        default:
            return;
    }

    if (event_recorder_query(evt_type, start_ts, end_ts, page, &r) != ESP_OK) {
        return;
    }

    result->count = r.count;
    result->total_matched = r.total_matched;
    result->total_pages = r.total_pages;
    result->current_page = r.current_page;
    for (int i = 0; i < r.count; i++) {
        result->records[i].timestamp = r.records[i].timestamp;
        memcpy(result->records[i].desc, r.records[i].desc, sizeof(result->records[i].desc));
    }
}

static esp_err_t on_query_current_alarms(ui_alarm_current_item_t *items,
    size_t max_items, size_t *out_count)
{
    alarm_manager_current_alarm_t backend_items[ALARM_MANAGER_MAX_CURRENT] = {0};
    size_t backend_count = 0;
    esp_err_t ret = alarm_manager_get_current_alarms(backend_items,
        ALARM_MANAGER_MAX_CURRENT, &backend_count);

    if (out_count) {
        *out_count = 0;
    }
    if (ret != ESP_OK) {
        return ret;
    }
    if (!items || !out_count) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t copy_count = backend_count;
    if (copy_count > max_items) {
        copy_count = max_items;
    }
    for (size_t i = 0; i < copy_count; i++) {
        items[i].timestamp = backend_items[i].timestamp;
        memcpy(items[i].desc, backend_items[i].desc, sizeof(items[i].desc));
    }
    *out_count = copy_count;
    return ESP_OK;
}

static esp_err_t on_clear_current_alarms(void)
{
    return alarm_manager_clear_current_alarms();
}

static esp_err_t on_load_alarm_settings(ui_alarm_settings_t *settings)
{
    alarm_manager_settings_t backend_settings = {0};
    esp_err_t ret;

    if (!settings) {
        return ESP_ERR_INVALID_ARG;
    }

    ret = alarm_manager_load_settings(&backend_settings);
    if (ret != ESP_OK) {
        return ret;
    }

    memset(settings, 0, sizeof(*settings));
    for (int i = 0; i < UI_ALARM_SETTINGS_COUNT && i < ALARM_MANAGER_SETTINGS_COUNT; i++) {
        memcpy(settings->items[i].threshold, backend_settings.items[i].threshold,
               sizeof(settings->items[i].threshold));
        settings->items[i].duration_s = backend_settings.items[i].duration_s;
        settings->items[i].action = backend_settings.items[i].action;
    }

    return ESP_OK;
}

static esp_err_t on_save_alarm_settings(const ui_alarm_settings_t *settings)
{
    alarm_manager_settings_t backend_settings = {0};

    if (!settings) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < UI_ALARM_SETTINGS_COUNT && i < ALARM_MANAGER_SETTINGS_COUNT; i++) {
        memcpy(backend_settings.items[i].threshold, settings->items[i].threshold,
               sizeof(backend_settings.items[i].threshold));
        backend_settings.items[i].duration_s = settings->items[i].duration_s;
        backend_settings.items[i].action = settings->items[i].action;
    }

    return alarm_manager_save_settings(&backend_settings);
}

static void on_query_log_records(ui_log_rec_type_t type, int64_t start_ts,
    int64_t end_ts, uint16_t page, ui_log_rec_status_filter_t status_filter,
    ui_log_rec_result_t *result)
{
    static evt_query_result_t basic_result;
    static evt_manual_query_result_t manual_result;
    static evt_program_query_result_t program_result;
    evt_type_t evt_type;

    memset(result, 0, sizeof(*result));

    if (type == UI_LOG_REC_CONTROL || type == UI_LOG_REC_OPERATION) {
        evt_type = (type == UI_LOG_REC_CONTROL) ? EVT_TYPE_CONTROL : EVT_TYPE_OPERATION;
        event_recorder_query(evt_type, start_ts, end_ts, page, &basic_result);

        result->count = basic_result.count;
        result->total_matched = basic_result.total_matched;
        result->total_pages = basic_result.total_pages;
        result->current_page = basic_result.current_page;
        for (int i = 0; i < basic_result.count; i++) {
            result->records[i].timestamp = basic_result.records[i].timestamp;
            memcpy(result->records[i].desc, basic_result.records[i].desc,
                   sizeof(basic_result.records[i].desc));
        }
        return;
    }

    if (type == UI_LOG_REC_MANUAL) {
        event_recorder_query_manual_records(start_ts, end_ts,
            (evt_status_filter_t)status_filter, page, &manual_result);

        result->count = manual_result.count;
        result->total_matched = manual_result.total_matched;
        result->total_pages = manual_result.total_pages;
        result->current_page = manual_result.current_page;
        for (int i = 0; i < manual_result.count; i++) {
            result->records[i].timestamp = manual_result.records[i].start_ts;
            result->records[i].planned_minutes = manual_result.records[i].planned_minutes;
            result->records[i].actual_minutes = manual_result.records[i].actual_minutes;
            memcpy(result->records[i].status, manual_result.records[i].status,
                   sizeof(manual_result.records[i].status));
            memcpy(result->records[i].detail, manual_result.records[i].detail,
                   sizeof(manual_result.records[i].detail));
        }
        return;
    }

    event_recorder_query_program_records(start_ts, end_ts,
        (evt_status_filter_t)status_filter, page, &program_result);

    result->count = program_result.count;
    result->total_matched = program_result.total_matched;
    result->total_pages = program_result.total_pages;
    result->current_page = program_result.current_page;
    for (int i = 0; i < program_result.count; i++) {
        result->records[i].timestamp = program_result.records[i].start_ts;
        result->records[i].planned_minutes = program_result.records[i].planned_minutes;
        result->records[i].actual_minutes = program_result.records[i].actual_minutes;
        memcpy(result->records[i].program_name, program_result.records[i].program_name,
               sizeof(program_result.records[i].program_name));
        memcpy(result->records[i].trigger, program_result.records[i].trigger,
               sizeof(program_result.records[i].trigger));
        memcpy(result->records[i].status, program_result.records[i].status,
               sizeof(program_result.records[i].status));
        memcpy(result->records[i].detail, program_result.records[i].detail,
               sizeof(program_result.records[i].detail));
    }
}

/* ---- WiFi background init task ---- */

static void wifi_init_task(void *pvParameters)
{
    (void)pvParameters;

    ESP_LOGI(TAG, "WiFi init starting (background)...");
    esp_err_t ret = wifi_manager_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi manager init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "WiFi manager initialized successfully");
    }

    /* Register connection status callback (needs event loop from wifi_manager_init) */
    wifi_manager_register_conn_cb(on_wifi_conn_status, NULL);

    vTaskDelete(NULL);
}

/* ---- Zigbee bridge callbacks ---- */

typedef struct {
    uint8_t dev_type;
    uint8_t dev_id;
} zb_ui_msg_t;

static int s_zb_frame_count = 0;         /* 收到的总帧数 */

static bool is_sensor_point_registered(uint32_t point_id)
{
    const dev_sensor_info_t *all = sensor_registry_get_all();

    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (all[i].valid && all[i].point_id == point_id) {
            return true;
        }
    }

    return false;
}

static bool is_pipe_valve_registered(int pipe_id)
{
    const dev_valve_info_t *all = valve_registry_get_all();

    if (pipe_id <= 0 || pipe_id >= ZB_MAX_PIPES) {
        return false;
    }

    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        if (all[i].valid && all[i].channel == (uint8_t)pipe_id) {
            return true;
        }
    }

    return false;
}

static uint8_t get_field_registered_mask(uint8_t field_id)
{
    uint32_t node_id;
    uint8_t mask = 0;

    if (field_id < 1 || field_id > ZB_MAX_FIELDS) {
        return 0;
    }

    node_id = 1000U + (uint32_t)field_id;
    for (uint8_t point_no = 1; point_no <= 6; point_no++) {
        uint32_t point_id = node_id * 100U + (uint32_t)point_no;
        if (is_sensor_point_registered(point_id)) {
            mask |= (uint8_t)(1U << (point_no - 1U));
        }
    }

    return mask;
}

static int resolve_field_index_from_device_id(uint16_t device_id)
{
    if (device_id >= 1001U && device_id <= 1006U) {
        uint8_t field_id = (uint8_t)(device_id - 1000U);
        return get_field_registered_mask(field_id) ? ((int)field_id - 1) : -1;
    }

    const dev_sensor_info_t *sensors = sensor_registry_get_all();
    uint8_t mask = 0;
    int field_index = -1;

    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (!sensors[i].valid || sensors[i].parent_device_id != device_id) {
            continue;
        }
        if (sensors[i].point_id < 100100U || sensors[i].point_id > 100699U) {
            continue;
        }

        uint32_t node_id = sensors[i].point_id / 100U;
        if (node_id < 1001U || node_id > 1006U) {
            continue;
        }

        int candidate_index = (int)(node_id - 1001U);
        uint32_t point_no = sensors[i].point_id % 100U;
        if (point_no < 1U || point_no > 6U) {
            continue;
        }

        if (field_index < 0) {
            field_index = candidate_index;
        }
        if (field_index != candidate_index) {
            return -1;
        }

        mask |= (uint8_t)(1U << (point_no - 1U));
    }

    return (mask != 0U) ? field_index : -1;
}

static int on_home_zone_field_resolve(int slot_index)
{
    ui_zone_add_params_t zone = {0};

    if (!on_get_zone_detail(slot_index, &zone)) {
        return -1;
    }

    for (int i = 0; i < zone.device_count; i++) {
        int field_index = resolve_field_index_from_device_id(zone.device_ids[i]);
        if (field_index >= 0) {
            return field_index;
        }
    }

    return -1;
}

static void get_pipe_binding_flags(int pipe_id,
    bool *valve_bound, bool *flow_bound, bool *pressure_bound)
{
    uint32_t node_id;

    if (valve_bound) {
        *valve_bound = false;
    }
    if (flow_bound) {
        *flow_bound = false;
    }
    if (pressure_bound) {
        *pressure_bound = false;
    }

    if (pipe_id < 0 || pipe_id >= ZB_MAX_PIPES) {
        return;
    }

    node_id = 2000U + (uint32_t)pipe_id;
    if (valve_bound) {
        *valve_bound = is_pipe_valve_registered(pipe_id) ||
            is_sensor_point_registered(node_id * 100U + 11U);
    }
    if (flow_bound) {
        *flow_bound = is_sensor_point_registered(node_id * 100U + 2U);
    }
    if (pressure_bound) {
        *pressure_bound = is_sensor_point_registered(node_id * 100U + 3U);
    }
}

static bool is_tank_level_bound(int tank_id)
{
    if (tank_id < 1 || tank_id > ZB_MAX_TANKS) {
        return false;
    }

    return is_sensor_point_registered(3000U * 100U + 20U + (uint32_t)tank_id);
}

static void project_field_to_ui(uint8_t field_id)
{
    const zb_field_data_t *f;
    uint8_t registered_mask;

    if (field_id < 1 || field_id > ZB_MAX_FIELDS) {
        return;
    }

    f = zigbee_bridge_get_field(field_id);
    if (!f) {
        return;
    }

    registered_mask = get_field_registered_mask(field_id);
    ui_home_update_field(field_id, registered_mask,
        f->nitrogen, f->phosphorus, f->potassium,
        f->temperature, f->humidity, f->light);
    ui_device_update_field(field_id, registered_mask,
        f->nitrogen, f->phosphorus, f->potassium,
        f->temperature, f->humidity, f->light);
}

static void project_pipe_to_ui(void)
{
    const zb_pipe_data_t *pd = zigbee_bridge_get_pipes();

    if (!pd) {
        return;
    }

    for (int i = 0; i < ZB_MAX_PIPES; i++) {
        bool valve_bound;
        bool flow_bound;
        bool pressure_bound;

        get_pipe_binding_flags(i, &valve_bound, &flow_bound, &pressure_bound);
        ui_home_update_pipe(i,
            valve_bound, pd->pipes[i].valve_on,
            flow_bound, pd->pipes[i].flow,
            pressure_bound, pd->pipes[i].pressure);
        ui_device_update_pipe(i,
            valve_bound, pd->pipes[i].valve_on,
            flow_bound, pd->pipes[i].flow,
            pressure_bound, pd->pipes[i].pressure);
    }
}

static void project_control_to_ui(void)
{
    const zb_control_data_t *cd = zigbee_bridge_get_control();

    if (!cd) {
        return;
    }

    ui_home_update_control(cd->water_pump_on, cd->fert_pump_on,
        cd->fert_valve_on, cd->water_valve_on, cd->mixer_on);
    ui_home_update_mixer(cd->mixer_on);
    ui_device_update_control(cd->water_pump_on, cd->fert_pump_on,
        cd->fert_valve_on, cd->water_valve_on, cd->mixer_on);

    for (int i = 0; i < ZB_MAX_TANKS; i++) {
        bool level_bound = is_tank_level_bound(i + 1);
        ui_home_update_tank(i + 1,
            cd->tanks[i].switch_on,
            level_bound,
            cd->tanks[i].level);
        ui_device_update_tank(i + 1,
            cd->tanks[i].switch_on,
            level_bound,
            cd->tanks[i].level);
    }
}

static void update_zb_ui_async(void *param)
{
    zb_ui_msg_t *msg = (zb_ui_msg_t *)param;

    /* 更新帧计数和 Zigbee 状态 */
    s_zb_frame_count++;
    ui_home_update_zigbee_status(true, s_zb_frame_count);

    if (msg->dev_type == ZB_DEV_FIELD) {
        project_field_to_ui(msg->dev_id);
    } else if (msg->dev_type == ZB_DEV_PIPE) {
        project_pipe_to_ui();
    } else if (msg->dev_type == ZB_DEV_CONTROL) {
        project_control_to_ui();
    }

    free(msg);
}

static void on_zb_data(uint8_t dev_type, uint8_t dev_id, void *user_data)
{
    (void)user_data;
    zb_ui_msg_t *msg = malloc(sizeof(zb_ui_msg_t));
    if (!msg) return;
    msg->dev_type = dev_type;
    msg->dev_id = dev_id;
    lv_async_call(update_zb_ui_async, msg);
}

static void refresh_all_zb_ui(void)
{
    const zb_pipe_data_t *pd = zigbee_bridge_get_pipes();
    const zb_control_data_t *cd = zigbee_bridge_get_control();
    bool any_online = false;

    if (pd && pd->online) {
        any_online = true;
        project_pipe_to_ui();
    }

    if (cd && cd->online) {
        any_online = true;
        project_control_to_ui();
    }

    for (uint8_t field_id = 1; field_id <= ZB_MAX_FIELDS; field_id++) {
        const zb_field_data_t *f = zigbee_bridge_get_field(field_id);
        if (f && f->online) {
            any_online = true;
            project_field_to_ui(field_id);
        }
    }

    ui_home_update_zigbee_status(any_online, s_zb_frame_count);
}

/* 设备控制回调（UI → zigbee_bridge） */
static void on_device_control(uint32_t point_id, bool on)
{
    zb_control_target_t target;

    if (!zigbee_bridge_resolve_control_target(point_id, &target)) {
        ESP_LOGW(TAG, "Unsupported control point_id=%lu", (unsigned long)point_id);
        return;
    }

    ESP_LOGI(TAG, "Device control: point_id=%lu type=0x%02X id=%u on=%d",
             (unsigned long)point_id, target.dev_type, target.dev_id, on);
    zigbee_bridge_send_control(target.dev_type, target.dev_id, on);

    char desc[64];
    snprintf(desc, sizeof(desc), "设备控制: point=%lu -> %s",
             (unsigned long)point_id, on ? "开启" : "关闭");
    event_recorder_add_control(desc);
}


static uint32_t discovered_item_to_point_id(const zb_discovered_item_t *item)
{
    if (!item) {
        return 0;
    }

    if (item->dev_type == ZB_DEV_FIELD) {
        if (item->dev_id >= 1 && item->dev_id <= 6 && item->sensor_index < 6) {
            uint32_t node_id = 1000U + (uint32_t)item->dev_id;
            uint8_t point_no = (uint8_t)(item->sensor_index + 1U);
            return node_id * 100U + (uint32_t)point_no;
        }
        return 0;
    }

    if (item->dev_type == ZB_DEV_PIPE) {
        if (item->dev_id <= 6) {
            static const uint8_t pipe_point_map[] = {11U, 2U, 3U};
            if ((unsigned)item->sensor_index < (sizeof(pipe_point_map) / sizeof(pipe_point_map[0]))) {
                uint32_t node_id = 2000U + (uint32_t)item->dev_id;
                return node_id * 100U + (uint32_t)pipe_point_map[item->sensor_index];
            }
        }
        return 0;
    }

    if (item->dev_type == ZB_DEV_CONTROL) {
        static const uint8_t control_point_map[] = {
            11U, 12U, 13U, 14U, 15U,
            16U, 21U,
            17U, 22U,
            18U, 23U
        };
        if ((unsigned)item->sensor_index < (sizeof(control_point_map) / sizeof(control_point_map[0]))) {
            return 3000U * 100U + (uint32_t)control_point_map[item->sensor_index];
        }
        return 0;
    }

    return 0;
}

/* 搜索传感器回调（在 LVGL 任务中执行，不可使用大栈数组） */
static void on_search_sensor(void)
{
    ESP_LOGI(TAG, "Sensor search requested");

    /* PSRAM 堆分配，避免 LVGL 任务栈溢出 */
    zb_discovered_item_t *items = malloc(64 * sizeof(zb_discovered_item_t));
    if (!items) {
        ESP_LOGE(TAG, "alloc discovered items failed");
        return;
    }
    int count = zigbee_bridge_get_discovered(items, 64);
    ESP_LOGI(TAG, "Found %d sensors", count);

    if (count > 0) {
        ui_sensor_found_item_t *ui_items = malloc(count * sizeof(ui_sensor_found_item_t));
        if (!ui_items) {
            free(items);
            ESP_LOGE(TAG, "alloc ui_items failed");
            return;
        }
        for (int i = 0; i < count; i++) {
            strncpy(ui_items[i].name, items[i].name, sizeof(ui_items[i].name) - 1);
            ui_items[i].name[sizeof(ui_items[i].name) - 1] = '\0';
            strncpy(ui_items[i].type_name, items[i].type_name, sizeof(ui_items[i].type_name) - 1);
            ui_items[i].type_name[sizeof(ui_items[i].type_name) - 1] = '\0';
            ui_items[i].point_id = discovered_item_to_point_id(&items[i]);
        }
        /* 已在 LVGL 任务上下文，无需 bsp_display_lock */
        ui_settings_update_search_results(ui_items, count);
        free(ui_items);
    } else {
        /* 未找到任何传感器，弹窗提示 */
        ui_settings_update_search_results(NULL, 0);
    }
    free(items);
}

/* ---- device_registry 桥接回调 ---- */

static bool on_device_add(const ui_device_add_params_t *params)
{
    dev_device_info_t dev = {0};
    dev.type = params->type;
    dev.port = params->port;
    dev.id   = params->id;
    snprintf(dev.name, sizeof(dev.name), "%s", params->name);
    esp_err_t ret = device_registry_add(&dev);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Add device failed: %s", esp_err_to_name(ret));
    } else {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 新增设备 id=%u", (unsigned)params->id);
        event_recorder_add_operation(desc);
    }
    return (ret == ESP_OK);
}

static bool on_valve_add(const ui_valve_add_params_t *params)
{
    dev_valve_info_t valve = {0};
    valve.type = params->type;
    valve.channel = params->channel;
    valve.parent_device_id = params->parent_device_id;
    snprintf(valve.name, sizeof(valve.name), "%s", params->name);
    esp_err_t ret = valve_registry_add(&valve);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Add valve failed: %s", esp_err_to_name(ret));
    } else {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 新增阀门 dev=%u ch=%u",
                 (unsigned)params->parent_device_id,
                 (unsigned)params->channel);
        event_recorder_add_operation(desc);
    }
    return (ret == ESP_OK);
}

static bool on_sensor_add(const ui_sensor_add_params_t *params)
{
    dev_sensor_info_t sensor = {0};
    sensor.type = params->type;
    sensor.point_no = params->point_no;
    sensor.parent_device_id = params->parent_device_id;
    sensor.point_id = params->point_id;
    sensor.proto_type = (params->point_id / 10000U == 1U) ? ZB_DEV_FIELD :
                        (params->point_id / 10000U == 2U) ? ZB_DEV_PIPE :
                        (params->point_id / 10000U == 3U) ? ZB_DEV_CONTROL : 0U;
    snprintf(sensor.name, sizeof(sensor.name), "%s", params->name);
    esp_err_t ret = sensor_registry_add(&sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Add sensor failed: %s", esp_err_to_name(ret));
    } else {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 新增传感器 point=%lu",
                 (unsigned long)params->point_id);
        event_recorder_add_operation(desc);
    }
    return (ret == ESP_OK);
}

static bool on_device_delete(uint16_t device_id)
{
    esp_err_t ret = device_registry_remove(device_id);
    if (ret == ESP_OK) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 删除设备 id=%u", (unsigned)device_id);
        event_recorder_add_operation(desc);
    }
    return (ret == ESP_OK);
}

static bool on_valve_delete(uint16_t valve_id)
{
    esp_err_t ret = valve_registry_remove(valve_id);
    if (ret == ESP_OK) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 删除阀门 id=%u", (unsigned)valve_id);
        event_recorder_add_operation(desc);
    }
    return (ret == ESP_OK);
}

static bool on_sensor_delete(uint32_t point_id)
{
    esp_err_t ret = sensor_registry_remove(point_id);
    if (ret == ESP_OK) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 删除传感器 point=%lu", (unsigned long)point_id);
        event_recorder_add_operation(desc);
    }
    return (ret == ESP_OK);
}

static bool on_device_edit(uint16_t id, const ui_device_edit_params_t *params)
{
    dev_device_info_t dev = {0};
    dev.type = params->type;
    dev.port = params->port;
    snprintf(dev.name, sizeof(dev.name), "%s", params->name);
    bool ok = device_registry_update(id, &dev) == ESP_OK;
    if (ok) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 编辑设备 id=%u", (unsigned)id);
        event_recorder_add_operation(desc);
    }
    return ok;
}

static bool on_valve_edit(uint16_t id, const ui_valve_add_params_t *params)
{
    dev_valve_info_t valve = {0};
    valve.type = params->type;
    valve.channel = params->channel;
    valve.parent_device_id = params->parent_device_id;
    snprintf(valve.name, sizeof(valve.name), "%s", params->name);
    bool ok = valve_registry_update(id, &valve) == ESP_OK;
    if (ok) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 编辑阀门 id=%u", (unsigned)id);
        event_recorder_add_operation(desc);
    }
    return ok;
}

static bool on_sensor_edit(uint32_t point_id, const ui_sensor_edit_params_t *params)
{
    dev_sensor_info_t sensor = {0};
    sensor.type = params->type;
    snprintf(sensor.name, sizeof(sensor.name), "%s", params->name);
    bool ok = sensor_registry_update(point_id, &sensor) == ESP_OK;
    if (ok) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 编辑传感器 point=%lu", (unsigned long)point_id);
        event_recorder_add_operation(desc);
    }
    return ok;
}

static int on_get_device_count(void)
{
    return device_registry_get_count();
}

static int on_get_device_list(ui_device_row_t *out, int max, int offset)
{
    const dev_device_info_t *all = device_registry_get_all();
    int filled = 0;
    int skipped = 0;
    for (int i = 0; i < DEV_REG_MAX_DEVICES && filled < max; i++) {
        if (!all[i].valid) continue;
        if (skipped < offset) { skipped++; continue; }
        out[filled].type = all[i].type;
        out[filled].port = all[i].port;
        out[filled].id   = all[i].id;
        snprintf(out[filled].name, sizeof(out[filled].name), "%s", all[i].name);
        filled++;
    }
    return filled;
}

static int on_get_valve_count(void)
{
    return valve_registry_get_count();
}

static int on_get_valve_list(ui_valve_row_t *out, int max, int offset)
{
    const dev_valve_info_t *all = valve_registry_get_all();
    const dev_device_info_t *devs = device_registry_get_all();
    int filled = 0;
    int skipped = 0;
    for (int i = 0; i < DEV_REG_MAX_VALVES && filled < max; i++) {
        if (!all[i].valid) continue;
        if (skipped < offset) { skipped++; continue; }
        out[filled].type = all[i].type;
        out[filled].channel = all[i].channel;
        out[filled].id   = all[i].id;
        out[filled].parent_device_id = all[i].parent_device_id;
        snprintf(out[filled].name, sizeof(out[filled].name), "%s", all[i].name);
        /* 查找父设备名称 */
        out[filled].parent_name[0] = '\0';
        for (int j = 0; j < DEV_REG_MAX_DEVICES; j++) {
            if (devs[j].valid && devs[j].id == all[i].parent_device_id) {
                snprintf(out[filled].parent_name, sizeof(out[filled].parent_name), "%s", devs[j].name);
                break;
            }
        }
        if (out[filled].parent_name[0] == '\0') {
            snprintf(out[filled].parent_name, sizeof(out[filled].parent_name), "ID:%d(已删除)", all[i].parent_device_id);
        }
        filled++;
    }
    return filled;
}

static int on_get_sensor_count(void)
{
    return sensor_registry_get_count();
}

static int on_get_sensor_list(ui_sensor_row_t *out, int max, int offset)
{
    const dev_sensor_info_t *all = sensor_registry_get_all();
    const dev_device_info_t *devs = device_registry_get_all();
    int filled = 0;
    int skipped = 0;
    for (int i = 0; i < DEV_REG_MAX_SENSORS && filled < max; i++) {
        if (!all[i].valid) continue;
        if (skipped < offset) { skipped++; continue; }
        out[filled].type = all[i].type;
        out[filled].point_no = all[i].point_no;
        out[filled].parent_device_id = all[i].parent_device_id;
        out[filled].point_id = all[i].point_id;
        snprintf(out[filled].name, sizeof(out[filled].name), "%s", all[i].name);
        /* 查找父设备名称 */
        out[filled].parent_name[0] = '\0';
        for (int j = 0; j < DEV_REG_MAX_DEVICES; j++) {
            if (devs[j].valid && devs[j].id == all[i].parent_device_id) {
                snprintf(out[filled].parent_name, sizeof(out[filled].parent_name), "%s", devs[j].name);
                break;
            }
        }
        if (out[filled].parent_name[0] == '\0') {
            snprintf(out[filled].parent_name, sizeof(out[filled].parent_name), "ID:%d(已删除)", all[i].parent_device_id);
        }
        filled++;
    }
    return filled;
}

static int on_get_device_dropdown(char *buf, int buf_size)
{
    return device_registry_build_dropdown_str(buf, buf_size);
}

static int on_get_channel_count(uint16_t device_id)
{
    return device_registry_get_channel_count(device_id);
}

static bool on_is_sensor_added(uint32_t point_id)
{
    return sensor_registry_is_id_taken(point_id);
}

static uint8_t on_next_sensor_point_no(uint16_t parent_device_id)
{
    return sensor_registry_next_point_no(parent_device_id);
}

static uint16_t on_parse_device_id(int dropdown_index)
{
    const dev_device_info_t *all = device_registry_get_all();
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (!all[i].valid) continue;
        if (count == dropdown_index) return all[i].id;
        count++;
    }
    return 0;
}

/* ---- 查重桥接回调 ---- */
static bool on_device_name_taken(const char *name) { return device_registry_is_name_taken(name); }
static bool on_device_id_taken(uint16_t id)        { return device_registry_is_id_taken(id); }
static bool on_valve_name_taken(const char *name)   { return valve_registry_is_name_taken(name); }
static bool on_sensor_name_taken(const char *name)  { return sensor_registry_is_name_taken(name); }
static bool on_zone_name_taken(const char *name)    { return zone_registry_is_name_taken(name); }

/* ---- zone 桥接回调 ---- */

static bool on_zone_add(const ui_zone_add_params_t *params)
{
    dev_zone_info_t zone = {0};
    snprintf(zone.name, sizeof(zone.name), "%s", params->name);
    zone.valve_count = params->valve_count;
    zone.device_count = params->device_count;
    memcpy(zone.valve_ids, params->valve_ids, sizeof(uint16_t) * params->valve_count);
    memcpy(zone.device_ids, params->device_ids, sizeof(uint16_t) * params->device_count);
    bool ok = zone_registry_add(&zone) == ESP_OK;
    if (ok) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 新增灌区 %.20s", params->name);
        event_recorder_add_operation(desc);
    }
    return ok;
}

static bool on_zone_delete(int slot_index)
{
    bool ok = zone_registry_remove(slot_index) == ESP_OK;
    if (ok) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 删除灌区 slot=%d", slot_index);
        event_recorder_add_operation(desc);
    }
    return ok;
}

static bool on_zone_edit(int slot_index, const ui_zone_add_params_t *params)
{
    dev_zone_info_t zone = {0};
    snprintf(zone.name, sizeof(zone.name), "%s", params->name);
    zone.valve_count = params->valve_count;
    zone.device_count = params->device_count;
    memcpy(zone.valve_ids, params->valve_ids, sizeof(uint16_t) * params->valve_count);
    memcpy(zone.device_ids, params->device_ids, sizeof(uint16_t) * params->device_count);
    bool ok = zone_registry_update(slot_index, &zone) == ESP_OK;
    if (ok) {
        char desc[64];
        snprintf(desc, sizeof(desc), "设置: 编辑灌区 slot=%d", slot_index);
        event_recorder_add_operation(desc);
    }
    return ok;
}

static int on_get_zone_count(void)
{
    return zone_registry_get_count();
}

static int on_get_zone_list(ui_zone_row_t *out, int max, int offset)
{
    const dev_zone_info_t *all = zone_registry_get_all();
    const dev_valve_info_t *valves = valve_registry_get_all();
    const dev_device_info_t *devs = device_registry_get_all();
    int filled = 0;
    int skipped = 0;

    for (int i = 0; i < DEV_REG_MAX_ZONES && filled < max; i++) {
        if (!all[i].valid) continue;
        if (skipped < offset) { skipped++; continue; }

        out[filled].slot_index = i;
        snprintf(out[filled].name, sizeof(out[filled].name), "%s", all[i].name);
        out[filled].valve_count = all[i].valve_count;
        out[filled].device_count = all[i].device_count;

        /* 拼接阀门名称摘要 */
        out[filled].valve_names[0] = '\0';
        int pos = 0;
        for (int v = 0; v < all[i].valve_count; v++) {
            const char *vname = NULL;
            for (int j = 0; j < DEV_REG_MAX_VALVES; j++) {
                if (valves[j].valid && valves[j].id == all[i].valve_ids[v]) {
                    vname = valves[j].name;
                    break;
                }
            }
            if (v > 0 && pos < (int)sizeof(out[filled].valve_names) - 2) {
                pos += snprintf(out[filled].valve_names + pos,
                    sizeof(out[filled].valve_names) - pos, ",");
            }
            if (vname) {
                pos += snprintf(out[filled].valve_names + pos,
                    sizeof(out[filled].valve_names) - pos, "%s", vname);
            } else {
                pos += snprintf(out[filled].valve_names + pos,
                    sizeof(out[filled].valve_names) - pos, "ID:%d", all[i].valve_ids[v]);
            }
        }

        /* 拼接设备名称摘要 */
        out[filled].device_names[0] = '\0';
        pos = 0;
        for (int d = 0; d < all[i].device_count; d++) {
            const char *dname = NULL;
            for (int j = 0; j < DEV_REG_MAX_DEVICES; j++) {
                if (devs[j].valid && devs[j].id == all[i].device_ids[d]) {
                    dname = devs[j].name;
                    break;
                }
            }
            if (d > 0 && pos < (int)sizeof(out[filled].device_names) - 2) {
                pos += snprintf(out[filled].device_names + pos,
                    sizeof(out[filled].device_names) - pos, ",");
            }
            if (dname) {
                pos += snprintf(out[filled].device_names + pos,
                    sizeof(out[filled].device_names) - pos, "%s", dname);
            } else {
                pos += snprintf(out[filled].device_names + pos,
                    sizeof(out[filled].device_names) - pos, "ID:%d", all[i].device_ids[d]);
            }
        }

        filled++;
    }
    return filled;
}

static bool on_get_zone_detail(int slot_index, ui_zone_add_params_t *out)
{
    const dev_zone_info_t *all = zone_registry_get_all();
    if (slot_index < 0 || slot_index >= DEV_REG_MAX_ZONES) return false;
    if (!all[slot_index].valid) return false;

    snprintf(out->name, sizeof(out->name), "%s", all[slot_index].name);
    out->valve_count = all[slot_index].valve_count;
    out->device_count = all[slot_index].device_count;
    memcpy(out->valve_ids, all[slot_index].valve_ids,
           sizeof(uint16_t) * all[slot_index].valve_count);
    memcpy(out->device_ids, all[slot_index].device_ids,
           sizeof(uint16_t) * all[slot_index].device_count);
    return true;
}

static void on_irrigation_status_get(void *arg)
{
    ui_irrigation_runtime_status_t *status = (ui_irrigation_runtime_status_t *)arg;
    irr_runtime_status_t runtime = {0};

    irrigation_scheduler_get_runtime_status(&runtime);

    status->auto_enabled = runtime.auto_enabled;
    status->busy = runtime.busy;
    status->program_active = runtime.program_active;
    status->manual_irrigation_active = runtime.manual_irrigation_active;
    status->active_program_index = runtime.active_program_index;
    snprintf(status->active_name, sizeof(status->active_name), "%s", runtime.active_name);
    snprintf(status->status_text, sizeof(status->status_text), "%s", runtime.status_text);
    status->total_duration = runtime.total_duration;
    status->elapsed_seconds = runtime.elapsed_seconds;
}

static bool on_home_auto_mode_set(bool enabled)
{
    bool ok = irrigation_scheduler_set_auto_enabled(enabled);
    if (ok) {
        event_recorder_add_operation(enabled ? "首页操作: 开启自动模式" : "首页操作: 关闭自动模式");
    }
    return ok;
}

static bool on_home_auto_mode_get(void)
{
    return irrigation_scheduler_get_auto_enabled();
}

static bool on_home_program_start(int index)
{
    bool ok = irrigation_scheduler_start_program(index);
    if (ok) {
        char desc[64];
        snprintf(desc, sizeof(desc), "首页操作: 启动程序 #%d", index + 1);
        event_recorder_add_operation(desc);
    }
    return ok;
}

static bool on_home_manual_irrigation_start(const ui_manual_irrigation_request_t *req)
{
    if (!req) {
        return false;
    }

    irr_manual_irrigation_request_t backend_req = {
        .pre_water = req->pre_water,
        .post_water = req->post_water,
        .total_duration = req->total_duration,
    };
    snprintf(backend_req.formula, sizeof(backend_req.formula), "%s", req->formula);

    bool ok = irrigation_scheduler_start_manual_irrigation(&backend_req);
    if (ok) {
        char desc[64];
        snprintf(desc, sizeof(desc), "首页操作: 启动手动灌溉 %.20s", req->formula);
        event_recorder_add_operation(desc);
    }
    return ok;
}

static bool on_home_irrigation_status_get(ui_irrigation_runtime_status_t *out)
{
    if (!out) {
        return false;
    }

    on_irrigation_status_get(out);
    return true;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting Smart Irrigation System");

    /* Configure display: rotate to landscape 1280x800 */
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_90,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_PARTIAL,
        .touch_flags = {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };
    cfg.lv_adapter_cfg.task_stack_size = 16 * 1024; /* 16KB，默认 8KB 不够 */

    /* Initialize BSP display + touch + LVGL task */
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    if (disp == NULL) {
        ESP_LOGE(TAG, "Display init failed!");
        return;
    }
    /* Turn on backlight immediately to avoid black flash before display_manager loads NVS */
    bsp_display_brightness_set(80);

    /* Initialize NVS early — needed by display_manager, event_recorder, and wifi_manager */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_ret = nvs_flash_init();
    }

    /* Initialize display manager (NVS settings, GPIO35 wake, auto-off monitor) */
    esp_err_t dm_ret = display_manager_init();
    if (dm_ret != ESP_OK) {
        ESP_LOGE(TAG, "Display manager init failed: %s", esp_err_to_name(dm_ret));
        bsp_display_backlight_on();  /* Fallback: turn on backlight manually */
    }

    /* Initialize event recorder (NVS-based) */
    esp_err_t er_ret = event_recorder_init();
    if (er_ret != ESP_OK) {
        ESP_LOGE(TAG, "Event recorder init failed: %s", esp_err_to_name(er_ret));
    } else {
        event_recorder_add_poweron("上电");
        event_recorder_add_operation("系统启动");
    }

    esp_err_t alarm_ret = alarm_manager_init();
    if (alarm_ret != ESP_OK) {
        ESP_LOGE(TAG, "Alarm manager init failed: %s", esp_err_to_name(alarm_ret));
    }

    /* Register display settings callbacks */
    ui_display_register_preview_brightness_cb(on_preview_brightness);
    ui_display_register_save_brightness_cb(on_save_brightness);
    ui_display_register_timeout_cb(on_timeout_change);

    /* Initialize device registry (NVS-based, before UI init) */
    esp_err_t dr_ret = device_registry_init();
    if (dr_ret != ESP_OK) {
        ESP_LOGE(TAG, "Device registry init failed: %s", esp_err_to_name(dr_ret));
    }

    /* Initialize irrigation scheduler after NVS init */
    esp_err_t irr_ret = irrigation_scheduler_init();
    if (irr_ret != ESP_OK) {
        ESP_LOGE(TAG, "Irrigation scheduler init failed: %s", esp_err_to_name(irr_ret));
    }
    irrigation_scheduler_set_time_valid(s_time_synced);

    /* Load programs & formulas from NVS (before UI init so lists are populated) */
    ui_program_store_init();

    /* Initialize UI first (before WiFi, so screen shows immediately) */
    bsp_display_lock(-1);

    ui_init();

    /* Set display settings initial values from NVS */
    ui_display_set_initial_values(
        display_manager_get_brightness(),
        display_manager_get_timeout_index()
    );

    /* Restore static IP UI state from NVS (so settings page shows correct mode after reboot) */
    {
        nvs_handle_t h;
        if (nvs_open("wifi_ip", NVS_READONLY, &h) == ESP_OK) {
            uint8_t mode = 0;
            nvs_get_u8(h, "mode", &mode);
            if (mode == 1) {
                char ip[20] = {0}, cidr[4] = {0}, gw[20] = {0}, dns[20] = {0};
                size_t len;
                len = sizeof(ip);   nvs_get_str(h, "ip", ip, &len);
                len = sizeof(cidr); nvs_get_str(h, "cidr", cidr, &len);
                len = sizeof(gw);   nvs_get_str(h, "gw", gw, &len);
                len = sizeof(dns);  nvs_get_str(h, "dns", dns, &len);
                ui_network_set_initial_ip_mode(1, ip, cidr, gw, dns);
            }
            nvs_close(h);
        }
    }

    bsp_display_unlock();

    /* Register home irrigation callbacks */
    ui_home_register_auto_mode_set_cb(on_home_auto_mode_set);
    ui_home_register_auto_mode_get_cb(on_home_auto_mode_get);
    ui_home_register_program_start_cb(on_home_program_start);
    ui_home_register_manual_irrigation_start_cb(on_home_manual_irrigation_start);
    ui_home_register_irrigation_status_get_cb(on_home_irrigation_status_get);
    ui_home_register_runtime_refresh_cb(refresh_all_zb_ui);
    ui_home_register_zone_query_cbs(
        on_get_zone_count,
        on_get_zone_list,
        on_get_zone_detail
    );
    ui_home_register_zone_field_resolve_cb(on_home_zone_field_resolve);

    /* Register WiFi callbacks for UI (just function pointers, safe before WiFi init) */
    ui_wifi_register_scan_callback(request_wifi_scan);
    ui_wifi_register_connect_callback(request_wifi_connect);
    ui_wifi_register_disconnect_callback(request_wifi_disconnect);
    ui_wifi_register_connect_saved_callback(request_wifi_connect_saved);

    /* Register network settings callbacks */
    ui_network_register_apply_static_ip_cb(on_apply_static_ip);
    ui_network_register_restore_dhcp_cb(on_restore_dhcp);
    ui_network_register_get_info_cb(on_get_network_info);

    /* Register alarm callbacks */
    ui_alarm_register_query_current_cb(on_query_current_alarms);
    ui_alarm_register_clear_current_cb(on_clear_current_alarms);
    ui_alarm_register_load_settings_cb(on_load_alarm_settings);
    ui_alarm_register_save_settings_cb(on_save_alarm_settings);

    /* Register event records query callback */
    ui_alarm_rec_register_query_cb(on_query_records);
    ui_log_rec_register_query_cb(on_query_log_records);

    /* Register device control callback (UI → zigbee_bridge) */
    ui_device_register_control_cb(on_device_control);

    ui_device_register_query_cbs(
        on_get_valve_count,
        on_get_valve_list,
        on_get_sensor_count,
        on_get_sensor_list,
        on_get_zone_count,
        on_get_zone_list,
        on_get_zone_detail
    );

    /* Register sensor search callback */
    ui_settings_register_search_sensor_cb(on_search_sensor);

    /* Register device registry callbacks (UI → device_registry) */
    ui_settings_register_device_add_cb(on_device_add);
    ui_settings_register_valve_add_cb(on_valve_add);
    ui_settings_register_sensor_add_cb(on_sensor_add);
    ui_settings_register_device_delete_cb(on_device_delete);
    ui_settings_register_valve_delete_cb(on_valve_delete);
    ui_settings_register_sensor_delete_cb(on_sensor_delete);
    ui_settings_register_device_edit_cb(on_device_edit);
    ui_settings_register_valve_edit_cb(on_valve_edit);
    ui_settings_register_sensor_edit_cb(on_sensor_edit);
    ui_settings_register_query_cbs(
        on_get_device_count,
        on_get_device_list,
        on_get_valve_count,
        on_get_valve_list,
        on_get_sensor_count,
        on_get_sensor_list,
        on_get_device_dropdown,
        on_get_channel_count,
        on_is_sensor_added,
        on_next_sensor_point_no,
        on_parse_device_id
    );

    /* Register duplicate-check callbacks */
    ui_settings_register_device_name_check_cb(on_device_name_taken);
    ui_settings_register_device_id_check_cb(on_device_id_taken);
    ui_settings_register_valve_name_check_cb(on_valve_name_taken);
    ui_settings_register_sensor_name_check_cb(on_sensor_name_taken);
    ui_settings_register_zone_name_check_cb(on_zone_name_taken);

    /* Register zone callbacks */
    ui_settings_register_zone_add_cb(on_zone_add);
    ui_settings_register_zone_delete_cb(on_zone_delete);
    ui_settings_register_zone_edit_cb(on_zone_edit);
    ui_settings_register_zone_query_cbs(on_get_zone_count, on_get_zone_list, on_get_zone_detail);
    ui_program_register_selection_query_cbs(
        on_get_valve_count,
        on_get_valve_list,
        on_get_zone_count,
        on_get_zone_list,
        on_get_zone_detail
    );

    /* Initialize Zigbee bridge (UART1: TX=GPIO49, RX=GPIO50) */
    esp_err_t zb_ret = zigbee_bridge_init(49, 50);
    if (zb_ret != ESP_OK) {
        ESP_LOGE(TAG, "Zigbee bridge init failed: %s", esp_err_to_name(zb_ret));
    } else {
        zigbee_bridge_register_data_cb(on_zb_data, NULL);
        ESP_LOGI(TAG, "Zigbee bridge initialized");
    }

    /* Initialize WiFi in background task (esp_hosted may block waiting for companion chip) */
    xTaskCreate(wifi_init_task, "wifi_init", 8192, NULL, 5, NULL);

    /* Start time update task */
    xTaskCreate(time_update_task, "time_update", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Smart Irrigation UI started successfully");
}
