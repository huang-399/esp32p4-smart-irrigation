#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEV_REG_MAX_DEVICES   16
#define DEV_REG_MAX_VALVES    32
#define DEV_REG_MAX_SENSORS   64
#define DEV_REG_MAX_ZONES     16
#define DEV_REG_ZONE_MAX_VALVES  16
#define DEV_REG_ZONE_MAX_DEVICES  8
#define DEV_REG_NAME_LEN      32

/* ---- 设备类型 ---- */
typedef enum {
    DEV_TYPE_CTRL_8CH = 0,     /* 8路控制器 */
    DEV_TYPE_CTRL_16CH,        /* 16路控制器 */
    DEV_TYPE_CTRL_32CH,        /* 32路控制器 */
    DEV_TYPE_ZB_SENSOR_NODE,   /* Zigbee传感节点 */
    DEV_TYPE_ZB_CTRL_NODE,     /* Zigbee控制节点 */
    DEV_TYPE_VIRTUAL,          /* 虚拟节点 */
    DEV_TYPE_MAX
} dev_type_t;

/* ---- 串口类型 ---- */
typedef enum {
    DEV_PORT_RS485_1 = 0,
    DEV_PORT_RS485_2,
    DEV_PORT_RS485_3,
    DEV_PORT_RS485_4,
    DEV_PORT_SERIAL,           /* 串口连接 */
    DEV_PORT_MAX
} dev_port_t;

/* ---- 阀门类型 ---- */
typedef enum {
    VALVE_TYPE_SOLENOID = 0,   /* 电磁阀 */
    VALVE_TYPE_MAX
} valve_type_t;

/* ---- 传感器类型 ---- */
typedef enum {
    SENSOR_TYPE_N = 0,         /* 氮(N) */
    SENSOR_TYPE_P,             /* 磷(P) */
    SENSOR_TYPE_K,             /* 钾(K) */
    SENSOR_TYPE_TEMP,          /* 温度 */
    SENSOR_TYPE_HUMI,          /* 湿度 */
    SENSOR_TYPE_LIGHT,         /* 光照 */
    SENSOR_TYPE_FLOW,          /* 流量 */
    SENSOR_TYPE_PRESSURE,      /* 压力 */
    SENSOR_TYPE_LEVEL,         /* 液位 */
    SENSOR_TYPE_VALVE_STATUS,  /* 阀门状态 */
    SENSOR_TYPE_SWITCH_STATUS, /* 开关状态 */
    SENSOR_TYPE_MAX
} sensor_type_t;

/* ---- 设备信息 ---- */
typedef struct __attribute__((packed)) {
    uint8_t    valid;       /* 槽位是否使用 */
    uint8_t    type;        /* dev_type_t */
    uint8_t    port;        /* dev_port_t */
    uint8_t    reserved;
    uint16_t   id;          /* 设备编号，如 1001 */
    char       name[DEV_REG_NAME_LEN];
} dev_device_info_t;        /* 38 bytes per entry, 16 * 38 = 608 bytes total */

/* ---- 阀门信息 ---- */
typedef struct __attribute__((packed)) {
    uint8_t    valid;
    uint8_t    type;        /* valve_type_t */
    uint8_t    channel;     /* 挂接通道号 (1-based) */
    uint8_t    reserved;
    uint16_t   parent_device_id;
    uint16_t   id;          /* 阀门编号 (自动分配) */
    char       name[DEV_REG_NAME_LEN];
} dev_valve_info_t;         /* 40 bytes per entry, 32 * 40 = 1280 bytes total */

/* ---- 传感器信息 / 业务点绑定 ---- */
typedef struct __attribute__((packed)) {
    uint8_t    valid;
    uint8_t    type;        /* sensor_type_t */
    uint8_t    point_no;    /* 点位号 (01-99) */
    uint8_t    proto_type;  /* Zigbee 协议类型 (0x01=田地, 0x02=管道, 0x03=控制) */
    uint16_t   parent_device_id;
    uint32_t   point_id;    /* 业务点编号: node_id * 100 + point_no */
    char       name[DEV_REG_NAME_LEN];
} dev_sensor_info_t;        /* 40 bytes per entry, 64 * 40 = 2560 bytes total */

/* ---- 灌区信息 ---- */
typedef struct __attribute__((packed)) {
    uint8_t    valid;
    uint8_t    valve_count;         /* 实际关联阀门数 */
    uint8_t    device_count;        /* 实际关联设备数 */
    uint8_t    reserved;
    uint16_t   valve_ids[DEV_REG_ZONE_MAX_VALVES];   /* 关联阀门 ID 列表 */
    uint16_t   device_ids[DEV_REG_ZONE_MAX_DEVICES];  /* 关联设备 ID 列表 */
    char       name[DEV_REG_NAME_LEN];
} dev_zone_info_t;          /* 84 bytes per entry, 16 * 84 = 1344 bytes total */

/* ========== API ========== */

/**
 * @brief 初始化设备注册表，从 NVS 加载数据
 */
esp_err_t device_registry_init(void);

/* ---- 设备 CRUD ---- */
int device_registry_get_count(void);
const dev_device_info_t *device_registry_get_all(void);
const dev_device_info_t *device_registry_get_by_id(uint16_t id);
esp_err_t device_registry_add(const dev_device_info_t *dev);
esp_err_t device_registry_remove(uint16_t id);
esp_err_t device_registry_update(uint16_t id, const dev_device_info_t *dev);

/**
 * @brief 根据设备类型返回通道数 (8/16/32)，其他类型返回 8
 */
int device_registry_get_channel_count(uint16_t device_id);

/* ---- 阀门 CRUD ---- */
int valve_registry_get_count(void);
const dev_valve_info_t *valve_registry_get_all(void);
esp_err_t valve_registry_add(const dev_valve_info_t *valve);
esp_err_t valve_registry_remove(uint16_t id);
esp_err_t valve_registry_update(uint16_t id, const dev_valve_info_t *valve);

/* ---- 传感器 CRUD ---- */
int sensor_registry_get_count(void);
const dev_sensor_info_t *sensor_registry_get_all(void);
esp_err_t sensor_registry_add(const dev_sensor_info_t *sensor);
esp_err_t sensor_registry_remove(uint32_t point_id);
esp_err_t sensor_registry_update(uint32_t point_id, const dev_sensor_info_t *sensor);

/**
 * @brief 判断业务点编号是否已被占用
 */
bool sensor_registry_is_id_taken(uint32_t point_id);

/**
 * @brief 获取指定父设备下的下一个可用点位号
 */
uint8_t sensor_registry_next_point_no(uint16_t parent_device_id);

/**
 * @brief 构建设备下拉选项字符串 (LVGL dropdown 格式, '\n' 分隔)
 * @return 有效设备数量
 */
int device_registry_build_dropdown_str(char *buf, int buf_size);

/**
 * @brief 获取设备类型名称字符串
 */
const char *device_registry_type_name(dev_type_t type);

/**
 * @brief 获取串口类型名称字符串
 */
const char *device_registry_port_name(dev_port_t port);

/**
 * @brief 获取传感器类型名称字符串
 */
const char *sensor_registry_type_name(sensor_type_t type);

/**
 * @brief 获取阀门类型名称字符串
 */
const char *valve_registry_type_name(valve_type_t type);

/* ---- 灌区 CRUD ---- */
int zone_registry_get_count(void);
const dev_zone_info_t *zone_registry_get_all(void);
esp_err_t zone_registry_add(const dev_zone_info_t *zone);
esp_err_t zone_registry_remove(int slot_index);
esp_err_t zone_registry_update(int slot_index, const dev_zone_info_t *zone);

/* ---- 查重函数 ---- */
bool device_registry_is_name_taken(const char *name);
bool device_registry_is_id_taken(uint16_t id);
bool valve_registry_is_name_taken(const char *name);
bool sensor_registry_is_name_taken(const char *name);
bool zone_registry_is_name_taken(const char *name);

#ifdef __cplusplus
}
#endif
