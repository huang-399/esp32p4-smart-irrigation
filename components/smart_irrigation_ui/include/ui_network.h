/**
 * @file ui_network.h
 * @brief Network settings UI callback interface - decouples UI from WiFi/network stack
 */

#ifndef UI_NETWORK_H
#define UI_NETWORK_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Result codes for network operations */
typedef enum {
    UI_NET_RESULT_OK = 0,
    UI_NET_RESULT_FAIL,
    UI_NET_RESULT_NOT_CONNECTED
} ui_net_result_t;

/** Callback type: apply static IP configuration */
typedef ui_net_result_t (*ui_network_apply_static_ip_fn)(
    const char *ip, const char *mask, const char *gateway, const char *dns);

/** Callback type: restore DHCP mode */
typedef ui_net_result_t (*ui_network_restore_dhcp_fn)(void);

void ui_network_register_apply_static_ip_cb(ui_network_apply_static_ip_fn fn);
void ui_network_register_restore_dhcp_cb(ui_network_restore_dhcp_fn fn);

/**
 * @brief Pass LVGL control references from ui_settings.c network form.
 */
void ui_network_set_controls(lv_obj_t *dd_eth, lv_obj_t *dd_mode,
                              lv_obj_t *input_ip, lv_obj_t *input_mask,
                              lv_obj_t *input_gw, lv_obj_t *input_dns);

/**
 * @brief Set initial IP mode from NVS (called from main.c on boot).
 * @param mode  0=DHCP, 1=Static IP
 */
void ui_network_set_initial_ip_mode(int mode, const char *ip,
                                     const char *mask, const char *gw,
                                     const char *dns);

/** Event callbacks for ui_settings.c to bind */
void ui_network_save_cb(lv_event_t *e);
void ui_network_cancel_cb(lv_event_t *e);

/* ---- Network info display ---- */

/** Callback type: get network info string */
typedef void (*ui_network_get_info_fn)(char *buf, int buf_size);

void ui_network_register_get_info_cb(ui_network_get_info_fn fn);
void ui_network_set_info_textarea(lv_obj_t *textarea);

/** Event callbacks for network info buttons */
void ui_network_get_info_btn_cb(lv_event_t *e);
void ui_network_clear_info_btn_cb(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /* UI_NETWORK_H */
