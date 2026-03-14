/**
 * @file display_manager.h
 * @brief Display manager - brightness control, auto-off timer, BOOT button wake
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Wake callback type */
typedef void (*display_mgr_wake_cb_t)(void);

/**
 * @brief Initialize display manager (NVS, GPIO35 button, monitor task).
 *        Applies saved brightness on init.
 */
esp_err_t display_manager_init(void);

/**
 * @brief Set brightness and save to NVS.
 * @param percent 0-100
 */
void display_manager_set_brightness(int percent);

/**
 * @brief Get current saved brightness.
 */
int display_manager_get_brightness(void);

/**
 * @brief Set auto-off timeout index and save to NVS.
 *        0=1min, 1=5min, 2=10min, 3=15min, 4=30min, 5=never
 */
void display_manager_set_timeout_index(int idx);

/**
 * @brief Get current timeout index.
 */
int display_manager_get_timeout_index(void);

/**
 * @brief Check if screen backlight is currently on.
 */
bool display_manager_is_screen_on(void);

/**
 * @brief Wake screen (restore saved brightness).
 */
void display_manager_wake(void);

/**
 * @brief Register callback invoked when screen wakes from BOOT button.
 */
void display_manager_register_wake_cb(display_mgr_wake_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_MANAGER_H */
