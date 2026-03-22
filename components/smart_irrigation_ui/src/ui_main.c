/**
 * @file ui_main.c
 * @brief 智慧种植园监控系统 - 主框架实现
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include "ui_keyboard.h"
#include "ui_alarm.h"
#include "ui_wifi.h"
#include <stdio.h>

/*********************
 *  STATIC VARIABLES
 *********************/
static ui_main_t g_ui_main = {0};

/* 导航按钮数组 */
static lv_obj_t *nav_btns[NAV_MAX] = {NULL};

/* 底部状态栏标签 */
static lv_obj_t *g_connected_label = NULL;
static lv_obj_t *g_time_label = NULL;

/* 第一个按钮的底部遮盖层（用于实现只有顶部圆角） */
static lv_obj_t *first_btn_cover = NULL;

/* 导航项名称 */
static const char *nav_names[NAV_MAX] = {
    "首页",
    "程序",
    "设备",
    "日志",
    "设置",
    "维护"
};

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_sidebar(void);
static void create_content(void);
static void create_statusbar(void);
static void nav_btn_event_cb(lv_event_t *e);
static void parallelogram_draw_event_cb(lv_event_t *e);
static void alarm_btn_click_cb(lv_event_t *e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 初始化UI系统
 */
void ui_init(void)
{
    /* 创建主屏幕 - 深蓝色背景 */
    g_ui_main.screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_ui_main.screen, lv_color_hex(0x0d47a1), 0);  /* 深蓝色 */
    lv_obj_clear_flag(g_ui_main.screen, LV_OBJ_FLAG_SCROLLABLE);  /* 禁止主屏幕滚动 */

    /* 创建各个区域 */
    create_sidebar();
    create_content();
    create_statusbar();

    /* 加载主屏幕 */
    lv_scr_load(g_ui_main.screen);

    /* 默认选中首页 */
    g_ui_main.current_nav = NAV_HOME;
    ui_switch_nav(NAV_HOME);
}

/**
 * @brief 获取UI主结构体
 */
ui_main_t* ui_get_main(void)
{
    return &g_ui_main;
}

/**
 * @brief 切换导航页面
 */
void ui_switch_nav(nav_item_t nav)
{
    if (nav >= NAV_MAX) return;

    g_ui_main.current_nav = nav;

    /* 关闭可能打开的弹窗和键盘 */
    ui_home_close_dialog();        /* 关闭首页的手动灌溉/程序弹窗 */
    ui_log_close_calendar();       /* 关闭日志页面的日历弹窗 */
    ui_program_close_calendar();   /* 关闭程序页面的日历弹窗 */
    ui_program_close_overlays();   /* 关闭程序页面的屏幕级弹窗 */
    ui_program_close_zone_dialog(); /* 关闭程序页面的灌区选择对话框 */
    ui_settings_close_overlays();  /* 关闭设置页面的屏幕级弹窗 */
    ui_wifi_close_overlays();      /* 关闭WiFi相关对话框和键盘 */
    ui_keyboard_close();           /* 关闭26键软键盘 */
    ui_numpad_close();             /* 关闭数字键盘 */

    /* 更新导航按钮状态 */
    for (int i = 0; i < NAV_MAX; i++) {
        if (nav_btns[i]) {
            if (i == nav) {
                /* 选中：浅蓝色背景 */
                lv_obj_set_style_bg_color(nav_btns[i], lv_color_hex(0xc8e5f5), 0);
            } else {
                /* 未选中：白色背景 */
                lv_obj_set_style_bg_color(nav_btns[i], lv_color_white(), 0);
            }
        }
    }

    /* 更新第一个按钮的遮盖层颜色 */
    if (first_btn_cover) {
        if (nav == NAV_HOME) {
            /* 首页选中时，遮盖层也是浅蓝色 */
            lv_obj_set_style_bg_color(first_btn_cover, lv_color_hex(0xc8e5f5), 0);
        } else {
            /* 首页未选中时，遮盖层是白色 */
            lv_obj_set_style_bg_color(first_btn_cover, lv_color_white(), 0);
        }
    }

    /* Invalidate WiFi widget pointers before destroying content
       to prevent async scan callbacks from accessing freed memory */
    ui_wifi_invalidate_objects();

    /* Invalidate home page pointers before destroying content
       to prevent zigbee async callbacks from accessing freed memory */
    ui_home_invalidate_objects();

    /* Invalidate device page pointers before destroying content
       to prevent zigbee async callbacks from accessing freed memory */
    ui_device_invalidate_objects();

    /* 清空内容区 */
    lv_obj_clean(g_ui_main.content);

    /* 根据导航项加载不同页面 */
    switch (nav) {
        case NAV_HOME:
            ui_home_create(g_ui_main.content);
            break;

        case NAV_PROGRAM:
            ui_program_create(g_ui_main.content);
            break;

        case NAV_DEVICE:
            ui_device_create(g_ui_main.content);
            break;

        case NAV_LOG:
            ui_log_create(g_ui_main.content);
            break;

        case NAV_SETTINGS:
            ui_settings_create(g_ui_main.content);
            break;

        case NAV_MAINTENANCE:
            ui_maintenance_create(g_ui_main.content);
            break;

        default:
            break;
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 创建左侧导航栏
 */
static void create_sidebar(void)
{
    /* 创建侧边栏容器 - 白色背景，圆角 */
    g_ui_main.sidebar = lv_obj_create(g_ui_main.screen);
    lv_obj_set_size(g_ui_main.sidebar, SIDEBAR_WIDTH, SIDEBAR_HEIGHT);
    lv_obj_set_pos(g_ui_main.sidebar, BORDER_WIDTH, BORDER_WIDTH);
    lv_obj_set_style_bg_color(g_ui_main.sidebar, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_ui_main.sidebar, 0, 0);
    lv_obj_set_style_radius(g_ui_main.sidebar, SIDEBAR_RADIUS, 0);  /* 圆角 */
    lv_obj_set_style_pad_all(g_ui_main.sidebar, 0, 0);
    lv_obj_clear_flag(g_ui_main.sidebar, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建导航按钮 - 从顶部开始，每个按钮高度约 100px */
    int btn_height = 100;
    for (int i = 0; i < NAV_MAX; i++) {
        lv_obj_t *btn = lv_obj_create(g_ui_main.sidebar);
        lv_obj_set_size(btn, SIDEBAR_WIDTH, btn_height);
        lv_obj_set_pos(btn, 0, i * btn_height);
        lv_obj_set_style_bg_color(btn, lv_color_white(), 0);  /* 默认白色 */
        lv_obj_set_style_border_width(btn, 0, 0);

        /* 第一个按钮：设置全圆角，底部圆角由 bottom_cover 遮盖实现仅顶部圆角效果 */
        if (i == 0) {
            lv_obj_set_style_radius(btn, SIDEBAR_RADIUS, 0);
        } else {
            lv_obj_set_style_radius(btn, 0, 0);  /* 其他按钮没有圆角 */
        }

        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

        /* 添加点击事件 */
        lv_obj_add_event_cb(btn, nav_btn_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        /* 添加文字（暂时只有文字，稍后可添加图标） */
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, nav_names[i]);
        lv_obj_set_style_text_color(label, COLOR_TEXT_MAIN, 0);  /* 深色文字 */
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);  /* 自定义中文字体 */
        lv_obj_center(label);

        nav_btns[i] = btn;
    }

    /* 为第一个按钮底部添加一个遮盖层，遮住底部圆角 */
    lv_obj_t *bottom_cover = lv_obj_create(g_ui_main.sidebar);
    lv_obj_set_size(bottom_cover, SIDEBAR_WIDTH, btn_height / 2 + 5);
    lv_obj_set_pos(bottom_cover, 0, btn_height / 2 - 5);
    lv_obj_set_style_bg_color(bottom_cover, lv_color_hex(0xc8e5f5), 0);  /* 默认选中颜色 */
    lv_obj_set_style_border_width(bottom_cover, 0, 0);
    lv_obj_set_style_radius(bottom_cover, 0, 0);  /* 无圆角 */
    lv_obj_clear_flag(bottom_cover, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(bottom_cover, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_background(bottom_cover);  /* 移到第一个按钮后面 */

    first_btn_cover = bottom_cover;  /* 保存引用以便后续更新颜色 */

    /* 底部添加用户头像图标区域 */
    lv_obj_t *user_icon = lv_obj_create(g_ui_main.screen);
    lv_obj_set_size(user_icon, 60, 60);
    lv_obj_set_pos(user_icon, BORDER_WIDTH + 13, BOTTOM_BAR_Y + BORDER_WIDTH);
    lv_obj_set_style_bg_color(user_icon, lv_color_white(), 0);
    lv_obj_set_style_border_width(user_icon, 2, 0);
    lv_obj_set_style_border_color(user_icon, lv_color_white(), 0);
    lv_obj_set_style_radius(user_icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(user_icon, LV_OBJ_FLAG_SCROLLABLE);

    /* 用户图标中的文字 */
    lv_obj_t *user_label = lv_label_create(user_icon);
    lv_label_set_text(user_label, "U");
    lv_obj_set_style_text_color(user_label, COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(user_label, &lv_font_montserrat_28, 0);
    lv_obj_center(user_label);
}

/**
 * @brief 创建主内容区
 */
static void create_content(void)
{
    g_ui_main.content = lv_obj_create(g_ui_main.screen);
    lv_obj_set_size(g_ui_main.content, CONTENT_WIDTH, CONTENT_HEIGHT);
    lv_obj_set_pos(g_ui_main.content, CONTENT_X, CONTENT_Y);
    lv_obj_set_style_bg_color(g_ui_main.content, lv_color_hex(0xe8e8e8), 0);  /* 浅灰色 */
    lv_obj_set_style_border_width(g_ui_main.content, 0, 0);
    lv_obj_set_style_radius(g_ui_main.content, CONTENT_RADIUS, 0);  /* 圆角 */
    lv_obj_set_style_pad_all(g_ui_main.content, 0, 0);
    lv_obj_clear_flag(g_ui_main.content, LV_OBJ_FLAG_SCROLLABLE);  /* 禁止滚动 */
}

/**
 * @brief 创建底部状态栏（直接放置元素，不用容器）
 */
static void create_statusbar(void)
{
    /* 底部区域已经是蓝色背景（主屏幕背景），直接添加元素 */

    /* 中间：系统名称 */
    lv_obj_t *system_label = lv_label_create(g_ui_main.screen);
    lv_label_set_text(system_label, "智能灌溉控制中心");
    lv_obj_set_style_text_color(system_label, COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(system_label, &my_fontbd_16, 0);  /* 粗体中文字体 */
    lv_obj_set_pos(system_label, 150, 755);

    /* 中间偏右：状态按钮组 */
    int btn_y = 754;  /* 底部状态栏Y位置 */
    int btn_width = 120;  /* 增加按钮宽度 */
    int btn_height = 35;
    int btn_spacing = -17;  /* 按钮间距为-17px，产生较大重叠效果 */
    int start_x = 380;  /* 起始X位置 */

    /* 创建梯形背景容器（包含三个按钮的大梯形） */
    int bg_width = btn_width * 2 + (btn_width + btn_spacing) * 2 + 10;  /* 背景宽度 */
    int bg_height = btn_height + 4;  /* 背景高度 */
    int bg_x = start_x - 5;  /* 背景起始X位置 */
    int bg_y = btn_y - 2;  /* 背景起始Y位置 */

    lv_obj_t *btn_bg = lv_obj_create(g_ui_main.screen);
    lv_obj_set_size(btn_bg, bg_width, bg_height);
    lv_obj_set_pos(btn_bg, bg_x, bg_y);
    lv_obj_set_style_bg_color(btn_bg, lv_color_hex(0x2565a8), 0);  /* 介于蓝色和深蓝之间 */
    lv_obj_set_style_bg_opa(btn_bg, LV_OPA_TRANSP, 0);  /* 背景透明，用自定义绘制 */
    lv_obj_set_style_border_width(btn_bg, 0, 0);
    lv_obj_set_style_radius(btn_bg, 0, 0);
    lv_obj_set_style_pad_all(btn_bg, 0, 0);
    lv_obj_set_style_shadow_width(btn_bg, 0, 0);
    lv_obj_clear_flag(btn_bg, LV_OBJ_FLAG_SCROLLABLE);
    /* 添加自定义绘制事件，用户数据设为3表示这是背景梯形 */
    lv_obj_add_event_cb(btn_bg, parallelogram_draw_event_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void*)3);

    /* 告警按钮 */
    lv_obj_t *alarm_btn = lv_obj_create(g_ui_main.screen);
    lv_obj_set_size(alarm_btn, btn_width, btn_height);
    lv_obj_set_pos(alarm_btn, start_x, btn_y);
    lv_obj_set_style_bg_color(alarm_btn, lv_color_hex(0x42a5f5), 0);  /* 中蓝色 */
    lv_obj_set_style_bg_opa(alarm_btn, LV_OPA_TRANSP, 0);  /* 背景透明，用自定义绘制 */
    lv_obj_set_style_border_width(alarm_btn, 0, 0);
    lv_obj_set_style_radius(alarm_btn, 0, 0);
    lv_obj_set_style_pad_all(alarm_btn, 0, 0);
    lv_obj_set_style_shadow_width(alarm_btn, 0, 0);
    lv_obj_clear_flag(alarm_btn, LV_OBJ_FLAG_SCROLLABLE);
    /* 添加自定义绘制事件，用户数据设为1表示需要圆角 */
    lv_obj_add_event_cb(alarm_btn, parallelogram_draw_event_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void*)1);
    /* 添加点击事件 */
    lv_obj_add_event_cb(alarm_btn, alarm_btn_click_cb, LV_EVENT_CLICKED, NULL);

    /* 告警图标 */
    lv_obj_t *alarm_icon = lv_label_create(alarm_btn);
    lv_label_set_text(alarm_icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_color(alarm_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(alarm_icon, &my_font_cn_16, 0);
    lv_obj_align(alarm_icon, LV_ALIGN_LEFT_MID, 20, 0);

    /* 告警文字 */
    lv_obj_t *alarm_label = lv_label_create(alarm_btn);
    lv_label_set_text(alarm_label, "告警");
    lv_obj_set_style_text_color(alarm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(alarm_label, &my_font_cn_16, 0);
    lv_obj_align(alarm_label, LV_ALIGN_LEFT_MID, 42, 0);  /* 紧挨图标 */

    /* 已连接按钮 */
    lv_obj_t *connected_btn = lv_obj_create(g_ui_main.screen);
    lv_obj_set_size(connected_btn, btn_width, btn_height);
    lv_obj_set_pos(connected_btn, start_x + btn_width + btn_spacing, btn_y);
    lv_obj_set_style_bg_color(connected_btn, lv_color_hex(0x42a5f5), 0);  /* 中蓝色 */
    lv_obj_set_style_bg_opa(connected_btn, LV_OPA_TRANSP, 0);  /* 背景透明 */
    lv_obj_set_style_border_width(connected_btn, 0, 0);
    lv_obj_set_style_radius(connected_btn, 0, 0);
    lv_obj_set_style_pad_all(connected_btn, 0, 0);
    lv_obj_set_style_shadow_width(connected_btn, 0, 0);
    lv_obj_clear_flag(connected_btn, LV_OBJ_FLAG_SCROLLABLE);
    /* 添加自定义绘制事件，用户数据设为0表示不需要圆角 */
    lv_obj_add_event_cb(connected_btn, parallelogram_draw_event_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void*)0);

    /* 连接图标 */
    lv_obj_t *connected_icon = lv_label_create(connected_btn);
    lv_label_set_text(connected_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(connected_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(connected_icon, &my_font_cn_16, 0);
    lv_obj_align(connected_icon, LV_ALIGN_LEFT_MID, 20, 0);

    /* 已连接文字 */
    g_connected_label = lv_label_create(connected_btn);
    lv_label_set_text(g_connected_label, "未连接");
    lv_obj_set_style_text_color(g_connected_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(g_connected_label, &my_font_cn_16, 0);
    lv_obj_align(g_connected_label, LV_ALIGN_LEFT_MID, 42, 0);  /* 紧挨图标 */

    /* 无运行按钮 */
    lv_obj_t *idle_btn = lv_obj_create(g_ui_main.screen);
    lv_obj_set_size(idle_btn, btn_width * 2, btn_height);  /* 宽度改为2倍 */
    lv_obj_set_pos(idle_btn, start_x + (btn_width + btn_spacing) * 2, btn_y);
    lv_obj_set_style_bg_color(idle_btn, lv_color_hex(0x42a5f5), 0);  /* 中蓝色 */
    lv_obj_set_style_bg_opa(idle_btn, LV_OPA_TRANSP, 0);  /* 背景透明 */
    lv_obj_set_style_border_width(idle_btn, 0, 0);
    lv_obj_set_style_radius(idle_btn, 0, 0);
    lv_obj_set_style_pad_all(idle_btn, 0, 0);
    lv_obj_set_style_shadow_width(idle_btn, 0, 0);
    lv_obj_clear_flag(idle_btn, LV_OBJ_FLAG_SCROLLABLE);
    /* 添加自定义绘制事件，用户数据设为2表示只有右下角圆角 */
    lv_obj_add_event_cb(idle_btn, parallelogram_draw_event_cb, LV_EVENT_DRAW_MAIN_BEGIN, (void*)2);

    /* 无运行文字 */
    lv_obj_t *idle_label = lv_label_create(idle_btn);
    lv_label_set_text(idle_label, "无运行");
    lv_obj_set_style_text_color(idle_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(idle_label, &my_font_cn_16, 0);
    lv_obj_center(idle_label);

    /* 右侧：时间日期 */
    g_time_label = lv_label_create(g_ui_main.screen);
    lv_label_set_text(g_time_label, "--:--:--\n----/--/-- --");
    lv_obj_set_size(g_time_label, 165, 40);
    lv_obj_set_style_text_color(g_time_label, COLOR_TEXT_WHITE, 0);
    lv_obj_set_style_text_font(g_time_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_time_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_long_mode(g_time_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_pos(g_time_label, 1100, 755);
}

/**
 * @brief 导航按钮点击事件
 */
static void nav_btn_event_cb(lv_event_t *e)
{
    nav_item_t nav = (nav_item_t)(intptr_t)lv_event_get_user_data(e);
    ui_switch_nav(nav);
}

/**
 * @brief 平行四边形按钮自定义绘制事件
 */
static void parallelogram_draw_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_DRAW_MAIN_BEGIN) {
        lv_layer_t *layer = lv_event_get_layer(e);

        /* 获取按钮区域 */
        lv_area_t obj_coords;
        lv_obj_get_coords(obj, &obj_coords);

        int skew = 20;  /* 倾斜偏移量 */

        /* 获取用户数据，判断按钮类型 */
        intptr_t user_data = (intptr_t)lv_event_get_user_data(e);

        lv_point_precise_t p1, p2, p3, p4;

        if (user_data == 3) {
            /* 背景大梯形：对称梯形 (上窄下宽) */
            p1.x = obj_coords.x1 + skew;     /* 左上 (向右偏移) */
            p1.y = obj_coords.y1;
            p2.x = obj_coords.x2 - skew;     /* 右上 (向左偏移) */
            p2.y = obj_coords.y1;
            p3.x = obj_coords.x2;            /* 右下 (不偏移) */
            p3.y = obj_coords.y2;
            p4.x = obj_coords.x1;            /* 左下 (不偏移) */
            p4.y = obj_coords.y2;
        } else if (user_data == 2) {
            /* 无运行按钮：对称梯形 (上窄下宽，左右对称倾斜) */
            p1.x = obj_coords.x1 + skew;     /* 左上 (向右偏移) */
            p1.y = obj_coords.y1;
            p2.x = obj_coords.x2 - skew;     /* 右上 (向左偏移) */
            p2.y = obj_coords.y1;
            p3.x = obj_coords.x2;            /* 右下 (不偏移) */
            p3.y = obj_coords.y2;
            p4.x = obj_coords.x1;            /* 左下 (不偏移) */
            p4.y = obj_coords.y2;
        } else {
            /* 告警和已连接按钮：平行四边形 (向右下倾斜 - 左高右低) */
            p1.x = obj_coords.x1 + skew;     /* 左上 (向右偏移) */
            p1.y = obj_coords.y1;
            p2.x = obj_coords.x2;            /* 右上 (不偏移) */
            p2.y = obj_coords.y1;
            p3.x = obj_coords.x2 - skew;     /* 右下 (向左偏移) */
            p3.y = obj_coords.y2;
            p4.x = obj_coords.x1;            /* 左下 (不偏移) */
            p4.y = obj_coords.y2;
        }

        /* 获取按钮的背景颜色 */
        lv_color_t bg_color = lv_obj_get_style_bg_color(obj, LV_PART_MAIN);

        /* 如果是背景梯形(user_data==3)，只绘制填充，不绘制边框 */
        if (user_data == 3) {
            /* 绘制背景梯形填充 */
            lv_draw_triangle_dsc_t tri_dsc;
            lv_draw_triangle_dsc_init(&tri_dsc);
            tri_dsc.color = bg_color;
            tri_dsc.opa = LV_OPA_90;

            tri_dsc.p[0] = p1;
            tri_dsc.p[1] = p2;
            tri_dsc.p[2] = p3;
            lv_draw_triangle(layer, &tri_dsc);

            tri_dsc.p[0] = p1;
            tri_dsc.p[1] = p3;
            tri_dsc.p[2] = p4;
            lv_draw_triangle(layer, &tri_dsc);
        } else {
            /* 其他按钮：绘制边框+填充 */
            /* 边框颜色：介于深蓝和蓝色之间 */
            lv_color_t border_color = lv_color_hex(0x1e5a8e);
            int border_width = 2;  /* 边框宽度 */

            /* 先绘制外层边框（稍大的形状） */
            lv_point_precise_t bp1, bp2, bp3, bp4;  /* border points */

            if (user_data == 2) {
                /* 无运行按钮：对称梯形边框 */
                bp1.x = p1.x - border_width;
                bp1.y = p1.y - border_width;
                bp2.x = p2.x + border_width;
                bp2.y = p2.y - border_width;
                bp3.x = p3.x + border_width;
                bp3.y = p3.y + border_width;
                bp4.x = p4.x - border_width;
                bp4.y = p4.y + border_width;
            } else {
                /* 平行四边形边框 */
                bp1.x = p1.x - border_width;
                bp1.y = p1.y - border_width;
                bp2.x = p2.x + border_width;
                bp2.y = p2.y - border_width;
                bp3.x = p3.x + border_width;
                bp3.y = p3.y + border_width;
                bp4.x = p4.x - border_width;
                bp4.y = p4.y + border_width;
            }

            /* 绘制边框三角形1 */
            lv_draw_triangle_dsc_t border_dsc;
            lv_draw_triangle_dsc_init(&border_dsc);
            border_dsc.color = border_color;
            border_dsc.opa = LV_OPA_90;

            border_dsc.p[0] = bp1;
            border_dsc.p[1] = bp2;
            border_dsc.p[2] = bp3;
            lv_draw_triangle(layer, &border_dsc);

            /* 绘制边框三角形2 */
            border_dsc.p[0] = bp1;
            border_dsc.p[1] = bp3;
            border_dsc.p[2] = bp4;
            lv_draw_triangle(layer, &border_dsc);

            /* 绘制内部填充三角形1: 左上 -> 右上 -> 右下 */
            lv_draw_triangle_dsc_t tri_dsc;
            lv_draw_triangle_dsc_init(&tri_dsc);
            tri_dsc.color = bg_color;
            tri_dsc.opa = LV_OPA_90;

            tri_dsc.p[0] = p1;
            tri_dsc.p[1] = p2;
            tri_dsc.p[2] = p3;
            lv_draw_triangle(layer, &tri_dsc);

            /* 绘制内部填充三角形2: 左上 -> 右下 -> 左下 */
            tri_dsc.p[0] = p1;
            tri_dsc.p[1] = p3;
            tri_dsc.p[2] = p4;
            lv_draw_triangle(layer, &tri_dsc);
        }
    }
}

/**
 * @brief 告警按钮点击回调
 */
static void alarm_btn_click_cb(lv_event_t *e)
{
    (void)e;
    /* 显示告警管理对话框 */
    ui_alarm_show(g_ui_main.screen);
}

void ui_statusbar_set_wifi_connected(bool connected)
{
    if (g_connected_label) {
        lv_label_set_text(g_connected_label, connected ? "已连接" : "未连接");
    }
}

void ui_statusbar_set_time(const char *time_str)
{
    if (g_time_label && time_str) {
        lv_label_set_text(g_time_label, time_str);
    }
}