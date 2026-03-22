#include "device_registry.h"
#include "esp_check.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "dev_reg";

#define NVS_NAMESPACE   "dev_reg"

/* ---- 静态数据 ---- */
static dev_device_info_t s_devices[DEV_REG_MAX_DEVICES];
static dev_valve_info_t  s_valves[DEV_REG_MAX_VALVES];
static dev_sensor_info_t s_sensors[DEV_REG_MAX_SENSORS];
static dev_zone_info_t   s_zones[DEV_REG_MAX_ZONES];
static SemaphoreHandle_t s_mutex = NULL;
static bool s_inited = false;

/* ---- 后台 NVS 持久化（不阻塞 LVGL 线程） ---- */
typedef struct {
    uint8_t type;   /* 0=device, 1=valve, 2=sensor, 3=zone */
    uint8_t slot;
} persist_msg_t;

static QueueHandle_t s_persist_queue = NULL;

/* ---- 类型名称表 ---- */
static const char *s_dev_type_names[] = {
    "8路控制器", "16路控制器", "32路控制器",
    "Zigbee传感节点", "Zigbee控制节点", "虚拟节点"
};

static const char *s_port_names[] = {
    "RS485-1", "RS485-2", "RS485-3", "RS485-4", "串口连接"
};

static const char *s_sensor_type_names[] = {
    "氮(N)", "磷(P)", "钾(K)", "温度", "湿度",
    "光照", "流量", "压力", "液位", "阀门状态", "开关状态"
};

static const char *s_valve_type_names[] = {
    "电磁阀"
};

/* ---- NVS 逐条读写 ---- */

static esp_err_t save_one_device(int slot)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_open failed");

    char key[8];
    snprintf(key, sizeof(key), "d_%d", slot);

    if (s_devices[slot].valid) {
        ret = nvs_set_blob(handle, key, &s_devices[slot], sizeof(dev_device_info_t));
    } else {
        ret = nvs_erase_key(handle, key);
        if (ret == ESP_ERR_NVS_NOT_FOUND) ret = ESP_OK;
    }
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

static esp_err_t save_one_valve(int slot)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_open failed");

    char key[8];
    snprintf(key, sizeof(key), "v_%d", slot);

    if (s_valves[slot].valid) {
        ret = nvs_set_blob(handle, key, &s_valves[slot], sizeof(dev_valve_info_t));
    } else {
        ret = nvs_erase_key(handle, key);
        if (ret == ESP_ERR_NVS_NOT_FOUND) ret = ESP_OK;
    }
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

static esp_err_t save_one_sensor(int slot)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_open failed");

    char key[8];
    snprintf(key, sizeof(key), "s_%d", slot);

    if (s_sensors[slot].valid) {
        ret = nvs_set_blob(handle, key, &s_sensors[slot], sizeof(dev_sensor_info_t));
    } else {
        ret = nvs_erase_key(handle, key);
        if (ret == ESP_ERR_NVS_NOT_FOUND) ret = ESP_OK;
    }
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

static esp_err_t save_one_zone(int slot)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_open failed");

    char key[8];
    snprintf(key, sizeof(key), "z_%d", slot);

    if (s_zones[slot].valid) {
        ret = nvs_set_blob(handle, key, &s_zones[slot], sizeof(dev_zone_info_t));
    } else {
        ret = nvs_erase_key(handle, key);
        if (ret == ESP_ERR_NVS_NOT_FOUND) ret = ESP_OK;
    }
    if (ret == ESP_OK) ret = nvs_commit(handle);
    nvs_close(handle);
    return ret;
}

/* ---- 后台持久化任务和投递函数 ---- */

static void persist_task(void *param)
{
    (void)param;
    persist_msg_t msg;

    while (xQueueReceive(s_persist_queue, &msg, portMAX_DELAY) == pdTRUE) {
        bool device_dirty[DEV_REG_MAX_DEVICES] = {0};
        bool valve_dirty[DEV_REG_MAX_VALVES] = {0};
        bool sensor_dirty[DEV_REG_MAX_SENSORS] = {0};
        bool zone_dirty[DEV_REG_MAX_ZONES] = {0};

        switch (msg.type) {
            case 0: device_dirty[msg.slot] = true; break;
            case 1: valve_dirty[msg.slot] = true; break;
            case 2: sensor_dirty[msg.slot] = true; break;
            case 3: zone_dirty[msg.slot] = true; break;
        }

        /* 排空队列中已积累的请求（去重） */
        persist_msg_t extra;
        while (xQueueReceive(s_persist_queue, &extra, 0) == pdTRUE) {
            switch (extra.type) {
                case 0: device_dirty[extra.slot] = true; break;
                case 1: valve_dirty[extra.slot] = true; break;
                case 2: sensor_dirty[extra.slot] = true; break;
                case 3: zone_dirty[extra.slot] = true; break;
            }
        }

        esp_err_t ret;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
            if (device_dirty[i]) {
                ret = save_one_device(i);
                if (ret != ESP_OK) ESP_LOGE(TAG, "Persist d_%d failed: %s", i, esp_err_to_name(ret));
            }
        }
        for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
            if (valve_dirty[i]) {
                ret = save_one_valve(i);
                if (ret != ESP_OK) ESP_LOGE(TAG, "Persist v_%d failed: %s", i, esp_err_to_name(ret));
            }
        }
        for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
            if (sensor_dirty[i]) {
                ret = save_one_sensor(i);
                if (ret != ESP_OK) ESP_LOGE(TAG, "Persist s_%d failed: %s", i, esp_err_to_name(ret));
            }
        }
        for (int i = 0; i < DEV_REG_MAX_ZONES; i++) {
            if (zone_dirty[i]) {
                ret = save_one_zone(i);
                if (ret != ESP_OK) ESP_LOGE(TAG, "Persist z_%d failed: %s", i, esp_err_to_name(ret));
            }
        }
        xSemaphoreGive(s_mutex);
    }
}

static void persist_post(uint8_t type, uint8_t slot)
{
    if (s_persist_queue) {
        persist_msg_t msg = { .type = type, .slot = slot };
        xQueueSend(s_persist_queue, &msg, 0);
    }
}

/* ---- NVS 加载（兼容旧整 blob 格式，自动迁移） ---- */

static esp_err_t load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "NVS namespace not found, starting empty");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_open failed");

    size_t len;
    bool migrated = false;

    /* --- 设备 --- */
    len = sizeof(s_devices);
    ret = nvs_get_blob(handle, "devices", s_devices, &len);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Migrating devices from old blob format");
        for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
            if (s_devices[i].valid) {
                char key[8];
                snprintf(key, sizeof(key), "d_%d", i);
                nvs_set_blob(handle, key, &s_devices[i], sizeof(dev_device_info_t));
            }
        }
        nvs_erase_key(handle, "devices");
        migrated = true;
    } else {
        for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
            char key[8];
            snprintf(key, sizeof(key), "d_%d", i);
            len = sizeof(dev_device_info_t);
            ret = nvs_get_blob(handle, key, &s_devices[i], &len);
            if (ret != ESP_OK) memset(&s_devices[i], 0, sizeof(dev_device_info_t));
        }
    }

    /* --- 阀门 --- */
    len = sizeof(s_valves);
    ret = nvs_get_blob(handle, "valves", s_valves, &len);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Migrating valves from old blob format");
        for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
            if (s_valves[i].valid) {
                char key[8];
                snprintf(key, sizeof(key), "v_%d", i);
                nvs_set_blob(handle, key, &s_valves[i], sizeof(dev_valve_info_t));
            }
        }
        nvs_erase_key(handle, "valves");
        migrated = true;
    } else {
        for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
            char key[8];
            snprintf(key, sizeof(key), "v_%d", i);
            len = sizeof(dev_valve_info_t);
            ret = nvs_get_blob(handle, key, &s_valves[i], &len);
            if (ret != ESP_OK) memset(&s_valves[i], 0, sizeof(dev_valve_info_t));
        }
    }

    /* --- 传感器 --- */
    len = sizeof(s_sensors);
    ret = nvs_get_blob(handle, "sensors", s_sensors, &len);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Migrating sensors from old blob format");
        for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
            if (s_sensors[i].valid) {
                char key[8];
                snprintf(key, sizeof(key), "s_%d", i);
                nvs_set_blob(handle, key, &s_sensors[i], sizeof(dev_sensor_info_t));
            }
        }
        nvs_erase_key(handle, "sensors");
        migrated = true;
    } else {
        for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
            char key[8];
            snprintf(key, sizeof(key), "s_%d", i);
            len = sizeof(dev_sensor_info_t);
            ret = nvs_get_blob(handle, key, &s_sensors[i], &len);
            if (ret != ESP_OK) memset(&s_sensors[i], 0, sizeof(dev_sensor_info_t));
        }
    }

    /* --- 灌区 --- */
    len = sizeof(s_zones);
    ret = nvs_get_blob(handle, "zones", s_zones, &len);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Migrating zones from old blob format");
        for (int i = 0; i < DEV_REG_MAX_ZONES; i++) {
            if (s_zones[i].valid) {
                char key[8];
                snprintf(key, sizeof(key), "z_%d", i);
                nvs_set_blob(handle, key, &s_zones[i], sizeof(dev_zone_info_t));
            }
        }
        nvs_erase_key(handle, "zones");
        migrated = true;
    } else {
        for (int i = 0; i < DEV_REG_MAX_ZONES; i++) {
            char key[8];
            snprintf(key, sizeof(key), "z_%d", i);
            len = sizeof(dev_zone_info_t);
            ret = nvs_get_blob(handle, key, &s_zones[i], &len);
            if (ret != ESP_OK) memset(&s_zones[i], 0, sizeof(dev_zone_info_t));
        }
    }

    if (migrated) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "NVS migration to per-entry format complete");
    }

    nvs_close(handle);
    return ESP_OK;
}

/* ========== 初始化 ========== */

esp_err_t device_registry_init(void)
{
    if (s_inited) return ESP_OK;

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(s_devices, 0, sizeof(s_devices));
    memset(s_valves, 0, sizeof(s_valves));
    memset(s_sensors, 0, sizeof(s_sensors));
    memset(s_zones, 0, sizeof(s_zones));

    esp_err_t ret = load_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS load error, starting with empty registry");
    }

    int dev_cnt = device_registry_get_count();
    int vlv_cnt = valve_registry_get_count();
    int sns_cnt = sensor_registry_get_count();
    int zn_cnt  = zone_registry_get_count();

    /* 创建后台 NVS 持久化队列和任务（优先级 3，低于 LVGL） */
    s_persist_queue = xQueueCreate(16, sizeof(persist_msg_t));
    xTaskCreate(persist_task, "nvs_persist", 3072, NULL, 3, NULL);

    ESP_LOGI(TAG, "Initialized: %d devices, %d valves, %d sensors, %d zones",
             dev_cnt, vlv_cnt, sns_cnt, zn_cnt);

    s_inited = true;
    return ESP_OK;
}

/* ========== 设备 CRUD ========== */

int device_registry_get_count(void)
{
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid) count++;
    }
    return count;
}

const dev_device_info_t *device_registry_get_all(void)
{
    return s_devices;
}

const dev_device_info_t *device_registry_get_by_id(uint16_t id)
{
    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid && s_devices[i].id == id) {
            return &s_devices[i];
        }
    }
    return NULL;
}

esp_err_t device_registry_add(const dev_device_info_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 检查 ID 是否重复 */
    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid && s_devices[i].id == dev->id) {
            xSemaphoreGive(s_mutex);
            ESP_LOGW(TAG, "Device ID %d already exists", dev->id);
            return ESP_ERR_INVALID_STATE;
        }
    }

    /* 找空槽 */
    int slot = -1;
    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (!s_devices[i].valid) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "Device registry full");
        return ESP_ERR_NO_MEM;
    }

    memcpy(&s_devices[slot], dev, sizeof(dev_device_info_t));
    s_devices[slot].valid = 1;
    s_devices[slot].name[DEV_REG_NAME_LEN - 1] = '\0';

    xSemaphoreGive(s_mutex);
    persist_post(0, slot);
    return ESP_OK;
}

esp_err_t device_registry_remove(uint16_t id)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid && s_devices[i].id == id) {
            memset(&s_devices[i], 0, sizeof(dev_device_info_t));
            xSemaphoreGive(s_mutex);
            persist_post(0, i);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t device_registry_update(uint16_t id, const dev_device_info_t *dev)
{
    if (!dev) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid && s_devices[i].id == id) {
            s_devices[i].type = dev->type;
            s_devices[i].port = dev->port;
            memcpy(s_devices[i].name, dev->name, DEV_REG_NAME_LEN);
            s_devices[i].name[DEV_REG_NAME_LEN - 1] = '\0';
            xSemaphoreGive(s_mutex);
            persist_post(0, i);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

int device_registry_get_channel_count(uint16_t device_id)
{
    const dev_device_info_t *dev = device_registry_get_by_id(device_id);
    if (!dev) return 8;

    switch ((dev_type_t)dev->type) {
        case DEV_TYPE_CTRL_16CH: return 16;
        case DEV_TYPE_CTRL_32CH: return 32;
        default: return 8;
    }
}

/* ========== 阀门 CRUD ========== */

int valve_registry_get_count(void)
{
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        if (s_valves[i].valid) count++;
    }
    return count;
}

const dev_valve_info_t *valve_registry_get_all(void)
{
    return s_valves;
}

esp_err_t valve_registry_add(const dev_valve_info_t *valve)
{
    if (!valve) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 找空槽 */
    int slot = -1;
    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        if (!s_valves[i].valid) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "Valve registry full");
        return ESP_ERR_NO_MEM;
    }

    memcpy(&s_valves[slot], valve, sizeof(dev_valve_info_t));
    s_valves[slot].valid = 1;
    s_valves[slot].name[DEV_REG_NAME_LEN - 1] = '\0';

    /* 自动分配阀门 ID：在同一父设备下找最大值 +1 */
    uint16_t max_id = 0;
    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        if (s_valves[i].valid && s_valves[i].id > max_id) {
            max_id = s_valves[i].id;
        }
    }
    s_valves[slot].id = max_id + 1;

    xSemaphoreGive(s_mutex);
    persist_post(1, slot);
    return ESP_OK;
}

esp_err_t valve_registry_remove(uint16_t id)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        if (s_valves[i].valid && s_valves[i].id == id) {
            memset(&s_valves[i], 0, sizeof(dev_valve_info_t));
            xSemaphoreGive(s_mutex);
            persist_post(1, i);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t valve_registry_update(uint16_t id, const dev_valve_info_t *valve)
{
    if (!valve) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        if (s_valves[i].valid && s_valves[i].id == id) {
            s_valves[i].type = valve->type;
            s_valves[i].channel = valve->channel;
            s_valves[i].parent_device_id = valve->parent_device_id;
            memcpy(s_valves[i].name, valve->name, DEV_REG_NAME_LEN);
            s_valves[i].name[DEV_REG_NAME_LEN - 1] = '\0';
            xSemaphoreGive(s_mutex);
            persist_post(1, i);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

/* ========== 传感器 CRUD ========== */

int sensor_registry_get_count(void)
{
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid) count++;
    }
    return count;
}

const dev_sensor_info_t *sensor_registry_get_all(void)
{
    return s_sensors;
}

esp_err_t sensor_registry_add(const dev_sensor_info_t *sensor)
{
    if (!sensor) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* 检查 point_id 是否重复 */
    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid && s_sensors[i].point_id == sensor->point_id) {
            xSemaphoreGive(s_mutex);
            ESP_LOGW(TAG, "Sensor point_id %lu already exists", (unsigned long)sensor->point_id);
            return ESP_ERR_INVALID_STATE;
        }
    }

    /* 找空槽 */
    int slot = -1;
    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (!s_sensors[i].valid) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "Sensor registry full");
        return ESP_ERR_NO_MEM;
    }

    memcpy(&s_sensors[slot], sensor, sizeof(dev_sensor_info_t));
    s_sensors[slot].valid = 1;
    s_sensors[slot].name[DEV_REG_NAME_LEN - 1] = '\0';

    xSemaphoreGive(s_mutex);
    persist_post(2, slot);
    return ESP_OK;
}

esp_err_t sensor_registry_remove(uint32_t point_id)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid && s_sensors[i].point_id == point_id) {
            memset(&s_sensors[i], 0, sizeof(dev_sensor_info_t));
            xSemaphoreGive(s_mutex);
            persist_post(2, i);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t sensor_registry_update(uint32_t point_id, const dev_sensor_info_t *sensor)
{
    if (!sensor) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid && s_sensors[i].point_id == point_id) {
            s_sensors[i].type = sensor->type;
            memcpy(s_sensors[i].name, sensor->name, DEV_REG_NAME_LEN);
            s_sensors[i].name[DEV_REG_NAME_LEN - 1] = '\0';
            xSemaphoreGive(s_mutex);
            persist_post(2, i);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

bool sensor_registry_is_id_taken(uint32_t point_id)
{
    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid && s_sensors[i].point_id == point_id) {
            return true;
        }
    }
    return false;
}

uint8_t sensor_registry_next_point_no(uint16_t parent_device_id)
{
    bool used[100] = {false};

    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid && s_sensors[i].parent_device_id == parent_device_id) {
            uint8_t point_no = s_sensors[i].point_no;
            if (point_no >= 1 && point_no <= 99) {
                used[point_no] = true;
            }
        }
    }

    for (uint8_t point_no = 1; point_no <= 99; point_no++) {
        if (!used[point_no]) {
            return point_no;
        }
    }

    return 99;
}

/* ========== 灌区 CRUD ========== */

int zone_registry_get_count(void)
{
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_ZONES; i++) {
        if (s_zones[i].valid) count++;
    }
    return count;
}

const dev_zone_info_t *zone_registry_get_all(void)
{
    return s_zones;
}

esp_err_t zone_registry_add(const dev_zone_info_t *zone)
{
    if (!zone) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int slot = -1;
    for (int i = 0; i < DEV_REG_MAX_ZONES; i++) {
        if (!s_zones[i].valid) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "Zone registry full");
        return ESP_ERR_NO_MEM;
    }

    memcpy(&s_zones[slot], zone, sizeof(dev_zone_info_t));
    s_zones[slot].valid = 1;
    s_zones[slot].name[DEV_REG_NAME_LEN - 1] = '\0';

    xSemaphoreGive(s_mutex);
    persist_post(3, slot);
    return ESP_OK;
}

esp_err_t zone_registry_remove(int slot_index)
{
    if (slot_index < 0 || slot_index >= DEV_REG_MAX_ZONES) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (!s_zones[slot_index].valid) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    memset(&s_zones[slot_index], 0, sizeof(dev_zone_info_t));
    xSemaphoreGive(s_mutex);
    persist_post(3, slot_index);
    return ESP_OK;
}

esp_err_t zone_registry_update(int slot_index, const dev_zone_info_t *zone)
{
    if (!zone || slot_index < 0 || slot_index >= DEV_REG_MAX_ZONES) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (!s_zones[slot_index].valid) {
        xSemaphoreGive(s_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    memcpy(&s_zones[slot_index], zone, sizeof(dev_zone_info_t));
    s_zones[slot_index].valid = 1;
    s_zones[slot_index].name[DEV_REG_NAME_LEN - 1] = '\0';

    xSemaphoreGive(s_mutex);
    persist_post(3, slot_index);
    return ESP_OK;
}

/* ========== 辅助函数 ========== */

int device_registry_build_dropdown_str(char *buf, int buf_size)
{
    if (!buf || buf_size <= 0) return 0;

    buf[0] = '\0';
    int pos = 0;
    int count = 0;

    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (!s_devices[i].valid) continue;

        int written;
        if (count > 0) {
            if (pos < buf_size - 1) {
                buf[pos++] = '\n';
                buf[pos] = '\0';
            }
        }
        /* 格式: "名称 (编号)" */
        written = snprintf(buf + pos, buf_size - pos, "%s (%d)",
                           s_devices[i].name, s_devices[i].id);
        if (written > 0 && pos + written < buf_size) {
            pos += written;
        }
        count++;
    }

    return count;
}

const char *device_registry_type_name(dev_type_t type)
{
    if (type < DEV_TYPE_MAX) return s_dev_type_names[type];
    return "未知";
}

const char *device_registry_port_name(dev_port_t port)
{
    if (port < DEV_PORT_MAX) return s_port_names[port];
    return "未知";
}

const char *sensor_registry_type_name(sensor_type_t type)
{
    if (type < SENSOR_TYPE_MAX) return s_sensor_type_names[type];
    return "未知";
}

const char *valve_registry_type_name(valve_type_t type)
{
    if (type < VALVE_TYPE_MAX) return s_valve_type_names[type];
    return "未知";
}

/* ========== 查重函数 ========== */

bool device_registry_is_name_taken(const char *name)
{
    if (!name) return false;
    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid && strcmp(s_devices[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

bool device_registry_is_id_taken(uint16_t id)
{
    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid && s_devices[i].id == id) {
            return true;
        }
    }
    return false;
}

bool valve_registry_is_name_taken(const char *name)
{
    if (!name) return false;
    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        if (s_valves[i].valid && strcmp(s_valves[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

bool sensor_registry_is_name_taken(const char *name)
{
    if (!name) return false;
    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid && strcmp(s_sensors[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}

bool zone_registry_is_name_taken(const char *name)
{
    if (!name) return false;
    for (int i = 0; i < DEV_REG_MAX_ZONES; i++) {
        if (s_zones[i].valid && strcmp(s_zones[i].name, name) == 0) {
            return true;
        }
    }
    return false;
}
