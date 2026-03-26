#include "device_registry.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_x.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "dev_reg";

#define NVS_NAMESPACE        "dev_reg"
#define TF_CONFIG_DIR        "config"
#define TF_SNAPSHOT_FILE     "device_registry.bin"
#define TF_SNAPSHOT_VERSION  1
#define TF_SNAPSHOT_MAGIC    "DEVREG1"

typedef struct __attribute__((packed)) {
    char magic[8];
    uint32_t version;
    uint32_t device_count;
    uint32_t valve_count;
    uint32_t sensor_count;
    uint32_t zone_count;
    uint32_t device_size;
    uint32_t valve_size;
    uint32_t sensor_size;
    uint32_t zone_size;
} dev_registry_snapshot_header_t;

/* ---- 静态数据 ---- */
static dev_device_info_t s_devices[DEV_REG_MAX_DEVICES];
static dev_valve_info_t  s_valves[DEV_REG_MAX_VALVES];
static dev_sensor_info_t s_sensors[DEV_REG_MAX_SENSORS];
static dev_zone_info_t   s_zones[DEV_REG_MAX_ZONES];
static SemaphoreHandle_t s_mutex = NULL;
static bool s_inited = false;
static bool s_storage_available = false;
static bool s_legacy_nvs_cleared = false;

/* ---- 后台快照持久化（不阻塞 LVGL 线程） ---- */
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

static int count_valid_devices(void)
{
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid) {
            count++;
        }
    }
    return count;
}

static int count_valid_valves(void)
{
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_VALVES; i++) {
        if (s_valves[i].valid) {
            count++;
        }
    }
    return count;
}

static int count_valid_sensors(void)
{
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid) {
            count++;
        }
    }
    return count;
}

static int count_valid_zones(void)
{
    int count = 0;
    for (int i = 0; i < DEV_REG_MAX_ZONES; i++) {
        if (s_zones[i].valid) {
            count++;
        }
    }
    return count;
}

static void build_snapshot_path(char *path, size_t path_size)
{
    snprintf(path, path_size, "%s/%s/%s", BSP_SD_MOUNT_POINT, TF_CONFIG_DIR, TF_SNAPSHOT_FILE);
}

static bool is_sd_mount_point_ready(void)
{
    struct stat st = {0};
    return (stat(BSP_SD_MOUNT_POINT, &st) == 0) && S_ISDIR(st.st_mode);
}

static esp_err_t ensure_directory_exists(const char *path)
{
    struct stat st = {0};

    if (!path || path[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? ESP_OK : ESP_FAIL;
    }

    if (mkdir(path, 0775) == 0 || errno == EEXIST) {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "mkdir failed for %s: errno=%d", path, errno);
    return ESP_FAIL;
}

static esp_err_t ensure_tf_ready(void)
{
    char config_dir[160];
    esp_err_t ret = ESP_OK;

    if (!is_sd_mount_point_ready()) {
        ret = bsp_sdcard_mount();
        if (ret != ESP_OK) {
            s_storage_available = false;
            ESP_LOGW(TAG, "TF mount failed: %s", esp_err_to_name(ret));
            return ret;
        }
    }

    snprintf(config_dir, sizeof(config_dir), "%s/%s", BSP_SD_MOUNT_POINT, TF_CONFIG_DIR);
    ret = ensure_directory_exists(config_dir);
    if (ret != ESP_OK) {
        s_storage_available = false;
        return ret;
    }

    s_storage_available = true;
    return ESP_OK;
}

static esp_err_t save_snapshot_to_tf_locked(void)
{
    char path[192];
    char temp_path[200];
    FILE *fp;
    dev_registry_snapshot_header_t header = {
        .magic = TF_SNAPSHOT_MAGIC,
        .version = TF_SNAPSHOT_VERSION,
        .device_count = (uint32_t)count_valid_devices(),
        .valve_count = (uint32_t)count_valid_valves(),
        .sensor_count = (uint32_t)count_valid_sensors(),
        .zone_count = (uint32_t)count_valid_zones(),
        .device_size = sizeof(dev_device_info_t),
        .valve_size = sizeof(dev_valve_info_t),
        .sensor_size = sizeof(dev_sensor_info_t),
        .zone_size = sizeof(dev_zone_info_t),
    };

    esp_err_t ret = ensure_tf_ready();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "TF storage unavailable, skip snapshot save: %s", esp_err_to_name(ret));
        return ret;
    }

    build_snapshot_path(path, sizeof(path));
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);

    fp = fopen(temp_path, "wb");
    if (!fp) {
        ESP_LOGE(TAG, "Open snapshot temp file failed: %s", temp_path);
        s_storage_available = false;
        return ESP_FAIL;
    }

    if (fwrite(&header, sizeof(header), 1, fp) != 1 ||
        fwrite(s_devices, sizeof(s_devices), 1, fp) != 1 ||
        fwrite(s_valves, sizeof(s_valves), 1, fp) != 1 ||
        fwrite(s_sensors, sizeof(s_sensors), 1, fp) != 1 ||
        fwrite(s_zones, sizeof(s_zones), 1, fp) != 1) {
        fclose(fp);
        remove(temp_path);
        ESP_LOGE(TAG, "Write TF snapshot failed");
        s_storage_available = false;
        return ESP_FAIL;
    }

    fclose(fp);

    if (remove(path) != 0 && errno != ENOENT) {
        ESP_LOGW(TAG, "Remove old snapshot failed: errno=%d", errno);
    }

    if (rename(temp_path, path) != 0) {
        remove(temp_path);
        ESP_LOGE(TAG, "Rename snapshot file failed: errno=%d", errno);
        s_storage_available = false;
        return ESP_FAIL;
    }

    s_storage_available = true;
    return ESP_OK;
}

static esp_err_t load_snapshot_from_tf(void)
{
    char path[192];
    FILE *fp;
    dev_registry_snapshot_header_t header;
    esp_err_t ret = ensure_tf_ready();

    if (ret != ESP_OK) {
        return ret;
    }

    build_snapshot_path(path, sizeof(path));
    fp = fopen(path, "rb");
    if (!fp) {
        return ESP_ERR_NOT_FOUND;
    }

    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (memcmp(header.magic, TF_SNAPSHOT_MAGIC, sizeof(header.magic)) != 0 ||
        header.version != TF_SNAPSHOT_VERSION ||
        header.device_size != sizeof(dev_device_info_t) ||
        header.valve_size != sizeof(dev_valve_info_t) ||
        header.sensor_size != sizeof(dev_sensor_info_t) ||
        header.zone_size != sizeof(dev_zone_info_t)) {
        fclose(fp);
        ESP_LOGW(TAG, "TF snapshot header mismatch, ignore file");
        return ESP_ERR_INVALID_VERSION;
    }

    if (fread(s_devices, sizeof(s_devices), 1, fp) != 1 ||
        fread(s_valves, sizeof(s_valves), 1, fp) != 1 ||
        fread(s_sensors, sizeof(s_sensors), 1, fp) != 1 ||
        fread(s_zones, sizeof(s_zones), 1, fp) != 1) {
        fclose(fp);
        ESP_LOGW(TAG, "TF snapshot content incomplete, ignore file");
        return ESP_ERR_INVALID_SIZE;
    }

    fclose(fp);
    s_storage_available = true;
    return ESP_OK;
}

/* ---- NVS 加载（兼容旧整 blob 格式，自动迁移） ---- */
static esp_err_t clear_legacy_nvs_namespace(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        s_legacy_nvs_cleared = true;
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_erase_all(handle);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    if (ret == ESP_OK) {
        s_legacy_nvs_cleared = true;
        ESP_LOGI(TAG, "Cleared legacy NVS namespace after TF confirmation");
    }
    return ret;
}

static esp_err_t clear_legacy_nvs_if_safe(void)
{
    if (s_legacy_nvs_cleared || !s_storage_available) {
        return ESP_OK;
    }
    return clear_legacy_nvs_namespace();
}

static esp_err_t load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "NVS namespace not found, starting empty");
        s_legacy_nvs_cleared = true;
        return ESP_ERR_NOT_FOUND;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs_open failed");

    size_t len;
    bool migrated = false;

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
            if (ret != ESP_OK) {
                memset(&s_devices[i], 0, sizeof(dev_device_info_t));
            }
        }
    }

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
            if (ret != ESP_OK) {
                memset(&s_valves[i], 0, sizeof(dev_valve_info_t));
            }
        }
    }

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
            if (ret != ESP_OK) {
                memset(&s_sensors[i], 0, sizeof(dev_sensor_info_t));
            }
        }
    }

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
            if (ret != ESP_OK) {
                memset(&s_zones[i], 0, sizeof(dev_zone_info_t));
            }
        }
    }

    if (migrated) {
        nvs_commit(handle);
        ESP_LOGI(TAG, "NVS migration to per-entry format complete");
    }

    nvs_close(handle);
    return ESP_OK;
}

/* ---- 后台持久化任务和投递函数 ---- */
static void persist_task(void *param)
{
    (void)param;
    persist_msg_t msg;

    while (xQueueReceive(s_persist_queue, &msg, portMAX_DELAY) == pdTRUE) {
        persist_msg_t extra;
        (void)msg;

        while (xQueueReceive(s_persist_queue, &extra, 0) == pdTRUE) {
        }

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        esp_err_t ret = save_snapshot_to_tf_locked();
        xSemaphoreGive(s_mutex);

        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Persist TF snapshot failed: %s", esp_err_to_name(ret));
        }
    }
}

static void persist_post(uint8_t type, uint8_t slot)
{
    if (s_persist_queue) {
        persist_msg_t msg = { .type = type, .slot = slot };
        xQueueSend(s_persist_queue, &msg, 0);
    }
}

/* ========== 初始化 ========== */
esp_err_t device_registry_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    memset(s_devices, 0, sizeof(s_devices));
    memset(s_valves, 0, sizeof(s_valves));
    memset(s_sensors, 0, sizeof(s_sensors));
    memset(s_zones, 0, sizeof(s_zones));

    esp_err_t ret = load_snapshot_from_tf();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded device registry from TF snapshot");
        esp_err_t cleanup_ret = clear_legacy_nvs_if_safe();
        if (cleanup_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to clear legacy device registry NVS: %s", esp_err_to_name(cleanup_ret));
        }
    } else {
        memset(s_devices, 0, sizeof(s_devices));
        memset(s_valves, 0, sizeof(s_valves));
        memset(s_sensors, 0, sizeof(s_sensors));
        memset(s_zones, 0, sizeof(s_zones));

        esp_err_t nvs_ret = load_from_nvs();
        if (nvs_ret == ESP_OK) {
            ESP_LOGI(TAG, "Loaded device registry from legacy NVS");
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            esp_err_t tf_ret = save_snapshot_to_tf_locked();
            xSemaphoreGive(s_mutex);
            if (tf_ret == ESP_OK) {
                ESP_LOGI(TAG, "Migrated device registry from NVS to TF snapshot");
                esp_err_t cleanup_ret = clear_legacy_nvs_if_safe();
                if (cleanup_ret != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to clear legacy device registry NVS: %s", esp_err_to_name(cleanup_ret));
                }
            } else {
                ESP_LOGW(TAG, "Device registry migration to TF skipped: %s", esp_err_to_name(tf_ret));
            }
        } else if (nvs_ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGI(TAG, "No TF snapshot or legacy NVS data, starting empty registry");
        } else {
            ESP_LOGW(TAG, "Legacy NVS load error, starting with empty registry: %s", esp_err_to_name(nvs_ret));
        }
    }

    s_persist_queue = xQueueCreate(16, sizeof(persist_msg_t));
    if (!s_persist_queue) {
        ESP_LOGE(TAG, "Failed to create persist queue");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(persist_task, "dev_persist", 4096, NULL, 3, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create persist task");
        vQueueDelete(s_persist_queue);
        s_persist_queue = NULL;
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Initialized: %d devices, %d valves, %d sensors, %d zones%s",
             count_valid_devices(),
             count_valid_valves(),
             count_valid_sensors(),
             count_valid_zones(),
             s_storage_available ? " (TF ready)" : " (TF unavailable)");

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

    for (int i = 0; i < DEV_REG_MAX_DEVICES; i++) {
        if (s_devices[i].valid && s_devices[i].id == dev->id) {
            xSemaphoreGive(s_mutex);
            ESP_LOGW(TAG, "Device ID %d already exists", dev->id);
            return ESP_ERR_INVALID_STATE;
        }
    }

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

    for (int i = 0; i < DEV_REG_MAX_SENSORS; i++) {
        if (s_sensors[i].valid && s_sensors[i].point_id == sensor->point_id) {
            xSemaphoreGive(s_mutex);
            ESP_LOGW(TAG, "Sensor point_id %lu already exists", (unsigned long)sensor->point_id);
            return ESP_ERR_INVALID_STATE;
        }
    }

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
