/**
 * @file ui_device.c
 * @brief 设备控制界面实现
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define UI_DEVICE_MAX_SENSOR_ITEMS 64
#define UI_DEVICE_MAX_VALVE_ROWS   32
#define UI_DEVICE_MAX_ZONE_ROWS    16
#define UI_DEVICE_MAX_ZONE_SWITCHES 64
#define UI_DEVICE_CONTROL_CONFIRM_TIMEOUT_MS 7000U
#define UI_DEVICE_CONTROL_CONFIRM_TIMER_MS   200U


/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_tab_buttons(lv_obj_t *parent);
static void create_control_panel(lv_obj_t *parent);
static void create_data_panel(lv_obj_t *parent);
static void create_main_control_view(lv_obj_t *parent);
static void create_valve_control_view(lv_obj_t *parent);
static void create_zone_control_view(lv_obj_t *parent);
static void create_sensor_monitor_view(lv_obj_t *parent);
static void switch_to_tab(int tab_index);
static void create_device_card(lv_obj_t *parent, const char *title, int x, int y, bool is_double);
static void tab_btn_cb(lv_event_t *e);
static void device_card_click_cb(lv_event_t *e);
static void valve_btn_cb(lv_event_t *e);
static void zone_switch_cb(lv_event_t *e);
static void show_device_confirm_dialog(const char *dev_name, bool to_on);
static void device_dialog_confirm_cb(lv_event_t *e);
static void device_dialog_cancel_cb(lv_event_t *e);
static void update_valve_open_count(void);
static void refresh_control_cards(void);
static void refresh_main_data_panel(void);
static void refresh_valve_card(int valve_idx);
static void refresh_zone_field_card(int field_idx);
static void refresh_tank_card(int tank_idx);
static void refresh_sensor_field_card(int field_idx);
static void refresh_sensor_pipe_label(int pipe_idx);
static void set_switch_checked(lv_obj_t *sw, bool checked);
static void clear_view_object_refs(void);
static uint32_t valve_row_to_point_id(const ui_valve_row_t *row);
static bool valve_point_id_is_on(uint32_t point_id);
static const char *sensor_type_name(uint8_t type);
static const char *zone_switch_title_for_point(uint32_t point_id, char *buf, size_t buf_size);
static int point_id_to_pipe_index(uint32_t point_id);
static bool format_runtime_point_value(uint32_t point_id, char *buf, size_t buf_size);
static void refresh_dynamic_valve_items(void);
static void refresh_dynamic_zone_switches(void);
static void refresh_dynamic_sensor_items(void);
static void control_confirm_timer_cb(lv_timer_t *timer);
static void start_control_confirm(uint32_t point_id, bool target_on);
static void resolve_control_confirm(uint32_t point_id, bool actual_on);

/* 全局变量 */
static lv_obj_t *g_left_panel = NULL;   /* 左侧白色面板（主机控制视图） */
static lv_obj_t *g_right_panel = NULL;  /* 右侧白色面板（主机控制视图） */
static lv_obj_t *g_view_container = NULL; /* 视图容器（懒加载） */
static lv_obj_t *g_tab_buttons[4] = {NULL};   /* 标签按钮数组 */
static int g_active_tab = 0;             /* 当前活动标签页 */

/* 设备页查询回调 */
static ui_get_valve_count_cb_t  g_get_valve_count_cb = NULL;
static ui_get_valve_list_cb_t   g_get_valve_list_cb = NULL;
static ui_get_sensor_count_cb_t g_get_sensor_count_cb = NULL;
static ui_get_sensor_list_cb_t  g_get_sensor_list_cb = NULL;
static ui_get_zone_count_cb_t   g_get_zone_count_cb = NULL;
static ui_get_zone_list_cb_t    g_get_zone_list_cb = NULL;
static ui_get_zone_detail_cb_t  g_get_zone_detail_cb = NULL;

/* 主机/阀门/灌区控制确认状态 */
typedef enum {
    DEV_CONFIRM_IDLE = 0,
    DEV_CONFIRM_PENDING,
    DEV_CONFIRM_TIMEOUT,
} device_confirm_state_t;

/* 灌区页动态开关映射 */
static lv_obj_t *g_zone_dynamic_info_labels[UI_DEVICE_MAX_ZONE_SWITCHES] = {NULL};
static lv_obj_t *g_zone_dynamic_switches[UI_DEVICE_MAX_ZONE_SWITCHES] = {NULL};
static uint32_t  g_zone_dynamic_point_ids[UI_DEVICE_MAX_ZONE_SWITCHES] = {0};
static int       g_zone_dynamic_switch_count = 0;
static device_confirm_state_t g_zone_confirm_states[UI_DEVICE_MAX_ZONE_SWITCHES] = {DEV_CONFIRM_IDLE};
static bool      g_zone_confirm_targets[UI_DEVICE_MAX_ZONE_SWITCHES] = {false};
static uint32_t  g_zone_confirm_deadlines[UI_DEVICE_MAX_ZONE_SWITCHES] = {0};

/* 传感监测页动态标签映射 */
static lv_obj_t *g_sensor_dynamic_value_labels[UI_DEVICE_MAX_SENSOR_ITEMS] = {NULL};
static uint32_t  g_sensor_dynamic_point_ids[UI_DEVICE_MAX_SENSOR_ITEMS] = {0};
static int       g_sensor_dynamic_item_count = 0;
/* 设备控制回调 */
static ui_device_control_cb_t g_device_control_cb = NULL;

/* 主机控制状态标签 */
static lv_obj_t *g_dev_status_labels[5] = {NULL};  /* 主水泵/施肥泵/出肥阀/注水阀/搅拌机 */
static device_confirm_state_t g_dev_confirm_states[5] = {DEV_CONFIRM_IDLE, DEV_CONFIRM_IDLE, DEV_CONFIRM_IDLE, DEV_CONFIRM_IDLE, DEV_CONFIRM_IDLE};
static bool g_dev_confirm_targets[5] = {false, false, false, false, false};
static uint32_t g_dev_confirm_deadlines[5] = {0};
static lv_timer_t *g_control_confirm_timer = NULL;

/* 右侧运行态摘要值标签 */
static lv_obj_t *g_main_data_vals[9] = {NULL};  /* 主管道阀门/流量/压力 + N/P/K罐开关与液位 */
static lv_obj_t *g_main_pipe_labels[6] = {NULL}; /* 副管道1~6摘要 */

/* 阀门控制视图标签 */
static lv_obj_t *g_valve_total_label = NULL;
static lv_obj_t *g_valve_open_label = NULL;
static lv_obj_t *g_valve_container = NULL;

/* 灌区控制视图标签 */
static lv_obj_t *g_zone_container = NULL;
static lv_obj_t *g_zone_field_info_labels[6] = {NULL};
static lv_obj_t *g_zone_field_switches[6] = {NULL};
static lv_obj_t *g_tank_info_labels[3] = {NULL};
static lv_obj_t *g_tank_switches[3] = {NULL};

/* 传感监测视图标签 */
static lv_obj_t *g_sensor_container = NULL;
static lv_obj_t *g_sensor_field_labels[6][6] = {{NULL}};
static lv_obj_t *g_sensor_pipe_labels[7] = {NULL};

/* 运行时缓存 */
typedef struct {
    bool valid;
    uint8_t registered_mask;
    float n;
    float p;
    float k;
    float temp;
    float humi;
    float light;
} device_field_cache_t;

typedef struct {
    bool valid;
    bool valve_bound;
    bool valve_on;
    bool flow_bound;
    float flow;
    bool pressure_bound;
    float pressure;
} device_pipe_cache_t;

typedef struct {
    bool valid;
    bool switch_on;
    bool level_bound;
    float level;
} device_tank_cache_t;

static device_field_cache_t s_field_cache[6] = {0};
static device_pipe_cache_t s_pipe_cache[7] = {0};
static device_tank_cache_t s_tank_cache[3] = {0};
static bool g_syncing_switch_state = false;

/* 确认对话框 */
static lv_obj_t *g_device_dialog = NULL;
static uint32_t g_pending_point_id = 0;
static bool g_pending_on = false;

/* 设备开关状态 */
static bool g_dev_states[5] = {false, false, false, false, false};

/* 阀门状态及状态标签 */
static bool g_valve_states[7] = {false, false, false, false, false, false, false};
static lv_obj_t *g_valve_status_labels[7] = {NULL};
static lv_obj_t *g_valve_btns[7] = {NULL};       /* 阀门开关按钮 */
static lv_obj_t *g_valve_btn_labels[7] = {NULL};  /* 按钮上的文字 */
static lv_obj_t *g_valve_dynamic_state_labels[UI_DEVICE_MAX_VALVE_ROWS] = {NULL};
static lv_obj_t *g_valve_dynamic_flow_labels[UI_DEVICE_MAX_VALVE_ROWS] = {NULL};
static lv_obj_t *g_valve_dynamic_pressure_labels[UI_DEVICE_MAX_VALVE_ROWS] = {NULL};
static lv_obj_t *g_valve_dynamic_btns[UI_DEVICE_MAX_VALVE_ROWS] = {NULL};
static lv_obj_t *g_valve_dynamic_btn_labels[UI_DEVICE_MAX_VALVE_ROWS] = {NULL};
static uint32_t  g_valve_dynamic_point_ids[UI_DEVICE_MAX_VALVE_ROWS] = {0};
static int       g_valve_dynamic_count = 0;
static device_confirm_state_t g_valve_confirm_states[UI_DEVICE_MAX_VALVE_ROWS] = {DEV_CONFIRM_IDLE};
static bool      g_valve_confirm_targets[UI_DEVICE_MAX_VALVE_ROWS] = {false};
static uint32_t  g_valve_confirm_deadlines[UI_DEVICE_MAX_VALVE_ROWS] = {0};

/* 设备名称/业务点编号映射 */
static const uint32_t s_dev_point_id_map[5] = {300001U, 300002U, 300003U, 300004U, 300005U};
static const char *s_dev_name_map[5] = {"主水泵", "施肥泵", "出肥阀", "注水阀", "搅拌机"};

static const char *sensor_type_name(uint8_t type)
{
    switch (type) {
    case 0: return "氮";
    case 1: return "磷";
    case 2: return "钾";
    case 3: return "温度";
    case 4: return "湿度";
    case 5: return "光照";
    case 6: return "流量";
    case 7: return "压力";
    case 8: return "液位";
    case 9: return "阀门状态";
    case 10: return "开关状态";
    default: return "未知";
    }
}

static int point_id_to_pipe_index(uint32_t point_id)
{
    uint32_t node_id = point_id / 100U;
    uint32_t point_no = point_id % 100U;

    if (node_id < 2000U || node_id > 2006U) {
        return -1;
    }
    if (point_no != 1U && point_no != 2U && point_no != 3U && point_no != 11U) {
        return -1;
    }
    return (int)(node_id - 2000U);
}

static uint32_t valve_row_to_point_id(const ui_valve_row_t *row)
{
    if (!row) {
        return 0;
    }

    return row->id;
}

static bool valve_point_id_is_on(uint32_t point_id)
{
    int pipe_idx = point_id_to_pipe_index(point_id);

    if (pipe_idx < 0 || pipe_idx >= 7) {
        return false;
    }

    return s_pipe_cache[pipe_idx].valid && s_pipe_cache[pipe_idx].valve_on;
}

static const char *zone_switch_title_for_point(uint32_t point_id, char *buf, size_t buf_size)
{
    if (point_id >= 300006U && point_id <= 300008U) {
        static const char *tank_names[] = {"储料罐N", "储料罐P", "储料罐K"};
        return tank_names[point_id - 300006U];
    }

    if (point_id >= 200001U && point_id <= 200601U && (point_id % 100U) == 1U) {
        int pipe_idx = point_id_to_pipe_index(point_id);
        if (pipe_idx == 0) {
            return "主管道阀";
        }
        if (pipe_idx > 0) {
            snprintf(buf, buf_size, "副管道%d阀", pipe_idx);
            return buf;
        }
    }

    snprintf(buf, buf_size, "点位%lu", (unsigned long)point_id);
    return buf;
}

static bool format_runtime_point_value(uint32_t point_id, char *buf, size_t buf_size)
{
    uint32_t node_id = point_id / 100U;
    uint32_t point_no = point_id % 100U;

    if (node_id >= 1001U && node_id <= 1006U) {
        const device_field_cache_t *field = &s_field_cache[node_id - 1001U];
        int bit_index = -1;
        float value = 0.0f;

        switch (point_no) {
        case 1: bit_index = 0; value = field->n; break;
        case 2: bit_index = 1; value = field->p; break;
        case 3: bit_index = 2; value = field->k; break;
        case 4: bit_index = 3; value = field->temp; break;
        case 5: bit_index = 4; value = field->humi; break;
        case 6: bit_index = 5; value = field->light; break;
        default: return false;
        }

        if (!field->valid || !(field->registered_mask & (1U << bit_index))) {
            return false;
        }
        snprintf(buf, buf_size, "%.1f", value);
        return true;
    }

    if (node_id >= 2000U && node_id <= 2006U) {
        const device_pipe_cache_t *pipe = &s_pipe_cache[node_id - 2000U];
        switch (point_no) {
        case 1:
        case 11:
            if (!pipe->valid || !pipe->valve_bound) {
                return false;
            }
            snprintf(buf, buf_size, "%s", pipe->valve_on ? "开启" : "关闭");
            return true;
        case 2:
            if (!pipe->valid || !pipe->flow_bound) {
                return false;
            }
            snprintf(buf, buf_size, "%.2f", pipe->flow);
            return true;
        case 3:
            if (!pipe->valid || !pipe->pressure_bound) {
                return false;
            }
            snprintf(buf, buf_size, "%.2f", pipe->pressure);
            return true;
        default:
            return false;
        }
    }

    if (node_id == 3000U) {
        if (point_no >= 11U && point_no <= 15U) {
            int dev_idx = (int)(point_no - 11U);
            snprintf(buf, buf_size, "%s", g_dev_states[dev_idx] ? "开启" : "关闭");
            return true;
        }
        if (point_no >= 16U && point_no <= 18U) {
            const device_tank_cache_t *tank = &s_tank_cache[point_no - 16U];
            if (!tank->valid) {
                return false;
            }
            snprintf(buf, buf_size, "%s", tank->switch_on ? "开启" : "关闭");
            return true;
        }
        if (point_no >= 6U && point_no <= 8U) {
            const device_tank_cache_t *tank = &s_tank_cache[point_no - 6U];
            if (!tank->valid) {
                return false;
            }
            snprintf(buf, buf_size, "%s", tank->switch_on ? "开启" : "关闭");
            return true;
        }
        if (point_no >= 21U && point_no <= 23U) {
            const device_tank_cache_t *tank = &s_tank_cache[point_no - 21U];
            if (!tank->valid || !tank->level_bound) {
                return false;
            }
            snprintf(buf, buf_size, "%.1f", tank->level);
            return true;
        }
    }

    return false;
}

static void refresh_dynamic_valve_items(void)
{
    for (int i = 0; i < g_valve_dynamic_count; i++) {
        uint32_t point_id = g_valve_dynamic_point_ids[i];
        uint32_t node_id = point_id / 100U;
        int pipe_idx = point_id_to_pipe_index(point_id);
        const device_pipe_cache_t *pipe;
        char buf[96];
        const char *state_text;
        const char *flow_text = "---";
        const char *pressure_text = "---";
        const char *btn_text;
        lv_color_t btn_color;
        char flow_buf[24];
        char pressure_buf[24];
        bool is_on;

        if (pipe_idx < 0 || node_id < 2000U || node_id > 2006U) {
            continue;
        }

        pipe = &s_pipe_cache[node_id - 2000U];
        is_on = pipe->valid && pipe->valve_on;
        g_valve_states[pipe_idx] = is_on;

        if (pipe->valid && pipe->flow_bound) {
            snprintf(flow_buf, sizeof(flow_buf), "%.2f", pipe->flow);
            flow_text = flow_buf;
        }
        if (pipe->valid && pipe->pressure_bound) {
            snprintf(pressure_buf, sizeof(pressure_buf), "%.2f", pipe->pressure);
            pressure_text = pressure_buf;
        }

        state_text = is_on ? "开启" : "关闭";
        btn_text = is_on ? "关闭" : "开启";
        btn_color = is_on ? lv_color_hex(0xE53935) : lv_color_hex(0x4CAF50);

        if (g_valve_confirm_states[i] == DEV_CONFIRM_PENDING) {
            state_text = "执行中...";
        } else if (g_valve_confirm_states[i] == DEV_CONFIRM_TIMEOUT) {
            state_text = "未确认";
        }

        if (g_valve_dynamic_state_labels[i]) {
            lv_label_set_text(g_valve_dynamic_state_labels[i], state_text);
        }
        if (g_valve_dynamic_flow_labels[i]) {
            lv_label_set_text_fmt(g_valve_dynamic_flow_labels[i], "流量:%s", flow_text);
        }
        if (g_valve_dynamic_pressure_labels[i]) {
            lv_label_set_text_fmt(g_valve_dynamic_pressure_labels[i], "压力:%s", pressure_text);
        }

        if (g_valve_dynamic_btns[i]) {
            lv_obj_set_style_bg_color(g_valve_dynamic_btns[i], btn_color, 0);
        }
        if (g_valve_dynamic_btn_labels[i]) {
            lv_label_set_text(g_valve_dynamic_btn_labels[i], btn_text);
        }
    }
}

static void refresh_dynamic_zone_switches(void)
{
    char info_buf[192];
    char name_buf[32];

    for (int i = 0; i < g_zone_dynamic_switch_count; i++) {
        uint32_t point_id = g_zone_dynamic_point_ids[i];
        lv_obj_t *sw = g_zone_dynamic_switches[i];
        bool checked = false;

        if (!sw) {
            continue;
        }

        if (point_id >= 300006U && point_id <= 300008U) {
            const device_tank_cache_t *tank = &s_tank_cache[point_id - 300006U];
            checked = tank->valid && tank->switch_on;
        } else {
            checked = valve_point_id_is_on(point_id);
        }

        if (g_zone_confirm_states[i] == DEV_CONFIRM_PENDING ||
            g_zone_confirm_states[i] == DEV_CONFIRM_TIMEOUT) {
            checked = g_zone_confirm_targets[i];
        }

        set_switch_checked(sw, checked);
    }

    for (int i = 0; i < g_zone_dynamic_switch_count; i++) {
        if (!g_zone_dynamic_info_labels[i]) {
            continue;
        }

        if (g_zone_confirm_states[i] == DEV_CONFIRM_PENDING) {
            lv_label_set_text(g_zone_dynamic_info_labels[i], "执行中...");
            continue;
        }
        if (g_zone_confirm_states[i] == DEV_CONFIRM_TIMEOUT) {
            lv_label_set_text(g_zone_dynamic_info_labels[i], "未确认");
            continue;
        }

        if (format_runtime_point_value(g_zone_dynamic_point_ids[i], info_buf, sizeof(info_buf))) {
            lv_label_set_text(g_zone_dynamic_info_labels[i], info_buf);
            continue;
        }

        zone_switch_title_for_point(g_zone_dynamic_point_ids[i], name_buf, sizeof(name_buf));
        lv_label_set_text_fmt(g_zone_dynamic_info_labels[i], "%s: ---", name_buf);
    }
}

static void refresh_dynamic_sensor_items(void)
{
    char value_buf[32];

    for (int i = 0; i < g_sensor_dynamic_item_count; i++) {
        if (!g_sensor_dynamic_value_labels[i]) {
            continue;
        }
        if (format_runtime_point_value(g_sensor_dynamic_point_ids[i], value_buf, sizeof(value_buf))) {
            lv_label_set_text(g_sensor_dynamic_value_labels[i], value_buf);
        } else {
            lv_label_set_text(g_sensor_dynamic_value_labels[i], "---");
        }
    }
}


void ui_device_create(lv_obj_t *parent)
{
    clear_view_object_refs();
    g_active_tab = 0;
    if (g_device_dialog) {
        lv_obj_del(g_device_dialog);
        g_device_dialog = NULL;
    }

    create_tab_buttons(parent);

    g_view_container = lv_obj_create(parent);
    lv_obj_set_size(g_view_container, 1168, 660);
    lv_obj_set_pos(g_view_container, 5, 70);
    lv_obj_set_style_bg_opa(g_view_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_view_container, 0, 0);
    lv_obj_set_style_pad_all(g_view_container, 0, 0);
    lv_obj_clear_flag(g_view_container, LV_OBJ_FLAG_SCROLLABLE);

    create_main_control_view(g_view_container);
}

static void clear_view_object_refs(void)
{
    g_left_panel = NULL;
    g_right_panel = NULL;
    g_valve_total_label = NULL;
    g_valve_open_label = NULL;
    g_valve_container = NULL;
    g_zone_container = NULL;
    g_sensor_container = NULL;
    for (int i = 0; i < 5; i++) {
        g_dev_status_labels[i] = NULL;
        g_dev_confirm_states[i] = DEV_CONFIRM_IDLE;
        g_dev_confirm_targets[i] = false;
        g_dev_confirm_deadlines[i] = 0;
    }
    for (int i = 0; i < 9; i++) {
        g_main_data_vals[i] = NULL;
    }
    for (int i = 0; i < 6; i++) {
        g_main_pipe_labels[i] = NULL;
    }
    for (int i = 0; i < UI_DEVICE_MAX_ZONE_SWITCHES; i++) {
        g_zone_dynamic_info_labels[i] = NULL;
        g_zone_dynamic_switches[i] = NULL;
        g_zone_dynamic_point_ids[i] = 0;
        g_zone_confirm_states[i] = DEV_CONFIRM_IDLE;
        g_zone_confirm_targets[i] = false;
        g_zone_confirm_deadlines[i] = 0;
    }
    g_zone_dynamic_switch_count = 0;
    for (int i = 0; i < UI_DEVICE_MAX_SENSOR_ITEMS; i++) {
        g_sensor_dynamic_value_labels[i] = NULL;
        g_sensor_dynamic_point_ids[i] = 0;
    }
    g_sensor_dynamic_item_count = 0;
    for (int i = 0; i < 7; i++) {
        g_valve_status_labels[i] = NULL;
        g_valve_btns[i] = NULL;
        g_valve_btn_labels[i] = NULL;
        g_sensor_pipe_labels[i] = NULL;
    }
    for (int i = 0; i < UI_DEVICE_MAX_VALVE_ROWS; i++) {
        g_valve_dynamic_state_labels[i] = NULL;
        g_valve_dynamic_flow_labels[i] = NULL;
        g_valve_dynamic_pressure_labels[i] = NULL;
        g_valve_dynamic_btns[i] = NULL;
        g_valve_dynamic_btn_labels[i] = NULL;
        g_valve_dynamic_point_ids[i] = 0;
        g_valve_confirm_states[i] = DEV_CONFIRM_IDLE;
        g_valve_confirm_targets[i] = false;
        g_valve_confirm_deadlines[i] = 0;
    }
    g_valve_dynamic_count = 0;
    for (int i = 0; i < 6; i++) {
        g_zone_field_info_labels[i] = NULL;
        g_zone_field_switches[i] = NULL;
        for (int j = 0; j < 6; j++) {
            g_sensor_field_labels[i][j] = NULL;
        }
    }
    for (int i = 0; i < 3; i++) {
        g_tank_info_labels[i] = NULL;
        g_tank_switches[i] = NULL;
    }

    g_pending_point_id = 0;
    g_pending_on = false;

    if (g_control_confirm_timer) {
        lv_timer_del(g_control_confirm_timer);
        g_control_confirm_timer = NULL;
    }
}


static void start_control_confirm(uint32_t point_id, bool target_on)
{
    if (point_id >= 300001U && point_id <= 300005U) {
        int idx = (int)(point_id - 300001U);
        if (idx >= 0 && idx < 5) {
            g_dev_confirm_states[idx] = DEV_CONFIRM_PENDING;
            g_dev_confirm_targets[idx] = target_on;
            g_dev_confirm_deadlines[idx] = lv_tick_get() + UI_DEVICE_CONTROL_CONFIRM_TIMEOUT_MS;
        }
    }

    for (int i = 0; i < g_valve_dynamic_count; i++) {
        if (g_valve_dynamic_point_ids[i] != point_id) {
            continue;
        }
        g_valve_confirm_states[i] = DEV_CONFIRM_PENDING;
        g_valve_confirm_targets[i] = target_on;
        g_valve_confirm_deadlines[i] = lv_tick_get() + UI_DEVICE_CONTROL_CONFIRM_TIMEOUT_MS;
    }

    for (int i = 0; i < g_zone_dynamic_switch_count; i++) {
        if (g_zone_dynamic_point_ids[i] != point_id) {
            continue;
        }
        g_zone_confirm_states[i] = DEV_CONFIRM_PENDING;
        g_zone_confirm_targets[i] = target_on;
        g_zone_confirm_deadlines[i] = lv_tick_get() + UI_DEVICE_CONTROL_CONFIRM_TIMEOUT_MS;
        if (g_zone_dynamic_switches[i]) {
            set_switch_checked(g_zone_dynamic_switches[i], target_on);
        }
    }

    if (!g_control_confirm_timer) {
        g_control_confirm_timer = lv_timer_create(control_confirm_timer_cb,
            UI_DEVICE_CONTROL_CONFIRM_TIMER_MS, NULL);
    }

    refresh_control_cards();
    refresh_dynamic_valve_items();
    refresh_dynamic_zone_switches();
}

static void resolve_control_confirm(uint32_t point_id, bool actual_on)
{
    if (point_id >= 300001U && point_id <= 300005U) {
        int idx = (int)(point_id - 300001U);
        if (idx >= 0 && idx < 5 &&
            (g_dev_confirm_states[idx] == DEV_CONFIRM_PENDING ||
             g_dev_confirm_states[idx] == DEV_CONFIRM_TIMEOUT) &&
            g_dev_confirm_targets[idx] == actual_on) {
            g_dev_confirm_states[idx] = DEV_CONFIRM_IDLE;
        }
    }

    for (int i = 0; i < g_valve_dynamic_count; i++) {
        if (g_valve_dynamic_point_ids[i] != point_id) {
            continue;
        }
        if ((g_valve_confirm_states[i] == DEV_CONFIRM_PENDING ||
             g_valve_confirm_states[i] == DEV_CONFIRM_TIMEOUT) &&
            g_valve_confirm_targets[i] == actual_on) {
            g_valve_confirm_states[i] = DEV_CONFIRM_IDLE;
        }
    }

    for (int i = 0; i < g_zone_dynamic_switch_count; i++) {
        if (g_zone_dynamic_point_ids[i] != point_id) {
            continue;
        }
        if ((g_zone_confirm_states[i] == DEV_CONFIRM_PENDING ||
             g_zone_confirm_states[i] == DEV_CONFIRM_TIMEOUT) &&
            g_zone_confirm_targets[i] == actual_on) {
            g_zone_confirm_states[i] = DEV_CONFIRM_IDLE;
        }
    }
}

static void set_switch_checked(lv_obj_t *sw, bool checked)
{
    if (!sw) {
        return;
    }

    g_syncing_switch_state = true;
    if (checked) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(sw, LV_STATE_CHECKED);
    }
    g_syncing_switch_state = false;
}

static void refresh_control_cards(void)
{
    for (int i = 0; i < 5; i++) {
        if (g_dev_status_labels[i]) {
            const char *text = g_dev_states[i] ? "开启" : "关闭";
            if (g_dev_confirm_states[i] == DEV_CONFIRM_PENDING) {
                text = "执行中...";
            } else if (g_dev_confirm_states[i] == DEV_CONFIRM_TIMEOUT) {
                text = "未确认";
            }
            lv_label_set_text(g_dev_status_labels[i], text);
        }
    }
}

static void control_confirm_timer_cb(lv_timer_t *timer)
{
    bool has_pending = false;
    uint32_t now = lv_tick_get();

    (void)timer;

    for (int i = 0; i < 5; i++) {
        if (g_dev_confirm_states[i] != DEV_CONFIRM_PENDING) {
            continue;
        }
        if ((int32_t)(now - g_dev_confirm_deadlines[i]) >= 0) {
            g_dev_confirm_states[i] = DEV_CONFIRM_TIMEOUT;
        } else {
            has_pending = true;
        }
    }

    for (int i = 0; i < g_valve_dynamic_count; i++) {
        if (g_valve_confirm_states[i] != DEV_CONFIRM_PENDING) {
            continue;
        }
        if ((int32_t)(now - g_valve_confirm_deadlines[i]) >= 0) {
            g_valve_confirm_states[i] = DEV_CONFIRM_TIMEOUT;
        } else {
            has_pending = true;
        }
    }

    for (int i = 0; i < g_zone_dynamic_switch_count; i++) {
        if (g_zone_confirm_states[i] != DEV_CONFIRM_PENDING) {
            continue;
        }
        if ((int32_t)(now - g_zone_confirm_deadlines[i]) >= 0) {
            g_zone_confirm_states[i] = DEV_CONFIRM_TIMEOUT;
        } else {
            has_pending = true;
        }
    }

    refresh_control_cards();
    refresh_dynamic_valve_items();
    refresh_dynamic_zone_switches();

    if (!has_pending && g_control_confirm_timer) {
        lv_timer_del(g_control_confirm_timer);
        g_control_confirm_timer = NULL;
    }
}

static void refresh_main_data_panel(void)
{
    static const char *unknown = "---";
    const device_pipe_cache_t *main_pipe = &s_pipe_cache[0];
    char buf[32];

    if (!g_main_data_vals[0] || !g_main_data_vals[1] || !g_main_data_vals[2] ||
        !g_main_data_vals[3] || !g_main_data_vals[4] || !g_main_data_vals[5] ||
        !g_main_data_vals[6] || !g_main_data_vals[7] || !g_main_data_vals[8]) {
        return;
    }

    lv_label_set_text(g_main_data_vals[0],
        (main_pipe->valid && main_pipe->valve_bound) ? (main_pipe->valve_on ? "开启" : "关闭") : unknown);

    if (main_pipe->valid && main_pipe->flow_bound) {
        snprintf(buf, sizeof(buf), "%.2f", main_pipe->flow);
        lv_label_set_text(g_main_data_vals[1], buf);
    } else {
        lv_label_set_text(g_main_data_vals[1], unknown);
    }

    if (main_pipe->valid && main_pipe->pressure_bound) {
        snprintf(buf, sizeof(buf), "%.2f", main_pipe->pressure);
        lv_label_set_text(g_main_data_vals[2], buf);
    } else {
        lv_label_set_text(g_main_data_vals[2], unknown);
    }

    for (int i = 0; i < 3; i++) {
        const device_tank_cache_t *tank = &s_tank_cache[i];
        int state_idx = 3 + i * 2;
        int level_idx = state_idx + 1;

        lv_label_set_text(g_main_data_vals[state_idx],
            (tank->valid) ? (tank->switch_on ? "开启" : "关闭") : unknown);

        if (tank->valid && tank->level_bound) {
            snprintf(buf, sizeof(buf), "%.1f", tank->level);
            lv_label_set_text(g_main_data_vals[level_idx], buf);
        } else {
            lv_label_set_text(g_main_data_vals[level_idx], unknown);
        }
    }

    for (int i = 0; i < 6; i++) {
        device_pipe_cache_t *pipe = &s_pipe_cache[i + 1];
        const char *flow_text = unknown;
        const char *pressure_text = unknown;
        char flow_buf[24];
        char pressure_buf[24];
        char summary[96];

        if (!g_main_pipe_labels[i]) {
            continue;
        }

        if (pipe->valid && pipe->flow_bound) {
            snprintf(flow_buf, sizeof(flow_buf), "%.2f", pipe->flow);
            flow_text = flow_buf;
        }
        if (pipe->valid && pipe->pressure_bound) {
            snprintf(pressure_buf, sizeof(pressure_buf), "%.2f", pipe->pressure);
            pressure_text = pressure_buf;
        }

        snprintf(summary, sizeof(summary), "阀:%s  流:%s  压:%s",
                 (pipe->valid && pipe->valve_bound) ? (pipe->valve_on ? "开" : "关") : unknown,
                 flow_text,
                 pressure_text);
        lv_label_set_text(g_main_pipe_labels[i], summary);
    }
}

static void refresh_valve_card(int valve_idx)
{
    char buf[96];
    device_pipe_cache_t *pipe;

    if (valve_idx < 0 || valve_idx >= 7) {
        return;
    }

    pipe = &s_pipe_cache[valve_idx];
    g_valve_states[valve_idx] = pipe->valid && pipe->valve_on;

    if (g_valve_status_labels[valve_idx]) {
        const char *flow_text = "---";
        const char *pressure_text = "---";
        char flow_buf[24];
        char pressure_buf[24];

        if (pipe->valid && pipe->flow_bound) {
            snprintf(flow_buf, sizeof(flow_buf), "%.2f", pipe->flow);
            flow_text = flow_buf;
        }
        if (pipe->valid && pipe->pressure_bound) {
            snprintf(pressure_buf, sizeof(pressure_buf), "%.2f", pipe->pressure);
            pressure_text = pressure_buf;
        }

        snprintf(buf, sizeof(buf), "%s  流量:%s  压力:%s",
                 g_valve_states[valve_idx] ? "开启" : "关闭",
                 flow_text,
                 pressure_text);
        lv_label_set_text(g_valve_status_labels[valve_idx], buf);
    }

    if (g_valve_btns[valve_idx]) {
        lv_obj_set_style_bg_color(g_valve_btns[valve_idx],
                                  g_valve_states[valve_idx] ? lv_color_hex(0xE53935) : lv_color_hex(0x4CAF50), 0);
    }
    if (g_valve_btn_labels[valve_idx]) {
        lv_label_set_text(g_valve_btn_labels[valve_idx], g_valve_states[valve_idx] ? "关闭" : "开启");
    }
}

static void refresh_zone_field_card(int field_idx)
{
    static const char *unknown = "---";
    device_field_cache_t *field;
    char n_buf[16], p_buf[16], k_buf[16], t_buf[16], h_buf[16], l_buf[16];
    const char *n_text = unknown;
    const char *p_text = unknown;
    const char *k_text = unknown;
    const char *t_text = unknown;
    const char *h_text = unknown;
    const char *l_text = unknown;
    bool valve_on;

    if (field_idx < 0 || field_idx >= 6) {
        return;
    }

    field = &s_field_cache[field_idx];
    valve_on = s_pipe_cache[field_idx + 1].valid && s_pipe_cache[field_idx + 1].valve_bound && s_pipe_cache[field_idx + 1].valve_on;

    if (field->valid && (field->registered_mask & (1U << 0))) {
        snprintf(n_buf, sizeof(n_buf), "%.1f", field->n);
        n_text = n_buf;
    }
    if (field->valid && (field->registered_mask & (1U << 1))) {
        snprintf(p_buf, sizeof(p_buf), "%.1f", field->p);
        p_text = p_buf;
    }
    if (field->valid && (field->registered_mask & (1U << 2))) {
        snprintf(k_buf, sizeof(k_buf), "%.1f", field->k);
        k_text = k_buf;
    }
    if (field->valid && (field->registered_mask & (1U << 3))) {
        snprintf(t_buf, sizeof(t_buf), "%.1f", field->temp);
        t_text = t_buf;
    }
    if (field->valid && (field->registered_mask & (1U << 4))) {
        snprintf(h_buf, sizeof(h_buf), "%.1f", field->humi);
        h_text = h_buf;
    }
    if (field->valid && (field->registered_mask & (1U << 5))) {
        snprintf(l_buf, sizeof(l_buf), "%.1f", field->light);
        l_text = l_buf;
    }

    if (g_zone_field_info_labels[field_idx]) {
        char buf[128];
        snprintf(buf, sizeof(buf), "N:%s P:%s K:%s\n温:%s 湿:%s 光:%s",
                 n_text, p_text, k_text, t_text, h_text, l_text);
        lv_label_set_text(g_zone_field_info_labels[field_idx], buf);
    }
    if (g_zone_field_switches[field_idx]) {
        set_switch_checked(g_zone_field_switches[field_idx], valve_on);
    }
}

static void refresh_tank_card(int tank_idx)
{
    device_tank_cache_t *tank;
    const char *level_text = "---";
    char level_buf[24];
    char buf[64];

    if (tank_idx < 0 || tank_idx >= 3) {
        return;
    }

    tank = &s_tank_cache[tank_idx];
    if (tank->valid && tank->level_bound) {
        snprintf(level_buf, sizeof(level_buf), "%.1f", tank->level);
        level_text = level_buf;
    }

    if (g_tank_info_labels[tank_idx]) {
        snprintf(buf, sizeof(buf), "液位: %sL  状态: %s",
                 level_text,
                 (tank->valid && tank->switch_on) ? "开启" : "关闭");
        lv_label_set_text(g_tank_info_labels[tank_idx], buf);
    }
    if (g_tank_switches[tank_idx]) {
        set_switch_checked(g_tank_switches[tank_idx], tank->valid && tank->switch_on);
    }
}

static void refresh_sensor_field_card(int field_idx)
{
    static const char *sensor_labels[] = {"N", "P", "K", "温", "湿", "光"};
    static const char *unknown = "---";
    const float values[6] = {
        s_field_cache[field_idx].n,
        s_field_cache[field_idx].p,
        s_field_cache[field_idx].k,
        s_field_cache[field_idx].temp,
        s_field_cache[field_idx].humi,
        s_field_cache[field_idx].light,
    };
    for (int i = 0; i < 6; i++) {
        char buf[32];
        const char *value_text = unknown;
        char value_buf[16];

        if (!g_sensor_field_labels[field_idx][i]) {
            continue;
        }
        if (s_field_cache[field_idx].valid && (s_field_cache[field_idx].registered_mask & (1U << i))) {
            snprintf(value_buf, sizeof(value_buf), "%.1f", values[i]);
            value_text = value_buf;
        }
        snprintf(buf, sizeof(buf), "%s:%s", sensor_labels[i], value_text);
        lv_label_set_text(g_sensor_field_labels[field_idx][i], buf);
    }
}

static void refresh_sensor_pipe_label(int pipe_idx)
{
    device_pipe_cache_t *pipe;
    const char *prefix;
    const char *flow_text = "---";
    const char *pressure_text = "---";
    char buf[96];
    char flow_buf[24];
    char pressure_buf[24];

    if (pipe_idx < 0 || pipe_idx >= 7 || !g_sensor_pipe_labels[pipe_idx]) {
        return;
    }

    pipe = &s_pipe_cache[pipe_idx];
    prefix = (pipe_idx == 0) ? "主" :
             (pipe_idx == 1) ? "P1" :
             (pipe_idx == 2) ? "P2" :
             (pipe_idx == 3) ? "P3" :
             (pipe_idx == 4) ? "P4" :
             (pipe_idx == 5) ? "P5" : "P6";

    if (pipe->valid && pipe->flow_bound) {
        snprintf(flow_buf, sizeof(flow_buf), "%.2f", pipe->flow);
        flow_text = flow_buf;
    }
    if (pipe->valid && pipe->pressure_bound) {
        snprintf(pressure_buf, sizeof(pressure_buf), "%.2f", pipe->pressure);
        pressure_text = pressure_buf;
    }

    snprintf(buf, sizeof(buf), "%s:阀%s 流%s 压%s",
             prefix,
             (pipe->valid && pipe->valve_bound) ? (pipe->valve_on ? "开" : "关") : "---",
             flow_text,
             pressure_text);
    lv_label_set_text(g_sensor_pipe_labels[pipe_idx], buf);
}

/**
 * @brief 创建顶部标签页按钮
 */
static void create_tab_buttons(lv_obj_t *parent)
{
    const char *tab_names[] = {"主机控制", "阀门控制", "灌区控制", "传感监测"};
    int btn_width = 150;
    int btn_height = 50;
    int x_start = 10;
    int y_pos = 10;

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, btn_width, btn_height);
        lv_obj_set_pos(btn, x_start + i * (btn_width + 10), y_pos);

        /* 第一个按钮默认选中 - 蓝色背景 */
        if (i == 0) {
            lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        }

        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);  /* 移除内边距 */
        lv_obj_add_event_cb(btn, tab_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        /* 按钮图标（暂用符号代替） */
        lv_obj_t *icon = lv_label_create(btn);
        if (i == 0) {
            lv_label_set_text(icon, LV_SYMBOL_SETTINGS);
        } else if (i == 1) {
            lv_label_set_text(icon, LV_SYMBOL_HOME);
        } else if (i == 2) {
            lv_label_set_text(icon, LV_SYMBOL_GPS);
        } else {
            lv_label_set_text(icon, LV_SYMBOL_EYE_OPEN);
        }
        lv_obj_set_style_text_font(icon, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(icon, i == 0 ? lv_color_white() : COLOR_PRIMARY, 0);
        lv_obj_align(icon, LV_ALIGN_CENTER, -28, 0);  /* 中心偏左 */

        /* 按钮文字 */
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, tab_names[i]);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, i == 0 ? lv_color_white() : lv_color_hex(0x333333), 0);
        lv_obj_align(label, LV_ALIGN_CENTER, 20, 0);  /* 中心偏右 */

        /* 保存按钮引用 */
        g_tab_buttons[i] = btn;
    }
}

/**
 * @brief 创建左侧控制面板
 */
static void create_control_panel(lv_obj_t *parent)
{
    /* 第一行：主水泵 + 施肥泵 */
    create_device_card(parent, "主水泵", 0, 0, true);
    create_device_card(parent, "施肥泵", 150, 0, true);

    /* 第二行：出肥阀 + 注水阀 + 搅拌机 */
    create_device_card(parent, "出肥阀", 0, 75, false);
    create_device_card(parent, "注水阀", 150, 75, false);
    create_device_card(parent, "搅拌机", 300, 75, false);
}

/**
 * @brief 设备卡片点击回调 - 弹出确认对话框
 * user_data = status_idx (0~4)
 */
static void device_card_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= 5) return;

    g_pending_point_id = s_dev_point_id_map[idx];
    g_pending_on = !g_dev_states[idx];

    show_device_confirm_dialog(s_dev_name_map[idx], g_pending_on);
}

/**
 * @brief 阀门按钮回调 - 弹出确认对话框（单按钮切换）
 * user_data = point_id
 */
static void valve_btn_cb(lv_event_t *e)
{
    uint32_t point_id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    char title_buf[32];

    if (point_id == 0) {
        return;
    }

    g_pending_point_id = point_id;
    g_pending_on = !valve_point_id_is_on(point_id);

    show_device_confirm_dialog(zone_switch_title_for_point(point_id, title_buf, sizeof(title_buf)), g_pending_on);
}

/**
 * @brief 创建设备卡片（带开关按钮）
 * @param parent 父对象
 * @param title 设备名称
 * @param x X坐标
 * @param y Y坐标
 * @param is_double 是否是双宽卡片（主水泵、施肥泵）
 */
static void create_device_card(lv_obj_t *parent, const char *title, int x, int y, bool is_double)
{
    int card_width = 140;
    int card_height = 65;

    int status_idx = -1;
    const char *icon_sym = LV_SYMBOL_SETTINGS;

    if (strcmp(title, "主水泵") == 0)       { status_idx = 0; icon_sym = LV_SYMBOL_CHARGE; }
    else if (strcmp(title, "施肥泵") == 0)  { status_idx = 1; icon_sym = LV_SYMBOL_CHARGE; }
    else if (strstr(title, "出肥阀"))      { status_idx = 2; icon_sym = LV_SYMBOL_DOWNLOAD; }
    else if (strstr(title, "注水阀"))      { status_idx = 3; icon_sym = LV_SYMBOL_DOWNLOAD; }
    else if (strstr(title, "搅拌机"))      { status_idx = 4; icon_sym = LV_SYMBOL_REFRESH; }

    /* 创建卡片容器 - 浅绿色背景，整体可点击 */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, card_width, card_height);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xd4edda), 0);  /* 浅绿色 */
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xc3e6cb), 0);
    lv_obj_set_style_radius(card, 5, 0);
    lv_obj_set_style_pad_all(card, 3, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);

    if (status_idx >= 0) {
        lv_obj_add_event_cb(card, device_card_click_cb, LV_EVENT_CLICKED, (void *)(intptr_t)status_idx);
    }

    /* 标题 - 左上角 */
    lv_obj_t *label_title = lv_label_create(card);
    lv_label_set_text(label_title, title);
    lv_obj_set_pos(label_title, 3, 3);
    lv_obj_set_style_text_font(label_title, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x333333), 0);

    /* 左下角：状态文字（根据当前状态显示） */
    lv_obj_t *label_status = lv_label_create(card);
    lv_label_set_text(label_status, (status_idx >= 0 && g_dev_states[status_idx]) ? "开启" : "关闭");
    lv_obj_set_pos(label_status, 3, 43);
    lv_obj_set_style_text_font(label_status, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_status, lv_color_hex(0x333333), 0);

    if (status_idx >= 0 && status_idx < 5) {
        g_dev_status_labels[status_idx] = label_status;
    }

    /* 右下角：图标（替代开关） */
    lv_obj_t *icon = lv_label_create(card);
    lv_label_set_text(icon, icon_sym);
    lv_obj_set_pos(icon, 110, 38);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x28a745), 0);
}

/**
 * @brief 创建右侧数据显示面板
 */
static void create_data_panel(lv_obj_t *parent)
{
    static const char *summary_labels[] = {
        "主管道阀门", "主管道流量(m³/h)", "主管道压力(MPa)",
        "N罐开关", "N罐液位(L)",
        "P罐开关", "P罐液位(L)",
        "K罐开关", "K罐液位(L)"
    };

    lv_obj_t *main_data_label = lv_label_create(parent);
    lv_label_set_text(main_data_label, "主机运行态");
    lv_obj_set_pos(main_data_label, 0, 0);
    lv_obj_set_style_text_font(main_data_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(main_data_label, lv_color_hex(0x333333), 0);

    lv_obj_t *summary_card = lv_obj_create(parent);
    lv_obj_set_size(summary_card, 650, 235);
    lv_obj_set_pos(summary_card, 0, 35);
    lv_obj_set_style_bg_color(summary_card, lv_color_hex(0xf7fbfd), 0);
    lv_obj_set_style_border_width(summary_card, 1, 0);
    lv_obj_set_style_border_color(summary_card, lv_color_hex(0xd8e6ee), 0);
    lv_obj_set_style_radius(summary_card, 10, 0);
    lv_obj_set_style_pad_all(summary_card, 12, 0);
    lv_obj_clear_flag(summary_card, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 9; i++) {
        int col = i / 3;
        int row = i % 3;
        int x = 15 + col * 205;
        int y = 15 + row * 66;

        lv_obj_t *label = lv_label_create(summary_card);
        lv_label_set_text(label, summary_labels[i]);
        lv_obj_set_pos(label, x, y);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);

        lv_obj_t *value = lv_label_create(summary_card);
        lv_label_set_text(value, "---");
        lv_obj_set_pos(value, x, y + 28);
        lv_obj_set_style_text_font(value, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(value, lv_color_hex(0x333333), 0);
        g_main_data_vals[i] = value;
    }

    lv_obj_t *pipe_title = lv_label_create(parent);
    lv_label_set_text(pipe_title, "副管道实时摘要");
    lv_obj_set_pos(pipe_title, 0, 290);
    lv_obj_set_style_text_font(pipe_title, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(pipe_title, lv_color_hex(0x333333), 0);

    lv_obj_t *pipe_panel = lv_obj_create(parent);
    lv_obj_set_size(pipe_panel, 650, 325);
    lv_obj_set_pos(pipe_panel, 0, 325);
    lv_obj_set_style_bg_color(pipe_panel, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(pipe_panel, 1, 0);
    lv_obj_set_style_border_color(pipe_panel, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(pipe_panel, 10, 0);
    lv_obj_set_style_pad_all(pipe_panel, 10, 0);
    lv_obj_set_style_pad_row(pipe_panel, 10, 0);
    lv_obj_set_style_pad_column(pipe_panel, 10, 0);
    lv_obj_set_flex_flow(pipe_panel, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_clear_flag(pipe_panel, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 6; i++) {
        char title_buf[24];
        lv_obj_t *card = lv_obj_create(pipe_panel);
        lv_obj_set_size(card, 305, 90);
        lv_obj_set_style_bg_color(card, lv_color_white(), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xd9e3ea), 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        snprintf(title_buf, sizeof(title_buf), "副管道%d", i + 1);
        lv_obj_t *title = lv_label_create(card);
        lv_label_set_text(title, title_buf);
        lv_obj_set_pos(title, 5, 5);
        lv_obj_set_style_text_font(title, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x333333), 0);

        lv_obj_t *info = lv_label_create(card);
        lv_label_set_text(info, "阀:---  流:---  压:---");
        lv_obj_set_pos(info, 5, 42);
        lv_obj_set_style_text_font(info, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(info, lv_color_hex(0x333333), 0);
        g_main_pipe_labels[i] = info;
    }
}

/**
 * @brief 创建主机控制视图（从 ui_device_create 中提取，懒加载用）
 */
static void create_main_control_view(lv_obj_t *parent)
{
    /* 左侧：控制面板 (4/10宽度) */
    int left_width = (int)(1168 * 0.4);  /* 约467px */
    g_left_panel = lv_obj_create(parent);
    lv_obj_set_size(g_left_panel, left_width, 660);
    lv_obj_set_pos(g_left_panel, 0, 0);
    lv_obj_set_style_bg_color(g_left_panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_left_panel, 0, 0);
    lv_obj_set_style_radius(g_left_panel, 10, 0);
    lv_obj_set_style_pad_all(g_left_panel, 15, 0);
    lv_obj_clear_flag(g_left_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 右侧：数据面板 (6/10宽度) */
    int right_width = (int)(1168 * 0.6);  /* 约701px */
    g_right_panel = lv_obj_create(parent);
    lv_obj_set_size(g_right_panel, right_width, 660);
    lv_obj_set_pos(g_right_panel, left_width + 5, 0);
    lv_obj_set_style_bg_color(g_right_panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_right_panel, 0, 0);
    lv_obj_set_style_radius(g_right_panel, 10, 0);
    lv_obj_set_style_pad_all(g_right_panel, 15, 0);
    lv_obj_clear_flag(g_right_panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建左侧控制面板内容 */
    create_control_panel(g_left_panel);

    /* 创建右侧数据显示面板内容 */
    create_data_panel(g_right_panel);

    refresh_control_cards();
    refresh_main_data_panel();
}

/**
 * @brief 懒加载切换视图：销毁旧视图，创建新视图
 */
static void switch_to_tab(int tab_index)
{
    if (!g_view_container) return;

    /* 清空视图容器（销毁当前视图的所有子对象） */
    lv_obj_clean(g_view_container);
    clear_view_object_refs();
    g_active_tab = tab_index;

    /* 根据选中的标签创建对应的视图 */
    if (tab_index == 0) {
        create_main_control_view(g_view_container);
    } else if (tab_index == 1) {
        create_valve_control_view(g_view_container);
    } else if (tab_index == 2) {
        create_zone_control_view(g_view_container);
    } else if (tab_index == 3) {
        create_sensor_monitor_view(g_view_container);
    }
}

/**
 * @brief 创建阀门控制视图 - 显示 7 个阀门（主管道+6副管道）
 */
static void create_valve_control_view(lv_obj_t *parent)
{
    ui_valve_row_t valve_rows[UI_DEVICE_MAX_VALVE_ROWS] = {0};
    int valve_count = g_get_valve_count_cb ? g_get_valve_count_cb() : 0;
    int fetch_count = 0;

    if (valve_count > UI_DEVICE_MAX_VALVE_ROWS) {
        valve_count = UI_DEVICE_MAX_VALVE_ROWS;
    }
    if (valve_count > 0 && g_get_valve_list_cb) {
        fetch_count = g_get_valve_list_cb(valve_rows, valve_count, 0);
        if (fetch_count < 0) {
            fetch_count = 0;
        }
    }

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 1168, 660);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 15, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label1 = lv_label_create(panel);
    lv_label_set_text(label1, "全部阀门个数: ");
    lv_obj_set_style_text_font(label1, &my_font_cn_16, 0);
    lv_obj_set_pos(label1, 10, 10);

    g_valve_total_label = lv_label_create(panel);
    lv_label_set_text_fmt(g_valve_total_label, "%d", fetch_count);
    lv_obj_set_style_text_font(g_valve_total_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(g_valve_total_label, lv_color_hex(0x2196F3), 0);
    lv_obj_set_pos(g_valve_total_label, 150, 10);

    lv_obj_t *label3 = lv_label_create(panel);
    lv_label_set_text(label3, "    开启阀门个数: ");
    lv_obj_set_style_text_font(label3, &my_font_cn_16, 0);
    lv_obj_set_pos(label3, 180, 10);

    g_valve_open_label = lv_label_create(panel);
    lv_label_set_text(g_valve_open_label, "0");
    lv_obj_set_style_text_font(g_valve_open_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(g_valve_open_label, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_pos(g_valve_open_label, 350, 10);

    g_valve_container = lv_obj_create(panel);
    lv_obj_set_size(g_valve_container, 1138, 580);
    lv_obj_set_pos(g_valve_container, 0, 50);
    lv_obj_set_style_bg_opa(g_valve_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_valve_container, 0, 0);
    lv_obj_set_style_pad_all(g_valve_container, 0, 0);
    lv_obj_set_flex_flow(g_valve_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_flex_main_place(g_valve_container, LV_FLEX_ALIGN_START, 0);
    lv_obj_set_style_pad_row(g_valve_container, 10, 0);
    lv_obj_set_style_pad_column(g_valve_container, 10, 0);

    for (int i = 0; i < fetch_count; i++) {
        uint32_t point_id = valve_row_to_point_id(&valve_rows[i]);
        bool valve_on = valve_point_id_is_on(point_id);
        lv_obj_t *card = lv_obj_create(g_valve_container);
        lv_obj_set_size(card, 260, 100);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xf0f8ff), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xd0e0f0), 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, valve_rows[i].name);
        lv_obj_set_style_text_font(name, &my_font_cn_16, 0);
        lv_obj_set_pos(name, 5, 5);

        lv_obj_t *state = lv_label_create(card);
        lv_label_set_text(state, valve_on ? "开启" : "关闭");
        lv_obj_set_style_text_font(state, &my_font_cn_16, 0);
        lv_obj_set_pos(state, 5, 40);
        g_valve_dynamic_state_labels[i] = state;

        lv_obj_t *flow = lv_label_create(card);
        lv_label_set_text(flow, "流量:---");
        lv_obj_set_style_text_font(flow, &my_font_cn_16, 0);
        lv_obj_set_pos(flow, 70, 40);
        g_valve_dynamic_flow_labels[i] = flow;

        lv_obj_t *pressure = lv_label_create(card);
        lv_label_set_text(pressure, "压力:---");
        lv_obj_set_style_text_font(pressure, &my_font_cn_16, 0);
        lv_obj_set_pos(pressure, 150, 40);
        g_valve_dynamic_pressure_labels[i] = pressure;
        g_valve_dynamic_point_ids[i] = point_id;

        lv_obj_t *btn = lv_btn_create(card);
        lv_obj_set_size(btn, 55, 26);
        lv_obj_set_pos(btn, 180, 5);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x4CAF50), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, valve_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)point_id);
        g_valve_dynamic_btns[i] = btn;

        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, "开启");
        lv_obj_set_style_text_font(btn_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
        lv_obj_center(btn_label);
        g_valve_dynamic_btn_labels[i] = btn_label;
    }

    g_valve_dynamic_count = fetch_count;
    refresh_dynamic_valve_items();
    update_valve_open_count();
}

/**
 * @brief 更新开启阀门计数
 */
static void update_valve_open_count(void)
{
    if (!g_valve_open_label) return;
    int count = 0;
    for (int i = 0; i < 7; i++) {
        if (g_valve_states[i]) count++;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", count);
    lv_label_set_text(g_valve_open_label, buf);
}

/**
 * @brief 确认对话框 - 确认按钮回调
 */
static void device_dialog_confirm_cb(lv_event_t *e)
{
    (void)e;

    if (g_pending_point_id != 0) {
        start_control_confirm(g_pending_point_id, g_pending_on);
    }

    if (g_device_control_cb && g_pending_point_id != 0) {
        g_device_control_cb(g_pending_point_id, g_pending_on);
    }

    if (g_device_dialog) {
        lv_obj_del(g_device_dialog);
        g_device_dialog = NULL;
    }
}

/**
 * @brief 确认对话框 - 取消按钮回调
 */
static void device_dialog_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (g_device_dialog) {
        lv_obj_del(g_device_dialog);
        g_device_dialog = NULL;
    }
}

/**
 * @brief 显示设备操作确认对话框（蓝色边框+白色内容，与首页自动化对话框同风格）
 * @param dev_name 设备名称
 * @param to_on    true=开启, false=关闭
 */
static void show_device_confirm_dialog(const char *dev_name, bool to_on)
{
    if (g_device_dialog) {
        lv_obj_del(g_device_dialog);
        g_device_dialog = NULL;
    }

    /* 外层蓝色背景（直角） */
    g_device_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_device_dialog, 630, 390);
    lv_obj_center(g_device_dialog);
    lv_obj_set_style_bg_color(g_device_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_device_dialog, 0, 0);
    lv_obj_set_style_radius(g_device_dialog, 0, 0);
    lv_obj_set_style_pad_all(g_device_dialog, 5, 0);
    lv_obj_clear_flag(g_device_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_device_dialog);
    lv_obj_set_size(content, 620, 380);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "操作确认");
    lv_obj_set_style_text_font(title, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* 提示文字 */
    char msg_buf[64];
    snprintf(msg_buf, sizeof(msg_buf), "确认%s「%s」?", to_on ? "开启" : "关闭", dev_name);
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, msg_buf);
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 取消按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 140, 50);
    lv_obj_set_pos(btn_cancel, 180, 300);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, device_dialog_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消");
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_center(label_cancel);

    /* 确认按钮（蓝色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 140, 50);
    lv_obj_set_pos(btn_confirm, 340, 300);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, device_dialog_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认");
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 灌区视图中的开关回调（直接发送业务点控制指令）
 * user_data = point_id
 */
static void zone_switch_cb(lv_event_t *e)
{
    uint32_t point_id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *sw = lv_event_get_target(e);
    bool on;

    if (g_syncing_switch_state) {
        return;
    }

    on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (point_id != 0) {
        start_control_confirm(point_id, on);
    }

    if (g_device_control_cb && point_id != 0) {
        g_device_control_cb(point_id, on);
    }
}

/**
 * @brief 创建灌区控制视图 - 显示 6 个田地灌区 + 3 个储料罐
 */
static void create_zone_control_view(lv_obj_t *parent)
{
    ui_zone_row_t zone_rows[UI_DEVICE_MAX_ZONE_ROWS] = {0};
    int zone_count = g_get_zone_count_cb ? g_get_zone_count_cb() : 0;
    int fetch_count = 0;

    if (zone_count > UI_DEVICE_MAX_ZONE_ROWS) {
        zone_count = UI_DEVICE_MAX_ZONE_ROWS;
    }
    if (zone_count > 0 && g_get_zone_list_cb) {
        fetch_count = g_get_zone_list_cb(zone_rows, zone_count, 0);
        if (fetch_count < 0) {
            fetch_count = 0;
        }
    }

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 1168, 660);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 15, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "灌区控制与储料罐状态");
    lv_obj_set_style_text_font(title, &my_font_cn_16, 0);
    lv_obj_set_pos(title, 10, 10);

    g_zone_container = lv_obj_create(panel);
    lv_obj_set_size(g_zone_container, 1138, 580);
    lv_obj_set_pos(g_zone_container, 0, 50);
    lv_obj_set_style_bg_opa(g_zone_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_zone_container, 0, 0);
    lv_obj_set_style_pad_all(g_zone_container, 0, 0);
    lv_obj_set_flex_flow(g_zone_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(g_zone_container, 10, 0);
    lv_obj_set_style_pad_column(g_zone_container, 10, 0);

    g_zone_dynamic_switch_count = 0;

    for (int i = 0; i < fetch_count && g_zone_dynamic_switch_count < UI_DEVICE_MAX_ZONE_SWITCHES; i++) {
        ui_zone_add_params_t detail = {0};
        lv_obj_t *card = lv_obj_create(g_zone_container);
        lv_obj_set_size(card, 360, 110);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xf0fff0), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xc3e6cb), 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, zone_rows[i].name);
        lv_obj_set_style_text_font(name, &my_font_cn_16, 0);
        lv_obj_set_pos(name, 5, 5);

        lv_obj_t *summary = lv_label_create(card);
        lv_label_set_text_fmt(summary, "阀门:%d  设备:%d", zone_rows[i].valve_count, zone_rows[i].device_count);
        lv_obj_set_style_text_font(summary, &my_font_cn_16, 0);
        lv_obj_set_pos(summary, 5, 32);

        if (g_get_zone_detail_cb && g_get_zone_detail_cb(zone_rows[i].slot_index, &detail) && detail.valve_count > 0) {
            uint32_t point_id = detail.valve_ids[0];

            if (point_id != 0) {
                int idx = g_zone_dynamic_switch_count++;
                lv_obj_t *info = lv_label_create(card);
                lv_label_set_text(info, "---");
                lv_obj_set_style_text_font(info, &my_font_cn_16, 0);
                lv_obj_set_pos(info, 5, 62);
                g_zone_dynamic_info_labels[idx] = info;
                g_zone_dynamic_point_ids[idx] = point_id;

                lv_obj_t *sw = lv_switch_create(card);
                lv_obj_set_size(sw, 45, 22);
                lv_obj_set_pos(sw, 295, 5);
                lv_obj_add_event_cb(sw, zone_switch_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)point_id);
                g_zone_dynamic_switches[idx] = sw;
            }
        }
    }

    for (int i = 0; i < 3 && g_zone_dynamic_switch_count < UI_DEVICE_MAX_ZONE_SWITCHES; i++) {
        uint32_t point_id = 300006U + (uint32_t)i;
        int idx = g_zone_dynamic_switch_count++;
        static const char *tank_names[] = {"储料罐N", "储料罐P", "储料罐K"};

        lv_obj_t *card = lv_obj_create(g_zone_container);
        lv_obj_set_size(card, 360, 80);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xfff8e1), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xf0e0a0), 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, tank_names[i]);
        lv_obj_set_style_text_font(name, &my_font_cn_16, 0);
        lv_obj_set_pos(name, 5, 5);

        lv_obj_t *info = lv_label_create(card);
        lv_label_set_text(info, "---");
        lv_obj_set_style_text_font(info, &my_font_cn_16, 0);
        lv_obj_set_pos(info, 5, 40);
        g_zone_dynamic_info_labels[idx] = info;
        g_zone_dynamic_point_ids[idx] = point_id;

        lv_obj_t *sw = lv_switch_create(card);
        lv_obj_set_size(sw, 45, 22);
        lv_obj_set_pos(sw, 295, 5);
        lv_obj_add_event_cb(sw, zone_switch_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)point_id);
        g_zone_dynamic_switches[idx] = sw;
    }

    refresh_dynamic_zone_switches();
}

/**
 * @brief 创建传感监测视图 - 显示所有在线传感器实时值
 */
static void create_sensor_monitor_view(lv_obj_t *parent)
{
    ui_sensor_row_t sensor_rows[UI_DEVICE_MAX_SENSOR_ITEMS] = {0};
    int sensor_count = g_get_sensor_count_cb ? g_get_sensor_count_cb() : 0;
    int fetch_count = 0;

    if (sensor_count > UI_DEVICE_MAX_SENSOR_ITEMS) {
        sensor_count = UI_DEVICE_MAX_SENSOR_ITEMS;
    }
    if (sensor_count > 0 && g_get_sensor_list_cb) {
        fetch_count = g_get_sensor_list_cb(sensor_rows, sensor_count, 0);
        if (fetch_count < 0) {
            fetch_count = 0;
        }
    }

    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 1168, 660);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 15, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "传感器实时监测");
    lv_obj_set_style_text_font(title, &my_font_cn_16, 0);
    lv_obj_set_pos(title, 10, 10);

    g_sensor_container = lv_obj_create(panel);
    lv_obj_set_size(g_sensor_container, 1138, 590);
    lv_obj_set_pos(g_sensor_container, 0, 45);
    lv_obj_set_style_bg_opa(g_sensor_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_sensor_container, 0, 0);
    lv_obj_set_style_pad_all(g_sensor_container, 0, 0);
    lv_obj_set_flex_flow(g_sensor_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(g_sensor_container, 8, 0);
    lv_obj_set_style_pad_column(g_sensor_container, 8, 0);

    g_sensor_dynamic_item_count = 0;

    for (int i = 0; i < fetch_count && i < UI_DEVICE_MAX_SENSOR_ITEMS; i++) {
        lv_obj_t *card = lv_obj_create(g_sensor_container);
        lv_obj_set_size(card, 360, 80);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xf5f5f5), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xe0e0e0), 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, sensor_rows[i].name);
        lv_obj_set_style_text_font(name, &my_font_cn_16, 0);
        lv_obj_set_pos(name, 5, 5);

        lv_obj_t *meta = lv_label_create(card);
        lv_label_set_text_fmt(meta, "%s | %s", sensor_type_name(sensor_rows[i].type), sensor_rows[i].parent_name);
        lv_obj_set_style_text_font(meta, &my_font_cn_16, 0);
        lv_obj_set_pos(meta, 5, 32);

        lv_obj_t *value = lv_label_create(card);
        lv_label_set_text(value, "---");
        lv_obj_set_style_text_font(value, &my_font_cn_16, 0);
        lv_obj_set_pos(value, 250, 24);
        g_sensor_dynamic_value_labels[g_sensor_dynamic_item_count] = value;
        g_sensor_dynamic_point_ids[g_sensor_dynamic_item_count] = sensor_rows[i].point_id;
        g_sensor_dynamic_item_count++;
    }

    refresh_dynamic_sensor_items();
}

/**
 * @brief 标签页按钮回调
 */
static void tab_btn_cb(lv_event_t *e)
{
    int tab_index = (int)(intptr_t)lv_event_get_user_data(e);

    /* 更新所有标签按钮的颜色和文字颜色 */
    for (int i = 0; i < 4; i++) {
        if (g_tab_buttons[i]) {
            if (i == tab_index) {
                /* 选中：蓝色背景 */
                lv_obj_set_style_bg_color(g_tab_buttons[i], COLOR_PRIMARY, 0);

                /* 更新图标和文字颜色为白色 */
                lv_obj_t *icon = lv_obj_get_child(g_tab_buttons[i], 0);
                lv_obj_t *label = lv_obj_get_child(g_tab_buttons[i], 1);
                if (icon) lv_obj_set_style_text_color(icon, lv_color_white(), 0);
                if (label) lv_obj_set_style_text_color(label, lv_color_white(), 0);
            } else {
                /* 未选中：白色背景 */
                lv_obj_set_style_bg_color(g_tab_buttons[i], lv_color_white(), 0);

                /* 更新图标和文字颜色为深色 */
                lv_obj_t *icon = lv_obj_get_child(g_tab_buttons[i], 0);
                lv_obj_t *label = lv_obj_get_child(g_tab_buttons[i], 1);
                if (icon) lv_obj_set_style_text_color(icon, COLOR_PRIMARY, 0);
                if (label) lv_obj_set_style_text_color(label, lv_color_hex(0x333333), 0);
            }
        }
    }

    /* 懒加载切换视图 */
    switch_to_tab(tab_index);
}

/* ---- 公开 API ---- */

void ui_device_update_control(bool water_pump, bool fert_pump,
    bool fert_valve, bool water_valve, bool mixer)
{
    bool new_states[5] = {water_pump, fert_pump, fert_valve, water_valve, mixer};

    for (int i = 0; i < 5; i++) {
        g_dev_states[i] = new_states[i];
        resolve_control_confirm(s_dev_point_id_map[i], new_states[i]);
    }

    refresh_control_cards();
    refresh_dynamic_sensor_items();
}

void ui_device_update_pipe(int pipe_id,
    bool valve_bound, bool valve_on,
    bool flow_bound, float flow,
    bool pressure_bound, float pressure)
{
    device_pipe_cache_t *pipe;
    uint32_t point_id;

    if (pipe_id < 0 || pipe_id >= 7) {
        return;
    }

    pipe = &s_pipe_cache[pipe_id];
    pipe->valid = true;
    pipe->valve_bound = valve_bound;
    pipe->valve_on = valve_on;
    pipe->flow_bound = flow_bound;
    pipe->flow = flow;
    pipe->pressure_bound = pressure_bound;
    pipe->pressure = pressure;

    point_id = ((uint32_t)pipe_id + 2000U) * 100U + 1U;
    resolve_control_confirm(point_id, valve_on);

    refresh_main_data_panel();
    refresh_valve_card(pipe_id);
    refresh_dynamic_valve_items();
    update_valve_open_count();
    if (pipe_id >= 1 && pipe_id <= 6) {
        refresh_zone_field_card(pipe_id - 1);
    }
    refresh_dynamic_zone_switches();
    refresh_sensor_pipe_label(pipe_id);
    refresh_dynamic_sensor_items();
}

void ui_device_update_tank(int tank_id, bool switch_on, bool level_bound, float level)
{
    device_tank_cache_t *tank;
    uint32_t point_id;

    if (tank_id < 1 || tank_id > 3) {
        return;
    }

    tank = &s_tank_cache[tank_id - 1];
    tank->valid = true;
    tank->switch_on = switch_on;
    tank->level_bound = level_bound;
    tank->level = level;

    point_id = 300006U + (uint32_t)(tank_id - 1);
    resolve_control_confirm(point_id, switch_on);

    refresh_main_data_panel();
    refresh_tank_card(tank_id - 1);
    refresh_dynamic_zone_switches();
    refresh_dynamic_sensor_items();
}

void ui_device_update_field(int field_id, uint8_t registered_mask,
    float n, float p, float k, float temp, float humi, float light)
{
    device_field_cache_t *field;

    if (field_id < 1 || field_id > 6) {
        return;
    }

    field = &s_field_cache[field_id - 1];
    field->valid = true;
    field->registered_mask = registered_mask;
    field->n = n;
    field->p = p;
    field->k = k;
    field->temp = temp;
    field->humi = humi;
    field->light = light;

    refresh_zone_field_card(field_id - 1);
    refresh_sensor_field_card(field_id - 1);
    refresh_dynamic_sensor_items();
}

void ui_device_invalidate_objects(void)
{
    if (g_device_dialog) {
        lv_obj_del(g_device_dialog);
        g_device_dialog = NULL;
    }
    g_view_container = NULL;
    clear_view_object_refs();
}

void ui_device_register_query_cbs(
    ui_get_valve_count_cb_t  valve_count_cb,
    ui_get_valve_list_cb_t   valve_list_cb,
    ui_get_sensor_count_cb_t sensor_count_cb,
    ui_get_sensor_list_cb_t  sensor_list_cb,
    ui_get_zone_count_cb_t   zone_count_cb,
    ui_get_zone_list_cb_t    zone_list_cb,
    ui_get_zone_detail_cb_t  zone_detail_cb)
{
    g_get_valve_count_cb = valve_count_cb;
    g_get_valve_list_cb = valve_list_cb;
    g_get_sensor_count_cb = sensor_count_cb;
    g_get_sensor_list_cb = sensor_list_cb;
    g_get_zone_count_cb = zone_count_cb;
    g_get_zone_list_cb = zone_list_cb;
    g_get_zone_detail_cb = zone_detail_cb;
}

void ui_device_register_control_cb(ui_device_control_cb_t cb)
{
    g_device_control_cb = cb;
}
