/**
 * @file wifi_manager.c
 * @brief WiFi manager implementation for ESP32-P4 (via esp_hosted)
 */

#include "wifi_manager.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"

static const char *TAG = "wifi_manager";

/* NVS credential storage */
#define WIFI_CRED_NVS_NS    "wifi_cred"
#define WIFI_CRED_MAX_SAVED 5

static bool s_initialized = false;
static bool s_scanning = false;
static wifi_mgr_scan_done_cb_t s_scan_cb = NULL;
static void *s_scan_user_data = NULL;
static EventGroupHandle_t s_wifi_event_group = NULL;
static TaskHandle_t s_auto_connect_task_handle = NULL;

/* Connection state */
static bool s_connected = false;
static char s_connected_ssid[33] = {0};
static char s_connecting_ssid[33] = {0};
static char s_connecting_password[65] = {0};
static wifi_mgr_conn_status_cb_t s_conn_cb = NULL;
static void *s_conn_user_data = NULL;

/* Boot auto-connect flag */
static bool s_auto_connecting = false;

/* WiFi switching state (disconnect old AP before connecting new AP) */
static bool s_switching = false;
static char s_pending_ssid[33] = {0};
static char s_pending_password[65] = {0};

/* Reconnect with backoff */
static TimerHandle_t s_reconnect_timer = NULL;
static int s_reconnect_count = 0;
static bool s_reconnect_active = false;
static bool s_manual_disconnect = false;
static char s_last_good_ssid[33] = {0};   /* Last successfully connected AP */
static bool s_reconnect_scanning = false;  /* Scan in progress for reconnect */

/* WiFi STA network interface handle */
static esp_netif_t *s_sta_netif = NULL;

#define WIFI_SCAN_DONE_BIT   BIT0
#define WIFI_RECONNECT_BIT   BIT1

/* ---- Forward declarations ---- */

static bool load_wifi_credentials(const char *ssid, char *password, size_t pwd_len);
static void start_reconnect(uint32_t delay_ms);
static void restore_static_ip_if_saved(void);

/* ---- Reconnect helpers ---- */

/**
 * @brief Get reconnect delay based on attempt count.
 *
 *   1-10:  10 s
 *  11-13:  30 s
 *  14-16:   1 min
 *  17-19:   6 min
 *  20-23:  18 min
 *  24+  :  40 min
 */
static uint32_t get_reconnect_delay_ms(int count)
{
    if (count < 10) return 10u * 1000;
    if (count < 13) return 30u * 1000;
    if (count < 16) return 60u * 1000;
    if (count < 19) return 6u * 60 * 1000;
    if (count < 23) return 18u * 60 * 1000;
    return 40u * 60 * 1000;
}

static void reconnect_try_direct(void)
{
    /* Connect directly to last known good AP */
    if (s_last_good_ssid[0]) {
        char password[65] = {0};
        load_wifi_credentials(s_last_good_ssid, password, sizeof(password));

        ESP_LOGI(TAG, "Reconnect attempt %d to: %s",
                 s_reconnect_count + 1, s_last_good_ssid);

        strlcpy(s_connecting_ssid, s_last_good_ssid, sizeof(s_connecting_ssid));
        strlcpy(s_connecting_password, password, sizeof(s_connecting_password));

        wifi_config_t wifi_config = {0};
        strlcpy((char *)wifi_config.sta.ssid, s_last_good_ssid,
                sizeof(wifi_config.sta.ssid));
        if (password[0]) {
            strlcpy((char *)wifi_config.sta.password, password,
                    sizeof(wifi_config.sta.password));
        }
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    } else {
        ESP_LOGI(TAG, "Reconnect attempt %d (no last good AP)", s_reconnect_count + 1);
    }
    esp_wifi_connect();
}

static void reconnect_timer_cb(TimerHandle_t timer)
{
    (void)timer;
    /* Only signal — heavy work is done in wifi_reconnect_task */
    if (s_wifi_event_group) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_RECONNECT_BIT);
    }
}

static void start_reconnect(uint32_t delay_ms)
{
    if (!s_reconnect_timer) return;
    s_reconnect_active = true;
    ESP_LOGI(TAG, "Scheduling reconnect in %lu ms (attempt %d)",
             (unsigned long)delay_ms, s_reconnect_count + 1);
    /* xTimerChangePeriod also (re-)starts the timer */
    xTimerChangePeriod(s_reconnect_timer, pdMS_TO_TICKS(delay_ms), portMAX_DELAY);
}

static void stop_reconnect(void)
{
    if (s_reconnect_active) {
        ESP_LOGI(TAG, "Reconnect stopped (after %d attempts)", s_reconnect_count);
    }
    s_reconnect_active = false;
    s_reconnect_count = 0;
    s_reconnect_scanning = false;
    if (s_reconnect_timer) {
        xTimerStop(s_reconnect_timer, 0);
    }
}

/* ---- NVS credential helpers ---- */

static void save_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    if (nvs_open(WIFI_CRED_NVS_NS, NVS_READWRITE, &handle) != ESP_OK) return;

    uint8_t count = 0;
    nvs_get_u8(handle, "cnt", &count);
    if (count > WIFI_CRED_MAX_SAVED) count = WIFI_CRED_MAX_SAVED;

    /* Check if SSID already exists */
    char key_s[8], key_p[8];
    int existing_idx = -1;
    for (int i = 0; i < count; i++) {
        snprintf(key_s, sizeof(key_s), "s%d", i);
        char saved_ssid[33] = {0};
        size_t len = sizeof(saved_ssid);
        if (nvs_get_str(handle, key_s, saved_ssid, &len) == ESP_OK) {
            if (strcmp(saved_ssid, ssid) == 0) {
                existing_idx = i;
                break;
            }
        }
    }

    int target_idx;
    if (existing_idx >= 0) {
        /* Update in place */
        target_idx = existing_idx;
    } else if (count < WIFI_CRED_MAX_SAVED) {
        /* Add new entry */
        target_idx = count;
        count++;
    } else {
        /* Full: shift down, drop oldest (index 0) */
        for (int i = 0; i < WIFI_CRED_MAX_SAVED - 1; i++) {
            char src_s[8], src_p[8], dst_s[8], dst_p[8];
            snprintf(src_s, sizeof(src_s), "s%d", i + 1);
            snprintf(src_p, sizeof(src_p), "p%d", i + 1);
            snprintf(dst_s, sizeof(dst_s), "s%d", i);
            snprintf(dst_p, sizeof(dst_p), "p%d", i);

            char buf[65] = {0};
            size_t len = sizeof(buf);
            if (nvs_get_str(handle, src_s, buf, &len) == ESP_OK)
                nvs_set_str(handle, dst_s, buf);
            len = sizeof(buf);
            if (nvs_get_str(handle, src_p, buf, &len) == ESP_OK)
                nvs_set_str(handle, dst_p, buf);
        }
        target_idx = WIFI_CRED_MAX_SAVED - 1;
    }

    snprintf(key_s, sizeof(key_s), "s%d", target_idx);
    snprintf(key_p, sizeof(key_p), "p%d", target_idx);
    nvs_set_str(handle, key_s, ssid);
    nvs_set_str(handle, key_p, password ? password : "");
    nvs_set_u8(handle, "cnt", count);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Saved credentials for: %s (slot %d, total %d)", ssid, target_idx, count);
}

static bool load_wifi_credentials(const char *ssid, char *password, size_t pwd_len)
{
    nvs_handle_t handle;
    if (nvs_open(WIFI_CRED_NVS_NS, NVS_READONLY, &handle) != ESP_OK) return false;

    uint8_t count = 0;
    nvs_get_u8(handle, "cnt", &count);

    bool found = false;
    for (int i = 0; i < count && i < WIFI_CRED_MAX_SAVED; i++) {
        char key_s[8];
        snprintf(key_s, sizeof(key_s), "s%d", i);
        char saved_ssid[33] = {0};
        size_t len = sizeof(saved_ssid);
        if (nvs_get_str(handle, key_s, saved_ssid, &len) == ESP_OK) {
            if (strcmp(saved_ssid, ssid) == 0) {
                if (password && pwd_len > 0) {
                    char key_p[8];
                    snprintf(key_p, sizeof(key_p), "p%d", i);
                    size_t plen = pwd_len;
                    nvs_get_str(handle, key_p, password, &plen);
                }
                found = true;
                break;
            }
        }
    }

    nvs_close(handle);
    return found;
}

/* ---- Event handler ---- */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_SCAN_DONE_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        strncpy(s_connected_ssid, (char *)event->ssid, 32);
        s_connected_ssid[32] = '\0';
        ESP_LOGI(TAG, "WiFi connected to: %s", s_connected_ssid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        char old_ssid[33];
        strncpy(old_ssid, s_connected_ssid[0] ? s_connected_ssid : s_connecting_ssid, 33);
        bool was_connected = s_connected;
        s_connected = false;
        s_connected_ssid[0] = '\0';

        /* Switching to a new AP: disconnect completed, now connect to pending AP */
        if (s_switching) {
            ESP_LOGI(TAG, "Switching WiFi: disconnected from old, connecting to %s", s_pending_ssid);
            s_switching = false;

            strncpy(s_connecting_ssid, s_pending_ssid, 32);
            s_connecting_ssid[32] = '\0';
            strncpy(s_connecting_password, s_pending_password, 64);
            s_connecting_password[64] = '\0';
            memset(s_pending_ssid, 0, sizeof(s_pending_ssid));
            memset(s_pending_password, 0, sizeof(s_pending_password));

            wifi_config_t wifi_config = {0};
            strlcpy((char *)wifi_config.sta.ssid, s_connecting_ssid,
                    sizeof(wifi_config.sta.ssid));
            if (s_connecting_password[0]) {
                strlcpy((char *)wifi_config.sta.password, s_connecting_password,
                        sizeof(wifi_config.sta.password));
            }
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();
            return;  /* Don't notify UI — connecting dialog stays up */
        }

        /* Reconnect retry failed — schedule next attempt with backoff */
        if (s_reconnect_active) {
            s_reconnect_count++;
            uint32_t delay = get_reconnect_delay_ms(s_reconnect_count);
            ESP_LOGI(TAG, "Reconnect %d failed, next in %lu ms",
                     s_reconnect_count, (unsigned long)delay);
            start_reconnect(delay);
            return;  /* Silent — don't notify UI */
        }

        /* Boot auto-connect failed — switch to reconnect backoff */
        if (s_auto_connecting) {
            s_auto_connecting = false;
            ESP_LOGI(TAG, "Auto-connect failed, starting reconnect backoff");
            start_reconnect(get_reconnect_delay_ms(0));
            return;  /* Silent — don't notify UI */
        }

        /* --- First-time disconnect: notify UI, then schedule reconnect --- */
        ESP_LOGI(TAG, "WiFi disconnected");
        if (was_connected && s_conn_cb) {
            s_conn_cb(WIFI_MGR_DISCONNECTED, old_ssid, s_conn_user_data);
        } else if (!was_connected && s_conn_cb) {
            s_conn_cb(WIFI_MGR_CONNECT_FAILED, s_connecting_ssid, s_conn_user_data);
        }

        if (s_manual_disconnect) {
            /* Manual disconnect: wait 60 s then start reconnecting */
            s_manual_disconnect = false;
            start_reconnect(60u * 1000);
        } else if (was_connected) {
            /* Unexpected disconnect: start reconnecting from 10 s */
            start_reconnect(get_reconnect_delay_ms(0));
        } else if (s_last_good_ssid[0]) {
            /* Failed to connect to a new AP, but we have a last-good AP to fall back to.
             * Use short delay so user gets back online quickly. */
            ESP_LOGI(TAG, "Connect failed, will reconnect to %s shortly", s_last_good_ssid);
            start_reconnect(5u * 1000);
        } else {
            /* No previously connected AP — wait 60 s before retrying */
            start_reconnect(60u * 1000);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        bool was_connected = s_connected;
        s_connected = true;
        s_auto_connecting = false;
        s_switching = false;
        s_manual_disconnect = false;
        stop_reconnect();

        /* Remember last good AP for reconnect */
        strncpy(s_last_good_ssid, s_connected_ssid, 32);
        s_last_good_ssid[32] = '\0';

        /* Save credentials only on first IP event per connection.
         * Skip on subsequent IP changes (e.g. static IP set) to avoid
         * overwriting the saved password with an empty string. */
        if (!was_connected) {
            save_wifi_credentials(s_connected_ssid, s_connecting_password);
            memset(s_connecting_password, 0, sizeof(s_connecting_password));
            /* Restore static IP if previously configured */
            restore_static_ip_if_saved();
        }

        if (s_conn_cb) {
            s_conn_cb(WIFI_MGR_CONNECTED, s_connected_ssid, s_conn_user_data);
        }
    }
}

/* ---- Scan task ---- */

/**
 * @brief Handle scan results for reconnect: pick the strongest AP with saved credentials.
 *
 * ap_records are already sorted by RSSI descending by ESP-IDF.
 * Returns true if a connection attempt was started.
 */
static bool reconnect_pick_best_ap(wifi_ap_record_t *ap_records, uint16_t count)
{
    for (int i = 0; i < count; i++) {
        char ssid[33] = {0};
        strncpy(ssid, (char *)ap_records[i].ssid, 32);
        if (ssid[0] == '\0') continue;

        char password[65] = {0};
        if (load_wifi_credentials(ssid, password, sizeof(password))) {
            ESP_LOGI(TAG, "Reconnect: found known AP '%s' (RSSI %d), connecting",
                     ssid, ap_records[i].rssi);

            strlcpy(s_connecting_ssid, ssid, sizeof(s_connecting_ssid));
            strlcpy(s_connecting_password, password, sizeof(s_connecting_password));

            wifi_config_t wifi_config = {0};
            strlcpy((char *)wifi_config.sta.ssid, ssid,
                    sizeof(wifi_config.sta.ssid));
            if (password[0]) {
                strlcpy((char *)wifi_config.sta.password, password,
                        sizeof(wifi_config.sta.password));
            }
            esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
            esp_wifi_connect();
            return true;
        }
    }
    return false;
}

static void wifi_scan_task(void *pvParameters)
{
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_SCAN_DONE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

        if (!(bits & WIFI_SCAN_DONE_BIT)) {
            continue;
        }

        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        ESP_LOGI(TAG, "Scan done, found %d APs", ap_count);

        uint16_t actual = ap_count;
        if (actual > WIFI_MANAGER_MAX_AP_RECORDS) {
            actual = WIFI_MANAGER_MAX_AP_RECORDS;
        }

        /* --- Reconnect scan path --- */
        if (s_reconnect_scanning) {
            s_reconnect_scanning = false;
            s_scanning = false;

            if (!s_reconnect_active || s_connected) {
                /* Reconnect cancelled or already connected — discard scan results */
                esp_wifi_clear_ap_list();
                continue;
            }

            if (actual == 0) {
                ESP_LOGW(TAG, "Reconnect scan: no APs found");
                s_reconnect_count++;
                start_reconnect(get_reconnect_delay_ms(s_reconnect_count));
                continue;
            }

            wifi_ap_record_t *ap_records = calloc(actual, sizeof(wifi_ap_record_t));
            if (!ap_records) {
                ESP_LOGE(TAG, "Reconnect scan: alloc failed");
                s_reconnect_count++;
                start_reconnect(get_reconnect_delay_ms(s_reconnect_count));
                continue;
            }
            esp_wifi_scan_get_ap_records(&actual, ap_records);

            bool started = reconnect_pick_best_ap(ap_records, actual);
            free(ap_records);

            if (!started) {
                ESP_LOGW(TAG, "Reconnect scan: no known AP found nearby");
                s_reconnect_count++;
                start_reconnect(get_reconnect_delay_ms(s_reconnect_count));
            }
            /* If started, the disconnect handler will manage the next retry on failure */
            continue;
        }

        /* --- Normal UI scan path --- */
        wifi_mgr_scan_result_t result = {0};
        if (actual == 0) {
            result.ap_count = 0;
            if (s_scan_cb) {
                s_scan_cb(&result, s_scan_user_data);
            }
            s_scanning = false;
            continue;
        }

        wifi_ap_record_t *ap_records = calloc(actual, sizeof(wifi_ap_record_t));
        if (!ap_records) {
            ESP_LOGE(TAG, "Failed to allocate memory for %d scan results", actual);
            result.ap_count = 0;
            if (s_scan_cb) {
                s_scan_cb(&result, s_scan_user_data);
            }
            s_scanning = false;
            continue;
        }

        esp_wifi_scan_get_ap_records(&actual, ap_records);

        /* Convert to UI-friendly structure */
        result.ap_count = actual;
        for (int i = 0; i < actual; i++) {
            strncpy(result.ap_list[i].ssid, (char *)ap_records[i].ssid, 32);
            result.ap_list[i].ssid[32] = '\0';
            result.ap_list[i].rssi = ap_records[i].rssi;
            result.ap_list[i].authmode = (uint8_t)ap_records[i].authmode;
            result.ap_list[i].is_connected = s_connected &&
                (strcmp(result.ap_list[i].ssid, s_connected_ssid) == 0);
            result.ap_list[i].has_saved_password =
                wifi_manager_has_saved_password(result.ap_list[i].ssid);
        }
        free(ap_records);

        if (s_scan_cb) {
            s_scan_cb(&result, s_scan_user_data);
        }

        s_scanning = false;
    }
}

/* ---- Reconnect task (runs heavy NVS/scan operations with adequate stack) ---- */

static void wifi_reconnect_task(void *pvParameters)
{
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_RECONNECT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

        if (!(bits & WIFI_RECONNECT_BIT)) continue;
        if (!s_reconnect_active || s_connected || s_switching) continue;

        if (s_reconnect_count < 6 && s_last_good_ssid[0]) {
            /* First 6 attempts: connect directly to last good AP */
            reconnect_try_direct();
        } else {
            /* After 6 attempts (or no last good AP): scan for known APs */
            if (s_scanning) {
                ESP_LOGW(TAG, "Scan in progress, retry later");
                start_reconnect(get_reconnect_delay_ms(s_reconnect_count));
                continue;
            }
            ESP_LOGI(TAG, "Reconnect attempt %d: scanning for known APs",
                     s_reconnect_count + 1);
            s_reconnect_scanning = true;
            s_scanning = true;
            wifi_scan_config_t scan_config = {
                .ssid = NULL, .bssid = NULL, .channel = 0,
                .show_hidden = false, .scan_type = WIFI_SCAN_TYPE_ACTIVE,
                .scan_time.active.min = 100, .scan_time.active.max = 300,
            };
            esp_err_t err = esp_wifi_scan_start(&scan_config, false);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Reconnect scan failed: %s", esp_err_to_name(err));
                s_reconnect_scanning = false;
                s_scanning = false;
                s_reconnect_count++;
                start_reconnect(get_reconnect_delay_ms(s_reconnect_count));
            }
        }
    }
}

/* ---- Auto-connect task ---- */

static void auto_connect_task(void *pvParameters)
{
    nvs_handle_t handle;
    if (nvs_open(WIFI_CRED_NVS_NS, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGI(TAG, "No saved WiFi credentials for auto-connect");
        goto exit;
    }

    uint8_t count = 0;
    nvs_get_u8(handle, "cnt", &count);

    if (count == 0) {
        nvs_close(handle);
        ESP_LOGI(TAG, "No saved WiFi credentials for auto-connect");
        goto exit;
    }

    /* Use most recent entry (highest index) */
    int idx = count - 1;
    char key_s[8], key_p[8];
    snprintf(key_s, sizeof(key_s), "s%d", idx);
    snprintf(key_p, sizeof(key_p), "p%d", idx);

    char ssid[33] = {0}, password[65] = {0};
    size_t len = sizeof(ssid);
    esp_err_t err = nvs_get_str(handle, key_s, ssid, &len);
    if (err != ESP_OK || ssid[0] == '\0') {
        nvs_close(handle);
        goto exit;
    }

    len = sizeof(password);
    nvs_get_str(handle, key_p, password, &len);
    nvs_close(handle);

    ESP_LOGI(TAG, "Auto-connecting to: %s", ssid);
    s_auto_connecting = true;
    wifi_manager_connect(ssid, password);

exit:
    s_auto_connect_task_handle = NULL;
    vTaskDelete(NULL);
}

/* ---- Public API ---- */

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) return ESP_OK;

    /* NVS init (required by WiFi) */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Network interface + event loop */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    /* WiFi driver init */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    /* Set STA mode and start */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Create event group, scan task, and reconnect task */
    s_wifi_event_group = xEventGroupCreate();
    xTaskCreate(wifi_scan_task, "wifi_scan", 4096, NULL, 5, NULL);
    xTaskCreate(wifi_reconnect_task, "wifi_reconn", 4096, NULL, 5, NULL);

    /* Create reconnect timer (one-shot) */
    s_reconnect_timer = xTimerCreate("wifi_reconn", pdMS_TO_TICKS(10000),
                                      pdFALSE, NULL, reconnect_timer_cb);

    /* Auto-connect task is started after connection callback registration */

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_start_scan(wifi_mgr_scan_done_cb_t cb, void *user_data)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (s_scanning) {
        ESP_LOGW(TAG, "Scan already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    s_scan_cb = cb;
    s_scan_user_data = user_data;
    s_scanning = true;

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, false);
    if (err != ESP_OK) {
        s_scanning = false;
        ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "WiFi scan started");
    }
    return err;
}

bool wifi_manager_is_scanning(void)
{
    return s_scanning;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    /* User initiated a new connection — cancel any pending reconnect */
    stop_reconnect();
    s_manual_disconnect = false;

    /* If already connected, disconnect first then connect to new AP */
    if (s_connected) {
        ESP_LOGI(TAG, "Already connected, switching to: %s", ssid);
        strncpy(s_pending_ssid, ssid, 32);
        s_pending_ssid[32] = '\0';
        strncpy(s_pending_password, password ? password : "", 64);
        s_pending_password[64] = '\0';
        s_switching = true;
        return esp_wifi_disconnect();
    }

    /* Save connecting info for status callback and credential storage */
    strncpy(s_connecting_ssid, ssid, 32);
    s_connecting_ssid[32] = '\0';
    strncpy(s_connecting_password, password ? password : "", 64);
    s_connecting_password[64] = '\0';

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password && strlen(password) > 0) {
        strncpy((char *)wifi_config.sta.password, password,
                sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    return esp_wifi_connect();
}

esp_err_t wifi_manager_disconnect(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    stop_reconnect();
    s_manual_disconnect = true;
    return esp_wifi_disconnect();
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}

const char *wifi_manager_get_connected_ssid(void)
{
    return s_connected_ssid;
}

void wifi_manager_register_conn_cb(wifi_mgr_conn_status_cb_t cb, void *user_data)
{
    s_conn_cb = cb;
    s_conn_user_data = user_data;
}

esp_err_t wifi_manager_start_auto_connect(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_auto_connect_task_handle) {
        return ESP_OK;
    }

    BaseType_t ok = xTaskCreate(auto_connect_task, "auto_conn", 4096, NULL, 4,
                                &s_auto_connect_task_handle);
    return (ok == pdPASS) ? ESP_OK : ESP_FAIL;
}

bool wifi_manager_has_saved_password(const char *ssid)
{
    return load_wifi_credentials(ssid, NULL, 0);
}

esp_err_t wifi_manager_connect_saved(const char *ssid)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    char password[65] = {0};
    if (!load_wifi_credentials(ssid, password, sizeof(password))) {
        ESP_LOGW(TAG, "No saved credentials for: %s", ssid);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "Connecting with saved credentials: %s", ssid);
    return wifi_manager_connect(ssid, password);
}

/* ---- Static IP / DHCP ---- */

#define WIFI_IP_NVS_NS  "wifi_ip"

/**
 * @brief Convert CIDR prefix length to network-order subnet mask.
 */
static uint32_t cidr_to_netmask(int prefix)
{
    if (prefix <= 0) return 0;
    if (prefix >= 32) return 0xFFFFFFFF;
    return htonl(~((1U << (32 - prefix)) - 1));
}

/**
 * @brief Apply static IP directly via lwIP (no NVS write).
 */
static esp_err_t apply_static_ip_internal(const char *ip, const char *cidr,
                                           const char *gateway, const char *dns)
{
    esp_err_t err = esp_netif_dhcpc_stop(s_sta_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGE(TAG, "Failed to stop DHCP: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    esp_netif_ip_info_t ip_info = {0};
    ip_info.ip.addr = ipaddr_addr(ip);
    ip_info.gw.addr = ipaddr_addr(gateway);
    ip_info.netmask.addr = cidr_to_netmask(atoi(cidr));

    err = esp_netif_set_ip_info(s_sta_netif, &ip_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set IP info: %s", esp_err_to_name(err));
        esp_netif_dhcpc_start(s_sta_netif);
        return ESP_FAIL;
    }

    esp_netif_dns_info_t dns_info = {0};
    dns_info.ip.u_addr.ip4.addr = ipaddr_addr(dns);
    dns_info.ip.type = ESP_IPADDR_TYPE_V4;
    err = esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns_info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set DNS: %s", esp_err_to_name(err));
    }

    /* Always set a backup DNS so NTP/hostname resolution works even if
     * the user-provided DNS is unreachable or misconfigured. */
    esp_netif_dns_info_t backup_dns = {0};
    backup_dns.ip.u_addr.ip4.addr = ipaddr_addr("223.5.5.5");  /* Alibaba public DNS */
    backup_dns.ip.type = ESP_IPADDR_TYPE_V4;
    esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &backup_dns);

    ESP_LOGI(TAG, "Static IP applied: %s/%s gw:%s dns:%s", ip, cidr, gateway, dns);
    return ESP_OK;
}

static void save_static_ip_config(const char *ip, const char *cidr,
                                   const char *gateway, const char *dns)
{
    nvs_handle_t handle;
    if (nvs_open(WIFI_IP_NVS_NS, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u8(handle, "mode", 1);
    nvs_set_str(handle, "ip", ip);
    nvs_set_str(handle, "cidr", cidr);
    nvs_set_str(handle, "gw", gateway);
    nvs_set_str(handle, "dns", dns);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Static IP config saved to NVS");
}

static void clear_static_ip_config(void)
{
    nvs_handle_t handle;
    if (nvs_open(WIFI_IP_NVS_NS, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u8(handle, "mode", 0);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "Static IP config cleared (DHCP mode)");
}

static void restore_static_ip_if_saved(void)
{
    nvs_handle_t handle;
    if (nvs_open(WIFI_IP_NVS_NS, NVS_READONLY, &handle) != ESP_OK) return;

    uint8_t mode = 0;
    nvs_get_u8(handle, "mode", &mode);
    if (mode != 1) {
        nvs_close(handle);
        return;
    }

    char ip[20] = {0}, cidr[4] = {0}, gw[20] = {0}, dns[20] = {0};
    size_t len;

    len = sizeof(ip);
    if (nvs_get_str(handle, "ip", ip, &len) != ESP_OK) { nvs_close(handle); return; }
    len = sizeof(cidr);
    if (nvs_get_str(handle, "cidr", cidr, &len) != ESP_OK) { nvs_close(handle); return; }
    len = sizeof(gw);
    if (nvs_get_str(handle, "gw", gw, &len) != ESP_OK) { nvs_close(handle); return; }
    len = sizeof(dns);
    if (nvs_get_str(handle, "dns", dns, &len) != ESP_OK) { nvs_close(handle); return; }

    nvs_close(handle);

    ESP_LOGI(TAG, "Restoring static IP from NVS: %s/%s gw:%s dns:%s", ip, cidr, gw, dns);
    apply_static_ip_internal(ip, cidr, gw, dns);
}

esp_err_t wifi_manager_set_static_ip(const char *ip, const char *cidr,
                                      const char *gateway, const char *dns)
{
    if (!s_initialized || !s_sta_netif) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_connected) {
        ESP_LOGW(TAG, "WiFi not connected, cannot set static IP");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = apply_static_ip_internal(ip, cidr, gateway, dns);
    if (err == ESP_OK) {
        save_static_ip_config(ip, cidr, gateway, dns);
    }
    return err;
}

esp_err_t wifi_manager_restore_dhcp(void)
{
    if (!s_initialized || !s_sta_netif) {
        ESP_LOGE(TAG, "WiFi not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_netif_dhcpc_start(s_sta_netif);
    if (err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGI(TAG, "DHCP already running");
        clear_static_ip_config();
        return ESP_OK;
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start DHCP: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }

    clear_static_ip_config();
    ESP_LOGI(TAG, "DHCP restored");
    return ESP_OK;
}

esp_netif_t *wifi_manager_get_sta_netif(void)
{
    return s_sta_netif;
}
