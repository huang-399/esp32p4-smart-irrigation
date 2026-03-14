/**
 * @file ui_settings.c
 * @brief 设置界面实现
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include "ui_keyboard.h"
#include "ui_wifi.h"
#include "ui_display.h"
#include "ui_network.h"
#include <stdio.h>
#include <string.h>

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_tab_buttons(lv_obj_t *parent);
static void tab_btn_cb(lv_event_t *e);
static void btn_single_add_cb(lv_event_t *e);
static void btn_batch_add_cb(lv_event_t *e);
static void network_mode_change_cb(lv_event_t *e);
static void show_add_zone_dialog(void);
static void show_add_device_dialog(void);
static void show_add_valve_dialog(void);
static void show_add_sensor_dialog(void);
static void show_lora_status_view(lv_event_t *e);
static void add_zone_confirm_cb(lv_event_t *e);
static void add_zone_back_cb(lv_event_t *e);
static void add_device_confirm_cb(lv_event_t *e);
static void add_device_cancel_cb(lv_event_t *e);
static void add_device_bg_click_cb(lv_event_t *e);
static void add_sensor_confirm_cb(lv_event_t *e);
static void add_sensor_cancel_cb(lv_event_t *e);
static void add_sensor_bg_click_cb(lv_event_t *e);
static void show_search_sensor_dialog(lv_event_t *e);
static void search_sensor_confirm_cb(lv_event_t *e);
static void search_sensor_cancel_cb(lv_event_t *e);
static void search_sensor_bg_click_cb(lv_event_t *e);
static void lora_status_back_cb(lv_event_t *e);
static void lora_status_refresh_cb(lv_event_t *e);
static void factory_reset_noop_cb(lv_event_t *e);

/* 全局变量 */
static lv_obj_t *g_zone_management_view = NULL;   /* 灌区管理视图 */
static lv_obj_t *g_device_management_view = NULL; /* 设备管理视图 */
static lv_obj_t *g_valve_management_view = NULL;  /* 阀门管理视图 */
static lv_obj_t *g_sensor_view = NULL;            /* 传感器视图 */
static lv_obj_t *g_system_settings_view = NULL;   /* 系统设置视图 */
static lv_obj_t *g_host_settings_view = NULL;     /* 主机设置视图 */
static lv_obj_t *g_tab_buttons[6] = {NULL};       /* 标签按钮数组 */

/* 系统设置子视图 */
static lv_obj_t *g_network_settings_form = NULL;  /* 网口设置表单 */
static lv_obj_t *g_4g_settings_form = NULL;        /* 4G设置表单 */
static lv_obj_t *g_wifi_settings_form = NULL;      /* WIFI设置表单 */
static lv_obj_t *g_password_settings_form = NULL;  /* 密码设置表单 */
static lv_obj_t *g_display_settings_form = NULL;   /* 显示设置表单 */
static lv_obj_t *g_factory_reset_form = NULL;      /* 恢复出厂表单 */
static lv_obj_t *g_check_update_form = NULL;       /* 检查更新表单 */
static lv_obj_t *g_network_info_form = NULL;       /* 网络信息表单 */
static lv_obj_t *g_system_info_form = NULL;        /* 系统信息表单 */
static lv_obj_t *g_left_menu_buttons[9] = {NULL};  /* 左侧菜单按钮数组 */

/* 添加灌区对话框 */
static lv_obj_t *g_add_zone_view = NULL;          /* 添加灌区视图 */
static lv_obj_t *g_zone_name_input = NULL;        /* 灌区名称输入框 */

/* 添加设备对话框 */
static lv_obj_t *g_add_device_bg = NULL;          /* 添加设备背景遮罩 */
static lv_obj_t *g_add_device_dialog = NULL;      /* 添加设备对话框 */
static lv_obj_t *g_device_type_dropdown = NULL;   /* 设备类型下拉框 */
static lv_obj_t *g_device_name_input = NULL;      /* 设备名称输入框 */
static lv_obj_t *g_device_id_input = NULL;        /* 设备编号输入框 */
static lv_obj_t *g_device_port_dropdown = NULL;   /* 串口下拉框 */

/* 添加传感器对话框 */
static lv_obj_t *g_add_sensor_bg = NULL;          /* 添加传感器背景遮罩 */
static lv_obj_t *g_add_sensor_dialog = NULL;      /* 添加传感器对话框 */
static lv_obj_t *g_sensor_type_dropdown = NULL;   /* 传感器类型下拉框 */
static lv_obj_t *g_sensor_name_input = NULL;      /* 传感器名称输入框 */
static lv_obj_t *g_sensor_parent_dropdown = NULL; /* 关联父设备下拉框 */
static lv_obj_t *g_sensor_id_input = NULL;        /* 传感器id号输入框 */

/* 搜索传感器对话框 */
static lv_obj_t *g_search_sensor_bg = NULL;       /* 搜索传感器背景遮罩 */
static lv_obj_t *g_search_sensor_dialog = NULL;   /* 搜索传感器对话框 */

/* Lora节点状态视图 */
static lv_obj_t *g_lora_status_view = NULL;       /* Lora状态视图 */

/* 网口设置输入框 */
static lv_obj_t *g_net_input_ip = NULL;      /* IP地址输入框 */
static lv_obj_t *g_net_input_mask = NULL;    /* 子网掩码输入框 */
static lv_obj_t *g_net_input_gateway = NULL; /* 默认网关输入框 */
static lv_obj_t *g_net_input_dns = NULL;     /* 首选DNS输入框 */

/* 前向声明 */
static void create_zone_management_view(lv_obj_t *parent);
static void create_device_management_view(lv_obj_t *parent);
static void create_valve_management_view(lv_obj_t *parent);
static void create_sensor_view(lv_obj_t *parent);
static void create_system_settings_view(lv_obj_t *parent);
static void create_host_settings_view(lv_obj_t *parent);
static void switch_to_zone_management(void);
static void switch_to_device_management(void);
static void switch_to_valve_management(void);
static void switch_to_sensor(void);
static void switch_to_system_settings(void);
static void switch_to_host_settings(void);
static void system_settings_menu_cb(lv_event_t *e);
static void create_4g_settings_form(lv_obj_t *parent);
static void create_wifi_settings_form(lv_obj_t *parent);
static void create_password_settings_form(lv_obj_t *parent);
static void create_display_settings_form(lv_obj_t *parent);
static void create_factory_reset_form(lv_obj_t *parent);
static void create_check_update_form(lv_obj_t *parent);
static void create_network_info_form(lv_obj_t *parent);
static void create_system_info_form(lv_obj_t *parent);
static void textarea_click_cb(lv_event_t *e);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 创建设置页面
 */
void ui_settings_create(lv_obj_t *parent)
{
    /* 重置静态指针（旧对象已被 ui_switch_nav 中的 lv_obj_clean 销毁） */
    g_zone_management_view = NULL;
    g_device_management_view = NULL;
    g_valve_management_view = NULL;
    g_sensor_view = NULL;
    g_system_settings_view = NULL;
    g_host_settings_view = NULL;
    for (int i = 0; i < 6; i++) g_tab_buttons[i] = NULL;

    g_network_settings_form = NULL;
    g_4g_settings_form = NULL;
    g_wifi_settings_form = NULL;
    g_password_settings_form = NULL;
    g_display_settings_form = NULL;
    g_factory_reset_form = NULL;
    g_check_update_form = NULL;
    g_network_info_form = NULL;
    g_system_info_form = NULL;
    for (int i = 0; i < 9; i++) g_left_menu_buttons[i] = NULL;

    g_add_zone_view = NULL;
    g_zone_name_input = NULL;
    g_add_device_bg = NULL;
    g_add_device_dialog = NULL;
    g_device_type_dropdown = NULL;
    g_device_name_input = NULL;
    g_device_id_input = NULL;
    g_device_port_dropdown = NULL;
    g_add_sensor_bg = NULL;
    g_add_sensor_dialog = NULL;
    g_sensor_type_dropdown = NULL;
    g_sensor_name_input = NULL;
    g_sensor_parent_dropdown = NULL;
    g_sensor_id_input = NULL;
    g_search_sensor_bg = NULL;
    g_search_sensor_dialog = NULL;
    g_lora_status_view = NULL;
    g_net_input_ip = NULL;
    g_net_input_mask = NULL;
    g_net_input_gateway = NULL;
    g_net_input_dns = NULL;

    /* 顶部标签页按钮 */
    create_tab_buttons(parent);

    /* 创建主容器 - 用于放置不同的视图 */
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, 1168, 660);
    lv_obj_set_pos(container, 5, 70);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_pad_all(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建灌区管理视图容器 */
    g_zone_management_view = lv_obj_create(container);
    lv_obj_set_size(g_zone_management_view, 1168, 660);
    lv_obj_set_pos(g_zone_management_view, 0, 0);
    lv_obj_set_style_bg_color(g_zone_management_view, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_zone_management_view, 0, 0);
    lv_obj_set_style_radius(g_zone_management_view, 10, 0);
    lv_obj_set_style_pad_all(g_zone_management_view, 15, 0);
    lv_obj_clear_flag(g_zone_management_view, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建灌区管理界面内容 */
    create_zone_management_view(g_zone_management_view);

    /* 创建设备管理视图 */
    g_device_management_view = lv_obj_create(container);
    lv_obj_set_size(g_device_management_view, 1168, 660);
    lv_obj_set_pos(g_device_management_view, 0, 0);
    lv_obj_set_style_bg_color(g_device_management_view, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_device_management_view, 0, 0);
    lv_obj_set_style_radius(g_device_management_view, 10, 0);
    lv_obj_set_style_pad_all(g_device_management_view, 15, 0);
    lv_obj_clear_flag(g_device_management_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);

    create_device_management_view(g_device_management_view);

    /* 创建阀门管理视图 */
    g_valve_management_view = lv_obj_create(container);
    lv_obj_set_size(g_valve_management_view, 1168, 660);
    lv_obj_set_pos(g_valve_management_view, 0, 0);
    lv_obj_set_style_bg_color(g_valve_management_view, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_valve_management_view, 0, 0);
    lv_obj_set_style_radius(g_valve_management_view, 10, 0);
    lv_obj_set_style_pad_all(g_valve_management_view, 15, 0);
    lv_obj_clear_flag(g_valve_management_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN);

    create_valve_management_view(g_valve_management_view);

    /* 创建传感器视图 */
    g_sensor_view = lv_obj_create(container);
    lv_obj_set_size(g_sensor_view, 1168, 660);
    lv_obj_set_pos(g_sensor_view, 0, 0);
    lv_obj_set_style_bg_color(g_sensor_view, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_sensor_view, 0, 0);
    lv_obj_set_style_radius(g_sensor_view, 10, 0);
    lv_obj_set_style_pad_all(g_sensor_view, 15, 0);
    lv_obj_clear_flag(g_sensor_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_sensor_view, LV_OBJ_FLAG_HIDDEN);

    create_sensor_view(g_sensor_view);

    /* 创建系统设置视图 */
    g_system_settings_view = lv_obj_create(container);
    lv_obj_set_size(g_system_settings_view, 1168, 660);
    lv_obj_set_pos(g_system_settings_view, 0, 0);
    lv_obj_set_style_bg_opa(g_system_settings_view, LV_OPA_TRANSP, 0);  /* 透明背景 */
    lv_obj_set_style_border_width(g_system_settings_view, 0, 0);
    lv_obj_set_style_radius(g_system_settings_view, 10, 0);
    lv_obj_set_style_pad_all(g_system_settings_view, 0, 0);  /* 无内边距 */
    lv_obj_clear_flag(g_system_settings_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_system_settings_view, LV_OBJ_FLAG_HIDDEN);

    create_system_settings_view(g_system_settings_view);

    /* 创建主机设置视图 */
    g_host_settings_view = lv_obj_create(container);
    lv_obj_set_size(g_host_settings_view, 1168, 660);
    lv_obj_set_pos(g_host_settings_view, 0, 0);
    lv_obj_set_style_bg_color(g_host_settings_view, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_host_settings_view, 0, 0);
    lv_obj_set_style_radius(g_host_settings_view, 10, 0);
    lv_obj_set_style_pad_all(g_host_settings_view, 15, 0);
    lv_obj_clear_flag(g_host_settings_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_host_settings_view, LV_OBJ_FLAG_HIDDEN);

    create_host_settings_view(g_host_settings_view);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 创建顶部标签页按钮
 */
static void create_tab_buttons(lv_obj_t *parent)
{
    const char *tab_names[] = {"灌区管理", "设备管理", "阀门管理", "传感器", "系统设置", "主机设置"};
    int btn_width = 150;
    int btn_height = 50;
    int x_start = 10;
    int y_pos = 10;

    for (int i = 0; i < 6; i++) {
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

        /* 按钮图标 */
        lv_obj_t *icon = lv_label_create(btn);
        if (i == 0) {
            lv_label_set_text(icon, LV_SYMBOL_GPS);  /* 灌区管理 */
        } else if (i == 1) {
            lv_label_set_text(icon, LV_SYMBOL_HOME);  /* 设备管理 */
        } else if (i == 2) {
            lv_label_set_text(icon, LV_SYMBOL_GPS);   /* 阀门管理 */
        } else if (i == 3) {
            lv_label_set_text(icon, LV_SYMBOL_IMAGE); /* 传感器 */
        } else if (i == 4) {
            lv_label_set_text(icon, LV_SYMBOL_SETTINGS); /* 系统设置 */
        } else {
            lv_label_set_text(icon, LV_SYMBOL_SETTINGS); /* 主机设置 */
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
 * @brief 创建灌区管理视图
 */
static void create_zone_management_view(lv_obj_t *parent)
{
    int y_pos = 5;

    /* 表格区域 */
    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, 555);
    lv_obj_set_pos(table_container, 0, y_pos);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头 */
    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "灌区名称", "关联阀门", "操作"};
    int header_x[] = {20, 250, 550, 950};

    for (int i = 0; i < 4; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    /* 底部按钮区 */
    int bottom_y = 570;

    /* 单个添加按钮 */
    lv_obj_t *btn_single_add = lv_btn_create(parent);
    lv_obj_set_size(btn_single_add, 150, 40);
    lv_obj_set_pos(btn_single_add, 200, bottom_y);
    lv_obj_set_style_bg_color(btn_single_add, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_single_add, 0, 0);
    lv_obj_set_style_radius(btn_single_add, 5, 0);
    lv_obj_add_event_cb(btn_single_add, btn_single_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *single_label = lv_label_create(btn_single_add);
    lv_label_set_text(single_label, "单个添加");
    lv_obj_set_style_text_color(single_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(single_label, &my_font_cn_16, 0);
    lv_obj_center(single_label);

    /* 批量添加按钮 */
    lv_obj_t *btn_batch_add = lv_btn_create(parent);
    lv_obj_set_size(btn_batch_add, 150, 40);
    lv_obj_set_pos(btn_batch_add, 370, bottom_y);
    lv_obj_set_style_bg_color(btn_batch_add, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_batch_add, 0, 0);
    lv_obj_set_style_radius(btn_batch_add, 5, 0);
    lv_obj_add_event_cb(btn_batch_add, btn_batch_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *batch_label = lv_label_create(btn_batch_add);
    lv_label_set_text(batch_label, "批量添加");
    lv_obj_set_style_text_color(batch_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(batch_label, &my_font_cn_16, 0);
    lv_obj_center(batch_label);

    /* 分页控件 */
    int pagination_y = bottom_y + 5;

    /* 首页按钮 */
    lv_obj_t *first_page_btn = lv_btn_create(parent);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, 550, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    lv_obj_t *prev_page_btn = lv_btn_create(parent);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, 630, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    lv_obj_t *page_label = lv_label_create(parent);
    lv_label_set_text(page_label, "0/0");
    lv_obj_set_pos(page_label, 740, pagination_y + 8);
    lv_obj_set_style_text_font(page_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    lv_obj_t *next_page_btn = lv_btn_create(parent);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, 800, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    lv_obj_t *last_page_btn = lv_btn_create(parent);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, 890, pagination_y);
    lv_obj_set_style_bg_color(last_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(last_page_btn, 0, 0);
    lv_obj_set_style_radius(last_page_btn, 5, 0);

    lv_obj_t *last_label = lv_label_create(last_page_btn);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);
}

/**
 * @brief 创建设备管理视图
 */
static void create_device_management_view(lv_obj_t *parent)
{
    int y_pos = 5;

    /* 表格区域 */
    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, 555);
    lv_obj_set_pos(table_container, 0, y_pos);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头 */
    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "设备名称", "类型", "编号", "串口", "操作"};
    int header_x[] = {20, 150, 400, 550, 700, 950};

    for (int i = 0; i < 6; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    /* 底部按钮区 */
    int bottom_y = 570;

    /* 单个添加按钮 */
    lv_obj_t *btn_single_add = lv_btn_create(parent);
    lv_obj_set_size(btn_single_add, 150, 40);
    lv_obj_set_pos(btn_single_add, 200, bottom_y);
    lv_obj_set_style_bg_color(btn_single_add, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_single_add, 0, 0);
    lv_obj_set_style_radius(btn_single_add, 5, 0);
    lv_obj_add_event_cb(btn_single_add, btn_single_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *single_label = lv_label_create(btn_single_add);
    lv_label_set_text(single_label, "单个添加");
    lv_obj_set_style_text_color(single_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(single_label, &my_font_cn_16, 0);
    lv_obj_center(single_label);

    /* lora状态按钮 - 青色 */
    lv_obj_t *btn_lora_status = lv_btn_create(parent);
    lv_obj_set_size(btn_lora_status, 150, 40);
    lv_obj_set_pos(btn_lora_status, 370, bottom_y);
    lv_obj_set_style_bg_color(btn_lora_status, lv_color_hex(0x00bcd4), 0);  /* 青色 */
    lv_obj_set_style_border_width(btn_lora_status, 0, 0);
    lv_obj_set_style_radius(btn_lora_status, 5, 0);
    lv_obj_add_event_cb(btn_lora_status, show_lora_status_view, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lora_label = lv_label_create(btn_lora_status);
    lv_label_set_text(lora_label, "lora状态");
    lv_obj_set_style_text_color(lora_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(lora_label, &my_font_cn_16, 0);
    lv_obj_center(lora_label);

    /* 分页控件 */
    int pagination_y = bottom_y + 5;

    /* 首页按钮 */
    lv_obj_t *first_page_btn = lv_btn_create(parent);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, 550, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    lv_obj_t *prev_page_btn = lv_btn_create(parent);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, 630, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    lv_obj_t *page_label = lv_label_create(parent);
    lv_label_set_text(page_label, "0/0");
    lv_obj_set_pos(page_label, 740, pagination_y + 8);
    lv_obj_set_style_text_font(page_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    lv_obj_t *next_page_btn = lv_btn_create(parent);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, 800, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    lv_obj_t *last_page_btn = lv_btn_create(parent);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, 890, pagination_y);
    lv_obj_set_style_bg_color(last_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(last_page_btn, 0, 0);
    lv_obj_set_style_radius(last_page_btn, 5, 0);

    lv_obj_t *last_label = lv_label_create(last_page_btn);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);
}

/**
 * @brief 创建阀门管理视图
 */
static void create_valve_management_view(lv_obj_t *parent)
{
    int y_pos = 5;

    /* 表格区域 */
    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, 555);
    lv_obj_set_pos(table_container, 0, y_pos);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头 */
    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "电磁阀名称", "类型", "编号", "关联设备", "通道号", "操作"};
    int header_x[] = {20, 120, 350, 480, 610, 830, 1000};

    for (int i = 0; i < 7; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    /* 底部按钮区 */
    int bottom_y = 570;

    /* 单个添加按钮 */
    lv_obj_t *btn_single_add = lv_btn_create(parent);
    lv_obj_set_size(btn_single_add, 150, 40);
    lv_obj_set_pos(btn_single_add, 200, bottom_y);
    lv_obj_set_style_bg_color(btn_single_add, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_single_add, 0, 0);
    lv_obj_set_style_radius(btn_single_add, 5, 0);
    lv_obj_add_event_cb(btn_single_add, btn_single_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *single_label = lv_label_create(btn_single_add);
    lv_label_set_text(single_label, "单个添加");
    lv_obj_set_style_text_color(single_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(single_label, &my_font_cn_16, 0);
    lv_obj_center(single_label);

    /* 分页控件 */
    int pagination_y = bottom_y + 5;

    /* 首页按钮 */
    lv_obj_t *first_page_btn = lv_btn_create(parent);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, 550, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    lv_obj_t *prev_page_btn = lv_btn_create(parent);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, 630, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    lv_obj_t *page_label = lv_label_create(parent);
    lv_label_set_text(page_label, "0/0");
    lv_obj_set_pos(page_label, 740, pagination_y + 8);
    lv_obj_set_style_text_font(page_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    lv_obj_t *next_page_btn = lv_btn_create(parent);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, 800, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    lv_obj_t *last_page_btn = lv_btn_create(parent);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, 890, pagination_y);
    lv_obj_set_style_bg_color(last_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(last_page_btn, 0, 0);
    lv_obj_set_style_radius(last_page_btn, 5, 0);

    lv_obj_t *last_label = lv_label_create(last_page_btn);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);
}

/**
 * @brief 创建传感器视图
 */
static void create_sensor_view(lv_obj_t *parent)
{
    int y_pos = 5;

    /* 表格区域 */
    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 1138, 555);
    lv_obj_set_pos(table_container, 0, y_pos);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头 */
    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 1138, 40);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"序号", "传感器名称", "类型", "编号", "关联设备", "从机号", "操作"};
    int header_x[] = {20, 120, 350, 480, 610, 830, 1000};

    for (int i = 0; i < 7; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    /* 底部按钮区 */
    int bottom_y = 570;

    /* 单个添加按钮 */
    lv_obj_t *btn_single_add = lv_btn_create(parent);
    lv_obj_set_size(btn_single_add, 150, 40);
    lv_obj_set_pos(btn_single_add, 200, bottom_y);
    lv_obj_set_style_bg_color(btn_single_add, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_single_add, 0, 0);
    lv_obj_set_style_radius(btn_single_add, 5, 0);
    lv_obj_add_event_cb(btn_single_add, btn_single_add_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *single_label = lv_label_create(btn_single_add);
    lv_label_set_text(single_label, "单个添加");
    lv_obj_set_style_text_color(single_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(single_label, &my_font_cn_16, 0);
    lv_obj_center(single_label);

    /* 搜索传感器按钮 - 橙色 */
    lv_obj_t *btn_search_sensor = lv_btn_create(parent);
    lv_obj_set_size(btn_search_sensor, 150, 40);
    lv_obj_set_pos(btn_search_sensor, 370, bottom_y);
    lv_obj_set_style_bg_color(btn_search_sensor, lv_color_hex(0xff9800), 0);  /* 橙色 */
    lv_obj_set_style_border_width(btn_search_sensor, 0, 0);
    lv_obj_set_style_radius(btn_search_sensor, 5, 0);
    lv_obj_add_event_cb(btn_search_sensor, show_search_sensor_dialog, LV_EVENT_CLICKED, NULL);

    lv_obj_t *search_label = lv_label_create(btn_search_sensor);
    lv_label_set_text(search_label, "搜索传感器");
    lv_obj_set_style_text_color(search_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(search_label, &my_font_cn_16, 0);
    lv_obj_center(search_label);

    /* 分页控件 */
    int pagination_y = bottom_y + 5;

    /* 首页按钮 */
    lv_obj_t *first_page_btn = lv_btn_create(parent);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, 550, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    lv_obj_t *prev_page_btn = lv_btn_create(parent);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, 630, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    lv_obj_t *page_label = lv_label_create(parent);
    lv_label_set_text(page_label, "0/0");
    lv_obj_set_pos(page_label, 740, pagination_y + 8);
    lv_obj_set_style_text_font(page_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    lv_obj_t *next_page_btn = lv_btn_create(parent);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, 800, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    lv_obj_t *last_page_btn = lv_btn_create(parent);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, 890, pagination_y);
    lv_obj_set_style_bg_color(last_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(last_page_btn, 0, 0);
    lv_obj_set_style_radius(last_page_btn, 5, 0);

    lv_obj_t *last_label = lv_label_create(last_page_btn);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);
}

/**
 * @brief 创建系统设置视图
 */
static void create_system_settings_view(lv_obj_t *parent)
{
    /* 左侧浅蓝色菜单区域 - 上左下边距都是0px，右边距离白色区域5px */
    lv_obj_t *left_menu = lv_obj_create(parent);
    lv_obj_set_size(left_menu, 250, 660);  /* 高度: 660 - 0(下边距) - 0(上边距) */
    lv_obj_set_pos(left_menu, 0, 0);  /* 左边0px，上边0px */
    lv_obj_set_style_bg_color(left_menu, lv_color_hex(0xa8d8ea), 0);  /* 浅蓝色 */
    lv_obj_set_style_border_width(left_menu, 0, 0);
    lv_obj_set_style_radius(left_menu, 10, 0);
    lv_obj_set_style_pad_all(left_menu, 0, 0);
    lv_obj_clear_flag(left_menu, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧菜单项 */
    const char *menu_items[] = {"网口设置", "4G设置", "WIFI设置", "密码设置", "显示设置", "恢复出厂", "检查更新", "网络信息", "系统信息"};

    for (int i = 0; i < 9; i++) {
        lv_obj_t *menu_btn = lv_btn_create(left_menu);
        lv_obj_set_size(menu_btn, 230, 50);
        lv_obj_set_pos(menu_btn, 10, 20 + i * 60);
        /* 第一个按钮(网口设置)默认选中为深蓝色，其他为浅蓝色 */
        if (i == 0) {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x70c1d8), 0);
        } else {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0xa8d8ea), 0);
        }
        lv_obj_set_style_radius(menu_btn, 5, 0);

        /* 添加点击事件 */
        lv_obj_add_event_cb(menu_btn, system_settings_menu_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t *menu_label = lv_label_create(menu_btn);
        lv_label_set_text_fmt(menu_label, "%s         >", menu_items[i]);
        lv_obj_set_style_text_font(menu_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(menu_label, lv_color_black(), 0);
        lv_obj_align(menu_label, LV_ALIGN_LEFT_MID, 20, 0);

        /* 保存按钮引用 */
        g_left_menu_buttons[i] = menu_btn;
    }

    /* 右侧白色表单区域 - 上下右边距都是0px，左边距离左侧菜单5px */
    lv_obj_t *form_area = lv_obj_create(parent);
    lv_obj_set_size(form_area, 913, 660);  /* 宽度: 1168 - 0(左边距) - 250(左侧) - 5(间距) - 0(右边距) = 913, 高度: 660 - 0(下边距) - 0(上边距) */
    lv_obj_set_pos(form_area, 255, 0);  /* x: 0(左边距) + 250(左侧菜单) + 5(间距) = 255, y: 0(上边距) */
    lv_obj_set_style_bg_color(form_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(form_area, 0, 0);
    lv_obj_set_style_radius(form_area, 10, 0);
    lv_obj_set_style_pad_all(form_area, 0, 0);  /* 使用0内边距，让子表单自己控制 */
    lv_obj_clear_flag(form_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建网口设置表单容器 */
    g_network_settings_form = lv_obj_create(form_area);
    lv_obj_set_size(g_network_settings_form, 913, 660);
    lv_obj_set_pos(g_network_settings_form, 0, 0);
    lv_obj_set_style_bg_opa(g_network_settings_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_network_settings_form, 0, 0);
    lv_obj_set_style_pad_all(g_network_settings_form, 30, 0);
    lv_obj_clear_flag(g_network_settings_form, LV_OBJ_FLAG_SCROLLABLE);

    /* 网口设置表单内容 */
    int y_pos = 0;
    int label_x = 0;
    int input_x = 140;

    /* 以太网标签 */
    lv_obj_t *label_eth = lv_label_create(g_network_settings_form);
    lv_label_set_text(label_eth, "以太网");
    lv_obj_set_pos(label_eth, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_eth, &my_font_cn_16, 0);

    /* 以太网下拉框 */
    lv_obj_t *dd_eth = lv_dropdown_create(g_network_settings_form);
    lv_dropdown_set_options(dd_eth, "eth0\neth1");
    lv_obj_set_size(dd_eth, 550, 40);
    lv_obj_set_style_text_font(dd_eth, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dd_eth), &my_font_cn_16, 0);
    lv_obj_set_pos(dd_eth, input_x, y_pos);
    lv_obj_set_style_bg_color(dd_eth, lv_color_white(), 0);
    lv_obj_set_style_border_width(dd_eth, 1, 0);
    lv_obj_set_style_border_color(dd_eth, lv_color_hex(0xcccccc), 0);

    y_pos += 70;

    /* 模式标签 */
    lv_obj_t *label_mode = lv_label_create(g_network_settings_form);
    lv_label_set_text(label_mode, "模式");
    lv_obj_set_pos(label_mode, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_mode, &my_font_cn_16, 0);

    /* 模式下拉框 */
    lv_obj_t *dd_mode = lv_dropdown_create(g_network_settings_form);
    lv_dropdown_set_options(dd_mode, "自动获取ip地址(DHCP)\n静态IP");
    lv_obj_set_size(dd_mode, 550, 40);
    lv_obj_set_style_text_font(dd_mode, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dd_mode), &my_font_cn_16, 0);
    lv_obj_set_pos(dd_mode, input_x, y_pos);
    lv_obj_set_style_bg_color(dd_mode, lv_color_white(), 0);
    lv_obj_set_style_border_width(dd_mode, 1, 0);
    lv_obj_set_style_border_color(dd_mode, lv_color_hex(0xcccccc), 0);


    /* 添加模式改变的回调函数 */
    lv_obj_add_event_cb(dd_mode, network_mode_change_cb, LV_EVENT_VALUE_CHANGED, NULL);

    y_pos += 70;

    /* ip地址标签 */
    lv_obj_t *label_ip = lv_label_create(g_network_settings_form);
    lv_label_set_text(label_ip, "ip地址");
    lv_obj_set_pos(label_ip, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_ip, &my_font_cn_16, 0);

    /* ip地址输入框 */
    g_net_input_ip = lv_textarea_create(g_network_settings_form);
    lv_obj_set_size(g_net_input_ip, 550, 40);
    lv_obj_set_pos(g_net_input_ip, input_x, y_pos);
    lv_textarea_set_text(g_net_input_ip, "");
    lv_textarea_set_placeholder_text(g_net_input_ip, "格式:192.168.1.101");
    lv_textarea_set_one_line(g_net_input_ip, true);
    lv_obj_set_style_bg_color(g_net_input_ip, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_net_input_ip, 1, 0);
    lv_obj_set_style_border_color(g_net_input_ip, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(g_net_input_ip, 8, 0);
    lv_obj_set_style_text_font(g_net_input_ip, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_net_input_ip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_net_input_ip, 0, 0);
    lv_obj_set_style_pad_right(g_net_input_ip, 0, 0);
    lv_obj_set_style_pad_top(g_net_input_ip, 4, 0);
    lv_obj_set_style_pad_bottom(g_net_input_ip, 0, 0);
    lv_obj_set_style_text_color(g_net_input_ip, lv_color_hex(0x999999), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_add_event_cb(g_net_input_ip, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    y_pos += 70;

    /* 子网掩码标签 */
    lv_obj_t *label_mask = lv_label_create(g_network_settings_form);
    lv_label_set_text(label_mask, "子网掩码");
    lv_obj_set_pos(label_mask, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_mask, &my_font_cn_16, 0);

    /* 子网掩码输入框 */
    g_net_input_mask = lv_textarea_create(g_network_settings_form);
    lv_obj_set_size(g_net_input_mask, 550, 40);
    lv_obj_set_pos(g_net_input_mask, input_x, y_pos);
    lv_textarea_set_text(g_net_input_mask, "");
    lv_textarea_set_placeholder_text(g_net_input_mask, "格式:24");
    lv_textarea_set_one_line(g_net_input_mask, true);
    lv_obj_set_style_bg_color(g_net_input_mask, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_net_input_mask, 1, 0);
    lv_obj_set_style_border_color(g_net_input_mask, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(g_net_input_mask, 8, 0);
    lv_obj_set_style_text_font(g_net_input_mask, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_net_input_mask, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_net_input_mask, 0, 0);
    lv_obj_set_style_pad_right(g_net_input_mask, 0, 0);
    lv_obj_set_style_pad_top(g_net_input_mask, 4, 0);
    lv_obj_set_style_pad_bottom(g_net_input_mask, 0, 0);
    lv_obj_set_style_text_color(g_net_input_mask, lv_color_hex(0x999999), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_add_event_cb(g_net_input_mask, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    y_pos += 70;

    /* 默认网关标签 */
    lv_obj_t *label_gateway = lv_label_create(g_network_settings_form);
    lv_label_set_text(label_gateway, "默认网关");
    lv_obj_set_pos(label_gateway, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_gateway, &my_font_cn_16, 0);

    /* 默认网关输入框 */
    g_net_input_gateway = lv_textarea_create(g_network_settings_form);
    lv_obj_set_size(g_net_input_gateway, 550, 40);
    lv_obj_set_pos(g_net_input_gateway, input_x, y_pos);
    lv_textarea_set_text(g_net_input_gateway, "");
    lv_textarea_set_placeholder_text(g_net_input_gateway, "格式:192.168.1.1");
    lv_textarea_set_one_line(g_net_input_gateway, true);
    lv_obj_set_style_bg_color(g_net_input_gateway, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_net_input_gateway, 1, 0);
    lv_obj_set_style_border_color(g_net_input_gateway, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(g_net_input_gateway, 8, 0);
    lv_obj_set_style_text_font(g_net_input_gateway, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_net_input_gateway, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_net_input_gateway, 0, 0);
    lv_obj_set_style_pad_right(g_net_input_gateway, 0, 0);
    lv_obj_set_style_pad_top(g_net_input_gateway, 4, 0);
    lv_obj_set_style_pad_bottom(g_net_input_gateway, 0, 0);
    lv_obj_set_style_text_color(g_net_input_gateway, lv_color_hex(0x999999), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_add_event_cb(g_net_input_gateway, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    y_pos += 70;

    /* 首选DNS标签 */
    lv_obj_t *label_dns = lv_label_create(g_network_settings_form);
    lv_label_set_text(label_dns, "首选DNS");
    lv_obj_set_pos(label_dns, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_dns, &my_font_cn_16, 0);

    /* 首选DNS输入框 */
    g_net_input_dns = lv_textarea_create(g_network_settings_form);
    lv_obj_set_size(g_net_input_dns, 550, 40);
    lv_obj_set_pos(g_net_input_dns, input_x, y_pos);
    lv_textarea_set_text(g_net_input_dns, "");
    lv_textarea_set_placeholder_text(g_net_input_dns, "格式:114.114.114.114");
    lv_textarea_set_one_line(g_net_input_dns, true);
    lv_obj_set_style_bg_color(g_net_input_dns, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_net_input_dns, 1, 0);
    lv_obj_set_style_border_color(g_net_input_dns, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(g_net_input_dns, 8, 0);
    lv_obj_set_style_text_font(g_net_input_dns, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_net_input_dns, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(g_net_input_dns, 0, 0);
    lv_obj_set_style_pad_right(g_net_input_dns, 0, 0);
    lv_obj_set_style_pad_top(g_net_input_dns, 4, 0);
    lv_obj_set_style_pad_bottom(g_net_input_dns, 0, 0);
    lv_obj_set_style_text_color(g_net_input_dns, lv_color_hex(0x999999), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_add_event_cb(g_net_input_dns, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    y_pos += 80;

    /* 底部按钮 */
    int btn_y = 520;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(g_network_settings_form);
    lv_obj_set_size(btn_cancel, 150, 45);
    lv_obj_set_pos(btn_cancel, 280, btn_y);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_radius(btn_cancel, 22, 0);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消设置");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 保存设置按钮 */
    lv_obj_t *btn_save = lv_btn_create(g_network_settings_form);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);

    /* Bind network settings event callbacks */
    lv_obj_add_event_cb(btn_save, ui_network_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_cancel, ui_network_cancel_cb, LV_EVENT_CLICKED, NULL);
    ui_network_set_controls(dd_eth, dd_mode,
                            g_net_input_ip, g_net_input_mask,
                            g_net_input_gateway, g_net_input_dns);

    /* 创建4G设置表单容器 */
    g_4g_settings_form = lv_obj_create(form_area);
    lv_obj_set_size(g_4g_settings_form, 913, 660);
    lv_obj_set_pos(g_4g_settings_form, 0, 0);
    lv_obj_set_style_bg_opa(g_4g_settings_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_4g_settings_form, 0, 0);
    lv_obj_set_style_pad_all(g_4g_settings_form, 30, 0);
    lv_obj_clear_flag(g_4g_settings_form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 调用4G设置表单内容创建函数 */
    create_4g_settings_form(g_4g_settings_form);

    /* 创建WIFI设置表单容器 */
    g_wifi_settings_form = lv_obj_create(form_area);
    lv_obj_set_size(g_wifi_settings_form, 913, 660);
    lv_obj_set_pos(g_wifi_settings_form, 0, 0);
    lv_obj_set_style_bg_opa(g_wifi_settings_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_wifi_settings_form, 0, 0);
    lv_obj_set_style_pad_all(g_wifi_settings_form, 30, 0);
    lv_obj_clear_flag(g_wifi_settings_form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 调用WIFI设置表单内容创建函数 */
    create_wifi_settings_form(g_wifi_settings_form);

    /* 创建密码设置表单容器 */
    g_password_settings_form = lv_obj_create(form_area);
    lv_obj_set_size(g_password_settings_form, 913, 660);
    lv_obj_set_pos(g_password_settings_form, 0, 0);
    lv_obj_set_style_bg_opa(g_password_settings_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_password_settings_form, 0, 0);
    lv_obj_set_style_pad_all(g_password_settings_form, 30, 0);
    lv_obj_clear_flag(g_password_settings_form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 调用密码设置表单内容创建函数 */
    create_password_settings_form(g_password_settings_form);

    /* 创建显示设置表单容器 */
    g_display_settings_form = lv_obj_create(form_area);
    lv_obj_set_size(g_display_settings_form, 913, 660);
    lv_obj_set_pos(g_display_settings_form, 0, 0);
    lv_obj_set_style_bg_opa(g_display_settings_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_display_settings_form, 0, 0);
    lv_obj_set_style_pad_all(g_display_settings_form, 30, 0);
    lv_obj_clear_flag(g_display_settings_form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 调用显示设置表单内容创建函数 */
    create_display_settings_form(g_display_settings_form);

    /* 创建恢复出厂表单容器 */
    g_factory_reset_form = lv_obj_create(form_area);
    lv_obj_set_size(g_factory_reset_form, 913, 660);
    lv_obj_set_pos(g_factory_reset_form, 0, 0);
    lv_obj_set_style_bg_opa(g_factory_reset_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_factory_reset_form, 0, 0);
    lv_obj_set_style_pad_all(g_factory_reset_form, 30, 0);
    lv_obj_clear_flag(g_factory_reset_form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 调用恢复出厂表单内容创建函数 */
    create_factory_reset_form(g_factory_reset_form);

    /* 创建检查更新表单容器 */
    g_check_update_form = lv_obj_create(form_area);
    lv_obj_set_size(g_check_update_form, 913, 660);
    lv_obj_set_pos(g_check_update_form, 0, 0);
    lv_obj_set_style_bg_opa(g_check_update_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_check_update_form, 0, 0);
    lv_obj_set_style_pad_all(g_check_update_form, 30, 0);
    lv_obj_clear_flag(g_check_update_form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 调用检查更新表单内容创建函数 */
    create_check_update_form(g_check_update_form);

    /* 创建网络信息表单容器 */
    g_network_info_form = lv_obj_create(form_area);
    lv_obj_set_size(g_network_info_form, 913, 660);
    lv_obj_set_pos(g_network_info_form, 0, 0);
    lv_obj_set_style_bg_opa(g_network_info_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_network_info_form, 0, 0);
    lv_obj_set_style_pad_all(g_network_info_form, 30, 0);
    lv_obj_clear_flag(g_network_info_form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 调用网络信息表单内容创建函数 */
    create_network_info_form(g_network_info_form);

    /* 创建系统信息表单容器 */
    g_system_info_form = lv_obj_create(form_area);
    lv_obj_set_size(g_system_info_form, 913, 660);
    lv_obj_set_pos(g_system_info_form, 0, 0);
    lv_obj_set_style_bg_opa(g_system_info_form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_system_info_form, 0, 0);
    lv_obj_set_style_pad_all(g_system_info_form, 30, 0);
    lv_obj_clear_flag(g_system_info_form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);  /* 默认隐藏 */

    /* 调用系统信息表单内容创建函数 */
    create_system_info_form(g_system_info_form);
}

/**
 * @brief 创建主机设置视图
 */
static void create_host_settings_view(lv_obj_t *parent)
{
    /* 主机设置表单内容 - 直接显示在parent（浅灰色背景）上，无需左侧菜单 */
    int y_pos = 20;
    int label_x = 15;
    int input_x = 225;

    /* 第一行：施肥泵延时启动 */
    lv_obj_t *label_pump_delay = lv_label_create(parent);
    lv_label_set_text(label_pump_delay, "施肥泵延时启动(S)：");
    lv_obj_set_pos(label_pump_delay, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_pump_delay, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_pump_delay, lv_color_black(), 0);

    lv_obj_t *input_pump_delay = lv_textarea_create(parent);
    lv_obj_set_size(input_pump_delay, 200, 40);
    lv_obj_set_pos(input_pump_delay, input_x, y_pos);
    lv_textarea_set_text(input_pump_delay, "10");
    lv_textarea_set_one_line(input_pump_delay, true);
    lv_obj_set_style_bg_color(input_pump_delay, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_pump_delay, 1, 0);
    lv_obj_set_style_border_color(input_pump_delay, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(input_pump_delay, 8, 0);
    /* 添加点击事件以弹出数字键盘 */
    lv_obj_set_style_text_font(input_pump_delay, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_pump_delay, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_pump_delay, 0, 0);
    lv_obj_set_style_pad_right(input_pump_delay, 0, 0);
    lv_obj_set_style_pad_top(input_pump_delay, 4, 0);
    lv_obj_set_style_pad_bottom(input_pump_delay, 0, 0);
    lv_obj_add_event_cb(input_pump_delay, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 第一行右侧：算法调节延时启动 */
    lv_obj_t *label_algo_delay = lv_label_create(parent);
    lv_label_set_text(label_algo_delay, "算法调节延时启动(S)：");
    lv_obj_set_pos(label_algo_delay, 565, y_pos + 10);
    lv_obj_set_style_text_font(label_algo_delay, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_algo_delay, lv_color_black(), 0);

    lv_obj_t *input_algo_delay = lv_textarea_create(parent);
    lv_obj_set_size(input_algo_delay, 100, 40);
    lv_obj_set_pos(input_algo_delay, 775, y_pos);
    lv_textarea_set_text(input_algo_delay, "2");
    lv_textarea_set_one_line(input_algo_delay, true);
    lv_obj_set_style_bg_color(input_algo_delay, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_algo_delay, 1, 0);
    lv_obj_set_style_border_color(input_algo_delay, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(input_algo_delay, 8, 0);
    /* 添加点击事件以弹出数字键盘 */
    lv_obj_set_style_text_font(input_algo_delay, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_algo_delay, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_algo_delay, 0, 0);
    lv_obj_set_style_pad_right(input_algo_delay, 0, 0);
    lv_obj_set_style_pad_top(input_algo_delay, 4, 0);
    lv_obj_set_style_pad_bottom(input_algo_delay, 0, 0);
    lv_obj_add_event_cb(input_algo_delay, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    y_pos += 70;

    /* 第二行：清洗肥管通时长 */
    lv_obj_t *label_clean_time = lv_label_create(parent);
    lv_label_set_text(label_clean_time, "清洗肥管通时长(S)：");
    lv_obj_set_pos(label_clean_time, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_clean_time, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_clean_time, lv_color_black(), 0);

    lv_obj_t *input_clean_time = lv_textarea_create(parent);
    lv_obj_set_size(input_clean_time, 200, 40);
    lv_obj_set_pos(input_clean_time, input_x, y_pos);
    lv_textarea_set_text(input_clean_time, "5");
    lv_textarea_set_one_line(input_clean_time, true);
    lv_obj_set_style_bg_color(input_clean_time, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_clean_time, 1, 0);
    lv_obj_set_style_border_color(input_clean_time, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(input_clean_time, 8, 0);
    /* 添加点击事件以弹出数字键盘 */
    lv_obj_set_style_text_font(input_clean_time, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_clean_time, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_clean_time, 0, 0);
    lv_obj_set_style_pad_right(input_clean_time, 0, 0);
    lv_obj_set_style_pad_top(input_clean_time, 4, 0);
    lv_obj_set_style_pad_bottom(input_clean_time, 0, 0);
    lv_obj_add_event_cb(input_clean_time, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 第二行右侧：ECPH主传感器 */
    lv_obj_t *label_ecph = lv_label_create(parent);
    lv_label_set_text(label_ecph, "ECPH主传感器:");
    lv_obj_set_pos(label_ecph, 565, y_pos + 10);
    lv_obj_set_style_text_font(label_ecph, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_ecph, lv_color_black(), 0);

    lv_obj_t *dd_ecph = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd_ecph, "ECPH1\nECPH2\nECPH3");
    lv_obj_set_size(dd_ecph, 220, 40);
    lv_obj_set_style_text_font(dd_ecph, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dd_ecph), &my_font_cn_16, 0);
    lv_obj_set_pos(dd_ecph, 755, y_pos);
    lv_obj_set_style_bg_color(dd_ecph, lv_color_white(), 0);
    lv_obj_set_style_border_width(dd_ecph, 1, 0);
    lv_obj_set_style_border_color(dd_ecph, lv_color_hex(0xcccccc), 0);

    y_pos += 80;

    /* 底部按钮 */
    int btn_y = 580;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
    lv_obj_set_size(btn_cancel, 150, 45);
    lv_obj_set_pos(btn_cancel, 170, btn_y);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_radius(btn_cancel, 22, 0);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消设置");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 保存设置按钮 */
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 340, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 切换到灌区管理视图
 */
static void switch_to_zone_management(void)
{
    if (g_zone_management_view) lv_obj_clear_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_device_management_view) lv_obj_add_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_valve_management_view) lv_obj_add_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_sensor_view) lv_obj_add_flag(g_sensor_view, LV_OBJ_FLAG_HIDDEN);
    if (g_system_settings_view) lv_obj_add_flag(g_system_settings_view, LV_OBJ_FLAG_HIDDEN);
    if (g_host_settings_view) lv_obj_add_flag(g_host_settings_view, LV_OBJ_FLAG_HIDDEN);
}

static void switch_to_device_management(void)
{
    if (g_zone_management_view) lv_obj_add_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_device_management_view) lv_obj_clear_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_valve_management_view) lv_obj_add_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_sensor_view) lv_obj_add_flag(g_sensor_view, LV_OBJ_FLAG_HIDDEN);
    if (g_system_settings_view) lv_obj_add_flag(g_system_settings_view, LV_OBJ_FLAG_HIDDEN);
    if (g_host_settings_view) lv_obj_add_flag(g_host_settings_view, LV_OBJ_FLAG_HIDDEN);
}

static void switch_to_valve_management(void)
{
    if (g_zone_management_view) lv_obj_add_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_device_management_view) lv_obj_add_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_valve_management_view) lv_obj_clear_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_sensor_view) lv_obj_add_flag(g_sensor_view, LV_OBJ_FLAG_HIDDEN);
    if (g_system_settings_view) lv_obj_add_flag(g_system_settings_view, LV_OBJ_FLAG_HIDDEN);
    if (g_host_settings_view) lv_obj_add_flag(g_host_settings_view, LV_OBJ_FLAG_HIDDEN);
}

static void switch_to_sensor(void)
{
    if (g_zone_management_view) lv_obj_add_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_device_management_view) lv_obj_add_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_valve_management_view) lv_obj_add_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_sensor_view) lv_obj_clear_flag(g_sensor_view, LV_OBJ_FLAG_HIDDEN);
    if (g_system_settings_view) lv_obj_add_flag(g_system_settings_view, LV_OBJ_FLAG_HIDDEN);
    if (g_host_settings_view) lv_obj_add_flag(g_host_settings_view, LV_OBJ_FLAG_HIDDEN);
}

static void switch_to_system_settings(void)
{
    if (g_zone_management_view) lv_obj_add_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_device_management_view) lv_obj_add_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_valve_management_view) lv_obj_add_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_sensor_view) lv_obj_add_flag(g_sensor_view, LV_OBJ_FLAG_HIDDEN);
    if (g_system_settings_view) lv_obj_clear_flag(g_system_settings_view, LV_OBJ_FLAG_HIDDEN);
    if (g_host_settings_view) lv_obj_add_flag(g_host_settings_view, LV_OBJ_FLAG_HIDDEN);
}

static void switch_to_host_settings(void)
{
    if (g_zone_management_view) lv_obj_add_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_device_management_view) lv_obj_add_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_valve_management_view) lv_obj_add_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN);
    if (g_sensor_view) lv_obj_add_flag(g_sensor_view, LV_OBJ_FLAG_HIDDEN);
    if (g_system_settings_view) lv_obj_add_flag(g_system_settings_view, LV_OBJ_FLAG_HIDDEN);
    if (g_host_settings_view) lv_obj_clear_flag(g_host_settings_view, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 标签页按钮回调
 */
static void tab_btn_cb(lv_event_t *e)
{
    int tab_index = (int)(intptr_t)lv_event_get_user_data(e);

    /* 更新所有标签按钮的颜色和文字颜色 */
    for (int i = 0; i < 6; i++) {
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

    /* 根据选中的标签页切换视图 */
    if (tab_index == 0) {
        switch_to_zone_management();
    } else if (tab_index == 1) {
        switch_to_device_management();
    } else if (tab_index == 2) {
        switch_to_valve_management();
    } else if (tab_index == 3) {
        switch_to_sensor();
    } else if (tab_index == 4) {
        switch_to_system_settings();
    } else if (tab_index == 5) {
        switch_to_host_settings();
    }
}

/**
 * @brief 单个添加按钮回调
 */
static void btn_single_add_cb(lv_event_t *e)
{
    (void)e;

    /* 根据当前激活的视图调用对应的添加函数 */
    if (g_zone_management_view && !lv_obj_has_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN)) {
        show_add_zone_dialog();
    } else if (g_device_management_view && !lv_obj_has_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN)) {
        show_add_device_dialog();
    } else if (g_valve_management_view && !lv_obj_has_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN)) {
        show_add_valve_dialog();
    } else if (g_sensor_view && !lv_obj_has_flag(g_sensor_view, LV_OBJ_FLAG_HIDDEN)) {
        show_add_sensor_dialog();
    }
}

/**
 * @brief 显示添加灌区对话框
 */
static void show_add_zone_dialog(void)
{
    /* 隐藏灌区管理视图 */
    if (g_zone_management_view) {
        lv_obj_add_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    }

    /* 隐藏顶部标签按钮 */
    for (int i = 0; i < 6; i++) {
        if (g_tab_buttons[i]) {
            lv_obj_add_flag(g_tab_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 如果添加灌区视图已存在，先删除 */
    if (g_add_zone_view != NULL) {
        lv_obj_del(g_add_zone_view);
        g_add_zone_view = NULL;
    }

    /* 创建添加灌区视图 - 覆盖标签页+内容区域 */
    /* 获取container的父对象（settings page的parent） */
    lv_obj_t *container = lv_obj_get_parent(g_zone_management_view);
    lv_obj_t *settings_parent = lv_obj_get_parent(container);

    g_add_zone_view = lv_obj_create(settings_parent);
    lv_obj_set_size(g_add_zone_view, 1168, 730);  /* 高度增加70px覆盖标签页区域 */
    lv_obj_set_pos(g_add_zone_view, 5, 0);  /* 从顶部开始 */
    lv_obj_set_style_bg_color(g_add_zone_view, lv_color_hex(0xe8e8e8), 0);  /* 浅灰色背景 */
    lv_obj_set_style_border_width(g_add_zone_view, 0, 0);
    lv_obj_set_style_radius(g_add_zone_view, 10, 0);
    lv_obj_set_style_pad_all(g_add_zone_view, 0, 0);  /* 去除padding，手动控制子元素位置 */
    lv_obj_clear_flag(g_add_zone_view, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶部标题栏区域 - 透明背景 */
    lv_obj_t *header = lv_obj_create(g_add_zone_view);
    lv_obj_set_size(header, 1156, 60);  /* 宽度: 1168 - 6(左) - 6(右) = 1156 */
    lv_obj_set_pos(header, 6, 5);  /* 距离左边6px */
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);  /* 透明背景 */
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 10, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* 返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 50, 50);
    lv_obj_set_pos(btn_back, 10, 5);
    lv_obj_set_style_bg_color(btn_back, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_radius(btn_back, LV_RADIUS_CIRCLE, 0);  /* 圆形 */
    lv_obj_add_event_cb(btn_back, add_zone_back_cb, LV_EVENT_CLICKED, NULL);

    /* 返回按钮图标 */
    lv_obj_t *back_icon = lv_label_create(btn_back);
    lv_label_set_text(back_icon, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_icon, lv_color_white(), 0);
    lv_obj_set_style_text_font(back_icon, &lv_font_montserrat_24, 0);
    lv_obj_center(back_icon);

    /* 标题 "添加灌区" */
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "添加灌区");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_pos(title, 80, 20);

    /* 灌区名称标签 - 在标题栏中间偏右 */
    lv_obj_t *label_zone_name = lv_label_create(header);
    lv_label_set_text(label_zone_name, "灌区名称");
    lv_obj_set_pos(label_zone_name, 380, 20);
    lv_obj_set_style_text_font(label_zone_name, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_zone_name, lv_color_black(), 0);

    /* 灌区名称输入框 - 在标题栏中 */
    g_zone_name_input = lv_textarea_create(header);
    lv_obj_set_size(g_zone_name_input, 400, 45);
    lv_obj_set_pos(g_zone_name_input, 470, 8);
    lv_textarea_set_text(g_zone_name_input, "灌区1");
    lv_textarea_set_one_line(g_zone_name_input, true);
    lv_obj_set_style_bg_color(g_zone_name_input, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_zone_name_input, 1, 0);
    lv_obj_set_style_border_color(g_zone_name_input, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(g_zone_name_input, 8, 0);
    lv_obj_set_style_text_font(g_zone_name_input, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_zone_name_input, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_pad_left(g_zone_name_input, 10, 0);
    lv_obj_set_style_pad_right(g_zone_name_input, 10, 0);
    lv_obj_set_style_pad_top(g_zone_name_input, 8, 0);
    lv_obj_set_style_pad_bottom(g_zone_name_input, 8, 0);
    lv_obj_set_style_text_color(g_zone_name_input, lv_color_hex(0x999999), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_add_event_cb(g_zone_name_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 确认添加按钮 */
    lv_obj_t *btn_confirm = lv_btn_create(header);
    lv_obj_set_size(btn_confirm, 120, 45);
    lv_obj_set_pos(btn_confirm, 1000, 8);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 22, 0);
    lv_obj_add_event_cb(btn_confirm, add_zone_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认添加");
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_center(label_confirm);

    /* 下方白色区域 - 距离左右下都是6px */
    lv_obj_t *white_area = lv_obj_create(g_add_zone_view);
    /* 宽度: 1168 - 6(左) - 6(右) = 1156 */
    /* 高度: 730 - 75(顶部header+间距) - 6(下边距) = 649 */
    lv_obj_set_size(white_area, 1156, 649);
    lv_obj_set_pos(white_area, 6, 75);  /* 距离左边6px */
    lv_obj_set_style_bg_color(white_area, lv_color_white(), 0);  /* 白色背景 */
    lv_obj_set_style_border_width(white_area, 0, 0);
    lv_obj_set_style_radius(white_area, 10, 0);
    lv_obj_set_style_pad_all(white_area, 20, 0);
    lv_obj_clear_flag(white_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 底部分页控件 - 在白色区域底部居中 */
    /* 白色区域内部高度：649，分页控件在底部，距离底边约40px */
    int pagination_y = 649 - 35 - 40;  /* 白色区域高度 - 按钮高度 - 底部间距 = 574 */

    /* 计算居中位置：
     * 总宽度约: 70(首页) + 10 + 80(上一页) + 30 + 60(0/0) + 30 + 80(下一页) + 10 + 70(尾页) = 440
     * 白色区域宽度1156，padding 20，内容区域宽度 = 1156 - 40 = 1116
     * 居中起始x = (1116 - 440) / 2 = 338
     */
    int center_x = 338;

    /* 首页按钮 */
    lv_obj_t *first_page_btn = lv_btn_create(white_area);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, center_x, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    lv_obj_t *prev_page_btn = lv_btn_create(white_area);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, center_x + 70 + 10, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    lv_obj_t *page_label = lv_label_create(white_area);
    lv_label_set_text(page_label, "0/0");
    lv_obj_set_pos(page_label, center_x + 70 + 10 + 80 + 30, pagination_y + 8);
    lv_obj_set_style_text_font(page_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    lv_obj_t *next_page_btn = lv_btn_create(white_area);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, center_x + 70 + 10 + 80 + 30 + 60 + 30, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    lv_obj_t *last_page_btn = lv_btn_create(white_area);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, center_x + 70 + 10 + 80 + 30 + 60 + 30 + 80 + 10, pagination_y);
    lv_obj_set_style_bg_color(last_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(last_page_btn, 0, 0);
    lv_obj_set_style_radius(last_page_btn, 5, 0);

    lv_obj_t *last_label = lv_label_create(last_page_btn);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);
}

/**
 * @brief 添加灌区确认按钮回调
 */
static void add_zone_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* TODO: 获取输入框的文字并添加到灌区列表 */
    const char *zone_name = lv_textarea_get_text(g_zone_name_input);

    /* 这里可以添加验证逻辑，检查名称是否为空等 */
    if (zone_name && strlen(zone_name) > 0) {
        /* TODO: 添加到数据结构并刷新列表显示 */
    }

    /* 删除添加灌区视图 */
    if (g_add_zone_view != NULL) {
        lv_obj_del(g_add_zone_view);
        g_add_zone_view = NULL;
        g_zone_name_input = NULL;
    }

    /* 恢复显示顶部标签按钮 */
    for (int i = 0; i < 6; i++) {
        if (g_tab_buttons[i]) {
            lv_obj_clear_flag(g_tab_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 显示灌区管理视图 */
    if (g_zone_management_view) {
        lv_obj_clear_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 添加灌区返回按钮回调
 */
static void add_zone_back_cb(lv_event_t *e)
{
    (void)e;

    /* 删除添加灌区视图 */
    if (g_add_zone_view != NULL) {
        lv_obj_del(g_add_zone_view);
        g_add_zone_view = NULL;
        g_zone_name_input = NULL;
    }

    /* 恢复显示顶部标签按钮 */
    for (int i = 0; i < 6; i++) {
        if (g_tab_buttons[i]) {
            lv_obj_clear_flag(g_tab_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 显示灌区管理视图 */
    if (g_zone_management_view) {
        lv_obj_clear_flag(g_zone_management_view, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 显示添加设备界面
 */
static void show_add_device_dialog(void)
{
    /* 如果对话框已存在，先关闭 */
    if (g_add_device_bg) {
        return;
    }

    ui_main_t *ui_main = ui_get_main();
    if (!ui_main || !ui_main->screen) {
        return;
    }

    /* 创建半透明背景遮罩 */
    g_add_device_bg = lv_obj_create(ui_main->screen);
    lv_obj_set_size(g_add_device_bg, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(g_add_device_bg, 0, 0);
    lv_obj_set_style_bg_opa(g_add_device_bg, LV_OPA_TRANSP, 0);  /* 透明背景，不变暗 */
    lv_obj_set_style_border_width(g_add_device_bg, 0, 0);
    lv_obj_clear_flag(g_add_device_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_add_device_bg, add_device_bg_click_cb, LV_EVENT_CLICKED, NULL);

    /* 创建对话框 - 居中显示 */
    g_add_device_dialog = lv_obj_create(g_add_device_bg);
    int dialog_width = 600;
    int dialog_height = 460;
    lv_obj_set_size(g_add_device_dialog, dialog_width, dialog_height);
    lv_obj_center(g_add_device_dialog);
    lv_obj_set_style_bg_color(g_add_device_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_add_device_dialog, 3, 0);
    lv_obj_set_style_border_color(g_add_device_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(g_add_device_dialog, 10, 0);
    lv_obj_set_style_pad_all(g_add_device_dialog, 5, 0);  /* 白色底到蓝色边框5px */
    lv_obj_clear_flag(g_add_device_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(g_add_device_dialog);
    lv_label_set_text(title, "添加设备");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_pos(title, (dialog_width - 80) / 2, 10);  /* 居中 */

    /* 设备类型 */
    lv_obj_t *label_type = lv_label_create(g_add_device_dialog);
    lv_label_set_text(label_type, "设备类型");
    lv_obj_set_pos(label_type, 10, 65);
    lv_obj_set_style_text_font(label_type, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_type, lv_color_black(), 0);

    g_device_type_dropdown = lv_dropdown_create(g_add_device_dialog);
    lv_dropdown_set_options(g_device_type_dropdown, "8路控制器\n16路控制器\n32路控制器");
    lv_obj_set_size(g_device_type_dropdown, 420, 45);
    lv_obj_set_pos(g_device_type_dropdown, 130, 55);
    lv_obj_set_style_text_font(g_device_type_dropdown, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_device_type_dropdown), &my_font_cn_16, 0);
    lv_obj_set_style_text_font(g_device_type_dropdown, &my_font_cn_16, LV_PART_SELECTED);

    /* 设置下拉列表的字体 */
    lv_dropdown_set_selected(g_device_type_dropdown, 0);

    /* 设备名称 */
    lv_obj_t *label_name = lv_label_create(g_add_device_dialog);
    lv_label_set_text(label_name, "设备名称");
    lv_obj_set_pos(label_name, 10, 135);
    lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_name, lv_color_black(), 0);

    g_device_name_input = lv_textarea_create(g_add_device_dialog);
    lv_obj_set_size(g_device_name_input, 420, 45);
    lv_obj_set_pos(g_device_name_input, 130, 125);
    lv_textarea_set_text(g_device_name_input, "8路控制器1");
    lv_textarea_set_one_line(g_device_name_input, true);
    lv_obj_set_style_text_font(g_device_name_input, &my_font_cn_16, 0);
    lv_obj_set_style_bg_color(g_device_name_input, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(g_device_name_input, 1, 0);
    lv_obj_set_style_border_color(g_device_name_input, lv_color_hex(0xcccccc), 0);
    lv_obj_add_event_cb(g_device_name_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 设备编号 */
    lv_obj_t *label_id = lv_label_create(g_add_device_dialog);
    lv_label_set_text(label_id, "设备编号");
    lv_obj_set_pos(label_id, 10, 205);
    lv_obj_set_style_text_font(label_id, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_id, lv_color_black(), 0);

    g_device_id_input = lv_textarea_create(g_add_device_dialog);
    lv_obj_set_size(g_device_id_input, 200, 45);
    lv_obj_set_pos(g_device_id_input, 130, 195);
    lv_textarea_set_text(g_device_id_input, "1000");
    lv_textarea_set_one_line(g_device_id_input, true);
    lv_obj_set_style_text_font(g_device_id_input, &my_font_cn_16, 0);
    lv_obj_set_style_bg_color(g_device_id_input, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(g_device_id_input, 1, 0);
    lv_obj_set_style_border_color(g_device_id_input, lv_color_hex(0xcccccc), 0);
    lv_obj_add_event_cb(g_device_id_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 第二个编号输入框 */
    lv_obj_t *device_id_input2 = lv_textarea_create(g_add_device_dialog);
    lv_obj_set_size(device_id_input2, 200, 45);
    lv_obj_set_pos(device_id_input2, 350, 195);
    lv_textarea_set_text(device_id_input2, "00000001");
    lv_textarea_set_one_line(device_id_input2, true);
    lv_obj_set_style_text_font(device_id_input2, &my_font_cn_16, 0);
    lv_obj_set_style_bg_color(device_id_input2, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(device_id_input2, 1, 0);
    lv_obj_set_style_border_color(device_id_input2, lv_color_hex(0xcccccc), 0);
    lv_obj_add_event_cb(device_id_input2, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 串口 */
    lv_obj_t *label_port = lv_label_create(g_add_device_dialog);
    lv_label_set_text(label_port, "串口");
    lv_obj_set_pos(label_port, 10, 275);
    lv_obj_set_style_text_font(label_port, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_port, lv_color_black(), 0);

    g_device_port_dropdown = lv_dropdown_create(g_add_device_dialog);
    lv_dropdown_set_options(g_device_port_dropdown, "RS485-1\nRS485-2\nRS485-3\nRS485-4");
    lv_obj_set_size(g_device_port_dropdown, 420, 45);
    lv_obj_set_pos(g_device_port_dropdown, 130, 265);
    lv_obj_set_style_text_font(g_device_port_dropdown, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_device_port_dropdown), &my_font_cn_16, 0);
    lv_obj_set_style_text_font(g_device_port_dropdown, &my_font_cn_16, LV_PART_SELECTED);

    /* 设置下拉列表的字体 */
    lv_dropdown_set_selected(g_device_port_dropdown, 1);

    /* 取消添加按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(g_add_device_dialog);
    lv_obj_set_size(btn_cancel, 180, 50);
    lv_obj_set_pos(btn_cancel, 60, 365);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 8, 0);
    lv_obj_add_event_cb(btn_cancel, add_device_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_label, "取消添加");
    lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(cancel_label, &my_font_cn_16, 0);
    lv_obj_center(cancel_label);

    /* 确认添加按钮 */
    lv_obj_t *btn_confirm = lv_btn_create(g_add_device_dialog);
    lv_obj_set_size(btn_confirm, 180, 50);
    lv_obj_set_pos(btn_confirm, 320, 365);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 8, 0);
    lv_obj_add_event_cb(btn_confirm, add_device_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *confirm_label = lv_label_create(btn_confirm);
    lv_label_set_text(confirm_label, "确认添加");
    lv_obj_set_style_text_color(confirm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(confirm_label, &my_font_cn_16, 0);
    lv_obj_center(confirm_label);
}

/**
 * @brief 显示添加阀门界面
 */
static void show_add_valve_dialog(void)
{
    /* 隐藏阀门管理视图和顶部标签按钮 */
    if (g_valve_management_view) {
        lv_obj_add_flag(g_valve_management_view, LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < 6; i++) {
        if (g_tab_buttons[i]) {
            lv_obj_add_flag(g_tab_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* TODO: 实现添加阀门界面 */
    /* 这里可以参考 show_add_zone_dialog 的实现 */
}

/**
 * @brief 显示添加传感器界面
 */
static void show_add_sensor_dialog(void)
{
    /* 如果对话框已存在，先关闭 */
    if (g_add_sensor_bg) {
        return;
    }

    ui_main_t *ui_main = ui_get_main();
    if (!ui_main || !ui_main->screen) {
        return;
    }

    /* 创建透明背景遮罩 */
    g_add_sensor_bg = lv_obj_create(ui_main->screen);
    lv_obj_set_size(g_add_sensor_bg, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(g_add_sensor_bg, 0, 0);
    lv_obj_set_style_bg_opa(g_add_sensor_bg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_add_sensor_bg, 0, 0);
    lv_obj_clear_flag(g_add_sensor_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_add_sensor_bg, add_sensor_bg_click_cb, LV_EVENT_CLICKED, NULL);

    /* 创建对话框 - 居中显示 */
    g_add_sensor_dialog = lv_obj_create(g_add_sensor_bg);
    int dialog_width = 600;
    int dialog_height = 420;
    lv_obj_set_size(g_add_sensor_dialog, dialog_width, dialog_height);
    lv_obj_center(g_add_sensor_dialog);
    lv_obj_set_style_bg_color(g_add_sensor_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_add_sensor_dialog, 3, 0);
    lv_obj_set_style_border_color(g_add_sensor_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(g_add_sensor_dialog, 10, 0);
    lv_obj_set_style_pad_all(g_add_sensor_dialog, 5, 0);
    lv_obj_clear_flag(g_add_sensor_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(g_add_sensor_dialog);
    lv_label_set_text(title, "添加传感器");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_pos(title, (dialog_width - 96) / 2, 10);

    /* 传感器类型 */
    lv_obj_t *label_type = lv_label_create(g_add_sensor_dialog);
    lv_label_set_text(label_type, "传感器类型");
    lv_obj_set_pos(label_type, 10, 65);
    lv_obj_set_style_text_font(label_type, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_type, lv_color_black(), 0);

    g_sensor_type_dropdown = lv_dropdown_create(g_add_sensor_dialog);
    lv_dropdown_set_options(g_sensor_type_dropdown, "空气湿度\n土壤湿度\n温度\n雨量");
    lv_obj_set_size(g_sensor_type_dropdown, 420, 45);
    lv_obj_set_pos(g_sensor_type_dropdown, 130, 55);
    lv_obj_set_style_text_font(g_sensor_type_dropdown, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_sensor_type_dropdown), &my_font_cn_16, 0);
    lv_obj_set_style_text_font(g_sensor_type_dropdown, &my_font_cn_16, LV_PART_SELECTED);

    /* 设置下拉列表的字体 */
    lv_dropdown_set_selected(g_sensor_type_dropdown, 0);

    /* 传感器名称 */
    lv_obj_t *label_name = lv_label_create(g_add_sensor_dialog);
    lv_label_set_text(label_name, "传感器名称");
    lv_obj_set_pos(label_name, 10, 135);
    lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_name, lv_color_black(), 0);

    g_sensor_name_input = lv_textarea_create(g_add_sensor_dialog);
    lv_obj_set_size(g_sensor_name_input, 420, 45);
    lv_obj_set_pos(g_sensor_name_input, 130, 125);
    lv_textarea_set_text(g_sensor_name_input, "#1空气湿度");
    lv_textarea_set_one_line(g_sensor_name_input, true);
    lv_obj_set_style_text_font(g_sensor_name_input, &my_font_cn_16, 0);
    lv_obj_set_style_bg_color(g_sensor_name_input, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(g_sensor_name_input, 1, 0);
    lv_obj_set_style_border_color(g_sensor_name_input, lv_color_hex(0xcccccc), 0);
    lv_obj_add_event_cb(g_sensor_name_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 关联父设备 */
    lv_obj_t *label_parent = lv_label_create(g_add_sensor_dialog);
    lv_label_set_text(label_parent, "关联父设备");
    lv_obj_set_pos(label_parent, 10, 205);
    lv_obj_set_style_text_font(label_parent, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_parent, lv_color_black(), 0);

    g_sensor_parent_dropdown = lv_dropdown_create(g_add_sensor_dialog);
    lv_dropdown_set_options(g_sensor_parent_dropdown, "设备1\n设备2\n设备3");
    lv_obj_set_size(g_sensor_parent_dropdown, 420, 45);
    lv_obj_set_pos(g_sensor_parent_dropdown, 130, 195);
    lv_obj_set_style_text_font(g_sensor_parent_dropdown, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(g_sensor_parent_dropdown), &my_font_cn_16, 0);
    lv_obj_set_style_text_font(g_sensor_parent_dropdown, &my_font_cn_16, LV_PART_SELECTED);

    /* 设置下拉列表的字体 */
    lv_dropdown_set_selected(g_sensor_parent_dropdown, 0);

    /* 传感器id号 */
    lv_obj_t *label_id = lv_label_create(g_add_sensor_dialog);
    lv_label_set_text(label_id, "传感器id号");
    lv_obj_set_pos(label_id, 10, 275);
    lv_obj_set_style_text_font(label_id, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_id, lv_color_black(), 0);

    g_sensor_id_input = lv_textarea_create(g_add_sensor_dialog);
    lv_obj_set_size(g_sensor_id_input, 420, 45);
    lv_obj_set_pos(g_sensor_id_input, 130, 265);
    lv_textarea_set_text(g_sensor_id_input, "1");
    lv_textarea_set_one_line(g_sensor_id_input, true);
    lv_obj_set_style_text_font(g_sensor_id_input, &my_font_cn_16, 0);
    lv_obj_set_style_bg_color(g_sensor_id_input, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(g_sensor_id_input, 1, 0);
    lv_obj_set_style_border_color(g_sensor_id_input, lv_color_hex(0xcccccc), 0);
    lv_obj_add_event_cb(g_sensor_id_input, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 取消添加按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(g_add_sensor_dialog);
    lv_obj_set_size(btn_cancel, 180, 50);
    lv_obj_set_pos(btn_cancel, 60, 335);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 8, 0);
    lv_obj_add_event_cb(btn_cancel, add_sensor_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_label, "取消添加");
    lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(cancel_label, &my_font_cn_16, 0);
    lv_obj_center(cancel_label);

    /* 确认添加按钮 */
    lv_obj_t *btn_confirm = lv_btn_create(g_add_sensor_dialog);
    lv_obj_set_size(btn_confirm, 180, 50);
    lv_obj_set_pos(btn_confirm, 320, 335);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 8, 0);
    lv_obj_add_event_cb(btn_confirm, add_sensor_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *confirm_label = lv_label_create(btn_confirm);
    lv_label_set_text(confirm_label, "确认添加");
    lv_obj_set_style_text_color(confirm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(confirm_label, &my_font_cn_16, 0);
    lv_obj_center(confirm_label);
}

/**
 * @brief 显示Lora节点状态视图
 */
static void show_lora_status_view(lv_event_t *e)
{
    (void)e;

    /* 如果已经显示，直接返回 */
    if (g_lora_status_view) {
        return;
    }

    /* 隐藏设备管理视图 */
    if (g_device_management_view) {
        lv_obj_add_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);
    }

    /* 隐藏顶部标签按钮 */
    for (int i = 0; i < 6; i++) {
        if (g_tab_buttons[i]) {
            lv_obj_add_flag(g_tab_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 获取settings page的parent */
    lv_obj_t *container = lv_obj_get_parent(g_device_management_view);
    lv_obj_t *settings_parent = lv_obj_get_parent(container);

    /* 创建Lora状态视图 - 全屏覆盖标签页+内容区域 */
    g_lora_status_view = lv_obj_create(settings_parent);
    lv_obj_set_size(g_lora_status_view, 1168, 730);
    lv_obj_set_pos(g_lora_status_view, 5, 0);
    lv_obj_set_style_bg_color(g_lora_status_view, lv_color_hex(0xe8e8e8), 0);
    lv_obj_set_style_pad_all(g_lora_status_view, 0, 0);
    lv_obj_set_style_border_width(g_lora_status_view, 0, 0);
    lv_obj_clear_flag(g_lora_status_view, LV_OBJ_FLAG_SCROLLABLE);

    /* 透明标题栏 */
    lv_obj_t *header = lv_obj_create(g_lora_status_view);
    lv_obj_set_size(header, 1156, 60);
    lv_obj_set_pos(header, 6, 5);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    /* 返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 50, 50);
    lv_obj_set_pos(btn_back, 10, 5);
    lv_obj_set_style_bg_color(btn_back, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_back, 25, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_add_event_cb(btn_back, lora_status_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(btn_back);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(back_label, &lv_font_montserrat_24, 0);
    lv_obj_center(back_label);

    /* 标题 */
    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Lora节点状态");
    lv_obj_set_pos(title, 80, 18);
    lv_obj_set_style_text_font(title, &my_font_cn_16, 0);  /* 普通字体 */
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    /* 刷新按钮 */
    lv_obj_t *btn_refresh = lv_btn_create(header);
    lv_obj_set_size(btn_refresh, 100, 45);
    lv_obj_set_pos(btn_refresh, 1040, 8);
    lv_obj_set_style_bg_color(btn_refresh, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_refresh, 0, 0);
    lv_obj_set_style_radius(btn_refresh, 8, 0);
    lv_obj_add_event_cb(btn_refresh, lora_status_refresh_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *refresh_label = lv_label_create(btn_refresh);
    lv_label_set_text(refresh_label, "刷新");
    lv_obj_set_style_text_color(refresh_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(refresh_label, &my_font_cn_16, 0);
    lv_obj_center(refresh_label);

    /* 白色内容区域 */
    lv_obj_t *white_area = lv_obj_create(g_lora_status_view);
    lv_obj_set_size(white_area, 1156, 649);
    lv_obj_set_pos(white_area, 6, 75);
    lv_obj_set_style_bg_color(white_area, lv_color_white(), 0);
    lv_obj_set_style_pad_all(white_area, 20, 0);
    lv_obj_set_style_border_width(white_area, 0, 0);
    lv_obj_clear_flag(white_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 表格标题行 */
    const char *headers[] = {"序号", "名称", "编号", "电池电压(V)", "信号强度(dB)", "状态", "最后采集时间"};
    int header_x[] = {20, 100, 240, 380, 530, 690, 810};
    int header_widths[] = {60, 120, 120, 130, 140, 100, 220};

    for (int i = 0; i < 7; i++) {
        lv_obj_t *header_label = lv_label_create(white_area);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_pos(header_label, header_x[i], 10);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);  /* 普通字体，不用粗体 */
        lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
    }

    /* 分隔线 */
    lv_obj_t *line_obj = lv_obj_create(white_area);
    lv_obj_set_size(line_obj, 1116, 2);
    lv_obj_set_pos(line_obj, 0, 45);
    lv_obj_set_style_bg_color(line_obj, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_border_width(line_obj, 0, 0);

    /* 分页控件 - 在白色区域底部居中 */
    int pagination_y = 574;
    int center_x = 338;

    lv_obj_t *first_page_btn = lv_btn_create(white_area);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, center_x, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);  /* 浅灰色 */
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);  /* 深灰色文字 */
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    lv_obj_t *prev_page_btn = lv_btn_create(white_area);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, center_x + 80, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);  /* 浅灰色 */
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);  /* 深灰色文字 */
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    lv_obj_t *page_info = lv_label_create(white_area);
    lv_label_set_text(page_info, "0/0");
    lv_obj_set_pos(page_info, center_x + 180, pagination_y + 8);
    lv_obj_set_style_text_font(page_info, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_info, lv_color_black(), 0);

    lv_obj_t *next_page_btn = lv_btn_create(white_area);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, center_x + 230, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);  /* 浅灰色 */
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);  /* 深灰色文字 */
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    lv_obj_t *last_page_btn = lv_btn_create(white_area);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, center_x + 320, pagination_y);
    lv_obj_set_style_bg_color(last_page_btn, lv_color_hex(0xe0e0e0), 0);  /* 浅灰色 */
    lv_obj_set_style_border_width(last_page_btn, 0, 0);
    lv_obj_set_style_radius(last_page_btn, 5, 0);

    lv_obj_t *last_label = lv_label_create(last_page_btn);
    lv_label_set_text(last_label, "尾页");
    lv_obj_set_style_text_color(last_label, lv_color_hex(0x333333), 0);  /* 深灰色文字 */
    lv_obj_set_style_text_font(last_label, &my_font_cn_16, 0);
    lv_obj_center(last_label);
}

/**
 * @brief 批量添加按钮回调
 */
static void btn_batch_add_cb(lv_event_t *e)
{
    (void)e;
    /* TODO: 实现批量添加逻辑 */
}

/**
 * @brief Lora状态返回按钮回调
 */
static void lora_status_back_cb(lv_event_t *e)
{
    (void)e;

    /* 删除Lora状态视图 */
    if (g_lora_status_view) {
        lv_obj_del(g_lora_status_view);
        g_lora_status_view = NULL;
    }

    /* 恢复显示顶部标签按钮 */
    for (int i = 0; i < 6; i++) {
        if (g_tab_buttons[i]) {
            lv_obj_clear_flag(g_tab_buttons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    /* 显示设备管理视图 */
    if (g_device_management_view) {
        lv_obj_clear_flag(g_device_management_view, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief Lora状态刷新按钮回调
 */
static void lora_status_refresh_cb(lv_event_t *e)
{
    (void)e;
    /* TODO: 实现刷新Lora节点状态数据逻辑 */
}

/**
 * @brief 添加设备确认回调
 */
static void add_device_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* TODO: 获取输入值并添加到设备列表 */
    char device_type[64];
    lv_dropdown_get_selected_str(g_device_type_dropdown, device_type, sizeof(device_type));
    const char *device_name = lv_textarea_get_text(g_device_name_input);
    const char *device_id = lv_textarea_get_text(g_device_id_input);

    /* 关闭对话框 */
    if (g_add_device_bg) {
        lv_obj_del(g_add_device_bg);
        g_add_device_bg = NULL;
        g_add_device_dialog = NULL;
        g_device_type_dropdown = NULL;
        g_device_name_input = NULL;
        g_device_id_input = NULL;
        g_device_port_dropdown = NULL;
    }
}

/**
 * @brief 添加设备取消回调
 */
static void add_device_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* 关闭对话框 */
    if (g_add_device_bg) {
        lv_obj_del(g_add_device_bg);
        g_add_device_bg = NULL;
        g_add_device_dialog = NULL;
        g_device_type_dropdown = NULL;
        g_device_name_input = NULL;
        g_device_id_input = NULL;
        g_device_port_dropdown = NULL;
    }
}

/**
 * @brief 添加传感器确认回调
 */
static void add_sensor_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* TODO: 获取输入值并添加到传感器列表 */
    char sensor_type[64];
    lv_dropdown_get_selected_str(g_sensor_type_dropdown, sensor_type, sizeof(sensor_type));
    const char *sensor_name = lv_textarea_get_text(g_sensor_name_input);
    const char *sensor_id = lv_textarea_get_text(g_sensor_id_input);

    /* 关闭对话框 */
    if (g_add_sensor_bg) {
        lv_obj_del(g_add_sensor_bg);
        g_add_sensor_bg = NULL;
        g_add_sensor_dialog = NULL;
        g_sensor_type_dropdown = NULL;
        g_sensor_name_input = NULL;
        g_sensor_parent_dropdown = NULL;
        g_sensor_id_input = NULL;
    }
}

/**
 * @brief 添加传感器取消回调
 */
static void add_sensor_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* 关闭对话框 */
    if (g_add_sensor_bg) {
        lv_obj_del(g_add_sensor_bg);
        g_add_sensor_bg = NULL;
        g_add_sensor_dialog = NULL;
        g_sensor_type_dropdown = NULL;
        g_sensor_name_input = NULL;
        g_sensor_parent_dropdown = NULL;
        g_sensor_id_input = NULL;
    }
}

/**
 * @brief 背景点击回调 - 关闭添加传感器对话框
 */
static void add_sensor_bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);

    /* 只有点击背景本身才关闭 */
    if (target == g_add_sensor_bg) {
        add_sensor_cancel_cb(e);
    }
}

/**
 * @brief 显示搜索传感器对话框
 */
static void show_search_sensor_dialog(lv_event_t *e)
{
    (void)e;

    /* 如果对话框已存在，先关闭 */
    if (g_search_sensor_bg) {
        return;
    }

    ui_main_t *ui_main = ui_get_main();
    if (!ui_main || !ui_main->screen) {
        return;
    }

    /* 创建透明背景遮罩 */
    g_search_sensor_bg = lv_obj_create(ui_main->screen);
    lv_obj_set_size(g_search_sensor_bg, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(g_search_sensor_bg, 0, 0);
    lv_obj_set_style_bg_opa(g_search_sensor_bg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_search_sensor_bg, 0, 0);
    lv_obj_clear_flag(g_search_sensor_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(g_search_sensor_bg, search_sensor_bg_click_cb, LV_EVENT_CLICKED, NULL);

    /* 创建对话框 - 居中显示 */
    g_search_sensor_dialog = lv_obj_create(g_search_sensor_bg);
    int dialog_width = 630;
    int dialog_height = 380;
    lv_obj_set_size(g_search_sensor_dialog, dialog_width, dialog_height);
    lv_obj_center(g_search_sensor_dialog);
    lv_obj_set_style_bg_color(g_search_sensor_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_search_sensor_dialog, 3, 0);
    lv_obj_set_style_border_color(g_search_sensor_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(g_search_sensor_dialog, 10, 0);
    lv_obj_set_style_pad_all(g_search_sensor_dialog, 20, 0);
    lv_obj_clear_flag(g_search_sensor_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(g_search_sensor_dialog);
    lv_label_set_text(title, "自动搜索传感器");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_pos(title, (dialog_width - 144) / 2, 30);

    /* 提示文字 */
    lv_obj_t *notice = lv_label_create(g_search_sensor_dialog);
    lv_label_set_text(notice, "请注意：自动搜索的传感器将全部覆盖旧的\n传感器！");
    lv_obj_set_style_text_font(notice, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(notice, lv_color_black(), 0);
    lv_obj_set_style_text_align(notice, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(notice, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(notice, dialog_width - 80);
    lv_obj_align(notice, LV_ALIGN_CENTER, 0, 0);

    /* 取消搜索按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(g_search_sensor_dialog);
    lv_obj_set_size(btn_cancel, 180, 50);
    lv_obj_set_pos(btn_cancel, 80, 260);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 8, 0);
    lv_obj_add_event_cb(btn_cancel, search_sensor_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cancel_label = lv_label_create(btn_cancel);
    lv_label_set_text(cancel_label, "取消搜索");
    lv_obj_set_style_text_color(cancel_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(cancel_label, &my_font_cn_16, 0);
    lv_obj_center(cancel_label);

    /* 确认搜索按钮 - 橙色 */
    lv_obj_t *btn_confirm = lv_btn_create(g_search_sensor_dialog);
    lv_obj_set_size(btn_confirm, 180, 50);
    lv_obj_set_pos(btn_confirm, 350, 260);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xff9800), 0);  /* 橙色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 8, 0);
    lv_obj_add_event_cb(btn_confirm, search_sensor_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *confirm_label = lv_label_create(btn_confirm);
    lv_label_set_text(confirm_label, "确认搜索");
    lv_obj_set_style_text_color(confirm_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(confirm_label, &my_font_cn_16, 0);
    lv_obj_center(confirm_label);
}

/**
 * @brief 搜索传感器确认回调
 */
static void search_sensor_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* TODO: 实现自动搜索传感器逻辑 */

    /* 关闭对话框 */
    if (g_search_sensor_bg) {
        lv_obj_del(g_search_sensor_bg);
        g_search_sensor_bg = NULL;
        g_search_sensor_dialog = NULL;
    }
}

/**
 * @brief 搜索传感器取消回调
 */
static void search_sensor_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* 关闭对话框 */
    if (g_search_sensor_bg) {
        lv_obj_del(g_search_sensor_bg);
        g_search_sensor_bg = NULL;
        g_search_sensor_dialog = NULL;
    }
}

/**
 * @brief 背景点击回调 - 关闭搜索传感器对话框
 */
static void search_sensor_bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);

    /* 只有点击背景本身才关闭 */
    if (target == g_search_sensor_bg) {
        search_sensor_cancel_cb(e);
    }
}

/**
 * @brief 背景点击回调 - 关闭添加设备对话框
 */
static void add_device_bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);

    /* 只有点击背景本身才关闭 */
    if (target == g_add_device_bg) {
        add_device_cancel_cb(e);
    }
}

/**
 * @brief 创建4G设置表单内容
 */
static void create_4g_settings_form(lv_obj_t *parent)
{
    int y_pos = 20;
    int label_width = 140;
    int value_width = 550;

    /* 左列：标签 */
    const char *labels[] = {"卡状态", "运营商", "ICCID", "IMEI", "信号强度", "网络状态", "ip地址"};
    /* 右列：值 (无卡则用---占位) */
    const char *values[] = {"---", "---", "---", "---", "---", "---", "---"};

    for (int i = 0; i < 7; i++) {
        /* 创建一个行容器 */
        lv_obj_t *row = lv_obj_create(parent);
        lv_obj_set_size(row, 750, 60);
        lv_obj_set_pos(row, 30, y_pos);
        lv_obj_set_style_bg_color(row, lv_color_hex(0xf0f0f0), 0);  /* 浅灰色背景 */
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_radius(row, 5, 0);
        lv_obj_set_style_pad_all(row, 15, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        /* 左侧标签 */
        lv_obj_t *label = lv_label_create(row);
        lv_label_set_text(label, labels[i]);
        lv_obj_set_pos(label, 10, 18);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x333333), 0);

        /* 右侧值 - 无卡时全部显示灰色 */
        lv_obj_t *value = lv_label_create(row);
        lv_label_set_text(value, values[i]);
        lv_obj_set_pos(value, 470, 18);
        lv_obj_set_style_text_font(value, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(value, lv_color_hex(0x999999), 0);  /* 无卡时全部灰色 */

        y_pos += 70;
    }
}

/**
 * @brief 创建WIFI设置表单内容
 */
static void create_wifi_settings_form(lv_obj_t *parent)
{
    /* 顶部连接状态区域 */
    lv_obj_t *status_area = lv_obj_create(parent);
    lv_obj_set_size(status_area, 850, 60);
    lv_obj_set_pos(status_area, 0, 0);
    lv_obj_set_style_bg_color(status_area, lv_color_hex(0xe8f4f8), 0);  /* 浅蓝色背景 */
    lv_obj_set_style_border_width(status_area, 0, 0);
    lv_obj_set_style_radius(status_area, 8, 0);
    lv_obj_set_style_pad_all(status_area, 15, 0);
    lv_obj_clear_flag(status_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 连接状态文字 */
    lv_obj_t *status_label = lv_label_create(status_area);
    lv_label_set_text(status_label, "已连接:  无");
    lv_obj_set_pos(status_label, 15, 15);
    lv_obj_set_style_text_font(status_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x333333), 0);

    /* 搜索WIFI按钮 - 绿色 */
    lv_obj_t *btn_search = lv_btn_create(parent);
    lv_obj_set_size(btn_search, 140, 50);
    lv_obj_set_pos(btn_search, 710, 5);
    lv_obj_set_style_bg_color(btn_search, lv_color_hex(0x27ae60), 0);  /* 绿色 */
    lv_obj_set_style_border_width(btn_search, 0, 0);
    lv_obj_set_style_radius(btn_search, 8, 0);

    lv_obj_t *search_label = lv_label_create(btn_search);
    lv_label_set_text(search_label, "搜索WIFI");
    lv_obj_set_style_text_color(search_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(search_label, &my_font_cn_16, 0);
    lv_obj_center(search_label);

    /* Attach WiFi scan callback */
    lv_obj_add_event_cb(btn_search, ui_wifi_search_btn_cb, LV_EVENT_CLICKED, NULL);

    /* WIFI列表表格区域 */
    lv_obj_t *table_container = lv_obj_create(parent);
    lv_obj_set_size(table_container, 850, 540);
    lv_obj_set_pos(table_container, 0, 80);
    lv_obj_set_style_bg_color(table_container, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_container, 1, 0);
    lv_obj_set_style_border_color(table_container, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(table_container, 5, 0);
    lv_obj_set_style_pad_all(table_container, 0, 0);
    lv_obj_clear_flag(table_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头 */
    lv_obj_t *table_header = lv_obj_create(table_container);
    lv_obj_set_size(table_header, 850, 45);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头列标题 */
    const char *headers[] = {"WIFI名称", "信号强度", "连接开关"};
    int header_x[] = {50, 350, 650};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 12);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    /* 提示：无WIFI列表时显示空状态 */
    lv_obj_t *empty_label = lv_label_create(table_container);
    lv_label_set_text(empty_label, "未搜索到WIFI，请点击右上角\"搜索WIFI\"按钮");
    lv_obj_set_pos(empty_label, 200, 250);
    lv_obj_set_style_text_font(empty_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(empty_label, lv_color_hex(0x999999), 0);

    /* Register table objects with wifi UI module */
    ui_wifi_set_table_objects(table_container, empty_label, status_label);
}

/**
 * @brief 创建密码设置表单内容
 */
static void create_password_settings_form(lv_obj_t *parent)
{
    int y_pos = 120;
    int label_x = 100;
    int input_x = 220;
    int input_width = 500;

    /* 原密码标签 */
    lv_obj_t *label_old_pwd = lv_label_create(parent);
    lv_label_set_text(label_old_pwd, "原密码:");
    lv_obj_set_pos(label_old_pwd, label_x, y_pos + 12);
    lv_obj_set_style_text_font(label_old_pwd, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_old_pwd, lv_color_hex(0x333333), 0);

    /* 原密码输入框 */
    lv_obj_t *input_old_pwd = lv_textarea_create(parent);
    lv_obj_set_size(input_old_pwd, input_width, 45);
    lv_obj_set_pos(input_old_pwd, input_x, y_pos);
    lv_textarea_set_text(input_old_pwd, "");
    lv_textarea_set_placeholder_text(input_old_pwd, "请输入6位密码");
    lv_textarea_set_one_line(input_old_pwd, true);
    lv_textarea_set_password_mode(input_old_pwd, true);
    lv_obj_set_style_bg_color(input_old_pwd, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_old_pwd, 1, 0);
    lv_obj_set_style_border_color(input_old_pwd, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(input_old_pwd, 8, 0);
    lv_obj_set_style_text_font(input_old_pwd, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_old_pwd, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_old_pwd, 0, 0);
    lv_obj_set_style_pad_right(input_old_pwd, 0, 0);
    lv_obj_set_style_pad_top(input_old_pwd, 4, 0);
    lv_obj_set_style_pad_bottom(input_old_pwd, 0, 0);
    lv_obj_add_event_cb(input_old_pwd, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    y_pos += 80;

    /* 新密码标签 */
    lv_obj_t *label_new_pwd = lv_label_create(parent);
    lv_label_set_text(label_new_pwd, "新密码:");
    lv_obj_set_pos(label_new_pwd, label_x, y_pos + 12);
    lv_obj_set_style_text_font(label_new_pwd, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_new_pwd, lv_color_hex(0x333333), 0);

    /* 新密码输入框 */
    lv_obj_t *input_new_pwd = lv_textarea_create(parent);
    lv_obj_set_size(input_new_pwd, input_width, 45);
    lv_obj_set_pos(input_new_pwd, input_x, y_pos);
    lv_textarea_set_text(input_new_pwd, "");
    lv_textarea_set_placeholder_text(input_new_pwd, "请输入6位密码");
    lv_textarea_set_one_line(input_new_pwd, true);
    lv_textarea_set_password_mode(input_new_pwd, true);
    lv_obj_set_style_bg_color(input_new_pwd, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_new_pwd, 1, 0);
    lv_obj_set_style_border_color(input_new_pwd, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(input_new_pwd, 8, 0);
    lv_obj_set_style_text_font(input_new_pwd, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_new_pwd, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_new_pwd, 0, 0);
    lv_obj_set_style_pad_right(input_new_pwd, 0, 0);
    lv_obj_set_style_pad_top(input_new_pwd, 4, 0);
    lv_obj_set_style_pad_bottom(input_new_pwd, 0, 0);
    lv_obj_add_event_cb(input_new_pwd, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    y_pos += 80;

    /* 确认新密码标签 */
    lv_obj_t *label_confirm_pwd = lv_label_create(parent);
    lv_label_set_text(label_confirm_pwd, "确认新密码:");
    lv_obj_set_pos(label_confirm_pwd, label_x, y_pos + 12);
    lv_obj_set_style_text_font(label_confirm_pwd, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_confirm_pwd, lv_color_hex(0x333333), 0);

    /* 确认新密码输入框 */
    lv_obj_t *input_confirm_pwd = lv_textarea_create(parent);
    lv_obj_set_size(input_confirm_pwd, input_width, 45);
    lv_obj_set_pos(input_confirm_pwd, input_x, y_pos);
    lv_textarea_set_text(input_confirm_pwd, "");
    lv_textarea_set_placeholder_text(input_confirm_pwd, "请输入6位密码");
    lv_textarea_set_one_line(input_confirm_pwd, true);
    lv_textarea_set_password_mode(input_confirm_pwd, true);
    lv_obj_set_style_bg_color(input_confirm_pwd, lv_color_white(), 0);
    lv_obj_set_style_border_width(input_confirm_pwd, 1, 0);
    lv_obj_set_style_border_color(input_confirm_pwd, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_all(input_confirm_pwd, 8, 0);
    lv_obj_set_style_text_font(input_confirm_pwd, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(input_confirm_pwd, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(input_confirm_pwd, 0, 0);
    lv_obj_set_style_pad_right(input_confirm_pwd, 0, 0);
    lv_obj_set_style_pad_top(input_confirm_pwd, 4, 0);
    lv_obj_set_style_pad_bottom(input_confirm_pwd, 0, 0);
    lv_obj_add_event_cb(input_confirm_pwd, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 底部按钮 */
    int btn_y = 500;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
    lv_obj_set_size(btn_cancel, 140, 45);
    lv_obj_set_pos(btn_cancel, 230, btn_y);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 22, 0);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消设置");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 保存设置按钮 */
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 140, 45);
    lv_obj_set_pos(btn_save, 400, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_save, 0, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建显示设置表单内容
 */
static void create_display_settings_form(lv_obj_t *parent)
{
    int y_pos = 80;
    int label_x = 80;
    int control_x = 240;

    /* 自动关闭屏幕标签 */
    lv_obj_t *label_screen_timeout = lv_label_create(parent);
    lv_label_set_text(label_screen_timeout, "自动关闭屏幕");
    lv_obj_set_pos(label_screen_timeout, label_x, y_pos + 10);
    lv_obj_set_style_text_font(label_screen_timeout, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_screen_timeout, lv_color_hex(0x333333), 0);

    /* 自动关闭屏幕下拉框 */
    lv_obj_t *dd_screen_timeout = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd_screen_timeout, "1分钟\n5分钟\n10分钟\n15分钟\n30分钟\n永不");
    lv_dropdown_set_selected(dd_screen_timeout, 2);  /* 默认选择10分钟 */
    lv_obj_set_size(dd_screen_timeout, 550, 45);
    lv_obj_set_style_text_font(dd_screen_timeout, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dd_screen_timeout), &my_font_cn_16, 0);
    lv_obj_set_pos(dd_screen_timeout, control_x, y_pos);
    lv_obj_set_style_bg_color(dd_screen_timeout, lv_color_white(), 0);
    lv_obj_set_style_border_width(dd_screen_timeout, 1, 0);
    lv_obj_set_style_border_color(dd_screen_timeout, lv_color_hex(0xcccccc), 0);

    y_pos += 100;

    /* 屏幕亮度调节标签 */
    lv_obj_t *label_brightness = lv_label_create(parent);
    lv_label_set_text(label_brightness, "屏幕亮度调节");
    lv_obj_set_pos(label_brightness, label_x, y_pos + 5);
    lv_obj_set_style_text_font(label_brightness, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_brightness, lv_color_hex(0x333333), 0);

    /* 亮度滑动条 */
    lv_obj_t *slider_brightness = lv_slider_create(parent);
    lv_obj_set_size(slider_brightness, 550, 15);
    lv_obj_set_pos(slider_brightness, control_x, y_pos);
    lv_slider_set_range(slider_brightness, 0, 100);
    lv_slider_set_value(slider_brightness, 80, LV_ANIM_OFF);  /* 默认亮度80% */

    /* 滑动条主色调 - 蓝色 */
    lv_obj_set_style_bg_color(slider_brightness, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_brightness, lv_color_hex(0xe0e0e0), LV_PART_MAIN);

    /* 滑动条旋钮 */
    lv_obj_set_style_bg_color(slider_brightness, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_border_color(slider_brightness, COLOR_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_border_width(slider_brightness, 3, LV_PART_KNOB);

    /* 底部按钮 */
    int btn_y = 500;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
    lv_obj_set_size(btn_cancel, 140, 45);
    lv_obj_set_pos(btn_cancel, 280, btn_y);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 22, 0);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消设置");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 保存设置按钮 */
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 140, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_save, 0, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);

    /* Bind display settings event callbacks */
    lv_obj_add_event_cb(slider_brightness, ui_display_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(btn_save, ui_display_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_cancel, ui_display_cancel_cb, LV_EVENT_CLICKED, NULL);

    /* Pass control references to ui_display for save/cancel logic */
    ui_display_set_controls(slider_brightness, dd_screen_timeout);
}

/**
 * @brief 恢复出厂/重启/关机按钮空回调（功能暂未实现）
 */
static void factory_reset_noop_cb(lv_event_t *e)
{
    (void)e;
}

/**
 * @brief 创建恢复出厂表单内容
 */
static void create_factory_reset_form(lv_obj_t *parent)
{
    /* 计算居中位置 */
    int form_width = 913;
    int btn_width = 300;
    int btn_height = 60;
    int btn_x = (form_width - btn_width) / 2;
    int start_y = 150;  /* 从上往下的起始位置 */
    int btn_spacing = 80;  /* 按钮之间的间距 */

    /* 恢复出厂按钮 - 绿色 */
    lv_obj_t *btn_factory_reset = lv_btn_create(parent);
    lv_obj_set_size(btn_factory_reset, btn_width, btn_height);
    lv_obj_set_pos(btn_factory_reset, btn_x, start_y);
    lv_obj_set_style_bg_color(btn_factory_reset, lv_color_hex(0x27ae60), 0);  /* 绿色 */
    lv_obj_set_style_border_width(btn_factory_reset, 0, 0);
    lv_obj_set_style_radius(btn_factory_reset, 10, 0);

    lv_obj_t *label_factory_reset = lv_label_create(btn_factory_reset);
    lv_label_set_text(label_factory_reset, "恢复出厂");
    lv_obj_set_style_text_color(label_factory_reset, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_factory_reset, &my_font_cn_16, 0);
    lv_obj_center(label_factory_reset);
    lv_obj_add_event_cb(btn_factory_reset, factory_reset_noop_cb, LV_EVENT_CLICKED, NULL);

    /* 重启按钮 - 橙色 */
    lv_obj_t *btn_restart = lv_btn_create(parent);
    lv_obj_set_size(btn_restart, btn_width, btn_height);
    lv_obj_set_pos(btn_restart, btn_x, start_y + btn_spacing);
    lv_obj_set_style_bg_color(btn_restart, lv_color_hex(0xff9800), 0);  /* 橙色 */
    lv_obj_set_style_border_width(btn_restart, 0, 0);
    lv_obj_set_style_radius(btn_restart, 10, 0);

    lv_obj_t *label_restart = lv_label_create(btn_restart);
    lv_label_set_text(label_restart, "重启");
    lv_obj_set_style_text_color(label_restart, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_restart, &my_font_cn_16, 0);
    lv_obj_center(label_restart);
    lv_obj_add_event_cb(btn_restart, factory_reset_noop_cb, LV_EVENT_CLICKED, NULL);

    /* 关机按钮 - 红色 */
    lv_obj_t *btn_shutdown = lv_btn_create(parent);
    lv_obj_set_size(btn_shutdown, btn_width, btn_height);
    lv_obj_set_pos(btn_shutdown, btn_x, start_y + btn_spacing * 2);
    lv_obj_set_style_bg_color(btn_shutdown, lv_color_hex(0xe74c3c), 0);  /* 红色 */
    lv_obj_set_style_border_width(btn_shutdown, 0, 0);
    lv_obj_set_style_radius(btn_shutdown, 10, 0);

    lv_obj_t *label_shutdown = lv_label_create(btn_shutdown);
    lv_label_set_text(label_shutdown, "关机");
    lv_obj_set_style_text_color(label_shutdown, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_shutdown, &my_font_cn_16, 0);
    lv_obj_center(label_shutdown);
    lv_obj_add_event_cb(btn_shutdown, factory_reset_noop_cb, LV_EVENT_CLICKED, NULL);
}

/**
 * @brief 创建检查更新表单内容
 */
static void create_check_update_form(lv_obj_t *parent)
{
    int y_pos = 80;
    int label_x = 50;
    int value_x = 400;

    /* 当前软件版本号 */
    lv_obj_t *label_current_version = lv_label_create(parent);
    lv_label_set_text(label_current_version, "当前软件版本号:");
    lv_obj_set_pos(label_current_version, label_x, y_pos);
    lv_obj_set_style_text_font(label_current_version, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_current_version, lv_color_hex(0x333333), 0);

    lv_obj_t *value_current_version = lv_label_create(parent);
    lv_label_set_text(value_current_version, "1.0.3");
    lv_obj_set_pos(value_current_version, value_x, y_pos);
    lv_obj_set_style_text_font(value_current_version, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(value_current_version, lv_color_hex(0x333333), 0);

    y_pos += 60;

    /* 可升级版本号 */
    lv_obj_t *label_upgrade_version = lv_label_create(parent);
    lv_label_set_text(label_upgrade_version, "可升级版本号:");
    lv_obj_set_pos(label_upgrade_version, label_x, y_pos);
    lv_obj_set_style_text_font(label_upgrade_version, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_upgrade_version, lv_color_hex(0x333333), 0);

    lv_obj_t *value_upgrade_version = lv_label_create(parent);
    lv_label_set_text(value_upgrade_version, "none");
    lv_obj_set_pos(value_upgrade_version, value_x, y_pos);
    lv_obj_set_style_text_font(value_upgrade_version, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(value_upgrade_version, lv_color_hex(0x333333), 0);

    y_pos += 80;

    /* 进度条 */
    lv_obj_t *progress_bar = lv_bar_create(parent);
    lv_obj_set_size(progress_bar, 800, 30);
    lv_obj_set_pos(progress_bar, 50, y_pos);
    lv_bar_set_value(progress_bar, 0, LV_ANIM_OFF);  /* 0% */
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0xa8d8ea), 0);  /* 浅蓝色背景 */
    lv_obj_set_style_bg_color(progress_bar, lv_color_hex(0x70c1d8), LV_PART_INDICATOR);  /* 深蓝色进度 */

    /* 进度百分比文字 */
    lv_obj_t *progress_label = lv_label_create(parent);
    lv_label_set_text(progress_label, "0%");
    lv_obj_set_pos(progress_label, 420, y_pos + 3);
    lv_obj_set_style_text_font(progress_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(progress_label, lv_color_white(), 0);

    /* 检查更新按钮 - 蓝色，居中靠下 */
    int btn_y = 520;
    lv_obj_t *btn_check_update = lv_btn_create(parent);
    lv_obj_set_size(btn_check_update, 180, 50);
    lv_obj_set_pos(btn_check_update, (913 - 180) / 2, btn_y);
    lv_obj_set_style_bg_color(btn_check_update, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_check_update, 0, 0);
    lv_obj_set_style_radius(btn_check_update, 25, 0);

    lv_obj_t *label_check_update = lv_label_create(btn_check_update);
    lv_label_set_text(label_check_update, "检查更新");
    lv_obj_set_style_text_color(label_check_update, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_check_update, &my_font_cn_16, 0);
    lv_obj_center(label_check_update);
}

/**
 * @brief 创建网络信息表单内容
 */
static void create_network_info_form(lv_obj_t *parent)
{
    /* 文本显示区域 - 白色背景，占据大部分空间 */
    lv_obj_t *text_area = lv_textarea_create(parent);
    lv_obj_set_size(text_area, 850, 520);
    lv_obj_set_pos(text_area, 0, 0);
    lv_obj_set_style_bg_color(text_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(text_area, 1, 0);
    lv_obj_set_style_border_color(text_area, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(text_area, 5, 0);
    lv_obj_set_style_pad_all(text_area, 10, 0);
    lv_obj_set_style_text_font(text_area, &my_font_cn_16, 0);
    lv_textarea_set_text(text_area, "");  /* 初始为空 */

    /* 底部按钮区域 */
    int btn_y = 550;

    /* 清空文本信息按钮 - 灰色 */
    lv_obj_t *btn_clear = lv_btn_create(parent);
    lv_obj_set_size(btn_clear, 180, 50);
    lv_obj_set_pos(btn_clear, 200, btn_y);
    lv_obj_set_style_bg_color(btn_clear, lv_color_hex(0xa0a0a0), 0);
    lv_obj_set_style_border_width(btn_clear, 0, 0);
    lv_obj_set_style_radius(btn_clear, 25, 0);

    lv_obj_t *label_clear = lv_label_create(btn_clear);
    lv_label_set_text(label_clear, "清空文本信息");
    lv_obj_set_style_text_color(label_clear, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_clear, &my_font_cn_16, 0);
    lv_obj_center(label_clear);

    /* 获取网络信息按钮 - 蓝色 */
    lv_obj_t *btn_get_info = lv_btn_create(parent);
    lv_obj_set_size(btn_get_info, 180, 50);
    lv_obj_set_pos(btn_get_info, 470, btn_y);
    lv_obj_set_style_bg_color(btn_get_info, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(btn_get_info, 0, 0);
    lv_obj_set_style_radius(btn_get_info, 25, 0);

    lv_obj_t *label_get_info = lv_label_create(btn_get_info);
    lv_label_set_text(label_get_info, "获取网络信息");
    lv_obj_set_style_text_color(label_get_info, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_get_info, &my_font_cn_16, 0);
    lv_obj_center(label_get_info);

    /* Bind network info event callbacks */
    lv_obj_add_event_cb(btn_get_info, ui_network_get_info_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_clear, ui_network_clear_info_btn_cb, LV_EVENT_CLICKED, NULL);
    ui_network_set_info_textarea(text_area);
}

/**
 * @brief 创建系统信息表单内容
 */
static void create_system_info_form(lv_obj_t *parent)
{
    int y_pos = 80;
    int label_x = 80;
    int value_x = 300;

    /* 系统名称 */
    lv_obj_t *label_system_name = lv_label_create(parent);
    lv_label_set_text(label_system_name, "系统名称:");
    lv_obj_set_pos(label_system_name, label_x, y_pos);
    lv_obj_set_style_text_font(label_system_name, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_system_name, lv_color_hex(0x333333), 0);

    lv_obj_t *value_system_name = lv_label_create(parent);
    lv_label_set_text(value_system_name, "智能灌溉中心");
    lv_obj_set_pos(value_system_name, value_x, y_pos);
    lv_obj_set_style_text_font(value_system_name, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(value_system_name, lv_color_hex(0x333333), 0);

    y_pos += 80;

    /* 设备编号 */
    lv_obj_t *label_device_id = lv_label_create(parent);
    lv_label_set_text(label_device_id, "设备编号:");
    lv_obj_set_pos(label_device_id, label_x, y_pos);
    lv_obj_set_style_text_font(label_device_id, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_device_id, lv_color_hex(0x333333), 0);

    lv_obj_t *value_device_id = lv_label_create(parent);
    lv_label_set_text(value_device_id, "A7B7AC754429E8B");
    lv_obj_set_pos(value_device_id, value_x, y_pos);
    lv_obj_set_style_text_font(value_device_id, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(value_device_id, lv_color_hex(0x333333), 0);

    y_pos += 80;

    /* 硬件版本号 */
    lv_obj_t *label_hw_version = lv_label_create(parent);
    lv_label_set_text(label_hw_version, "硬件版本号:");
    lv_obj_set_pos(label_hw_version, label_x, y_pos);
    lv_obj_set_style_text_font(label_hw_version, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_hw_version, lv_color_hex(0x333333), 0);

    lv_obj_t *value_hw_version = lv_label_create(parent);
    lv_label_set_text(value_hw_version, "V2.0.00");
    lv_obj_set_pos(value_hw_version, value_x, y_pos);
    lv_obj_set_style_text_font(value_hw_version, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(value_hw_version, lv_color_hex(0x333333), 0);

    y_pos += 80;

    /* 软件版本号 */
    lv_obj_t *label_sw_version = lv_label_create(parent);
    lv_label_set_text(label_sw_version, "软件版本号:");
    lv_obj_set_pos(label_sw_version, label_x, y_pos);
    lv_obj_set_style_text_font(label_sw_version, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_sw_version, lv_color_hex(0x333333), 0);

    lv_obj_t *value_sw_version = lv_label_create(parent);
    lv_label_set_text(value_sw_version, "V1.0.3");
    lv_obj_set_pos(value_sw_version, value_x, y_pos);
    lv_obj_set_style_text_font(value_sw_version, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(value_sw_version, lv_color_hex(0x333333), 0);
}


/**
 * @brief 系统设置左侧菜单按钮回调
 */
static void system_settings_menu_cb(lv_event_t *e)
{
    int menu_index = (int)(intptr_t)lv_event_get_user_data(e);

    /* 更新所有菜单按钮的颜色 */
    for (int i = 0; i < 9; i++) {
        if (g_left_menu_buttons[i]) {
            if (i == menu_index) {
                /* 选中：深蓝色背景 */
                lv_obj_set_style_bg_color(g_left_menu_buttons[i], lv_color_hex(0x70c1d8), 0);
            } else {
                /* 未选中：浅蓝色背景 */
                lv_obj_set_style_bg_color(g_left_menu_buttons[i], lv_color_hex(0xa8d8ea), 0);
            }
        }
    }

    /* 根据选中的菜单项切换表单 */
    if (menu_index == 0) {
        /* 网口设置 */
        if (g_network_settings_form) lv_obj_clear_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    } else if (menu_index == 1) {
        /* 4G设置 */
        if (g_network_settings_form) lv_obj_add_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_clear_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    } else if (menu_index == 2) {
        /* WIFI设置 */
        if (g_network_settings_form) lv_obj_add_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_clear_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    } else if (menu_index == 3) {
        /* 密码设置 */
        if (g_network_settings_form) lv_obj_add_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_clear_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    } else if (menu_index == 4) {
        /* 显示设置 */
        if (g_network_settings_form) lv_obj_add_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_clear_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    } else if (menu_index == 5) {
        /* 恢复出厂 */
        if (g_network_settings_form) lv_obj_add_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_clear_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    } else if (menu_index == 6) {
        /* 检查更新 */
        if (g_network_settings_form) lv_obj_add_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_clear_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    } else if (menu_index == 7) {
        /* 网络信息 */
        if (g_network_settings_form) lv_obj_add_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_clear_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_add_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    } else if (menu_index == 8) {
        /* 系统信息 */
        if (g_network_settings_form) lv_obj_add_flag(g_network_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_4g_settings_form) lv_obj_add_flag(g_4g_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_wifi_settings_form) lv_obj_add_flag(g_wifi_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_password_settings_form) lv_obj_add_flag(g_password_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_display_settings_form) lv_obj_add_flag(g_display_settings_form, LV_OBJ_FLAG_HIDDEN);
        if (g_factory_reset_form) lv_obj_add_flag(g_factory_reset_form, LV_OBJ_FLAG_HIDDEN);
        if (g_check_update_form) lv_obj_add_flag(g_check_update_form, LV_OBJ_FLAG_HIDDEN);
        if (g_network_info_form) lv_obj_add_flag(g_network_info_form, LV_OBJ_FLAG_HIDDEN);
        if (g_system_info_form) lv_obj_clear_flag(g_system_info_form, LV_OBJ_FLAG_HIDDEN);
    }
}

/**
 * @brief 网口模式改变回调 - 启用/禁用输入框
 */
static void network_mode_change_cb(lv_event_t *e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);

    /* selected = 0: DHCP自动获取, selected = 1: 静态IP */
    if (selected == 0) {
        /* DHCP模式 - 禁用所有输入框 */
        if (g_net_input_ip) {
            lv_obj_add_state(g_net_input_ip, LV_STATE_DISABLED);
            lv_textarea_set_text(g_net_input_ip, "");
        }
        if (g_net_input_mask) {
            lv_obj_add_state(g_net_input_mask, LV_STATE_DISABLED);
            lv_textarea_set_text(g_net_input_mask, "");
        }
        if (g_net_input_gateway) {
            lv_obj_add_state(g_net_input_gateway, LV_STATE_DISABLED);
            lv_textarea_set_text(g_net_input_gateway, "");
        }
        if (g_net_input_dns) {
            lv_obj_add_state(g_net_input_dns, LV_STATE_DISABLED);
            lv_textarea_set_text(g_net_input_dns, "");
        }
    } else {
        /* 静态IP模式 - 启用所有输入框 */
        if (g_net_input_ip) {
            lv_obj_clear_state(g_net_input_ip, LV_STATE_DISABLED);
        }
        if (g_net_input_mask) {
            lv_obj_clear_state(g_net_input_mask, LV_STATE_DISABLED);
        }
        if (g_net_input_gateway) {
            lv_obj_clear_state(g_net_input_gateway, LV_STATE_DISABLED);
        }
        if (g_net_input_dns) {
            lv_obj_clear_state(g_net_input_dns, LV_STATE_DISABLED);
        }
    }
}

/**
 * @brief 输入框点击回调 - 显示数字键盘或26键键盘
 */
static void textarea_click_cb(lv_event_t *e)
{
    lv_obj_t *textarea = lv_event_get_target(e);

    /* 如果输入框被禁用，不显示键盘 */
    if (lv_obj_has_state(textarea, LV_STATE_DISABLED)) {
        return;
    }

    ui_main_t *ui_main = ui_get_main();

    if (textarea && ui_main && ui_main->screen) {
        /* 如果是灌区名称输入框，显示26键键盘 */
        if (textarea == g_zone_name_input) {
            ui_keyboard_show(textarea, ui_main->screen);
        } else {
            /* 其他输入框显示数字键盘 */
            ui_numpad_show(textarea, ui_main->screen);
        }
    }
}
