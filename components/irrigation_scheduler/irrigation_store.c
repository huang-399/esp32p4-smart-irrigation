#include "irrigation_scheduler.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_x.h"
#include "esp_log.h"
#include "nvs.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define PROG_NVS_NS           "prog_nvs"
#define FORM_NVS_NS           "form_nvs"
#define TF_CONFIG_DIR         "config"
#define TF_SNAPSHOT_FILE      "irrigation_store.bin"
#define TF_SNAPSHOT_VERSION   1
#define TF_SNAPSHOT_MAGIC     "IRRSTO1"

static const char *TAG = "irr_store";

typedef struct __attribute__((packed)) {
    char magic[8];
    uint32_t version;
    uint32_t program_count;
    uint32_t formula_count;
    uint32_t program_size;
    uint32_t formula_size;
} irr_store_snapshot_header_t;

static irr_program_t s_programs[IRR_MAX_PROGRAMS];
static int s_program_count = 0;
static irr_formula_t s_formulas[IRR_MAX_FORMULAS];
static int s_formula_count = 0;
static bool s_store_loaded = false;
static bool s_storage_available = false;
static bool s_legacy_nvs_cleared = false;

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    size_t len = strlen(src);
    if (len >= dst_size) {
        len = dst_size - 1;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
}

static void normalize_program(irr_program_t *program)
{
    if (!program) {
        return;
    }

    if (program->next_start[0] == '\0') {
        copy_text(program->next_start, sizeof(program->next_start), "--");
    }
}

static void normalize_programs(void)
{
    for (int i = 0; i < s_program_count; i++) {
        normalize_program(&s_programs[i]);
    }
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

static esp_err_t save_snapshot_to_tf(void)
{
    char path[192];
    char temp_path[200];
    FILE *fp;
    irr_store_snapshot_header_t header = {
        .magic = TF_SNAPSHOT_MAGIC,
        .version = TF_SNAPSHOT_VERSION,
        .program_count = (uint32_t)s_program_count,
        .formula_count = (uint32_t)s_formula_count,
        .program_size = sizeof(irr_program_t),
        .formula_size = sizeof(irr_formula_t),
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
        fwrite(s_programs, sizeof(s_programs), 1, fp) != 1 ||
        fwrite(s_formulas, sizeof(s_formulas), 1, fp) != 1) {
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
    irr_store_snapshot_header_t header;
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
        header.program_size != sizeof(irr_program_t) ||
        header.formula_size != sizeof(irr_formula_t) ||
        header.program_count > IRR_MAX_PROGRAMS ||
        header.formula_count > IRR_MAX_FORMULAS) {
        fclose(fp);
        ESP_LOGW(TAG, "TF snapshot header mismatch, ignore file");
        return ESP_ERR_INVALID_VERSION;
    }

    if (fread(s_programs, sizeof(s_programs), 1, fp) != 1 ||
        fread(s_formulas, sizeof(s_formulas), 1, fp) != 1) {
        fclose(fp);
        ESP_LOGW(TAG, "TF snapshot content incomplete, ignore file");
        return ESP_ERR_INVALID_SIZE;
    }

    fclose(fp);

    s_program_count = (int)header.program_count;
    s_formula_count = (int)header.formula_count;
    normalize_programs();
    s_storage_available = true;
    return ESP_OK;
}

static esp_err_t clear_namespace_if_present(const char *ns)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(ns, NVS_READWRITE, &h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_erase_all(h);
    if (ret == ESP_OK) {
        ret = nvs_commit(h);
    }
    nvs_close(h);
    return ret;
}

static esp_err_t clear_legacy_nvs_if_safe(void)
{
    if (s_legacy_nvs_cleared || !s_storage_available) {
        return ESP_OK;
    }

    esp_err_t ret = clear_namespace_if_present(PROG_NVS_NS);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = clear_namespace_if_present(FORM_NVS_NS);
    if (ret == ESP_OK) {
        s_legacy_nvs_cleared = true;
        ESP_LOGI(TAG, "Cleared legacy irrigation NVS namespaces after TF confirmation");
    }
    return ret;
}

static esp_err_t load_programs_from_nvs(void)
{
    nvs_handle_t h;
    s_program_count = 0;
    memset(s_programs, 0, sizeof(s_programs));

    esp_err_t ret = nvs_open(PROG_NVS_NS, NVS_READONLY, &h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Program namespace not found, using empty store");
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t cnt = 0;
    nvs_get_u8(h, "cnt", &cnt);
    if (cnt > IRR_MAX_PROGRAMS) {
        cnt = IRR_MAX_PROGRAMS;
    }

    for (int i = 0; i < cnt; i++) {
        char key[8];
        size_t len = sizeof(irr_program_t);
        snprintf(key, sizeof(key), "p%d", i);
        if (nvs_get_blob(h, key, &s_programs[i], &len) == ESP_OK && len == sizeof(irr_program_t)) {
            normalize_program(&s_programs[i]);
            s_program_count++;
        }
    }

    nvs_close(h);
    ESP_LOGI(TAG, "Loaded %d programs from legacy NVS", s_program_count);
    return ESP_OK;
}

static esp_err_t load_formulas_from_nvs(void)
{
    nvs_handle_t h;
    s_formula_count = 0;
    memset(s_formulas, 0, sizeof(s_formulas));

    esp_err_t ret = nvs_open(FORM_NVS_NS, NVS_READONLY, &h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Formula namespace not found, using empty store");
        return ESP_ERR_NOT_FOUND;
    }
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t cnt = 0;
    nvs_get_u8(h, "cnt", &cnt);
    if (cnt > IRR_MAX_FORMULAS) {
        cnt = IRR_MAX_FORMULAS;
    }

    for (int i = 0; i < cnt; i++) {
        char key[8];
        size_t len = sizeof(irr_formula_t);
        snprintf(key, sizeof(key), "f%d", i);
        if (nvs_get_blob(h, key, &s_formulas[i], &len) == ESP_OK && len == sizeof(irr_formula_t)) {
            s_formula_count++;
        }
    }

    nvs_close(h);
    ESP_LOGI(TAG, "Loaded %d formulas from legacy NVS", s_formula_count);
    return ESP_OK;
}

static esp_err_t load_from_legacy_nvs(bool *loaded_any)
{
    if (loaded_any) {
        *loaded_any = false;
    }

    esp_err_t ret = load_programs_from_nvs();
    if (ret == ESP_OK) {
        if (loaded_any) {
            *loaded_any = true;
        }
    } else if (ret != ESP_ERR_NOT_FOUND) {
        return ret;
    }

    ret = load_formulas_from_nvs();
    if (ret == ESP_OK) {
        if (loaded_any) {
            *loaded_any = true;
        }
    } else if (ret != ESP_ERR_NOT_FOUND) {
        return ret;
    }

    return (loaded_any && *loaded_any) ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t irrigation_store_init(void)
{
    if (s_store_loaded) {
        return ESP_OK;
    }

    s_program_count = 0;
    s_formula_count = 0;
    memset(s_programs, 0, sizeof(s_programs));
    memset(s_formulas, 0, sizeof(s_formulas));

    esp_err_t ret = load_snapshot_from_tf();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded irrigation store from TF snapshot: %d programs, %d formulas",
                 s_program_count, s_formula_count);
        esp_err_t cleanup_ret = clear_legacy_nvs_if_safe();
        if (cleanup_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to clear legacy irrigation NVS: %s", esp_err_to_name(cleanup_ret));
        }
        s_store_loaded = true;
        return ESP_OK;
    }

    bool loaded_any = false;
    ret = load_from_legacy_nvs(&loaded_any);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Loaded irrigation store from legacy NVS: %d programs, %d formulas",
                 s_program_count, s_formula_count);
        esp_err_t tf_ret = save_snapshot_to_tf();
        if (tf_ret == ESP_OK) {
            ESP_LOGI(TAG, "Migrated irrigation store from NVS to TF snapshot");
            esp_err_t cleanup_ret = clear_legacy_nvs_if_safe();
            if (cleanup_ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to clear legacy irrigation NVS: %s", esp_err_to_name(cleanup_ret));
            }
        } else {
            ESP_LOGW(TAG, "Irrigation store migration to TF skipped: %s", esp_err_to_name(tf_ret));
        }
        s_store_loaded = true;
        return ESP_OK;
    }

    if (ret == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "No TF snapshot or legacy NVS data, starting empty irrigation store");
        s_store_loaded = true;
        return ESP_OK;
    }

    return ret;
}

int irrigation_scheduler_get_program_count(void)
{
    return s_program_count;
}

bool irrigation_scheduler_get_program(int index, irr_program_t *out)
{
    if (!out || index < 0 || index >= s_program_count) {
        return false;
    }
    *out = s_programs[index];
    return true;
}

esp_err_t irrigation_scheduler_replace_programs(const irr_program_t *programs, int count)
{
    if ((count > 0 && !programs) || count < 0 || count > IRR_MAX_PROGRAMS) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_programs, 0, sizeof(s_programs));
    s_program_count = count;
    for (int i = 0; i < count; i++) {
        s_programs[i] = programs[i];
        normalize_program(&s_programs[i]);
    }

    s_store_loaded = true;
    ESP_LOGI(TAG, "Replacing all programs, count=%d", count);
    return save_snapshot_to_tf();
}

bool irrigation_scheduler_set_program_next_start(int index, const char *next_start)
{
    if (index < 0 || index >= s_program_count) {
        return false;
    }

    copy_text(s_programs[index].next_start, sizeof(s_programs[index].next_start),
              (next_start && next_start[0]) ? next_start : "--");
    return true;
}

int irrigation_scheduler_get_formula_count(void)
{
    return s_formula_count;
}

bool irrigation_scheduler_get_formula(int index, irr_formula_t *out)
{
    if (!out || index < 0 || index >= s_formula_count) {
        return false;
    }
    *out = s_formulas[index];
    return true;
}

esp_err_t irrigation_scheduler_replace_formulas(const irr_formula_t *formulas, int count)
{
    if ((count > 0 && !formulas) || count < 0 || count > IRR_MAX_FORMULAS) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_formulas, 0, sizeof(s_formulas));
    s_formula_count = count;
    for (int i = 0; i < count; i++) {
        s_formulas[i] = formulas[i];
    }

    s_store_loaded = true;
    ESP_LOGI(TAG, "Replacing all formulas, count=%d", count);
    return save_snapshot_to_tf();
}
