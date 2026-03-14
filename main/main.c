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

    /* Initialize WiFi in background task (esp_hosted may block waiting for companion chip) */
    xTaskCreate(wifi_init_task, "wifi_init", 8192, NULL, 5, NULL);

    /* Start time update task */
    xTaskCreate(time_update_task, "time_update", 4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "Smart Irrigation UI started successfully");
}
