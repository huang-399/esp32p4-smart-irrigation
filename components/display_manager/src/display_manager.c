/**
 * @file display_manager.c
 * @brief Display manager - brightness, auto-off, BOOT button wake
 */

#include "display_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"

static const char *TAG = "display_manager";

#define DISP_NVS_NS        "disp_cfg"
#define BOOT_BUTTON_GPIO    GPIO_NUM_35

/* Timeout values in milliseconds, indexed by dropdown selection */
static const uint32_t s_timeout_ms[] = {
    60000,      /* 0: 1分钟 */
    300000,     /* 1: 5分钟 */
    600000,     /* 2: 10分钟 */
    900000,     /* 3: 15分钟 */
    1800000,    /* 4: 30分钟 */
    0           /* 5: 永不 (0 = disabled) */
};

static bool s_initialized = false;
static int  s_brightness = 80;      /* Default 80% */
static int  s_timeout_idx = 2;      /* Default 10min */
static bool s_screen_on = true;
static TaskHandle_t s_monitor_task_handle = NULL;
static display_mgr_wake_cb_t s_wake_cb = NULL;

/* ---- NVS helpers ---- */

static void load_settings(void)
{
    nvs_handle_t handle;
    if (nvs_open(DISP_NVS_NS, NVS_READONLY, &handle) != ESP_OK) {
        ESP_LOGI(TAG, "No saved display settings, using defaults");
        return;
    }

    uint8_t val;
    if (nvs_get_u8(handle, "bright", &val) == ESP_OK) {
        s_brightness = val;
        if (s_brightness > 100) s_brightness = 100;
        if (s_brightness < 5) s_brightness = 5;  /* Minimum visible brightness */
    }
    if (nvs_get_u8(handle, "tout_idx", &val) == ESP_OK) {
        s_timeout_idx = val;
        if (s_timeout_idx > 5) s_timeout_idx = 5;
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded settings: brightness=%d%%, timeout_idx=%d", s_brightness, s_timeout_idx);
}

static void save_brightness(void)
{
    nvs_handle_t handle;
    if (nvs_open(DISP_NVS_NS, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u8(handle, "bright", (uint8_t)s_brightness);
    nvs_commit(handle);
    nvs_close(handle);
}

static void save_timeout(void)
{
    nvs_handle_t handle;
    if (nvs_open(DISP_NVS_NS, NVS_READWRITE, &handle) != ESP_OK) return;
    nvs_set_u8(handle, "tout_idx", (uint8_t)s_timeout_idx);
    nvs_commit(handle);
    nvs_close(handle);
}

/* ---- GPIO ISR ---- */

static void IRAM_ATTR boot_button_isr(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_monitor_task_handle) {
        vTaskNotifyGiveFromISR(s_monitor_task_handle, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ---- Monitor task ---- */

static void display_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        /* Wait up to 1 second, or wake immediately on button press notification */
        uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));

        if (notified > 0 && !s_screen_on) {
            /* BOOT button pressed while screen is off → wake */
            ESP_LOGI(TAG, "BOOT button wake");
            bsp_display_brightness_set(s_brightness);
            s_screen_on = true;

            /* Reset LVGL inactivity timer so auto-off won't trigger immediately */
            bsp_display_lock(-1);
            lv_display_trigger_activity(NULL);
            bsp_display_unlock();

            if (s_wake_cb) {
                s_wake_cb();
            }
            continue;
        }

        /* Auto-off check */
        if (!s_screen_on) continue;

        uint32_t timeout = s_timeout_ms[s_timeout_idx];
        if (timeout == 0) continue;  /* "永不" selected */

        bsp_display_lock(-1);
        uint32_t inactive = lv_display_get_inactive_time(NULL);
        bsp_display_unlock();

        if (inactive >= timeout) {
            ESP_LOGI(TAG, "Screen auto-off (inactive %lu ms >= %lu ms)", inactive, timeout);
            bsp_display_brightness_set(0);
            s_screen_on = false;
        }
    }
}

/* ---- Public API ---- */

esp_err_t display_manager_init(void)
{
    if (s_initialized) return ESP_OK;

    /* Load saved settings from NVS */
    load_settings();

    /* Apply saved brightness */
    bsp_display_brightness_set(s_brightness);
    s_screen_on = true;

    /* Configure BOOT button GPIO35 as input with pull-up, falling edge interrupt */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&io_conf);
    esp_err_t isr_ret = gpio_install_isr_service(0);
    if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(isr_ret));
        return isr_ret;
    }
    gpio_isr_handler_add(BOOT_BUTTON_GPIO, boot_button_isr, NULL);

    /* Create monitor task */
    xTaskCreate(display_monitor_task, "disp_mon", 4096, NULL, 3, &s_monitor_task_handle);

    s_initialized = true;
    ESP_LOGI(TAG, "Display manager initialized (brightness=%d%%, timeout_idx=%d)",
             s_brightness, s_timeout_idx);
    return ESP_OK;
}

void display_manager_preview_brightness(int percent)
{
    if (percent < 5) percent = 5;
    if (percent > 100) percent = 100;

    if (s_screen_on) {
        bsp_display_brightness_set(percent);
    }
}

void display_manager_set_brightness(int percent)
{
    if (percent < 5) percent = 5;
    if (percent > 100) percent = 100;
    s_brightness = percent;
    if (s_screen_on) {
        bsp_display_brightness_set(s_brightness);
    }
    save_brightness();
}

int display_manager_get_brightness(void)
{
    return s_brightness;
}

void display_manager_set_timeout_index(int idx)
{
    if (idx < 0) idx = 0;
    if (idx > 5) idx = 5;
    s_timeout_idx = idx;
    save_timeout();
    ESP_LOGI(TAG, "Timeout set to index %d (%lu ms)", idx, s_timeout_ms[idx]);
}

int display_manager_get_timeout_index(void)
{
    return s_timeout_idx;
}

bool display_manager_is_screen_on(void)
{
    return s_screen_on;
}

void display_manager_wake(void)
{
    if (!s_screen_on) {
        bsp_display_brightness_set(s_brightness);
        s_screen_on = true;
        ESP_LOGI(TAG, "Screen wake (manual)");
    }
}

void display_manager_register_wake_cb(display_mgr_wake_cb_t cb)
{
    s_wake_cb = cb;
}
