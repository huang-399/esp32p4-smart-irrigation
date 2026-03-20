/**
 * @file ui_wifi.c
 * @brief WiFi scanning UI implementation - renders scan results and connect dialog
 */

#include "ui_wifi.h"
#include "ui_common.h"
#include "ui_keyboard.h"
#include <string.h>
#include <stdio.h>

extern const lv_font_t my_font_cn_16;
extern const lv_font_t my_fontbd_16;

/* Registered callbacks */
static ui_wifi_request_scan_fn s_scan_request_fn = NULL;
static ui_wifi_request_connect_fn s_connect_request_fn = NULL;
static ui_wifi_request_disconnect_fn s_disconnect_request_fn = NULL;
static ui_wifi_request_connect_saved_fn s_connect_saved_request_fn = NULL;

/* References to LVGL objects created in ui_settings.c */
static lv_obj_t *s_wifi_table_container = NULL;
static lv_obj_t *s_wifi_empty_label = NULL;
static lv_obj_t *s_wifi_status_label = NULL;

/* Store result rows for cleanup */
static lv_obj_t *s_wifi_rows[UI_WIFI_MAX_AP] = {NULL};
static int s_wifi_row_count = 0;

/* Connect dialog objects */
static lv_obj_t *s_connect_dialog = NULL;
static lv_obj_t *s_pwd_textarea = NULL;
static char s_connect_ssid[33] = {0};

/* Connection state */
static bool s_wifi_connected = false;
static char s_connected_ssid[33] = {0};

/* Last scan results (kept for re-rendering after connect/disconnect) */
static ui_wifi_scan_result_t s_last_scan = {0};
static bool s_has_scan_results = false;

/* Static storage for AP info used in button callbacks */
static ui_wifi_ap_info_t s_ap_info_store[UI_WIFI_MAX_AP];

/* Connection failure dialog */
static lv_obj_t *s_fail_dialog = NULL;
static char s_fail_ssid[33] = {0};

/* Connecting (loading) dialog */
static lv_obj_t *s_connecting_dialog = NULL;

/* Forward declarations */
static void update_status_label(int ap_count);
static void render_scan_list(const ui_wifi_scan_result_t *result);
static void show_connecting_dialog(void);
static void close_connecting_dialog(void);

void ui_wifi_register_scan_callback(ui_wifi_request_scan_fn fn)
{
    s_scan_request_fn = fn;
}

void ui_wifi_register_connect_callback(ui_wifi_request_connect_fn fn)
{
    s_connect_request_fn = fn;
}

void ui_wifi_register_disconnect_callback(ui_wifi_request_disconnect_fn fn)
{
    s_disconnect_request_fn = fn;
}

void ui_wifi_register_connect_saved_callback(ui_wifi_request_connect_saved_fn fn)
{
    s_connect_saved_request_fn = fn;
}

void ui_wifi_set_table_objects(lv_obj_t *container, lv_obj_t *empty_label,
                               lv_obj_t *status_label)
{
    s_wifi_table_container = container;
    s_wifi_empty_label = empty_label;
    s_wifi_status_label = status_label;

    /* Restore current state when re-entering WiFi settings page */
    update_status_label(s_has_scan_results ? s_last_scan.ap_count : 0);
    if (s_has_scan_results) {
        render_scan_list(&s_last_scan);
    }
}

void ui_wifi_search_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_scan_request_fn) {
        ui_wifi_show_scanning();
        s_scan_request_fn();
    }
}

static void clear_wifi_rows(void)
{
    for (int i = 0; i < s_wifi_row_count; i++) {
        if (s_wifi_rows[i]) {
            lv_obj_del(s_wifi_rows[i]);
            s_wifi_rows[i] = NULL;
        }
    }
    s_wifi_row_count = 0;
}

void ui_wifi_show_scanning(void)
{
    clear_wifi_rows();

    if (s_wifi_empty_label) {
        lv_label_set_text(s_wifi_empty_label, "正在搜索WIFI，请稍候...");
        lv_obj_clear_flag(s_wifi_empty_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static const char *rssi_to_str(int8_t rssi)
{
    if (rssi >= -50) return "强";
    if (rssi >= -65) return "较强";
    if (rssi >= -75) return "中";
    return "弱";
}

static lv_color_t rssi_to_color(int8_t rssi)
{
    if (rssi >= -50) return lv_color_hex(0x27ae60);  /* green */
    if (rssi >= -65) return lv_color_hex(0x2ecc71);  /* light green */
    if (rssi >= -75) return lv_color_hex(0xf39c12);  /* orange */
    return lv_color_hex(0xe74c3c);                    /* red */
}

/* ---- Connect dialog ---- */

static void close_connect_dialog(void)
{
    if (s_connect_dialog) {
        lv_obj_del(s_connect_dialog);
        s_connect_dialog = NULL;
        s_pwd_textarea = NULL;
    }
}

static void connect_cancel_cb(lv_event_t *e)
{
    (void)e;
    close_connect_dialog();
}

static void connect_confirm_cb(lv_event_t *e)
{
    (void)e;
    const char *pwd = lv_textarea_get_text(s_pwd_textarea);
    if (s_connect_request_fn) {
        s_connect_request_fn(s_connect_ssid, pwd);
    }
    close_connect_dialog();
    show_connecting_dialog();
}

static void pwd_textarea_clicked_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    ui_keyboard_show(ta, lv_scr_act());
}

static void show_connect_dialog(const char *ssid, uint8_t authmode)
{
    close_connect_dialog();
    strncpy(s_connect_ssid, ssid, 32);
    s_connect_ssid[32] = '\0';

    /* Blue outer frame */
    s_connect_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_connect_dialog, 500, 320);
    lv_obj_center(s_connect_dialog);
    lv_obj_set_style_bg_color(s_connect_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(s_connect_dialog, 0, 0);
    lv_obj_set_style_pad_all(s_connect_dialog, 5, 0);
    lv_obj_set_style_border_width(s_connect_dialog, 0, 0);
    lv_obj_clear_flag(s_connect_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* White inner content */
    lv_obj_t *content = lv_obj_create(s_connect_dialog);
    lv_obj_set_size(content, 490, 310);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* Title: 连接到"SSID" - centered */
    lv_obj_t *title = lv_label_create(content);
    char title_buf[64];
    snprintf(title_buf, sizeof(title_buf), "连接到\"%s\"", ssid);
    lv_label_set_text(title, title_buf);
    lv_obj_set_width(title, 490);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(title, 0, 18);
    lv_obj_set_style_text_font(title, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_MAIN, 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

    /* SSID display */
    lv_obj_t *ssid_label_title = lv_label_create(content);
    lv_label_set_text(ssid_label_title, "网络名称:");
    lv_obj_set_pos(ssid_label_title, 20, 60);
    lv_obj_set_style_text_font(ssid_label_title, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(ssid_label_title, COLOR_TEXT_GRAY, 0);

    lv_obj_t *ssid_value = lv_label_create(content);
    lv_label_set_text(ssid_value, ssid);
    lv_obj_set_pos(ssid_value, 120, 60);
    lv_obj_set_style_text_font(ssid_value, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(ssid_value, COLOR_TEXT_MAIN, 0);
    lv_obj_set_width(ssid_value, 340);
    lv_label_set_long_mode(ssid_value, LV_LABEL_LONG_DOT);

    /* Password input (only for secured networks) */
    if (authmode > 0) {
        lv_obj_t *pwd_label = lv_label_create(content);
        lv_label_set_text(pwd_label, "密码:");
        lv_obj_set_pos(pwd_label, 20, 105);
        lv_obj_set_style_text_font(pwd_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(pwd_label, COLOR_TEXT_GRAY, 0);

        s_pwd_textarea = lv_textarea_create(content);
        lv_obj_set_size(s_pwd_textarea, 340, 45);
        lv_obj_set_pos(s_pwd_textarea, 120, 92);
        lv_textarea_set_placeholder_text(s_pwd_textarea, "请输入WIFI密码");
        lv_textarea_set_password_mode(s_pwd_textarea, true);
        lv_textarea_set_one_line(s_pwd_textarea, true);
        lv_obj_set_style_text_font(s_pwd_textarea, &my_font_cn_16, 0);
        lv_obj_set_style_border_color(s_pwd_textarea, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_border_width(s_pwd_textarea, 1, 0);
        lv_obj_set_style_radius(s_pwd_textarea, 5, 0);
        lv_obj_add_event_cb(s_pwd_textarea, pwd_textarea_clicked_cb, LV_EVENT_CLICKED, NULL);
    } else {
        s_pwd_textarea = lv_textarea_create(content);
        lv_obj_add_flag(s_pwd_textarea, LV_OBJ_FLAG_HIDDEN);
        lv_textarea_set_text(s_pwd_textarea, "");

        lv_obj_t *open_label = lv_label_create(content);
        lv_label_set_text(open_label, "此网络无需密码");
        lv_obj_set_pos(open_label, 20, 105);
        lv_obj_set_style_text_font(open_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(open_label, COLOR_SUCCESS, 0);
    }

    /* Separator line */
    lv_obj_t *line = lv_obj_create(content);
    lv_obj_set_size(line, 450, 1);
    lv_obj_set_pos(line, 20, 210);
    lv_obj_set_style_bg_color(line, lv_color_hex(0xeeeeee), 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);

    /* Cancel button */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 130, 45);
    lv_obj_set_pos(btn_cancel, 110, 230);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(btn_cancel, 5, 0);
    lv_obj_add_event_cb(btn_cancel, connect_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_label, "取消");
    lv_obj_center(cancel_label);
    lv_obj_set_style_text_font(cancel_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);

    /* Connect button */
    lv_obj_t *btn_connect = lv_btn_create(content);
    lv_obj_set_size(btn_connect, 130, 45);
    lv_obj_set_pos(btn_connect, 260, 230);
    lv_obj_set_style_bg_color(btn_connect, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_connect, 5, 0);
    lv_obj_add_event_cb(btn_connect, connect_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *conn_label = lv_label_create(btn_connect);
    lv_label_set_text(conn_label, "连接");
    lv_obj_center(conn_label);
    lv_obj_set_style_text_font(conn_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(conn_label, lv_color_white(), 0);
}

/* ---- Connecting (loading) dialog ---- */

static void close_connecting_dialog(void)
{
    if (s_connecting_dialog) {
        lv_obj_del(s_connecting_dialog);
        s_connecting_dialog = NULL;
    }
}

static void show_connecting_dialog(void)
{
    close_connecting_dialog();

    /* Full-screen overlay to block all clicks */
    s_connecting_dialog = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(s_connecting_dialog);
    lv_obj_set_size(s_connecting_dialog, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(s_connecting_dialog, 0, 0);
    lv_obj_set_style_bg_opa(s_connecting_dialog, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(s_connecting_dialog, lv_color_black(), 0);
    lv_obj_add_flag(s_connecting_dialog, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_connecting_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* Blue outer frame (smaller than warning dialog) */
    lv_obj_t *frame = lv_obj_create(s_connecting_dialog);
    lv_obj_set_size(frame, 400, 200);
    lv_obj_center(frame);
    lv_obj_set_style_bg_color(frame, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(frame, 0, 0);
    lv_obj_set_style_radius(frame, 0, 0);
    lv_obj_set_style_pad_all(frame, 5, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    /* White inner content */
    lv_obj_t *content = lv_obj_create(frame);
    lv_obj_set_size(content, 390, 190);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* "正在连接中..." text */
    lv_obj_t *label = lv_label_create(content);
    lv_label_set_text(label, "正在连接中...");
    lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT_MAIN, 0);
    lv_obj_center(label);
}

/* ---- Connection failure dialog ---- */

static void close_fail_dialog(void)
{
    if (s_fail_dialog) {
        lv_obj_del(s_fail_dialog);
        s_fail_dialog = NULL;
    }
}

static void fail_ok_cb(lv_event_t *e)
{
    (void)e;
    close_fail_dialog();
}

static void fail_reconnect_cb(lv_event_t *e)
{
    (void)e;
    char ssid[33];
    strncpy(ssid, s_fail_ssid, 32);
    ssid[32] = '\0';

    /* Find authmode from last scan results */
    uint8_t authmode = 1; /* default: secured */
    if (s_has_scan_results) {
        for (int i = 0; i < s_last_scan.ap_count; i++) {
            if (strcmp(s_last_scan.ap_list[i].ssid, ssid) == 0) {
                authmode = s_last_scan.ap_list[i].authmode;
                break;
            }
        }
    }

    close_fail_dialog();
    show_connect_dialog(ssid, authmode);
}

static void show_connect_fail_dialog(const char *ssid)
{
    close_fail_dialog();
    strncpy(s_fail_ssid, ssid ? ssid : "", 32);
    s_fail_ssid[32] = '\0';

    /* Blue outer frame */
    s_fail_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(s_fail_dialog, 630, 390);
    lv_obj_center(s_fail_dialog);
    lv_obj_set_style_bg_color(s_fail_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(s_fail_dialog, 0, 0);
    lv_obj_set_style_radius(s_fail_dialog, 0, 0);
    lv_obj_set_style_pad_all(s_fail_dialog, 5, 0);
    lv_obj_clear_flag(s_fail_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* White inner content */
    lv_obj_t *content = lv_obj_create(s_fail_dialog);
    lv_obj_set_size(content, 620, 380);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* Title: 连接失败 (red, bold, centered) */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, "连接失败");
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, COLOR_ERROR, 0);
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    /* Message: 连接失败，密码错误 (black, centered) */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, "连接失败，密码错误");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* "确认" button (gray) */
    lv_obj_t *btn_ok = lv_btn_create(content);
    lv_obj_set_size(btn_ok, 160, 50);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, -100, -30);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 25, 0);
    lv_obj_add_event_cb(btn_ok, fail_ok_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_ok = lv_label_create(btn_ok);
    lv_label_set_text(label_ok, "确认");
    lv_obj_set_style_text_font(label_ok, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_ok, lv_color_white(), 0);
    lv_obj_center(label_ok);

    /* "重新连接" button (blue) */
    lv_obj_t *btn_retry = lv_btn_create(content);
    lv_obj_set_size(btn_retry, 160, 50);
    lv_obj_align(btn_retry, LV_ALIGN_BOTTOM_MID, 100, -30);
    lv_obj_set_style_bg_color(btn_retry, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_retry, 0, 0);
    lv_obj_set_style_radius(btn_retry, 25, 0);
    lv_obj_add_event_cb(btn_retry, fail_reconnect_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_retry = lv_label_create(btn_retry);
    lv_label_set_text(label_retry, "重新连接");
    lv_obj_set_style_text_font(label_retry, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_retry, lv_color_white(), 0);
    lv_obj_center(label_retry);
}

void ui_wifi_show_connect_failed(const char *ssid)
{
    close_connecting_dialog();
    show_connect_fail_dialog(ssid);
}

/* ---- Row button callbacks ---- */

/* Connect button click in each row */
static void row_connect_btn_cb(lv_event_t *e)
{
    ui_wifi_ap_info_t *ap_info = (ui_wifi_ap_info_t *)lv_event_get_user_data(e);
    if (ap_info) {
        if (ap_info->has_saved_password && s_connect_saved_request_fn) {
            /* Saved credentials exist, connect directly without dialog */
            s_connect_saved_request_fn(ap_info->ssid);
            show_connecting_dialog();
        } else {
            show_connect_dialog(ap_info->ssid, ap_info->authmode);
        }
    }
}

/* Disconnect button click in each row */
static void row_disconnect_btn_cb(lv_event_t *e)
{
    (void)e;
    if (s_disconnect_request_fn) {
        s_disconnect_request_fn();
    }
}

/* ---- Update status label helper ---- */

static void update_status_label(int ap_count)
{
    if (!s_wifi_status_label) return;
    char status_buf[96];
    if (s_wifi_connected && s_connected_ssid[0]) {
        snprintf(status_buf, sizeof(status_buf), "已连接: %s  |  搜索到 %d 个WIFI",
                 s_connected_ssid, ap_count);
    } else {
        snprintf(status_buf, sizeof(status_buf), "已连接: 无  |  搜索到 %d 个WIFI",
                 ap_count);
    }
    lv_label_set_text(s_wifi_status_label, status_buf);
}

/* ---- Render scan results ---- */

static void render_scan_list(const ui_wifi_scan_result_t *result)
{
    if (!s_wifi_table_container || !result) return;

    clear_wifi_rows();

    if (result->ap_count == 0) {
        if (s_wifi_empty_label) {
            lv_label_set_text(s_wifi_empty_label,
                "未搜索到WIFI，请点击右上角\"搜索WIFI\"按钮");
            lv_obj_clear_flag(s_wifi_empty_label, LV_OBJ_FLAG_HIDDEN);
        }
        update_status_label(0);
        return;
    }

    if (s_wifi_empty_label) {
        lv_obj_add_flag(s_wifi_empty_label, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_flag(s_wifi_table_container, LV_OBJ_FLAG_SCROLLABLE);

    int y_pos = 50;
    int row_height = 55;

    for (int i = 0; i < result->ap_count && i < UI_WIFI_MAX_AP; i++) {
        memcpy(&s_ap_info_store[i], &result->ap_list[i], sizeof(ui_wifi_ap_info_t));

        /* Check if this AP is the connected one */
        bool is_this_connected = s_wifi_connected &&
            (strcmp(result->ap_list[i].ssid, s_connected_ssid) == 0);
        s_ap_info_store[i].is_connected = is_this_connected;

        lv_obj_t *row = lv_obj_create(s_wifi_table_container);
        lv_obj_set_size(row, 840, row_height);
        lv_obj_set_pos(row, 5, y_pos);
        lv_obj_set_style_bg_color(row, (i % 2 == 0) ?
            lv_color_white() : lv_color_hex(0xf9f9f9), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xeeeeee), 0);
        lv_obj_set_style_radius(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        /* Lock icon for secured networks */
        if (result->ap_list[i].authmode > 0) {
            lv_obj_t *lock = lv_label_create(row);
            lv_label_set_text(lock, LV_SYMBOL_EYE_CLOSE);
            lv_obj_set_pos(lock, 15, 17);
            lv_obj_set_style_text_color(lock, lv_color_hex(0x999999), 0);
        }

        /* SSID name */
        lv_obj_t *name_label = lv_label_create(row);
        lv_label_set_text(name_label, result->ap_list[i].ssid);
        lv_obj_set_pos(name_label, 45, 15);
        lv_obj_set_style_text_font(name_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0x333333), 0);
        lv_obj_set_width(name_label, 280);
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);

        /* Signal strength */
        lv_obj_t *rssi_label = lv_label_create(row);
        char rssi_buf[32];
        snprintf(rssi_buf, sizeof(rssi_buf), "%s (%ddBm)",
                 rssi_to_str(result->ap_list[i].rssi),
                 result->ap_list[i].rssi);
        lv_label_set_text(rssi_label, rssi_buf);
        lv_obj_set_pos(rssi_label, 345, 15);
        lv_obj_set_style_text_font(rssi_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(rssi_label,
            rssi_to_color(result->ap_list[i].rssi), 0);

        /* Connect / Disconnect button */
        lv_obj_t *btn = lv_btn_create(row);
        lv_obj_t *btn_label = lv_label_create(btn);

        if (is_this_connected) {
            /* Red "断开连接" button */
            lv_obj_set_size(btn, 110, 35);
            lv_obj_set_pos(btn, 605, 10);
            lv_obj_set_style_bg_color(btn, COLOR_ERROR, 0);
            lv_obj_set_style_radius(btn, 5, 0);
            lv_obj_add_event_cb(btn, row_disconnect_btn_cb, LV_EVENT_CLICKED, NULL);
            lv_label_set_text(btn_label, "断开连接");
        } else {
            /* Blue "连接" button */
            lv_obj_set_size(btn, 80, 35);
            lv_obj_set_pos(btn, 635, 10);
            lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, 0);
            lv_obj_set_style_radius(btn, 5, 0);
            lv_obj_add_event_cb(btn, row_connect_btn_cb, LV_EVENT_CLICKED,
                                &s_ap_info_store[i]);
            lv_label_set_text(btn_label, "连接");
        }
        lv_obj_center(btn_label);
        lv_obj_set_style_text_font(btn_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);

        s_wifi_rows[i] = row;
        s_wifi_row_count++;
        y_pos += row_height;
    }

    update_status_label(result->ap_count);
}

void ui_wifi_update_scan_results(const ui_wifi_scan_result_t *result)
{
    if (!result) return;

    /* Save scan results for re-rendering */
    memcpy(&s_last_scan, result, sizeof(ui_wifi_scan_result_t));
    s_has_scan_results = true;

    render_scan_list(result);
}

void ui_wifi_set_connected(bool connected, const char *ssid)
{
    /* Close connecting dialog on any result */
    close_connecting_dialog();

    s_wifi_connected = connected;
    if (connected && ssid) {
        strncpy(s_connected_ssid, ssid, 32);
        s_connected_ssid[32] = '\0';
    } else {
        s_connected_ssid[0] = '\0';
    }

    /* Update bottom status bar */
    ui_statusbar_set_wifi_connected(connected);

    /* Re-render the WiFi list if we have scan results */
    if (s_has_scan_results) {
        render_scan_list(&s_last_scan);
    } else {
        /* No scan results, just update the status label */
        update_status_label(0);
    }
}

void ui_wifi_invalidate_objects(void)
{
    /* Null out all LVGL widget pointers so async callbacks
       (scan results, connect status) won't access freed memory. */
    s_wifi_table_container = NULL;
    s_wifi_empty_label = NULL;
    s_wifi_status_label = NULL;

    /* Row pointers are children of the container, already destroyed */
    for (int i = 0; i < s_wifi_row_count; i++) {
        s_wifi_rows[i] = NULL;
    }
    s_wifi_row_count = 0;
}

void ui_wifi_close_overlays(void)
{
    ui_keyboard_close();
    close_connect_dialog();
    close_connecting_dialog();
    close_fail_dialog();
}
