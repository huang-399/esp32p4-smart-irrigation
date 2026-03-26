/**
 * @file wifi_manager.h
 * @brief WiFi manager - handles WiFi init, scan, connect, and disconnect for ESP32-P4
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_MANAGER_MAX_AP_RECORDS  20

/** Single WiFi AP scan result */
typedef struct {
    char     ssid[33];
    int8_t   rssi;
    uint8_t  authmode;       /**< 0=open, 1=WEP, 2=WPA, 3=WPA2, etc. */
    bool     is_connected;
    bool     has_saved_password;  /**< true if credentials saved in NVS */
} wifi_mgr_ap_info_t;

/** Scan result set */
typedef struct {
    wifi_mgr_ap_info_t ap_list[WIFI_MANAGER_MAX_AP_RECORDS];
    uint16_t           ap_count;
} wifi_mgr_scan_result_t;

/** Connection status */
typedef enum {
    WIFI_MGR_CONNECTED,
    WIFI_MGR_DISCONNECTED,
    WIFI_MGR_CONNECT_FAILED,
} wifi_mgr_conn_status_t;

/** Scan completion callback type */
typedef void (*wifi_mgr_scan_done_cb_t)(const wifi_mgr_scan_result_t *result, void *user_data);

/** Connection status change callback type */
typedef void (*wifi_mgr_conn_status_cb_t)(wifi_mgr_conn_status_t status, const char *ssid, void *user_data);

/**
 * @brief Initialize the WiFi manager (NVS, netif, WiFi driver).
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Start an asynchronous WiFi scan.
 */
esp_err_t wifi_manager_start_scan(wifi_mgr_scan_done_cb_t cb, void *user_data);

/**
 * @brief Check if a scan is currently in progress.
 */
bool wifi_manager_is_scanning(void);

/**
 * @brief Connect to a specific WiFi AP.
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/**
 * @brief Disconnect from the current WiFi AP.
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * @brief Check if WiFi is connected.
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get the SSID of the currently connected AP.
 * @return SSID string or empty string if not connected.
 */
const char *wifi_manager_get_connected_ssid(void);

/**
 * @brief Register connection status change callback.
 */
void wifi_manager_register_conn_cb(wifi_mgr_conn_status_cb_t cb, void *user_data);

/**
 * @brief Start delayed boot auto-connect after callbacks are ready.
 */
esp_err_t wifi_manager_start_auto_connect(void);

/**
 * @brief Check if credentials are saved for a given SSID.
 */
bool wifi_manager_has_saved_password(const char *ssid);

/**
 * @brief Connect using saved credentials from NVS.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no saved credentials.
 */
esp_err_t wifi_manager_connect_saved(const char *ssid);

/**
 * @brief Apply static IP configuration to WiFi STA interface.
 *        Stops DHCP client, sets IP/mask/gateway/DNS.
 * @param ip       IP address string (e.g. "192.168.1.101")
 * @param cidr     CIDR prefix length string (e.g. "24")
 * @param gateway  Gateway string (e.g. "192.168.1.1")
 * @param dns      DNS server string (e.g. "114.114.114.114")
 * @return ESP_OK on success, ESP_FAIL or ESP_ERR_INVALID_STATE on failure.
 */
esp_err_t wifi_manager_set_static_ip(const char *ip, const char *cidr,
                                      const char *gateway, const char *dns);

/**
 * @brief Restore DHCP mode on WiFi STA interface.
 */
esp_err_t wifi_manager_restore_dhcp(void);

/**
 * @brief Get the WiFi STA network interface handle.
 */
esp_netif_t *wifi_manager_get_sta_netif(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H */
