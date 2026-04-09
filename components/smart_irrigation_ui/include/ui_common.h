/**
 * @file ui_common.h
 * @brief 智慧种植园监控系统 - UI公共定义
 *
 * 屏幕分辨率: 1280 x 800
 * 图形库: LVGL 9.x
 */

#ifndef UI_COMMON_H
#define UI_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/

/* 屏幕尺寸 */
#define SCREEN_WIDTH                1280
#define SCREEN_HEIGHT               800

/* 主框架布局 */
#define BORDER_WIDTH                5     /* 蓝色边框宽度 */
#define GAP_WIDTH                   5     /* 侧边栏和内容区之间的间隙 */
#define SIDEBAR_WIDTH               87
#define SIDEBAR_HEIGHT              735   /* 800 - 5(上边框) - 60(底部) */

#define SIDEBAR_RADIUS              10    /* 侧边栏圆角 */

#define CONTENT_X                   97    /* 5(边框) + 87(侧边栏) + 5(间隙) */
#define CONTENT_Y                   5     /* 上边框 */
#define CONTENT_WIDTH               1178  /* 1280 - 5(左边框) - 87(侧边栏) - 5(间隙) - 5(右边框) */
#define CONTENT_HEIGHT              735   /* 800 - 5(上边框) - 60(底部) */
#define CONTENT_RADIUS              10    /* 内容区圆角 */

#define BOTTOM_BAR_Y                745   /* 再下移5px */
#define BOTTOM_BAR_WIDTH            1270  /* 1280 - 5*2 */
#define BOTTOM_BAR_HEIGHT           50    /* 55 - 5 = 50 */

/* 颜色定义 */
#define COLOR_PRIMARY               lv_color_hex(0x3498db)  /* 主色蓝 */
#define COLOR_DARK_BG               lv_color_hex(0x2c3e50)  /* 深蓝背景 */
#define COLOR_LIGHT_BG              lv_color_hex(0xecf5f8)  /* 淡蓝背景 */
#define COLOR_SUCCESS               lv_color_hex(0x27ae60)  /* 成功/在线 */
#define COLOR_WARNING               lv_color_hex(0xf39c12)  /* 警告 */
#define COLOR_ERROR                 lv_color_hex(0xe74c3c)  /* 错误 */
#define COLOR_TEXT_MAIN             lv_color_hex(0x2c3e50)  /* 主文字 */
#define COLOR_TEXT_WHITE            lv_color_hex(0xffffff)  /* 白色文字 */
#define COLOR_TEXT_GRAY             lv_color_hex(0x7f8c8d)  /* 灰色文字 */

/* 字体大小 */
#define FONT_SIZE_TITLE             24  /* 标题 */
#define FONT_SIZE_NORMAL            20  /* 正文 */
#define FONT_SIZE_LABEL             18  /* 标签 */
#define FONT_SIZE_SMALL             16  /* 辅助 */

/* 自定义字体声明 */
LV_FONT_DECLARE(my_font_cn_16);
LV_FONT_DECLARE(my_fontbd_16);  /* 粗体字体 */

/**
 * @brief dropdown下拉列表打开时设置中文字体的回调
 *
 * LVGL v9中，dropdown的list在打开时通过LV_EVENT_READY事件允许设置样式。
 * 用此回调替代旧的 lv_dropdown_get_list() + set_style 模式。
 */
static inline void ui_dropdown_list_font_cb(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    lv_obj_t *list = lv_dropdown_get_list(dd);
    if(list) {
        lv_obj_set_style_text_font(list, &my_font_cn_16, 0);
    }
}

/* 导航项定义 */
typedef enum {
    NAV_HOME = 0,       /* 首页 */
    NAV_PROGRAM,        /* 程序 */
    NAV_DEVICE,         /* 设备 */
    NAV_LOG,            /* 日志 */
    NAV_SETTINGS,       /* 设置 */
    NAV_MAINTENANCE,    /* 维护 */
    NAV_MAX
} nav_item_t;

/**********************
 *      TYPEDEFS
 **********************/

/* UI主结构体 */
typedef struct {
    lv_obj_t *screen;           /* 主屏幕 */
    lv_obj_t *sidebar;          /* 左侧导航栏 */
    lv_obj_t *content;          /* 主内容区 */
    lv_obj_t *statusbar;        /* 底部状态栏 */
    nav_item_t current_nav;     /* 当前导航项 */
} ui_main_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * @brief 初始化UI系统
 */
void ui_init(void);

/**
 * @brief 获取UI主结构体
 * @return UI主结构体指针
 */
ui_main_t* ui_get_main(void);

/**
 * @brief 切换导航页面
 * @param nav 导航项
 */
void ui_switch_nav(nav_item_t nav);

/**
 * @brief 创建首页界面
 * @param parent 父容器
 */
void ui_home_create(lv_obj_t *parent);

/**
 * @brief 关闭首页弹窗（如果存在）
 */
void ui_home_close_dialog(void);
void ui_home_invalidate_objects(void);

/**
 * @brief 创建程序管理界面
 * @param parent 父容器
 */
void ui_program_create(lv_obj_t *parent);

/**
 * @brief 创建设备控制界面
 * @param parent 父容器
 */
void ui_device_create(lv_obj_t *parent);
void ui_device_invalidate_objects(void);

/**
 * @brief 创建日志界面
 * @param parent 父容器
 */
void ui_log_create(lv_obj_t *parent);

/**
 * @brief 创建设置界面
 * @param parent 父容器
 */
void ui_settings_create(lv_obj_t *parent);

/**
 * @brief 创建维护界面
 * @param parent 父容器
 */
void ui_maintenance_create(lv_obj_t *parent);

/**
 * @brief 关闭日志页面的日历弹窗（如果存在）
 */
void ui_log_close_calendar(void);

/**
 * @brief 关闭设置页面的屏幕级弹窗（如果存在）
 */
void ui_settings_close_overlays(void);

/**
 * @brief 关闭程序页面的日历弹窗（如果存在）
 */
void ui_program_close_calendar(void);

/**
 * @brief 关闭程序页面的屏幕级弹窗（如果存在）
 */
void ui_program_close_overlays(void);

/**
 * @brief 关闭程序页面的灌区选择对话框（如果存在）
 */
void ui_program_close_zone_dialog(void);

/**
 * @brief 显示数字键盘
 * @param textarea 目标输入框对象
 * @param parent 键盘显示的父容器（通常是主屏幕）
 */
void ui_numpad_show(lv_obj_t *textarea, lv_obj_t *parent);

/**
 * @brief 关闭数字键盘
 */
void ui_numpad_close(void);

/**
 * @brief 检查键盘是否正在显示
 * @return true if visible, false otherwise
 */
bool ui_numpad_is_visible(void);

/**
 * @brief 显示告警管理对话框（全屏弹窗）
 * @param parent 父容器（通常是主屏幕）
 */
void ui_alarm_show(lv_obj_t *parent);

/**
 * @brief 关闭告警管理对话框
 */
void ui_alarm_close(void);

/**
 * @brief 检查告警对话框是否正在显示
 * @return true if visible, false otherwise
 */
bool ui_alarm_is_visible(void);

/**
 * @brief 初始化程序/配方存储
 */
void ui_program_store_init(void);

/**
 * @brief 获取程序数量
 * @return 程序数量
 */
int ui_program_get_count(void);

/**
 * @brief 获取指定索引的程序名称
 * @param index 程序索引
 * @return 程序名称指针（如果索引无效返回NULL）
 */
const char* ui_program_get_name(int index);

/**
 * @brief 获取指定索引的程序合计时长
 * @param index 程序索引
 * @return 合计时长（分钟）
 */
int ui_program_get_duration(int index);

/**
 * @brief 获取指定索引的程序关联配方
 * @param index 程序索引
 * @return 关联配方名称指针（如果索引无效返回NULL）
 */
const char* ui_program_get_formula(int index);

/**
 * @brief 获取指定索引的程序启动条件文本
 * @param index 程序索引
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void ui_program_get_condition_text(int index, char *buf, int buf_size);

/**
 * @brief 获取指定索引的程序下次启动文本
 * @param index 程序索引
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void ui_program_get_next_start_text(int index, char *buf, int buf_size);

/**
 * @brief 获取指定索引的程序启动时段摘要
 * @param index 程序索引
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 */
void ui_program_get_period_text(int index, char *buf, int buf_size);

/* ---- 灌溉调度桥接回调 ---- */

typedef bool (*ui_program_auto_mode_set_cb_t)(bool enabled);
typedef bool (*ui_program_auto_mode_get_cb_t)(void);
typedef bool (*ui_program_start_cb_t)(int index);

typedef struct {
    int pre_water;
    int post_water;
    int total_duration;
    char formula[32];
} ui_manual_irrigation_request_t;

typedef bool (*ui_manual_irrigation_start_cb_t)(const ui_manual_irrigation_request_t *req);
typedef void (*ui_home_runtime_refresh_cb_t)(void);
typedef int (*ui_home_zone_field_resolve_cb_t)(int slot_index);

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
} ui_irrigation_runtime_status_t;

typedef bool (*ui_irrigation_status_get_cb_t)(ui_irrigation_runtime_status_t *out);

void ui_home_register_auto_mode_set_cb(ui_program_auto_mode_set_cb_t cb);
void ui_home_register_auto_mode_get_cb(ui_program_auto_mode_get_cb_t cb);
void ui_home_register_program_start_cb(ui_program_start_cb_t cb);
void ui_home_register_manual_irrigation_start_cb(ui_manual_irrigation_start_cb_t cb);
void ui_home_register_irrigation_status_get_cb(ui_irrigation_status_get_cb_t cb);
void ui_home_register_runtime_refresh_cb(ui_home_runtime_refresh_cb_t cb);

/**
 * @brief 更新底部状态栏WiFi连接状态
 * @param connected true=已连接, false=未连接
 */
void ui_statusbar_set_wifi_connected(bool connected);

/**
 * @brief 更新底部状态栏时间显示
 * @param time_str 时间字符串，如 "15:41:48\n2026/01/12 周一" 或 "--:--:--\n----/--/-- --"
 */
void ui_statusbar_set_time(const char *time_str);

/* ---- Zigbee 设备数据更新（由 main.c 在 LVGL 线程中调用） ---- */

void ui_home_update_field(int field_id, uint8_t registered_mask,
    float n, float p, float k, float temp, float humi, float light);
void ui_home_update_pipe(int pipe_id,
    bool valve_bound, bool valve_on,
    bool flow_bound, float flow,
    bool pressure_bound, float pressure);
void ui_home_update_tank(int tank_id, bool switch_on, bool level_bound, float level);
void ui_home_update_mixer(bool on);
void ui_home_update_control(bool water_pump, bool fert_pump,
    bool fert_valve, bool water_valve, bool mixer);
void ui_home_update_zigbee_status(bool online, int frame_count);

/* ---- 设备页真实数据更新（由 main.c 在 LVGL 线程中调用） ---- */

void ui_device_update_control(bool water_pump, bool fert_pump,
    bool fert_valve, bool water_valve, bool mixer);
void ui_device_update_pipe(int pipe_id,
    bool valve_bound, bool valve_on,
    bool flow_bound, float flow,
    bool pressure_bound, float pressure);
void ui_device_update_tank(int tank_id, bool switch_on, bool level_bound, float level);
void ui_device_update_field(int field_id, uint8_t registered_mask,
    float n, float p, float k, float temp, float humi, float light);

/* ---- 设备控制回调（UI → main.c → zigbee_bridge） ---- */

typedef void (*ui_device_control_cb_t)(uint32_t point_id, bool on);
void ui_device_register_control_cb(ui_device_control_cb_t cb);

/* ---- 传感器搜索回调（设置页面） ---- */

typedef struct {
    char     name[32];
    char     type_name[16];
    uint32_t point_id;
} ui_sensor_found_item_t;

typedef void (*ui_search_sensor_cb_t)(void);
void ui_settings_register_search_sensor_cb(ui_search_sensor_cb_t cb);
void ui_settings_update_search_results(const ui_sensor_found_item_t *items, int count);

/* ---- 设备注册表回调类型（UI → main.c → device_registry） ---- */

/* 添加参数结构体 */
typedef struct {
    uint8_t  type;       /* dev_type_t */
    uint8_t  port;       /* dev_port_t */
    uint16_t id;
    char     name[32];
} ui_device_add_params_t;

typedef struct {
    uint8_t  type;       /* dev_type_t */
    uint8_t  port;       /* dev_port_t */
    char     name[32];
} ui_device_edit_params_t;

typedef struct {
    uint8_t  type;       /* valve_type_t */
    uint8_t  channel;
    uint16_t parent_device_id;
    char     name[32];
} ui_valve_add_params_t;

typedef struct {
    uint8_t  type;       /* sensor_type_t */
    uint8_t  point_no;   /* 点位号 */
    uint16_t parent_device_id;
    uint32_t point_id;
    char     name[32];
} ui_sensor_add_params_t;

typedef struct {
    uint8_t  type;       /* sensor_type_t */
    char     name[32];
} ui_sensor_edit_params_t;


/* 添加/删除/编辑回调类型 */
typedef bool (*ui_device_add_cb_t)(const ui_device_add_params_t *params);
typedef bool (*ui_valve_add_cb_t)(const ui_valve_add_params_t *params);
typedef bool (*ui_sensor_add_cb_t)(const ui_sensor_add_params_t *params);
typedef bool (*ui_device_delete_cb_t)(uint16_t device_id);
typedef bool (*ui_valve_delete_cb_t)(uint32_t valve_id);
typedef bool (*ui_sensor_delete_cb_t)(uint32_t point_id);
typedef bool (*ui_device_edit_cb_t)(uint16_t device_id, const ui_device_edit_params_t *params);
typedef bool (*ui_valve_edit_cb_t)(uint32_t valve_id, const ui_valve_add_params_t *params);
typedef bool (*ui_sensor_edit_cb_t)(uint32_t point_id, const ui_sensor_edit_params_t *params);

/* 查重回调类型 */
typedef bool (*ui_is_name_taken_cb_t)(const char *name);
typedef bool (*ui_is_device_id_taken_cb_t)(uint16_t id);

/* 表格行数据结构 */
typedef struct {
    uint8_t  type;
    uint8_t  port;
    uint16_t id;
    char     name[32];
} ui_device_row_t;

typedef struct {
    uint8_t  type;
    uint8_t  channel;
    uint32_t id;
    uint16_t parent_device_id;
    char     name[32];
    char     parent_name[32];
} ui_valve_row_t;

typedef struct {
    uint8_t  type;
    uint8_t  point_no;
    uint16_t parent_device_id;
    uint32_t point_id;
    char     name[32];
    char     parent_name[32];
} ui_sensor_row_t;

/* 查询回调类型 */
typedef int  (*ui_get_device_count_cb_t)(void);
typedef int  (*ui_get_device_list_cb_t)(ui_device_row_t *out, int max, int offset);
typedef int  (*ui_get_valve_count_cb_t)(void);
typedef int  (*ui_get_valve_list_cb_t)(ui_valve_row_t *out, int max, int offset);
typedef int  (*ui_get_sensor_count_cb_t)(void);
typedef int  (*ui_get_sensor_list_cb_t)(ui_sensor_row_t *out, int max, int offset);
typedef int  (*ui_get_device_dropdown_cb_t)(char *buf, int buf_size);
typedef int  (*ui_get_valve_parent_dropdown_cb_t)(char *buf, int buf_size);
typedef bool (*ui_is_sensor_added_cb_t)(uint32_t point_id);
typedef uint8_t (*ui_next_sensor_point_no_cb_t)(uint16_t parent_device_id);

/* 从下拉框选项解析出设备 ID */
typedef uint16_t (*ui_parse_device_id_cb_t)(int dropdown_index);
typedef uint16_t (*ui_parse_valve_parent_device_id_cb_t)(int dropdown_index);

/* 注册函数 */
void ui_settings_register_device_add_cb(ui_device_add_cb_t cb);
void ui_settings_register_valve_add_cb(ui_valve_add_cb_t cb);
void ui_settings_register_sensor_add_cb(ui_sensor_add_cb_t cb);
void ui_settings_register_device_delete_cb(ui_device_delete_cb_t cb);
void ui_settings_register_valve_delete_cb(ui_valve_delete_cb_t cb);
void ui_settings_register_sensor_delete_cb(ui_sensor_delete_cb_t cb);
void ui_settings_register_device_edit_cb(ui_device_edit_cb_t cb);
void ui_settings_register_valve_edit_cb(ui_valve_edit_cb_t cb);
void ui_settings_register_sensor_edit_cb(ui_sensor_edit_cb_t cb);

/* 查重回调注册 */
void ui_settings_register_device_name_check_cb(ui_is_name_taken_cb_t cb);
void ui_settings_register_device_id_check_cb(ui_is_device_id_taken_cb_t cb);
void ui_settings_register_valve_name_check_cb(ui_is_name_taken_cb_t cb);
void ui_settings_register_sensor_name_check_cb(ui_is_name_taken_cb_t cb);
void ui_settings_register_zone_name_check_cb(ui_is_name_taken_cb_t cb);

void ui_settings_register_query_cbs(
    ui_get_device_count_cb_t     dev_count_cb,
    ui_get_device_list_cb_t      dev_list_cb,
    ui_get_valve_count_cb_t      valve_count_cb,
    ui_get_valve_list_cb_t       valve_list_cb,
    ui_get_sensor_count_cb_t     sensor_count_cb,
    ui_get_sensor_list_cb_t      sensor_list_cb,
    ui_get_device_dropdown_cb_t  dev_dropdown_cb,
    ui_get_valve_parent_dropdown_cb_t valve_parent_dropdown_cb,
    ui_is_sensor_added_cb_t      is_added_cb,
    ui_next_sensor_point_no_cb_t next_point_no_cb,
    ui_parse_device_id_cb_t      parse_id_cb,
    ui_parse_valve_parent_device_id_cb_t parse_valve_parent_id_cb
);

/* 刷新表格（添加/删除后调用） */
void ui_settings_refresh_device_table(void);
void ui_settings_refresh_valve_table(void);
void ui_settings_refresh_sensor_table(void);
void ui_settings_refresh_zone_table(void);

/* ---- 灌区管理回调类型 ---- */

typedef struct {
    int      slot_index;
    char     name[32];
    uint8_t  valve_count;
    uint8_t  device_count;
    char     valve_names[128];
    char     device_names[128];
} ui_zone_row_t;

typedef struct {
    char     name[32];
    uint8_t  valve_count;
    uint8_t  device_count;
    uint32_t valve_ids[16];
    uint16_t device_ids[8];
} ui_zone_add_params_t;

typedef bool (*ui_zone_add_cb_t)(const ui_zone_add_params_t *params);
typedef bool (*ui_zone_delete_cb_t)(int slot_index);
typedef bool (*ui_zone_edit_cb_t)(int slot_index, const ui_zone_add_params_t *params);
typedef int  (*ui_get_zone_count_cb_t)(void);
typedef int  (*ui_get_zone_list_cb_t)(ui_zone_row_t *out, int max, int offset);
typedef bool (*ui_get_zone_detail_cb_t)(int slot_index, ui_zone_add_params_t *out);

void ui_home_register_zone_query_cbs(
    ui_get_zone_count_cb_t count_cb,
    ui_get_zone_list_cb_t list_cb,
    ui_get_zone_detail_cb_t detail_cb);
void ui_home_register_zone_field_resolve_cb(ui_home_zone_field_resolve_cb_t cb);

void ui_settings_register_zone_add_cb(ui_zone_add_cb_t cb);
void ui_settings_register_zone_delete_cb(ui_zone_delete_cb_t cb);
void ui_settings_register_zone_edit_cb(ui_zone_edit_cb_t cb);
void ui_settings_register_zone_query_cbs(
    ui_get_zone_count_cb_t  count_cb,
    ui_get_zone_list_cb_t   list_cb,
    ui_get_zone_detail_cb_t detail_cb);

/* ---- 程序页灌区选择查询回调 ---- */
void ui_program_register_selection_query_cbs(
    ui_get_valve_count_cb_t valve_count_cb,
    ui_get_valve_list_cb_t  valve_list_cb,
    ui_get_zone_count_cb_t  zone_count_cb,
    ui_get_zone_list_cb_t   zone_list_cb,
    ui_get_zone_detail_cb_t zone_detail_cb);

/* ---- 设备页查询回调 ---- */
void ui_device_register_query_cbs(
    ui_get_valve_count_cb_t  valve_count_cb,
    ui_get_valve_list_cb_t   valve_list_cb,
    ui_get_sensor_count_cb_t sensor_count_cb,
    ui_get_sensor_list_cb_t  sensor_list_cb,
    ui_get_zone_count_cb_t   zone_count_cb,
    ui_get_zone_list_cb_t    zone_list_cb,
    ui_get_zone_detail_cb_t  zone_detail_cb);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UI_COMMON_H */