/**
 * @file ui_wifi.h
 * @brief WiFi UI callback interface - decouples UI from WiFi driver
 */

#ifndef UI_WIFI_H
#define UI_WIFI_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_WIFI_MAX_AP  20

/** AP info structure (UI-side, no esp_wifi dependency) */
typedef struct {
    char     ssid[33];
    int8_t   rssi;
    uint8_t  authmode;       /**< 0=open, others=secured */
    bool     is_connected;
    bool     has_saved_password;  /**< true if credentials saved */
} ui_wifi_ap_info_t;

/** Scan result delivered to UI */
typedef struct {
    ui_wifi_ap_info_t ap_list[UI_WIFI_MAX_AP];
    uint16_t          ap_count;
} ui_wifi_scan_result_t;

/** Callback type: request WiFi scan */
typedef void (*ui_wifi_request_scan_fn)(void);

/** Callback type: request WiFi connect */
typedef void (*ui_wifi_request_connect_fn)(const char *ssid, const char *password);

/** Callback type: request WiFi disconnect */
typedef void (*ui_wifi_request_disconnect_fn)(void);

/** Callback type: request WiFi connect using saved credentials */
typedef void (*ui_wifi_request_connect_saved_fn)(const char *ssid);

void ui_wifi_register_scan_callback(ui_wifi_request_scan_fn fn);
void ui_wifi_register_connect_callback(ui_wifi_request_connect_fn fn);
void ui_wifi_register_disconnect_callback(ui_wifi_request_disconnect_fn fn);
void ui_wifi_register_connect_saved_callback(ui_wifi_request_connect_saved_fn fn);

void ui_wifi_update_scan_results(const ui_wifi_scan_result_t *result);
void ui_wifi_show_scanning(void);

void ui_wifi_set_table_objects(lv_obj_t *container, lv_obj_t *empty_label,
                               lv_obj_t *status_label);

void ui_wifi_search_btn_cb(lv_event_t *e);

/**
 * @brief Notify the WiFi UI that connection status changed.
 *        Must be called from LVGL context.
 * @param connected  true if connected, false if disconnected
 * @param ssid       SSID of the AP (connected or just disconnected)
 */
void ui_wifi_set_connected(bool connected, const char *ssid);

/**
 * @brief Show connection failure dialog.
 *        Must be called from LVGL context.
 * @param ssid  SSID of the AP that failed to connect
 */
void ui_wifi_show_connect_failed(const char *ssid);

/**
 * @brief Invalidate all LVGL widget pointers.
 *        Must be called before lv_obj_clean() destroys the settings page,
 *        so that async callbacks (scan results, connect status) won't
 *        access freed memory.
 */
void ui_wifi_invalidate_objects(void);

/**
 * @brief Close all WiFi screen-level dialogs/overlays.
 *        Must be called before page switching destroys related widgets.
 */
void ui_wifi_close_overlays(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_WIFI_H */
