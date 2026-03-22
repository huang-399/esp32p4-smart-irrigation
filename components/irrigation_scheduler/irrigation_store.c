#include "irrigation_scheduler.h"

#include "nvs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

#define PROG_NVS_NS  "prog_nvs"
#define FORM_NVS_NS  "form_nvs"

static const char *TAG = "irr_store";

static irr_program_t s_programs[IRR_MAX_PROGRAMS];
static int s_program_count = 0;
static irr_formula_t s_formulas[IRR_MAX_FORMULAS];
static int s_formula_count = 0;
static bool s_store_loaded = false;

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

static esp_err_t load_programs_from_nvs(void)
{
    nvs_handle_t h;
    s_program_count = 0;
    memset(s_programs, 0, sizeof(s_programs));

    esp_err_t ret = nvs_open(PROG_NVS_NS, NVS_READONLY, &h);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Program namespace not found, using empty store");
        return ESP_OK;
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
    ESP_LOGI(TAG, "Loaded %d programs", s_program_count);
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
        return ESP_OK;
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
    ESP_LOGI(TAG, "Loaded %d formulas", s_formula_count);
    return ESP_OK;
}

static esp_err_t save_programs_to_nvs(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(PROG_NVS_NS, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = 0; i < IRR_MAX_PROGRAMS; i++) {
        char key[8];
        snprintf(key, sizeof(key), "p%d", i);
        if (i < s_program_count) {
            nvs_set_blob(h, key, &s_programs[i], sizeof(irr_program_t));
        } else {
            nvs_erase_key(h, key);
        }
    }

    nvs_set_u8(h, "cnt", (uint8_t)s_program_count);
    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

static esp_err_t save_formulas_to_nvs(void)
{
    nvs_handle_t h;
    esp_err_t ret = nvs_open(FORM_NVS_NS, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        return ret;
    }

    for (int i = 0; i < IRR_MAX_FORMULAS; i++) {
        char key[8];
        snprintf(key, sizeof(key), "f%d", i);
        if (i < s_formula_count) {
            nvs_set_blob(h, key, &s_formulas[i], sizeof(irr_formula_t));
        } else {
            nvs_erase_key(h, key);
        }
    }

    nvs_set_u8(h, "cnt", (uint8_t)s_formula_count);
    ret = nvs_commit(h);
    nvs_close(h);
    return ret;
}

esp_err_t irrigation_store_init(void)
{
    if (s_store_loaded) {
        return ESP_OK;
    }

    esp_err_t ret = load_programs_from_nvs();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = load_formulas_from_nvs();
    if (ret != ESP_OK) {
        return ret;
    }

    s_store_loaded = true;
    return ESP_OK;
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
    if (count < 0 || count > IRR_MAX_PROGRAMS) {
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
    return save_programs_to_nvs();
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
    if (count < 0 || count > IRR_MAX_FORMULAS) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(s_formulas, 0, sizeof(s_formulas));
    s_formula_count = count;
    for (int i = 0; i < count; i++) {
        s_formulas[i] = formulas[i];
    }

    s_store_loaded = true;
    ESP_LOGI(TAG, "Replacing all formulas, count=%d", count);
    return save_formulas_to_nvs();
}
