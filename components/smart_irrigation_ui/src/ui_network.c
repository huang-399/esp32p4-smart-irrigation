/**
 * @file ui_network.c
 * @brief Network settings UI - static IP validation, dialogs, save/cancel callbacks
 */

#include "ui_network.h"
#include "ui_common.h"
#include <string.h>
#include <stdlib.h>

/* Registered callbacks */
static ui_network_apply_static_ip_fn s_apply_static_ip_cb = NULL;
static ui_network_restore_dhcp_fn s_restore_dhcp_cb = NULL;

/* LVGL control references */
static lv_obj_t *s_dd_eth = NULL;
static lv_obj_t *s_dd_mode = NULL;
static lv_obj_t *s_input_ip = NULL;
static lv_obj_t *s_input_mask = NULL;
static lv_obj_t *s_input_gw = NULL;
static lv_obj_t *s_input_dns = NULL;

/* Dialog handle */
static lv_obj_t *s_net_dialog = NULL;

/* Saved state - survives widget destruction on page switch */
static uint16_t s_saved_mode = 0;       /* 0=DHCP, 1=Static IP */
static char s_saved_ip[16] = "";
static char s_saved_mask[16] = "";  /* 支持 CIDR("24") 和点分格式("255.255.255.0") */
static char s_saved_gw[16] = "";
static char s_saved_dns[16] = "";

/* ---- Registration functions ---- */

void ui_network_register_apply_static_ip_cb(ui_network_apply_static_ip_fn fn)
{
    s_apply_static_ip_cb = fn;
}

void ui_network_register_restore_dhcp_cb(ui_network_restore_dhcp_fn fn)
{
    s_restore_dhcp_cb = fn;
}

void ui_network_set_controls(lv_obj_t *dd_eth, lv_obj_t *dd_mode,
                              lv_obj_t *input_ip, lv_obj_t *input_mask,
                              lv_obj_t *input_gw, lv_obj_t *input_dns)
{
    s_dd_eth = dd_eth;
    s_dd_mode = dd_mode;
    s_input_ip = input_ip;
    s_input_mask = input_mask;
    s_input_gw = input_gw;
    s_input_dns = input_dns;

    /* Restore saved state (survives page switch / widget recreation) */
    if (s_saved_mode == 1 && s_dd_mode) {
        lv_dropdown_set_selected(s_dd_mode, 1);

        /* Enable input fields and restore values */
        if (s_input_ip) {
            lv_obj_clear_state(s_input_ip, LV_STATE_DISABLED);
            lv_textarea_set_text(s_input_ip, s_saved_ip);
        }
        if (s_input_mask) {
            lv_obj_clear_state(s_input_mask, LV_STATE_DISABLED);
            lv_textarea_set_text(s_input_mask, s_saved_mask);
        }
        if (s_input_gw) {
            lv_obj_clear_state(s_input_gw, LV_STATE_DISABLED);
            lv_textarea_set_text(s_input_gw, s_saved_gw);
        }
        if (s_input_dns) {
            lv_obj_clear_state(s_input_dns, LV_STATE_DISABLED);
            lv_textarea_set_text(s_input_dns, s_saved_dns);
        }
    } else {
        /* Default DHCP mode - disable all input fields */
        if (s_input_ip)   lv_obj_add_state(s_input_ip, LV_STATE_DISABLED);
        if (s_input_mask) lv_obj_add_state(s_input_mask, LV_STATE_DISABLED);
        if (s_input_gw)   lv_obj_add_state(s_input_gw, LV_STATE_DISABLED);
        if (s_input_dns)  lv_obj_add_state(s_input_dns, LV_STATE_DISABLED);
    }
}

void ui_network_set_initial_ip_mode(int mode, const char *ip,
                                     const char *mask, const char *gw,
                                     const char *dns)
{
    s_saved_mode = (mode == 1) ? 1 : 0;
    if (mode == 1 && ip && mask && gw && dns) {
        strncpy(s_saved_ip, ip, sizeof(s_saved_ip) - 1);
        s_saved_ip[sizeof(s_saved_ip) - 1] = '\0';
        strncpy(s_saved_mask, mask, sizeof(s_saved_mask) - 1);
        s_saved_mask[sizeof(s_saved_mask) - 1] = '\0';
        strncpy(s_saved_gw, gw, sizeof(s_saved_gw) - 1);
        s_saved_gw[sizeof(s_saved_gw) - 1] = '\0';
        strncpy(s_saved_dns, dns, sizeof(s_saved_dns) - 1);
        s_saved_dns[sizeof(s_saved_dns) - 1] = '\0';
    } else {
        s_saved_ip[0] = '\0';
        s_saved_mask[0] = '\0';
        s_saved_gw[0] = '\0';
        s_saved_dns[0] = '\0';
    }
}

/* ---- Validation functions ---- */

/**
 * @brief Validate an IPv4 address string (X.X.X.X, each octet 0-255).
 */
static bool validate_ipv4(const char *ip_str)
{
    if (!ip_str || ip_str[0] == '\0') return false;

    int count = 0;
    char buf[16];
    strncpy(buf, ip_str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(buf, ".", &saveptr);
    while (token && count < 4) {
        /* Check for non-numeric characters */
        for (int i = 0; token[i]; i++) {
            if (token[i] < '0' || token[i] > '9') return false;
        }
        if (token[0] == '\0') return false;
        int val = atoi(token);
        if (val < 0 || val > 255) return false;
        /* No leading zeros (e.g., "01" is invalid) */
        if (strlen(token) > 1 && token[0] == '0') return false;
        count++;
        token = strtok_r(NULL, ".", &saveptr);
    }

    return (count == 4 && token == NULL);
}

/**
 * @brief Validate CIDR prefix length string (1-32).
 */
static bool validate_cidr(const char *cidr_str)
{
    if (!cidr_str || cidr_str[0] == '\0') return false;

    for (int i = 0; cidr_str[i]; i++) {
        if (cidr_str[i] < '0' || cidr_str[i] > '9') return false;
    }

    int val = atoi(cidr_str);
    return (val >= 1 && val <= 32);
}

/* ---- Dialog helper ---- */

static void net_dialog_ok_cb(lv_event_t *e)
{
    (void)e;
    if (s_net_dialog) {
        lv_obj_del(s_net_dialog);
        s_net_dialog = NULL;
    }
}

/**
 * @brief Show a dialog (warning or success), same style as ui_home.c show_warning_dialog.
 */
static void show_net_dialog(const char *title, const char *message, lv_color_t title_color)
{
    if (s_net_dialog != NULL) {
        lv_obj_del(s_net_dialog);
        s_net_dialog = NULL;
    }

    /* Outer: 630x390 blue border, no radius */
    s_net_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_net_dialog, 630, 390);
    lv_obj_center(s_net_dialog);
    lv_obj_set_style_bg_color(s_net_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_net_dialog, 0, 0);
    lv_obj_set_style_radius(s_net_dialog, 0, 0);
    lv_obj_set_style_pad_all(s_net_dialog, 5, 0);
    lv_obj_clear_flag(s_net_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* Inner: 620x380 white bg, radius 10 */
    lv_obj_t *content = lv_obj_create(s_net_dialog);
    lv_obj_set_size(content, 620, 380);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* Title: bold, parameterized color, top center +30px */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, title_color, 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    /* Message: black, centered */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* OK button: 160x50, blue, radius 25, bottom center -30px */
    lv_obj_t *btn_ok = lv_btn_create(content);
    lv_obj_set_size(btn_ok, 160, 50);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(btn_ok, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 25, 0);
    lv_obj_add_event_cb(btn_ok, net_dialog_ok_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_ok = lv_label_create(btn_ok);
    lv_label_set_text(label_ok, "确定");
    lv_obj_set_style_text_font(label_ok, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_ok, lv_color_white(), 0);
    lv_obj_center(label_ok);
}

/* ---- Event callbacks ---- */

void ui_network_save_cb(lv_event_t *e)
{
    (void)e;

    /* Only support eth0 (WiFi STA). eth1 not available. */
    if (s_dd_eth) {
        uint16_t eth_sel = lv_dropdown_get_selected(s_dd_eth);
        if (eth_sel != 0) {
            show_net_dialog("告警提示", "不支持此网口", COLOR_ERROR);
            return;
        }
    }

    /* Check mode: 0=DHCP, 1=Static IP */
    if (!s_dd_mode) return;
    uint16_t mode_sel = lv_dropdown_get_selected(s_dd_mode);

    if (mode_sel == 0) {
        /* DHCP mode - restore DHCP */
        if (s_restore_dhcp_cb) {
            ui_net_result_t ret = s_restore_dhcp_cb();
            if (ret == UI_NET_RESULT_OK) {
                /* Clear saved state */
                s_saved_mode = 0;
                s_saved_ip[0] = '\0';
                s_saved_mask[0] = '\0';
                s_saved_gw[0] = '\0';
                s_saved_dns[0] = '\0';
                show_net_dialog("提示", "已恢复为DHCP模式", COLOR_SUCCESS);
            } else {
                show_net_dialog("告警提示", "恢复DHCP失败", COLOR_ERROR);
            }
        }
        return;
    }

    /* Static IP mode - read all 4 fields */
    const char *ip_str   = s_input_ip   ? lv_textarea_get_text(s_input_ip)   : "";
    const char *mask_str = s_input_mask ? lv_textarea_get_text(s_input_mask) : "";
    const char *gw_str   = s_input_gw   ? lv_textarea_get_text(s_input_gw)   : "";
    const char *dns_str  = s_input_dns  ? lv_textarea_get_text(s_input_dns)  : "";

    /* Validate all fields */
    if (!validate_ipv4(ip_str) || !validate_cidr(mask_str) ||
        !validate_ipv4(gw_str) || !validate_ipv4(dns_str)) {
        show_net_dialog("告警提示", "格式填写错误", COLOR_ERROR);
        return;
    }

    /* Apply via registered callback */
    if (!s_apply_static_ip_cb) {
        show_net_dialog("告警提示", "系统未就绪", COLOR_ERROR);
        return;
    }

    ui_net_result_t ret = s_apply_static_ip_cb(ip_str, mask_str, gw_str, dns_str);

    switch (ret) {
        case UI_NET_RESULT_OK:
            /* Save state so it persists across page switches */
            s_saved_mode = 1;
            strncpy(s_saved_ip, ip_str, sizeof(s_saved_ip) - 1);
            s_saved_ip[sizeof(s_saved_ip) - 1] = '\0';
            strncpy(s_saved_mask, mask_str, sizeof(s_saved_mask) - 1);
            s_saved_mask[sizeof(s_saved_mask) - 1] = '\0';
            strncpy(s_saved_gw, gw_str, sizeof(s_saved_gw) - 1);
            s_saved_gw[sizeof(s_saved_gw) - 1] = '\0';
            strncpy(s_saved_dns, dns_str, sizeof(s_saved_dns) - 1);
            s_saved_dns[sizeof(s_saved_dns) - 1] = '\0';
            show_net_dialog("提示", "静态IP设置成功", COLOR_SUCCESS);
            break;
        case UI_NET_RESULT_NOT_CONNECTED:
            show_net_dialog("告警提示", "WiFi未连接，无法设置IP", COLOR_ERROR);
            break;
        case UI_NET_RESULT_FAIL:
        default:
            show_net_dialog("告警提示", "不支持此IP", COLOR_ERROR);
            break;
    }
}

void ui_network_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* Clear saved state */
    s_saved_mode = 0;
    s_saved_ip[0] = '\0';
    s_saved_mask[0] = '\0';
    s_saved_gw[0] = '\0';
    s_saved_dns[0] = '\0';

    /* Restore mode dropdown to DHCP (index 0) */
    if (s_dd_mode) {
        lv_dropdown_set_selected(s_dd_mode, 0);
    }

    /* Clear and disable all input fields */
    if (s_input_ip) {
        lv_textarea_set_text(s_input_ip, "");
        lv_obj_add_state(s_input_ip, LV_STATE_DISABLED);
    }
    if (s_input_mask) {
        lv_textarea_set_text(s_input_mask, "");
        lv_obj_add_state(s_input_mask, LV_STATE_DISABLED);
    }
    if (s_input_gw) {
        lv_textarea_set_text(s_input_gw, "");
        lv_obj_add_state(s_input_gw, LV_STATE_DISABLED);
    }
    if (s_input_dns) {
        lv_textarea_set_text(s_input_dns, "");
        lv_obj_add_state(s_input_dns, LV_STATE_DISABLED);
    }

    /* Restore DHCP on the backend */
    if (s_restore_dhcp_cb) {
        s_restore_dhcp_cb();
    }
}

/* ---- Network info display ---- */

static ui_network_get_info_fn s_get_info_cb = NULL;
static lv_obj_t *s_info_textarea = NULL;

void ui_network_register_get_info_cb(ui_network_get_info_fn fn)
{
    s_get_info_cb = fn;
}

void ui_network_set_info_textarea(lv_obj_t *textarea)
{
    s_info_textarea = textarea;
}

void ui_network_get_info_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!s_info_textarea) return;

    if (!s_get_info_cb) {
        lv_textarea_set_text(s_info_textarea, "系统未就绪");
        return;
    }

    char buf[512];
    buf[0] = '\0';
    s_get_info_cb(buf, sizeof(buf));
    lv_textarea_set_text(s_info_textarea, buf);
}

void ui_network_clear_info_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_info_textarea) {
        lv_textarea_set_text(s_info_textarea, "");
    }
}
