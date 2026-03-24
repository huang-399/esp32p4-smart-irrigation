/**
 * @file ui_display.h
 * @brief Display settings UI callback interface - decouples UI from display hardware
 */

#ifndef UI_DISPLAY_H
#define UI_DISPLAY_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Callback type: preview brightness in real time (0-100) */
typedef void (*ui_display_preview_brightness_fn)(int percent);

/** Callback type: persist brightness setting (0-100) */
typedef void (*ui_display_save_brightness_fn)(int percent);

/** Callback type: set auto-off timeout index */
typedef void (*ui_display_set_timeout_fn)(int index);

void ui_display_register_preview_brightness_cb(ui_display_preview_brightness_fn fn);
void ui_display_register_save_brightness_cb(ui_display_save_brightness_fn fn);
void ui_display_register_timeout_cb(ui_display_set_timeout_fn fn);

/**
 * @brief Pass LVGL control references from ui_settings.c
 */
void ui_display_set_controls(lv_obj_t *slider, lv_obj_t *dropdown);

/**
 * @brief Set initial values (called from main.c after loading from NVS)
 */
void ui_display_set_initial_values(int brightness, int timeout_idx);

/** Event callbacks for ui_settings.c to bind */
void ui_display_slider_cb(lv_event_t *e);
void ui_display_save_cb(lv_event_t *e);
void ui_display_cancel_cb(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif /* UI_DISPLAY_H */
