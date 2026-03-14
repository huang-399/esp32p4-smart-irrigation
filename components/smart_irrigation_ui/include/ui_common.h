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
 * @brief 关闭程序页面的日历弹窗（如果存在）
 */
void ui_program_close_calendar(void);

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
 * @brief 从NVS加载程序和配方数据（在ui_init之前调用）
 */
void ui_program_store_init(void);

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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UI_COMMON_H */