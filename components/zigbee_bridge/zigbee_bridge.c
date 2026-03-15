#include "zigbee_bridge.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "zigbee_bridge";

#define ZB_UART_NUM         UART_NUM_1
#define ZB_UART_BAUD        115200
#define ZB_UART_BUF_SIZE    256
#define ZB_FRAME_MAX_DATA   40

/* 帧头标识 */
#define ZB_HEADER_UP_0      0xAA   /* 上行帧头字节1 */
#define ZB_HEADER_UP_1      0x55   /* 上行帧头字节2 */
#define ZB_HEADER_DN_0      0x55   /* 下行帧头字节1 */
#define ZB_HEADER_DN_1      0xAA   /* 下行帧头字节2 */

/* ---- 状态机 ---- */
typedef enum {
    PS_WAIT_AA,
    PS_WAIT_55,
    PS_READ_TYPE,
    PS_READ_ID,
    PS_READ_LEN,
    PS_READ_DATA,
    PS_READ_CHECKSUM,
} parse_state_t;

/* ---- 内部数据存储 ---- */
static struct {
    zb_field_data_t   fields[ZB_MAX_FIELDS];  /* index 0~5 对应 ID 1~6 */
    zb_pipe_data_t    pipes;
    zb_control_data_t control;
    uint32_t          heartbeat_uptime;
    uint8_t           heartbeat_dev_count;
} s_state;

static SemaphoreHandle_t s_mutex = NULL;
static zb_bridge_data_cb_t s_data_cb = NULL;
static void *s_data_cb_user = NULL;

/* ---- 工具函数 ---- */

static uint16_t read_u16_be(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

/* ---- 帧解析 ---- */

static void process_field_frame(uint8_t id, const uint8_t *data, uint8_t len)
{
    if (id < 1 || id > ZB_MAX_FIELDS || len < 12) return;

    int idx = id - 1;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.fields[idx].nitrogen    = read_u16_be(data + 0)  / 100.0f;
    s_state.fields[idx].phosphorus  = read_u16_be(data + 2)  / 100.0f;
    s_state.fields[idx].potassium   = read_u16_be(data + 4)  / 100.0f;
    s_state.fields[idx].temperature = read_u16_be(data + 6)  / 100.0f;
    s_state.fields[idx].humidity    = read_u16_be(data + 8)  / 100.0f;
    s_state.fields[idx].light       = read_u16_be(data + 10) / 100.0f;
    s_state.fields[idx].online      = true;
    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "Field %d: N=%.1f P=%.1f K=%.1f T=%.1f H=%.1f L=%.1f",
             id,
             s_state.fields[idx].nitrogen,
             s_state.fields[idx].phosphorus,
             s_state.fields[idx].potassium,
             s_state.fields[idx].temperature,
             s_state.fields[idx].humidity,
             s_state.fields[idx].light);
}

static void process_pipe_frame(const uint8_t *data, uint8_t len)
{
    /* 7 条管道，每条 5 字节: valve(u8) + flow(u16) + pressure(u16) = 35 字节 */
    if (len < 35) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < ZB_MAX_PIPES; i++) {
        int off = i * 5;
        s_state.pipes.pipes[i].valve_on  = data[off] != 0;
        s_state.pipes.pipes[i].flow      = read_u16_be(data + off + 1) / 100.0f;
        s_state.pipes.pipes[i].pressure  = read_u16_be(data + off + 3) / 100.0f;
    }
    s_state.pipes.online = true;
    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "Pipes: main valve=%d flow=%.2f press=%.2f",
             s_state.pipes.pipes[0].valve_on,
             s_state.pipes.pipes[0].flow,
             s_state.pipes.pipes[0].pressure);
}

static void process_control_frame(const uint8_t *data, uint8_t len)
{
    /*
     * 14 字节:
     * [0]   water_pump_on (u8)
     * [1]   fert_pump_on (u8)
     * [2]   fert_valve_on (u8)
     * [3]   water_valve_on (u8)
     * [4]   mixer_on (u8)
     * [5]   tank_N switch (u8)
     * [6~7] tank_N level (u16)
     * [8]   tank_P switch (u8)
     * [9~10] tank_P level (u16)
     * [11]  tank_K switch (u8)
     * [12~13] tank_K level (u16)
     */
    if (len < 14) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.control.water_pump_on  = data[0] != 0;
    s_state.control.fert_pump_on   = data[1] != 0;
    s_state.control.fert_valve_on  = data[2] != 0;
    s_state.control.water_valve_on = data[3] != 0;
    s_state.control.mixer_on       = data[4] != 0;
    for (int i = 0; i < ZB_MAX_TANKS; i++) {
        int off = 5 + i * 3;
        s_state.control.tanks[i].switch_on = data[off] != 0;
        s_state.control.tanks[i].level     = read_u16_be(data + off + 1) / 100.0f;
    }
    s_state.control.online = true;
    xSemaphoreGive(s_mutex);

    ESP_LOGD(TAG, "Control: pump=%d fert=%d mixer=%d tankN=%.1fL",
             s_state.control.water_pump_on,
             s_state.control.fert_pump_on,
             s_state.control.mixer_on,
             s_state.control.tanks[0].level);
}

static void process_heartbeat_frame(const uint8_t *data, uint8_t len)
{
    if (len < 5) return;

    uint32_t uptime = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                      ((uint32_t)data[2] << 8) | data[3];
    uint8_t dev_count = data[4];

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.heartbeat_uptime = uptime;
    s_state.heartbeat_dev_count = dev_count;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Heartbeat: uptime=%lus devices=%d", (unsigned long)uptime, dev_count);
}

static void dispatch_frame(uint8_t type, uint8_t id, const uint8_t *data, uint8_t len)
{
    switch (type) {
    case ZB_DEV_FIELD:
        process_field_frame(id, data, len);
        break;
    case ZB_DEV_PIPE:
        process_pipe_frame(data, len);
        break;
    case ZB_DEV_CONTROL:
        process_control_frame(data, len);
        break;
    case ZB_DEV_HEARTBEAT:
        process_heartbeat_frame(data, len);
        break;
    default:
        ESP_LOGW(TAG, "Unknown frame type: 0x%02X", type);
        return;
    }

    /* 通知回调 */
    if (s_data_cb) {
        s_data_cb(type, id, s_data_cb_user);
    }
}

/* ---- UART 接收任务 ---- */

static void uart_rx_task(void *pvParameters)
{
    (void)pvParameters;

    uint8_t byte;
    parse_state_t state = PS_WAIT_AA;
    uint8_t frame_type = 0, frame_id = 0, frame_len = 0;
    uint8_t frame_data[ZB_FRAME_MAX_DATA];
    uint8_t data_idx = 0;
    uint8_t checksum = 0;

    ESP_LOGI(TAG, "UART RX task started");

    while (1) {
        int rxlen = uart_read_bytes(ZB_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (rxlen <= 0) continue;

        switch (state) {
        case PS_WAIT_AA:
            if (byte == ZB_HEADER_UP_0) state = PS_WAIT_55;
            break;

        case PS_WAIT_55:
            if (byte == ZB_HEADER_UP_1) {
                state = PS_READ_TYPE;
            } else if (byte == ZB_HEADER_UP_0) {
                /* 连续 0xAA，保持等待 0x55 */
            } else {
                state = PS_WAIT_AA;
            }
            break;

        case PS_READ_TYPE:
            frame_type = byte;
            checksum = byte;
            state = PS_READ_ID;
            break;

        case PS_READ_ID:
            frame_id = byte;
            checksum ^= byte;
            state = PS_READ_LEN;
            break;

        case PS_READ_LEN:
            frame_len = byte;
            checksum ^= byte;
            data_idx = 0;
            if (frame_len == 0) {
                state = PS_READ_CHECKSUM;
            } else if (frame_len > ZB_FRAME_MAX_DATA) {
                ESP_LOGW(TAG, "Frame too long: %d", frame_len);
                state = PS_WAIT_AA;
            } else {
                state = PS_READ_DATA;
            }
            break;

        case PS_READ_DATA:
            frame_data[data_idx++] = byte;
            checksum ^= byte;
            if (data_idx >= frame_len) {
                state = PS_READ_CHECKSUM;
            }
            break;

        case PS_READ_CHECKSUM:
            if (byte == checksum) {
                dispatch_frame(frame_type, frame_id, frame_data, frame_len);
            } else {
                ESP_LOGW(TAG, "Checksum mismatch: expected 0x%02X got 0x%02X", checksum, byte);
            }
            state = PS_WAIT_AA;
            break;
        }
    }
}

/* ---- 公开 API ---- */

esp_err_t zigbee_bridge_init(int tx_gpio, int rx_gpio)
{
    memset(&s_state, 0, sizeof(s_state));

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    uart_config_t uart_cfg = {
        .baud_rate  = ZB_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(ZB_UART_NUM, ZB_UART_BUF_SIZE * 2, ZB_UART_BUF_SIZE, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(ZB_UART_NUM, &uart_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(ZB_UART_NUM, tx_gpio, rx_gpio, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

    xTaskCreate(uart_rx_task, "zb_rx", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "Initialized: TX=GPIO%d RX=GPIO%d @ %d baud", tx_gpio, rx_gpio, ZB_UART_BAUD);
    return ESP_OK;
}

void zigbee_bridge_register_data_cb(zb_bridge_data_cb_t cb, void *user_data)
{
    s_data_cb = cb;
    s_data_cb_user = user_data;
}

const zb_field_data_t *zigbee_bridge_get_field(uint8_t id)
{
    if (id < 1 || id > ZB_MAX_FIELDS) return NULL;
    return &s_state.fields[id - 1];
}

const zb_pipe_data_t *zigbee_bridge_get_pipes(void)
{
    return &s_state.pipes;
}

const zb_control_data_t *zigbee_bridge_get_control(void)
{
    return &s_state.control;
}

/* ---- 自动发现 ---- */

int zigbee_bridge_get_discovered(zb_discovered_item_t *out_items, int max_items)
{
    static const char *field_sensor_names[] = {"氮", "磷", "钾", "温度", "湿度", "光照"};
    static const char *pipe_sensor_names[]  = {"阀门", "流量", "压力"};
    static const char *tank_names[]         = {"储料罐N", "储料罐P", "储料罐K"};

    int count = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 田地传感器 */
    for (int f = 0; f < ZB_MAX_FIELDS && count < max_items; f++) {
        if (!s_state.fields[f].online) continue;
        for (int s = 0; s < 6 && count < max_items; s++) {
            zb_discovered_item_t *item = &out_items[count++];
            item->dev_type = ZB_DEV_FIELD;
            item->dev_id = f + 1;
            item->sensor_index = s;
            snprintf(item->name, sizeof(item->name), "田地%d-%s", f + 1, field_sensor_names[s]);
            snprintf(item->type_name, sizeof(item->type_name), "%s", field_sensor_names[s]);
        }
    }

    /* 管道传感器 */
    if (s_state.pipes.online) {
        for (int p = 0; p < ZB_MAX_PIPES && count < max_items; p++) {
            for (int s = 0; s < 3 && count < max_items; s++) {
                zb_discovered_item_t *item = &out_items[count++];
                item->dev_type = ZB_DEV_PIPE;
                item->dev_id = p;
                item->sensor_index = s;
                if (p == 0)
                    snprintf(item->name, sizeof(item->name), "主管道-%s", pipe_sensor_names[s]);
                else
                    snprintf(item->name, sizeof(item->name), "副管道%d-%s", p, pipe_sensor_names[s]);
                snprintf(item->type_name, sizeof(item->type_name), "%s", pipe_sensor_names[s]);
            }
        }
    }

    /* 控制系统 */
    if (s_state.control.online) {
        /* 5 个开关设备 */
        static const char *dev_names[] = {"主水泵", "施肥泵", "出肥阀", "注水阀", "搅拌机"};
        for (int d = 0; d < 5 && count < max_items; d++) {
            zb_discovered_item_t *item = &out_items[count++];
            item->dev_type = ZB_DEV_CONTROL;
            item->dev_id = 0;
            item->sensor_index = d;
            snprintf(item->name, sizeof(item->name), "%s", dev_names[d]);
            snprintf(item->type_name, sizeof(item->type_name), "开关状态");
        }

        /* 3 个储料罐（开关+液位） */
        for (int t = 0; t < ZB_MAX_TANKS && count < max_items; t++) {
            /* 开关 */
            if (count < max_items) {
                zb_discovered_item_t *item = &out_items[count++];
                item->dev_type = ZB_DEV_CONTROL;
                item->dev_id = 0;
                item->sensor_index = 5 + t * 2;
                snprintf(item->name, sizeof(item->name), "%s-开关", tank_names[t]);
                snprintf(item->type_name, sizeof(item->type_name), "开关状态");
            }
            /* 液位 */
            if (count < max_items) {
                zb_discovered_item_t *item = &out_items[count++];
                item->dev_type = ZB_DEV_CONTROL;
                item->dev_id = 0;
                item->sensor_index = 5 + t * 2 + 1;
                snprintf(item->name, sizeof(item->name), "%s-液位", tank_names[t]);
                snprintf(item->type_name, sizeof(item->type_name), "液位");
            }
        }
    }

    xSemaphoreGive(s_mutex);
    return count;
}

/* ---- 下行控制 ---- */

esp_err_t zigbee_bridge_send_control(uint8_t dev_type, uint8_t dev_id, bool on)
{
    /*
     * 下行帧: 0x55 0xAA Type ID Cmd Checksum (6字节)
     * Checksum = Type ^ ID ^ Cmd
     */
    uint8_t cmd = on ? 0x01 : 0x00;
    uint8_t checksum = dev_type ^ dev_id ^ cmd;

    uint8_t frame[6] = {
        ZB_HEADER_DN_0,
        ZB_HEADER_DN_1,
        dev_type,
        dev_id,
        cmd,
        checksum,
    };

    int written = uart_write_bytes(ZB_UART_NUM, frame, sizeof(frame));
    if (written != sizeof(frame)) {
        ESP_LOGE(TAG, "UART write failed: wrote %d/%d", written, (int)sizeof(frame));
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Control sent: type=0x%02X id=%d cmd=%d", dev_type, dev_id, cmd);
    return ESP_OK;
}
