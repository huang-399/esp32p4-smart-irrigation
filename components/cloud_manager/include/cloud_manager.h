#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char host[128];
    int  port;
    char access_token[128];
    bool enable_tls;
    bool enabled;
} cloud_manager_config_t;

typedef enum {
    CLOUD_RPC_UNKNOWN = 0,
    CLOUD_RPC_SET_AUTO_MODE,
    CLOUD_RPC_START_PROGRAM,
    CLOUD_RPC_START_MANUAL_IRRIGATION,
    CLOUD_RPC_STOP_IRRIGATION,
    CLOUD_RPC_CONTROL_DEVICE,
    CLOUD_RPC_CONTROL_VALVE,
    CLOUD_RPC_REPORT_NOW,
} cloud_rpc_command_type_t;

typedef struct {
    int  pre_water;
    int  post_water;
    int  total_duration;
    char formula[32];
} cloud_rpc_manual_irrigation_request_t;

typedef struct {
    cloud_rpc_command_type_t type;
    bool enabled;
    bool on;
    int program_index;
    uint32_t point_id;
    uint8_t channel;
    cloud_rpc_manual_irrigation_request_t manual;
} cloud_rpc_command_t;

typedef struct {
    bool success;
    char code[32];
    char message[96];
} cloud_rpc_result_t;

typedef bool (*cloud_manager_rpc_handler_t)(const cloud_rpc_command_t *cmd, cloud_rpc_result_t *result);

esp_err_t cloud_manager_init(const cloud_manager_config_t *cfg);
esp_err_t cloud_manager_start(void);

void cloud_manager_register_rpc_handler(cloud_manager_rpc_handler_t handler);
void cloud_manager_notify_wifi(bool connected);
void cloud_manager_notify_time_synced(bool synced);

bool cloud_manager_is_connected(void);
esp_err_t cloud_manager_request_report_now(void);

#ifdef __cplusplus
}
#endif
