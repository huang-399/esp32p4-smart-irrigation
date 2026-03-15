#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 设备类型（与 CC2530 协议一致） */
#define ZB_DEV_FIELD        0x01   /* 田地传感器（ID=1~6） */
#define ZB_DEV_PIPE         0x02   /* 管道，全部打包（ID=0） */
#define ZB_DEV_CONTROL      0x03   /* 控制系统，打包（ID=0） */
#define ZB_DEV_MIXER        0x04   /* 独立设备控制（下行用） */
#define ZB_DEV_HEARTBEAT    0xFE   /* 心跳 */

#define ZB_MAX_FIELDS       6
#define ZB_MAX_PIPES        7      /* 0=主管道, 1~6=副管道 */
#define ZB_MAX_TANKS        3

/* 田地传感器数据 */
typedef struct {
    float nitrogen;      /* 氮 mg/kg */
    float phosphorus;    /* 磷 mg/kg */
    float potassium;     /* 钾 mg/kg */
    float temperature;   /* 温度 °C */
    float humidity;      /* 湿度 % */
    float light;         /* 光照 lux */
    bool  online;
} zb_field_data_t;

/* 单条管道数据 */
typedef struct {
    bool  valve_on;      /* 阀门开关 */
    float flow;          /* 流量 m³/h */
    float pressure;      /* 压力 MPa */
} zb_pipe_item_t;

/* 全部管道（7条打包） */
typedef struct {
    zb_pipe_item_t pipes[ZB_MAX_PIPES]; /* [0]=主管道, [1~6]=副管道 */
    bool online;
} zb_pipe_data_t;

/* 储料罐数据 */
typedef struct {
    bool  switch_on;
    float level;         /* 剩余量 L */
} zb_tank_item_t;

/* 控制系统数据（打包） */
typedef struct {
    bool           water_pump_on;         /* 主水泵 */
    bool           fert_pump_on;          /* 施肥泵 */
    bool           fert_valve_on;         /* 出肥阀 */
    bool           water_valve_on;        /* 注水阀 */
    bool           mixer_on;              /* 搅拌机 */
    zb_tank_item_t tanks[ZB_MAX_TANKS];   /* [0]=N, [1]=P, [2]=K */
    bool           online;
} zb_control_data_t;

/* 回调：收到任意设备数据时触发（非 LVGL 线程） */
typedef void (*zb_bridge_data_cb_t)(uint8_t dev_type, uint8_t dev_id, void *user_data);

/**
 * @brief 初始化 Zigbee 桥接（UART 收发）
 * @param tx_gpio  ESP32-P4 TX 引脚（连接 CC2530 RX）
 * @param rx_gpio  ESP32-P4 RX 引脚（连接 CC2530 TX）
 */
esp_err_t zigbee_bridge_init(int tx_gpio, int rx_gpio);

/**
 * @brief 注册数据接收回调
 */
void zigbee_bridge_register_data_cb(zb_bridge_data_cb_t cb, void *user_data);

/* 查询接口（线程安全） */
const zb_field_data_t   *zigbee_bridge_get_field(uint8_t id);   /* id: 1~6 */
const zb_pipe_data_t    *zigbee_bridge_get_pipes(void);
const zb_control_data_t *zigbee_bridge_get_control(void);

/* ---- 自动发现接口 ---- */

typedef struct {
    uint8_t dev_type;       /* ZB_DEV_FIELD / ZB_DEV_PIPE / ZB_DEV_CONTROL */
    uint8_t dev_id;         /* 设备编号 */
    uint8_t sensor_index;   /* 子传感器索引 */
    char    name[32];       /* 如 "田地1-氮" */
    char    type_name[16];  /* 如 "氮" "流量" "液位" */
} zb_discovered_item_t;

/**
 * @brief 获取所有已发现的在线传感器列表
 * @return 实际填入 out_items 的条目数
 */
int zigbee_bridge_get_discovered(zb_discovered_item_t *out_items, int max_items);

/* ---- 下行控制接口 ---- */

/**
 * @brief 发送控制指令给 CC2530
 * @param dev_type  设备类型
 * @param dev_id    设备编号
 * @param on        true=开, false=关
 */
esp_err_t zigbee_bridge_send_control(uint8_t dev_type, uint8_t dev_id, bool on);

#ifdef __cplusplus
}
#endif
