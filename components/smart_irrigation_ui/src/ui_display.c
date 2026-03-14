/**
 * @file ui_display.c
 * @brief Display settings UI - brightness slider, auto-off dropdown, save/cancel
 */

#include "ui_display.h"
#include <string.h>

/* Registered callbacks */
static ui_display_set_brightness_fn s_brightness_cb = NULL;
static ui_display_set_timeout_fn s_timeout_cb = NULL;

/* LVGL control references */
static lv_obj_t *s_slider = NULL;
static lv_obj_t *s_dropdown = NULL;

/* Saved values (for cancel/restore) */
static int s_saved_brightness = 80;
static int s_saved_timeout_idx = 2;

void ui_display_register_brightness_cb(ui_display_set_brightness_fn fn)
{
    s_brightness_cb = fn;
}

void ui_display_register_timeout_cb(ui_display_set_timeout_fn fn)
{
    s_timeout_cb = fn;
}

void ui_display_set_controls(lv_obj_t *slider, lv_obj_t *dropdown)
{
    s_slider = slider;
    s_dropdown = dropdown;

    /* Apply saved values to controls */
    if (s_slider) {
        lv_slider_set_value(s_slider, s_saved_brightness, LV_ANIM_OFF);
    }
    if (s_dropdown) {
        lv_dropdown_set_selected(s_dropdown, s_saved_timeout_idx);
    }
}

void ui_display_set_initial_values(int brightness, int timeout_idx)
{
    s_saved_brightness = brightness;
    s_saved_timeout_idx = timeout_idx;

    if (s_slider) {
        lv_slider_set_value(s_slider, brightness, LV_ANIM_OFF);
    }
    if (s_dropdown) {
        lv_dropdown_set_selected(s_dropdown, timeout_idx);
    }
}

void ui_display_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);

    /* Real-time brightness preview (don't save to NVS yet) */
    if (s_brightness_cb) {
        s_brightness_cb(value);
    }
}

void ui_display_save_cb(lv_event_t *e)
{
    (void)e;

    /* Read current control values */
    int brightness = s_slider ? lv_slider_get_value(s_slider) : s_saved_brightness;
    int timeout_idx = s_dropdown ? lv_dropdown_get_selected(s_dropdown) : s_saved_timeout_idx;

    /* Apply and save */
    if (s_brightness_cb) {
        s_brightness_cb(brightness);
    }
    if (s_timeout_cb) {
        s_timeout_cb(timeout_idx);
    }

    /* Update saved values */
    s_saved_brightness = brightness;
    s_saved_timeout_idx = timeout_idx;
}

void ui_display_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* Restore controls to saved values */
    if (s_slider) {
        lv_slider_set_value(s_slider, s_saved_brightness, LV_ANIM_OFF);
    }
    if (s_dropdown) {
        lv_dropdown_set_selected(s_dropdown, s_saved_timeout_idx);
    }

    /* Restore brightness */
    if (s_brightness_cb) {
        s_brightness_cb(s_saved_brightness);
    }
}
