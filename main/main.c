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
#include "ui_alarm_records.h"
#include "wifi_manager.h"
#include "display_manager.h"
#include "event_recorder.h"
#include "zigbee_bridge.h"
#include "device_registry.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "smart_irrigation";

/* NTP time sync state */
static bool s_time_synced = false;
static bool s_ntp_started = false;
static bool s_wifi_connected = false;

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

static void refresh_alarm_records_async(void *arg)
{
    (void)arg;
    ui_alarm_rec_refresh_visible();
}

static void sntp_sync_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "NTP time synchronized");
    s_time_synced = true;
    event_recorder_fix_timestamps();
    lv_async_call(refresh_alarm_records_async, NULL);
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
    }
}

static void request_wifi_disconnect(void)
{
    ESP_LOGI(TAG, "WiFi disconnect requested by UI");
    esp_err_t ret = wifi_manager_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi disconnect failed: %s", esp_err_to_name(ret));
    }
}

static void request_wifi_connect_saved(const char *ssid)
{
    ESP_LOGI(TAG, "WiFi connect (saved) requested: SSID=%s", ssid);
    esp_err_t ret = wifi_manager_connect_saved(ssid);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect saved failed: %s", esp_err_to_name(ret));
    }
}

/* ---- Display settings bridge callbacks ---- */

static void on_brightness_change(int percent)
{
    display_manager_set_brightness(percent);
}

static void on_timeout_change(int index)
{
    display_manager_set_timeout_index(index);
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
    return (ret == ESP_OK) ? UI_NET_RESULT_OK : UI_NET_RESULT_FAIL;
}

static ui_net_result_t on_restore_dhcp(void)
{
    ESP_LOGI(TAG, "DHCP restore requested");
    esp_err_t ret = wifi_manager_restore_dhcp();
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
    /* Use static to reduce stack usage (called only from LVGL task) */
    static evt_query_result_t r;
    event_recorder_query((evt_type_t)type, start_ts, end_ts, page, &r);

    result->count = r.count;
    result->total_matched = r.total_matched;
    result->total_pages = r.total_pages;
    result->current_page = r.current_page;
    for (int i = 0; i < r.count; i++) {
        result->records[i].timestamp = r.records[i].timestamp;
        memcpy(result->records[i].desc, r.records[i].desc, sizeof(r.records[i].desc));
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

static void update_zb_ui_async(void *param)
{
    zb_ui_msg_t *msg = (zb_ui_msg_t *)param;

    /* 更新帧计数和 Zigbee 状态 */
    s_zb_frame_count++;
    ui_home_update_zigbee_status(true, s_zb_frame_count);

    if (msg->dev_type == ZB_DEV_FIELD) {
        const zb_field_data_t *f = zigbee_bridge_get_field(msg->dev_id);
        if (f) {
            ui_home_update_field(msg->dev_id,
                f->nitrogen, f->phosphorus, f->potassium,
                f->temperature, f->humidity, f->light);
        }
    } else if (msg->dev_type == ZB_DEV_PIPE) {
        const zb_pipe_data_t *pd = zigbee_bridge_get_pipes();
        if (pd) {
            for (int i = 0; i < 7; i++) {
                ui_home_update_pipe(i, pd->pipes[i].valve_on,
                    pd->pipes[i].flow, pd->pipes[i].pressure);
            }
        }
    } else if (msg->dev_type == ZB_DEV_CONTROL) {
        const zb_control_data_t *cd = zigbee_bridge_get_control();
        if (cd) {
            ui_home_update_control(cd->water_pump_on, cd->fert_pump_on,
                cd->fert_valve_on, cd->water_valve_on, cd->mixer_on);
            for (int i = 0; i < 3; i++) {
                ui_home_update_tank(i + 1, cd->tanks[i].switch_on,
                    cd->tanks[i].level);
            }
        }
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

/* 设备控制回调（UI → zigbee_bridge） */
static void on_device_control(uint8_t dev_type, uint8_t dev_id, bool on)
{
    ESP_LOGI(TAG, "Device control: type=0x%02X id=%d on=%d", dev_type, dev_id, on);
    zigbee_bridge_send_control(dev_type, dev_id, on);
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
            strncpy(ui_items[i].name, items[i].name, 32);
            strncpy(ui_items[i].type_name, items[i].type_name, 16);
            ui_items[i].dev_type = items[i].dev_type;
            ui_items[i].dev_id = items[i].dev_id;
            ui_items[i].sensor_index = items[i].sensor_index;
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
    }
    return (ret == ESP_OK);
}

static bool on_sensor_add(const ui_sensor_add_params_t *params)
{
    dev_sensor_info_t sensor = {0};
    sensor.type = params->type;
    sensor.sensor_index = params->sensor_index;
    sensor.zb_dev_type = params->zb_dev_type;
    sensor.parent_device_id = params->parent_device_id;
    sensor.composed_id = (uint32_t)params->zb_dev_type * 1000000
                       + (uint32_t)params->parent_device_id * 100
                       + params->sensor_index;
    snprintf(sensor.name, sizeof(sensor.name), "%s", params->name);
    esp_err_t ret = sensor_registry_add(&sensor);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Add sensor failed: %s", esp_err_to_name(ret));
    }
    return (ret == ESP_OK);
}

static bool on_device_delete(uint16_t device_id)
{
    esp_err_t ret = device_registry_remove(device_id);
    return (ret == ESP_OK);
}

static bool on_valve_delete(uint16_t valve_id)
{
    esp_err_t ret = valve_registry_remove(valve_id);
    return (ret == ESP_OK);
}

static bool on_sensor_delete(uint32_t composed_id)
{
    esp_err_t ret = sensor_registry_remove(composed_id);
    return (ret == ESP_OK);
}

static bool on_device_edit(uint16_t id, const ui_device_edit_params_t *params)
{
    dev_device_info_t dev = {0};
    dev.type = params->type;
    dev.port = params->port;
    snprintf(dev.name, sizeof(dev.name), "%s", params->name);
    bool ok = device_registry_update(id, &dev) == ESP_OK;
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
    return ok;
}

static bool on_sensor_edit(uint32_t composed_id, const ui_sensor_edit_params_t *params)
{
    dev_sensor_info_t sensor = {0};
    sensor.type = params->type;
    snprintf(sensor.name, sizeof(sensor.name), "%s", params->name);
    bool ok = sensor_registry_update(composed_id, &sensor) == ESP_OK;
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
        out[filled].sensor_index = all[i].sensor_index;
        out[filled].zb_dev_type = all[i].zb_dev_type;
        out[filled].parent_device_id = all[i].parent_device_id;
        out[filled].composed_id = all[i].composed_id;
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

static bool on_is_sensor_added(uint32_t composed_id)
{
    return sensor_registry_is_id_taken(composed_id);
}

static uint8_t on_next_sensor_index(uint16_t parent_device_id)
{
    return sensor_registry_next_index(parent_device_id);
}

static uint8_t on_get_sensor_parent_zb_type(uint16_t parent_device_id)
{
    const dev_device_info_t *dev = device_registry_get_by_id(parent_device_id);
    if (!dev) return 0;

    switch ((dev_type_t)dev->type) {
        case DEV_TYPE_ZB_SENSOR_NODE:
            return ZB_DEV_FIELD;
        case DEV_TYPE_ZB_CTRL_NODE:
            return ZB_DEV_CONTROL;
        default:
            return 0;
    }
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
    return ok;
}

static bool on_zone_delete(int slot_index)
{
    bool ok = zone_registry_remove(slot_index) == ESP_OK;
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
    }

    /* Register display settings callbacks */
    ui_display_register_brightness_cb(on_brightness_change);
    ui_display_register_timeout_cb(on_timeout_change);

    /* Initialize device registry (NVS-based, before UI init) */
    esp_err_t dr_ret = device_registry_init();
    if (dr_ret != ESP_OK) {
        ESP_LOGE(TAG, "Device registry init failed: %s", esp_err_to_name(dr_ret));
    }

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

    /* Register WiFi callbacks for UI (just function pointers, safe before WiFi init) */
    ui_wifi_register_scan_callback(request_wifi_scan);
    ui_wifi_register_connect_callback(request_wifi_connect);
    ui_wifi_register_disconnect_callback(request_wifi_disconnect);
    ui_wifi_register_connect_saved_callback(request_wifi_connect_saved);

    /* Register network settings callbacks */
    ui_network_register_apply_static_ip_cb(on_apply_static_ip);
    ui_network_register_restore_dhcp_cb(on_restore_dhcp);
    ui_network_register_get_info_cb(on_get_network_info);

    /* Register event records query callback */
    ui_alarm_rec_register_query_cb(on_query_records);

    /* Register device control callback (UI → zigbee_bridge) */
    ui_device_register_control_cb(on_device_control);

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
        on_next_sensor_index,
        on_get_sensor_parent_zb_type,
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
