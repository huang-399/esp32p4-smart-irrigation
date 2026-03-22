#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IRR_MAX_PROGRAMS 15
#define IRR_MAX_FORMULAS 15
#define IRR_MAX_PERIODS  10
#define IRR_SCHED_QUEUE_LEN 8

typedef struct {
    bool enabled;
    char time[16];
} irr_period_t;

typedef struct {
    char name[32];
    bool auto_enabled;
    char start_date[16];
    char end_date[16];
    char condition[16];
    char formula[32];
    int pre_water;
    int post_water;
    char mode[32];
    char next_start[32];
    int total_duration;
    int period_count;
    irr_period_t periods[IRR_MAX_PERIODS];
    bool selected_valves[10];
    bool selected_zones[10];
} irr_program_t;

typedef struct {
    char name[32];
    int method;
    int dilution;
    float ec;
    float ph;
    float valve_opening;
    int stir_time;
    int channel_count;
} irr_formula_t;

typedef struct {
    int pre_water;
    int post_water;
    int total_duration;
    char formula[32];
} irr_manual_irrigation_request_t;

typedef struct {
    bool auto_enabled;
    bool busy;
    bool program_active;
    bool manual_irrigation_active;
    int  active_program_index;
    char active_name[32];
    char status_text[64];
    int  total_duration;
    int  elapsed_seconds;
} irr_runtime_status_t;

esp_err_t irrigation_scheduler_init(void);

int irrigation_scheduler_get_program_count(void);
bool irrigation_scheduler_get_program(int index, irr_program_t *out);
esp_err_t irrigation_scheduler_replace_programs(const irr_program_t *programs, int count);
bool irrigation_scheduler_set_program_next_start(int index, const char *next_start);

int irrigation_scheduler_get_formula_count(void);
bool irrigation_scheduler_get_formula(int index, irr_formula_t *out);
esp_err_t irrigation_scheduler_replace_formulas(const irr_formula_t *formulas, int count);

void irrigation_scheduler_set_time_valid(bool valid);
bool irrigation_scheduler_get_time_valid(void);

bool irrigation_scheduler_set_auto_enabled(bool enabled);
bool irrigation_scheduler_get_auto_enabled(void);
bool irrigation_scheduler_start_program(int index);
bool irrigation_scheduler_start_manual_irrigation(const irr_manual_irrigation_request_t *req);
void irrigation_scheduler_get_runtime_status(irr_runtime_status_t *out);

#ifdef __cplusplus
}
#endif
