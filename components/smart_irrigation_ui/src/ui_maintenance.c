/**
 * @file ui_maintenance.c
 * @brief 维护界面实现
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include <stdio.h>
#include <string.h>

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_comm_params_layout(lv_obj_t *parent);
static void create_calibration_params_layout(lv_obj_t *parent);
static void create_algorithm_params_layout(lv_obj_t *parent);
static void create_tab_buttons(lv_obj_t *parent);
static void tab_btn_cb(lv_event_t *e);
static void comm_params_menu_cb(lv_event_t *e);
static void create_bus_settings_form(lv_obj_t *parent);
static void create_slave_settings_form(lv_obj_t *parent);
static void create_inverter_settings_form(lv_obj_t *parent);
static void create_remote_settings_form(lv_obj_t *parent);
static void create_node_settings_form(lv_obj_t *parent);
static void create_local_sync_form(lv_obj_t *parent);
static void create_flow_calibration_form(lv_obj_t *parent);
static void create_level_calibration_form(lv_obj_t *parent);
static void create_ecph_calibration_form(lv_obj_t *parent);
static void calibration_menu_cb(lv_event_t *e);
static void calib_switch_form(int menu_index);
static void create_pid_settings_form(lv_obj_t *parent);
static void create_channel_settings_form(lv_obj_t *parent);
static void create_filter_settings_form(lv_obj_t *parent);
static void algorithm_menu_cb(lv_event_t *e);
static void algo_switch_form(int menu_index);
static void textarea_click_cb(lv_event_t *e);

/*********************
 *  SHARED STYLES
 *********************/
static lv_style_t s_style_textarea;
static lv_style_t s_style_dropdown;
static lv_style_t s_style_label;
static bool s_styles_inited = false;

static void ensure_styles_init(void)
{
    if (s_styles_inited) return;

    lv_style_init(&s_style_textarea);
    lv_style_set_bg_color(&s_style_textarea, lv_color_white());
    lv_style_set_border_width(&s_style_textarea, 1);
    lv_style_set_border_color(&s_style_textarea, lv_color_hex(0xcccccc));
    lv_style_set_text_font(&s_style_textarea, &my_font_cn_16);
    lv_style_set_text_align(&s_style_textarea, LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_left(&s_style_textarea, 0);
    lv_style_set_pad_right(&s_style_textarea, 0);
    lv_style_set_pad_top(&s_style_textarea, 4);
    lv_style_set_pad_bottom(&s_style_textarea, 0);

    lv_style_init(&s_style_dropdown);
    lv_style_set_bg_color(&s_style_dropdown, lv_color_white());
    lv_style_set_border_width(&s_style_dropdown, 1);
    lv_style_set_border_color(&s_style_dropdown, lv_color_hex(0xcccccc));
    lv_style_set_text_font(&s_style_dropdown, &my_font_cn_16);

    lv_style_init(&s_style_label);
    lv_style_set_text_font(&s_style_label, &my_font_cn_16);

    s_styles_inited = true;
}

/** 创建带共享样式的 textarea（减少 ~10 行/个） */
static lv_obj_t* mt_textarea(lv_obj_t *parent, int w, int h, int x, int y, const char *text)
{
    lv_obj_t *ta = lv_textarea_create(parent);
    lv_obj_set_size(ta, w, h);
    lv_obj_set_pos(ta, x, y);
    lv_textarea_set_text(ta, text);
    lv_textarea_set_one_line(ta, true);
    lv_obj_add_style(ta, &s_style_textarea, 0);
    lv_obj_add_event_cb(ta, textarea_click_cb, LV_EVENT_CLICKED, NULL);
    return ta;
}

/** 创建带共享样式的 dropdown（延迟创建下拉列表） */
static lv_obj_t* mt_dropdown(lv_obj_t *parent, int w, int h, int x, int y, const char *options, int selected)
{
    lv_obj_t *dd = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd, options);
    lv_dropdown_set_selected(dd, selected);
    lv_obj_set_size(dd, w, h);
    lv_obj_set_pos(dd, x, y);
    lv_obj_add_style(dd, &s_style_dropdown, 0);
    lv_obj_add_event_cb(dd, ui_dropdown_list_font_cb, LV_EVENT_READY, NULL);
    return dd;
}

/** 创建带共享样式的 label */
static lv_obj_t* mt_label(lv_obj_t *parent, int x, int y, const char *text)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_add_style(lbl, &s_style_label, 0);
    return lbl;
}

/*********************
 *  STATIC VARIABLES
 *********************/
static lv_obj_t *g_tab_buttons[3] = {NULL};   /* 3个标签按钮的引用 */
static lv_obj_t *g_content_container = NULL;  /* 内容容器引用 */
static lv_obj_t *g_comm_params_menu_buttons[7] = {NULL};  /* 通信参数左侧菜单按钮数组 */
static lv_obj_t *g_calibration_menu_buttons[3] = {NULL};  /* 校正参数左侧菜单按钮数组 */
static lv_obj_t *g_comm_content_area = NULL;   /* 通信参数右侧内容区域 */
static lv_obj_t *g_comm_active_form = NULL;    /* 通信参数当前活跃表单 */
static int g_comm_active_menu = -1;            /* 通信参数当前菜单索引 */
static lv_obj_t *g_calib_content_area = NULL;  /* 校正参数右侧内容区域 */
static lv_obj_t *g_calib_active_form = NULL;   /* 校正参数当前活跃表单 */
static int g_calib_active_menu = -1;           /* 校正参数当前菜单索引 */
static lv_obj_t *g_algorithm_menu_buttons[3] = {NULL};  /* 算法参数左侧菜单按钮数组 */
static lv_obj_t *g_algo_content_area = NULL;   /* 算法参数右侧内容区域 */
static lv_obj_t *g_algo_active_form = NULL;    /* 算法参数当前活跃表单 */
static int g_algo_active_menu = -1;            /* 算法参数当前菜单索引 */

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 创建维护页面
 */
void ui_maintenance_create(lv_obj_t *parent)
{
    /* 初始化共享样式（仅首次） */
    ensure_styles_init();

    /* 重置懒加载状态（旧对象已被 lv_obj_clean 销毁） */
    g_comm_content_area = NULL;
    g_comm_active_form = NULL;
    g_comm_active_menu = -1;
    g_calib_content_area = NULL;
    g_calib_active_form = NULL;
    g_calib_active_menu = -1;
    g_algo_content_area = NULL;
    g_algo_active_form = NULL;
    g_algo_active_menu = -1;
    g_content_container = NULL;

    /* 顶部标签按钮区域 */
    create_tab_buttons(parent);

    /* 创建内容容器 - 用于放置通信参数/校正参数/算法参数的整个布局 */
    g_content_container = lv_obj_create(parent);
    lv_obj_set_size(g_content_container, 1168, 660);
    lv_obj_set_pos(g_content_container, 5, 70);
    lv_obj_set_style_bg_opa(g_content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_content_container, 0, 0);
    lv_obj_set_style_pad_all(g_content_container, 0, 0);
    lv_obj_clear_flag(g_content_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 默认显示通信参数布局 */
    create_comm_params_layout(g_content_container);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 创建通信参数的表单容器（懒加载用）
 */
static lv_obj_t* create_form_container(lv_obj_t *parent)
{
    lv_obj_t *form = lv_obj_create(parent);
    lv_obj_set_size(form, 913, 660);
    lv_obj_set_pos(form, 0, 0);
    lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_style_pad_all(form, 20, 0);
    lv_obj_set_scroll_dir(form, LV_DIR_VER);
    return form;
}

/**
 * @brief 切换通信参数子表单（懒加载：销毁旧表单，创建新表单）
 */
static void comm_switch_form(int menu_index)
{
    if (menu_index == g_comm_active_menu) return;

    /* 销毁旧表单 */
    if (g_comm_active_form) {
        lv_obj_del(g_comm_active_form);
        g_comm_active_form = NULL;
    }

    /* 创建新表单 */
    g_comm_active_form = create_form_container(g_comm_content_area);

    switch (menu_index) {
        case 0: create_bus_settings_form(g_comm_active_form); break;
        case 1: create_slave_settings_form(g_comm_active_form); break;
        case 2: create_inverter_settings_form(g_comm_active_form); break;
        case 3: create_remote_settings_form(g_comm_active_form); break;
        case 4: create_node_settings_form(g_comm_active_form); break;
        case 6: create_local_sync_form(g_comm_active_form); break;
        default: break;
    }
    g_comm_active_menu = menu_index;
}

/**
 * @brief 创建通信参数布局（左侧菜单 + 右侧内容）
 */
static void create_comm_params_layout(lv_obj_t *parent)
{
    /* 左侧浅蓝色菜单区域 */
    lv_obj_t *left_menu = lv_obj_create(parent);
    lv_obj_set_size(left_menu, 250, 660);
    lv_obj_set_pos(left_menu, 0, 0);
    lv_obj_set_style_bg_color(left_menu, lv_color_hex(0xa8d8ea), 0);
    lv_obj_set_style_border_width(left_menu, 0, 0);
    lv_obj_set_style_radius(left_menu, 10, 0);
    lv_obj_set_style_pad_all(left_menu, 0, 0);
    lv_obj_clear_flag(left_menu, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧菜单项 */
    const char *menu_items[] = {"总线设置", "从机设置", "变频器设置", "远程设置", "节点设置", "通信诊断", "本地同步"};

    for (int i = 0; i < 7; i++) {
        lv_obj_t *menu_btn = lv_btn_create(left_menu);
        lv_obj_set_size(menu_btn, 230, 50);
        lv_obj_set_pos(menu_btn, 10, 20 + i * 60);
        /* 第一个按钮默认选中为深蓝色，其他为浅蓝色 */
        if (i == 0) {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x70c1d8), 0);
        } else {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0xa8d8ea), 0);
        }
        lv_obj_set_style_radius(menu_btn, 5, 0);

        /* 添加点击事件 */
        lv_obj_add_event_cb(menu_btn, comm_params_menu_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t *menu_label = lv_label_create(menu_btn);
        lv_label_set_text_fmt(menu_label, "%s         >", menu_items[i]);
        lv_obj_set_style_text_font(menu_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(menu_label, lv_color_black(), 0);
        lv_obj_align(menu_label, LV_ALIGN_LEFT_MID, 20, 0);

        /* 保存按钮引用 */
        g_comm_params_menu_buttons[i] = menu_btn;
    }

    /* 右侧白色内容区域 */
    g_comm_content_area = lv_obj_create(parent);
    lv_obj_set_size(g_comm_content_area, 913, 660);
    lv_obj_set_pos(g_comm_content_area, 255, 0);
    lv_obj_set_style_bg_color(g_comm_content_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_comm_content_area, 0, 0);
    lv_obj_set_style_radius(g_comm_content_area, 10, 0);
    lv_obj_set_style_pad_all(g_comm_content_area, 20, 0);
    lv_obj_clear_flag(g_comm_content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 懒加载：仅创建默认的总线设置表单 */
    g_comm_active_menu = -1;
    comm_switch_form(0);
}

/**
 * @brief 通信参数左侧菜单按钮回调
 */
static void comm_params_menu_cb(lv_event_t *e)
{
    int menu_index = (int)(intptr_t)lv_event_get_user_data(e);

    /* 更新所有菜单按钮的颜色 */
    for (int i = 0; i < 7; i++) {
        if (g_comm_params_menu_buttons[i]) {
            if (i == menu_index) {
                /* 选中：深蓝色背景 */
                lv_obj_set_style_bg_color(g_comm_params_menu_buttons[i], lv_color_hex(0x70c1d8), 0);
            } else {
                /* 未选中：浅蓝色背景 */
                lv_obj_set_style_bg_color(g_comm_params_menu_buttons[i], lv_color_hex(0xa8d8ea), 0);
            }
        }
    }

    /* 懒加载：销毁旧表单，创建新表单 */
    comm_switch_form(menu_index);
}

/**
 * @brief 创建总线设置表单内容
 */
static void create_bus_settings_form(lv_obj_t *parent)
{
    int y_pos = 5;
    int label_x = 20;
    int input_x = 280;
    int input_width = 200;
    int row_height = 50;

    /* 第一行：485-1波特率 + 485-2波特率 */
    mt_label(parent, label_x, y_pos + 10, "485-1波特率:");

    lv_obj_t *dd_485_1_baud = mt_dropdown(parent, input_width, 40, input_x, y_pos, "4800\n9600\n19200\n38400\n57600\n115200", 1);

    mt_label(parent, 520, y_pos + 10, "485-2波特率:");

    lv_obj_t *dd_485_2_baud = mt_dropdown(parent, input_width, 40, 660, y_pos, "4800\n9600\n19200\n38400\n57600\n115200", 1);

    y_pos += row_height;

    /* 第二行：485-3波特率 + 485-4波特率 */
    mt_label(parent, label_x, y_pos + 10, "485-3波特率:");

    lv_obj_t *dd_485_3_baud = mt_dropdown(parent, input_width, 40, input_x, y_pos, "4800\n9600\n19200\n38400\n57600\n115200", 1);

    mt_label(parent, 520, y_pos + 10, "485-4波特率:");

    lv_obj_t *dd_485_4_baud = mt_dropdown(parent, input_width, 40, 660, y_pos, "4800\n9600\n19200\n38400\n57600\n115200", 1);

    y_pos += row_height;

    /* 第三行：485-1超时时间 + 485-4超时时间 */
    mt_label(parent, label_x, y_pos + 10, "485-1超时时间(ms):");

    lv_obj_t *input_485_1_timeout = mt_textarea(parent, input_width, 40, input_x, y_pos, "1000");

    mt_label(parent, 520, y_pos + 10, "485-4超时时间(ms):");

    lv_obj_t *input_485_4_timeout = mt_textarea(parent, input_width, 40, 660, y_pos, "1000");

    y_pos += row_height;

    /* 第四行：485-1轮询间隔 + 485-4轮询间隔 */
    mt_label(parent, label_x, y_pos + 10, "485-1轮询间隔(ms):");

    lv_obj_t *input_485_1_poll = mt_textarea(parent, input_width, 40, input_x, y_pos, "500");

    mt_label(parent, 520, y_pos + 10, "485-4轮询间隔(ms):");

    lv_obj_t *input_485_4_poll = mt_textarea(parent, input_width, 40, 660, y_pos, "500");

    y_pos += row_height;

    /* 第五行：485-2modbus超时时间 */
    mt_label(parent, label_x, y_pos + 10, "485-2modbus超时时间(ms):");

    lv_obj_t *input_485_2_modbus = mt_textarea(parent, 400, 40, input_x, y_pos, "2000");

    y_pos += row_height;

    /* 第六行：485-2Lora控制超时时间 */
    mt_label(parent, label_x, y_pos + 10, "485-2Lora控制超时时间(ms):");

    lv_obj_t *input_485_2_lora_ctrl = mt_textarea(parent, 400, 40, input_x, y_pos, "5000");

    y_pos += row_height;

    /* 第七行：485-2Lora采集超时时间 */
    mt_label(parent, label_x, y_pos + 10, "485-2Lora采集超时时间(ms):");

    lv_obj_t *input_485_2_lora_collect = mt_textarea(parent, 400, 40, input_x, y_pos, "7000");

    y_pos += row_height;

    /* 第八行：485-3modbus超时时间 */
    mt_label(parent, label_x, y_pos + 10, "485-3modbus超时时间(ms):");

    lv_obj_t *input_485_3_modbus = mt_textarea(parent, 400, 40, input_x, y_pos, "2000");

    y_pos += row_height;

    /* 第九行：485-3Lora控制超时时间 */
    mt_label(parent, label_x, y_pos + 10, "485-3Lora控制超时时间(ms):");

    lv_obj_t *input_485_3_lora_ctrl = mt_textarea(parent, 400, 40, input_x, y_pos, "5000");

    y_pos += row_height;

    /* 第十行：485-3Lora采集超时时间 */
    mt_label(parent, label_x, y_pos + 10, "485-3Lora采集超时时间(ms):");

    lv_obj_t *input_485_3_lora_collect = mt_textarea(parent, 400, 40, input_x, y_pos, "7000");

    y_pos += row_height + 20;

    /* 底部按钮 */
    int btn_y = 560;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建从机设置表单内容
 */
static void create_slave_settings_form(lv_obj_t *parent)
{
    int y_pos = 10;
    int row_height = 55;
    int label_width = 90;

    /* 第一行：【水肥机】 RS485-1 【ECPH】RS485-4 【MOS模块】RS485-4 */
    /* 水肥机 */
    mt_label(parent, 15, y_pos + 10, "【水肥机】");

    lv_obj_t *dd_water = mt_dropdown(parent, 150, 35, 110, y_pos + 5, "RS485-1\nRS485-2\nRS485-3\nRS485-4", 0);

    /* ECPH */
    mt_label(parent, 300, y_pos + 10, "【ECPH】");

    lv_obj_t *dd_ecph = mt_dropdown(parent, 150, 35, 380, y_pos + 5, "RS485-1\nRS485-2\nRS485-3\nRS485-4", 3);

    /* MOS模块 */
    mt_label(parent, 570, y_pos + 10, "【MOS模块】");

    lv_obj_t *dd_mos = mt_dropdown(parent, 150, 35, 670, y_pos + 5, "RS485-1\nRS485-2\nRS485-3\nRS485-4", 3);

    y_pos += row_height;

    /* 第二行：【主流量】兰中电磁流量计 地址:1 【MOS模块】地址:10 */
    mt_label(parent, 15, y_pos + 10, "【主流量】");

    lv_obj_t *dd_main_flow = mt_dropdown(parent, 180, 35, 110, y_pos + 5, "兰中电磁流量计\n涡轮流量计", 0);

    mt_label(parent, 310, y_pos + 10, "地址:");

    lv_obj_t *input_main_addr = mt_textarea(parent, 70, 35, 360, y_pos + 5, "1");

    /* MOS模块地址 */
    mt_label(parent, 470, y_pos + 10, "【MOS模块】");

    mt_label(parent, 570, y_pos + 10, "地址:");

    lv_obj_t *input_mos_addr = mt_textarea(parent, 70, 35, 620, y_pos + 5, "10");

    y_pos += row_height;

    /* 第三行：【控制器1】地址:8 【控制器2】地址:9 */
    mt_label(parent, 15, y_pos + 10, "【控制器1】");

    mt_label(parent, 120, y_pos + 10, "地址:");

    lv_obj_t *input_ctrl1_addr = mt_textarea(parent, 70, 35, 170, y_pos + 5, "8");

    mt_label(parent, 380, y_pos + 10, "【控制器2】");

    mt_label(parent, 485, y_pos + 10, "地址:");

    lv_obj_t *input_ctrl2_addr = mt_textarea(parent, 70, 35, 535, y_pos + 5, "9");

    y_pos += row_height;

    /* 第四行：【肥流量器】地址:4 启用 【水流量器】地址:5 启用 */
    mt_label(parent, 15, y_pos + 10, "【肥流量器】");

    mt_label(parent, 120, y_pos + 10, "地址:");

    lv_obj_t *input_fert_addr = mt_textarea(parent, 70, 35, 170, y_pos + 5, "4");

    lv_obj_t *cb_fert = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_fert, "启用");
    lv_obj_set_style_text_font(cb_fert, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_fert, 250, y_pos + 7);
    lv_obj_add_state(cb_fert, LV_STATE_CHECKED);

    mt_label(parent, 380, y_pos + 10, "【水流量器】");

    mt_label(parent, 485, y_pos + 10, "地址:");

    lv_obj_t *input_water_addr = mt_textarea(parent, 70, 35, 535, y_pos + 5, "5");

    lv_obj_t *cb_water = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_water, "启用");
    lv_obj_set_style_text_font(cb_water, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_water, 615, y_pos + 7);
    lv_obj_add_state(cb_water, LV_STATE_CHECKED);

    y_pos += row_height;

    /* 第五行：【主压力】地址:7 启用 【肥压力】地址:6 启用 */
    mt_label(parent, 15, y_pos + 10, "【主压力】");

    mt_label(parent, 120, y_pos + 10, "地址:");

    lv_obj_t *input_main_pres_addr = mt_textarea(parent, 70, 35, 170, y_pos + 5, "7");

    lv_obj_t *cb_main_pres = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_main_pres, "启用");
    lv_obj_set_style_text_font(cb_main_pres, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_main_pres, 250, y_pos + 7);
    lv_obj_add_state(cb_main_pres, LV_STATE_CHECKED);

    mt_label(parent, 380, y_pos + 10, "【肥压力】");

    mt_label(parent, 485, y_pos + 10, "地址:");

    lv_obj_t *input_fert_pres_addr = mt_textarea(parent, 70, 35, 535, y_pos + 5, "6");

    lv_obj_t *cb_fert_pres = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_fert_pres, "启用");
    lv_obj_set_style_text_font(cb_fert_pres, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_fert_pres, 615, y_pos + 7);
    lv_obj_add_state(cb_fert_pres, LV_STATE_CHECKED);

    y_pos += row_height;

    /* 第六行：【ECPH1】地址:2 启用 【ECPH2】地址:3 启用 */
    mt_label(parent, 15, y_pos + 10, "【ECPH1】");

    mt_label(parent, 120, y_pos + 10, "地址:");

    lv_obj_t *input_ecph1_addr = mt_textarea(parent, 70, 35, 170, y_pos + 5, "2");

    lv_obj_t *cb_ecph1 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_ecph1, "启用");
    lv_obj_set_style_text_font(cb_ecph1, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_ecph1, 250, y_pos + 7);
    lv_obj_add_state(cb_ecph1, LV_STATE_CHECKED);

    mt_label(parent, 380, y_pos + 10, "【ECPH2】");

    mt_label(parent, 485, y_pos + 10, "地址:");

    lv_obj_t *input_ecph2_addr = mt_textarea(parent, 70, 35, 535, y_pos + 5, "3");

    lv_obj_t *cb_ecph2 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_ecph2, "启用");
    lv_obj_set_style_text_font(cb_ecph2, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_ecph2, 615, y_pos + 7);
    lv_obj_add_state(cb_ecph2, LV_STATE_CHECKED);

    y_pos += row_height;

    /* 第七行：【1号液位】地址:20 启用 【2号液位】地址:21 启用 */
    mt_label(parent, 15, y_pos + 10, "【1号液位】");

    mt_label(parent, 120, y_pos + 10, "地址:");

    lv_obj_t *input_level1_addr = mt_textarea(parent, 70, 35, 170, y_pos + 5, "20");

    lv_obj_t *cb_level1 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_level1, "启用");
    lv_obj_set_style_text_font(cb_level1, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_level1, 250, y_pos + 7);

    mt_label(parent, 380, y_pos + 10, "【2号液位】");

    mt_label(parent, 485, y_pos + 10, "地址:");

    lv_obj_t *input_level2_addr = mt_textarea(parent, 70, 35, 535, y_pos + 5, "21");

    lv_obj_t *cb_level2 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_level2, "启用");
    lv_obj_set_style_text_font(cb_level2, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_level2, 615, y_pos + 7);

    y_pos += row_height;

    /* 第八行：【3号液位】地址:22 启用 【4号液位】地址:23 启用 */
    mt_label(parent, 15, y_pos + 10, "【3号液位】");

    mt_label(parent, 120, y_pos + 10, "地址:");

    lv_obj_t *input_level3_addr = mt_textarea(parent, 70, 35, 170, y_pos + 5, "22");

    lv_obj_t *cb_level3 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_level3, "启用");
    lv_obj_set_style_text_font(cb_level3, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_level3, 250, y_pos + 7);

    mt_label(parent, 380, y_pos + 10, "【4号液位】");

    mt_label(parent, 485, y_pos + 10, "地址:");

    lv_obj_t *input_level4_addr = mt_textarea(parent, 70, 35, 535, y_pos + 5, "23");

    lv_obj_t *cb_level4 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_level4, "启用");
    lv_obj_set_style_text_font(cb_level4, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_level4, 615, y_pos + 7);

    y_pos += row_height;

    /* 第九行：【5号液位】地址:24 启用 【ECPH变送器】支持1个ECPH探头 */
    mt_label(parent, 15, y_pos + 10, "【5号液位】");

    mt_label(parent, 120, y_pos + 10, "地址:");

    lv_obj_t *input_level5_addr = mt_textarea(parent, 70, 35, 170, y_pos + 5, "24");

    lv_obj_t *cb_level5 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_level5, "启用");
    lv_obj_set_style_text_font(cb_level5, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_level5, 250, y_pos + 7);

    /* ECPH变送器 - 同一行 */
    mt_label(parent, 380, y_pos + 10, "【ECPH变送器】");

    lv_obj_t *dd_ecph_trans = mt_dropdown(parent, 240, 35, 510, y_pos + 5, "支持1个ECPH探头\n支持2个ECPH探头\n支持3个ECPH探头", 0);

    /* 底部按钮 */
    int btn_y = 550;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建变频器设置表单内容
 */
static void create_inverter_settings_form(lv_obj_t *parent)
{
    int y_pos = 10;

    /* 第一行：【变频器】类型下拉框 地址输入框 启用复选框 */
    mt_label(parent, 15, y_pos + 10, "【变频器】");

    mt_label(parent, 120, y_pos + 10, "类型:");

    lv_obj_t *dd_inverter_type = mt_dropdown(parent, 250, 38, 180, y_pos + 5, "三晶VM1000B\n台达VFD-M\n施耐德ATV320", 0);

    mt_label(parent, 470, y_pos + 10, "地址:");

    lv_obj_t *input_addr = mt_textarea(parent, 120, 38, 520, y_pos + 5, "12");

    /* 启用复选框 */
    lv_obj_t *cb_enable = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_enable, "启用");
    lv_obj_set_style_text_font(cb_enable, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_enable, 680, y_pos + 7);
    lv_obj_set_style_text_font(cb_enable, &my_font_cn_16, 0);

    /* 底部按钮 */
    int btn_y = 550;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建远程设置表单内容
 */
static void create_remote_settings_form(lv_obj_t *parent)
{
    int y_pos = 10;
    int row_height = 55;
    int label_width = 380;
    int input_width = 200;
    int label_x = 20;
    int input_x = 420;

    /* 配置项数据 */
    const char *param_labels[] = {
        "水肥机自身状态上报周期 (S) :",
        "灌区电磁阀状态上报周期 (S) :",
        "母液调配运行状态上报周期 (S) :",
        "手动轮灌运行状态上报周期 (S) :",
        "灌溉程序运行状态上报周期 (S) :",
        "第三方电磁阀同步周期 (S) :",
        "第三方电磁阀响应超时 (S) :",
        "心跳周期 (S) :"
    };

    const char *default_values[] = {
        "300",
        "1800",
        "180",
        "180",
        "180",
        "1800",
        "10",
        "30"
    };

    /* 创建8行配置项 */
    for (int i = 0; i < 8; i++) {
        /* 参数标签 */
        mt_label(parent, label_x, y_pos + 10, param_labels[i]);

        /* 参数输入框 */
        lv_obj_t *input = mt_textarea(parent, input_width, 35, input_x, y_pos + 5, default_values[i]);

        y_pos += row_height;
    }

    /* 底部按钮 */
    int btn_y = 550;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建节点设置表单内容
 */
static void create_node_settings_form(lv_obj_t *parent)
{
    int y_pos = 10;
    int row_height = 55;
    int label_width = 380;
    int input_width = 200;
    int label_x = 20;
    int input_x = 420;

    /* 配置项数据 */
    const char *param_labels[] = {
        "Lora电池电压采集周期 (min) :",
        "Lora挂接的传感器采集周期 (min) :",
        "Lora下发电磁阀脉冲宽度 (ms) :"
    };

    const char *default_values[] = {
        "30",
        "15",
        "100"
    };

    /* 创建3行配置项 */
    for (int i = 0; i < 3; i++) {
        /* 参数标签 */
        mt_label(parent, label_x, y_pos + 10, param_labels[i]);

        /* 参数输入框 */
        lv_obj_t *input = mt_textarea(parent, input_width, 35, input_x, y_pos + 5, default_values[i]);

        y_pos += row_height;
    }

    /* 底部按钮 */
    int btn_y = 550;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建本地同步表单内容
 */
static void create_local_sync_form(lv_obj_t *parent)
{
    /* 两个按钮居中显示,垂直排列 */
    int btn_width = 200;
    int btn_height = 50;
    int btn_spacing = 30;  /* 按钮之间的间距 */

    /* 第一个按钮：同步第三方电磁阀 - 使用对齐居中 */
    lv_obj_t *btn_sync_valve = lv_btn_create(parent);
    lv_obj_set_size(btn_sync_valve, btn_width, btn_height);
    lv_obj_align(btn_sync_valve, LV_ALIGN_CENTER, 0, -40);  /* 向上偏移40px */
    lv_obj_set_style_bg_color(btn_sync_valve, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_sync_valve, 5, 0);

    lv_obj_t *label_sync_valve = lv_label_create(btn_sync_valve);
    lv_label_set_text(label_sync_valve, "同步第三方电磁阀");
    lv_obj_set_style_text_color(label_sync_valve, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_sync_valve, &my_font_cn_16, 0);
    lv_obj_center(label_sync_valve);

    /* 第二个按钮：同步灌区电磁阀 - 使用对齐居中 */
    lv_obj_t *btn_sync_zone = lv_btn_create(parent);
    lv_obj_set_size(btn_sync_zone, btn_width, btn_height);
    lv_obj_align(btn_sync_zone, LV_ALIGN_CENTER, 0, 40);  /* 向下偏移40px */
    lv_obj_set_style_bg_color(btn_sync_zone, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_sync_zone, 5, 0);

    lv_obj_t *label_sync_zone = lv_label_create(btn_sync_zone);
    lv_label_set_text(label_sync_zone, "同步灌区电磁阀");
    lv_obj_set_style_text_color(label_sync_zone, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_sync_zone, &my_font_cn_16, 0);
    lv_obj_center(label_sync_zone);
}

/**
 * @brief 创建校正参数布局（左侧菜单 + 右侧内容）
 */
static void create_calibration_params_layout(lv_obj_t *parent)
{
    /* 左侧浅蓝色菜单区域 */
    lv_obj_t *left_menu = lv_obj_create(parent);
    lv_obj_set_size(left_menu, 250, 660);
    lv_obj_set_pos(left_menu, 0, 0);
    lv_obj_set_style_bg_color(left_menu, lv_color_hex(0xa8d8ea), 0);
    lv_obj_set_style_border_width(left_menu, 0, 0);
    lv_obj_set_style_radius(left_menu, 10, 0);
    lv_obj_set_style_pad_all(left_menu, 0, 0);
    lv_obj_clear_flag(left_menu, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧菜单项 */
    const char *menu_items[] = {"流量校正", "液位校正", "ECPH校正"};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *menu_btn = lv_btn_create(left_menu);
        lv_obj_set_size(menu_btn, 230, 50);
        lv_obj_set_pos(menu_btn, 10, 20 + i * 60);
        /* 第一个按钮默认选中为深蓝色，其他为浅蓝色 */
        if (i == 0) {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x70c1d8), 0);
        } else {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0xa8d8ea), 0);
        }
        lv_obj_set_style_radius(menu_btn, 5, 0);

        /* 添加点击事件 */
        lv_obj_add_event_cb(menu_btn, calibration_menu_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t *menu_label = lv_label_create(menu_btn);
        lv_label_set_text_fmt(menu_label, "%s         >", menu_items[i]);
        lv_obj_set_style_text_font(menu_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(menu_label, lv_color_black(), 0);
        lv_obj_align(menu_label, LV_ALIGN_LEFT_MID, 20, 0);

        /* 保存按钮引用 */
        g_calibration_menu_buttons[i] = menu_btn;
    }

    /* 右侧白色内容区域 */
    g_calib_content_area = lv_obj_create(parent);
    lv_obj_set_size(g_calib_content_area, 913, 660);
    lv_obj_set_pos(g_calib_content_area, 255, 0);
    lv_obj_set_style_bg_color(g_calib_content_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_calib_content_area, 0, 0);
    lv_obj_set_style_radius(g_calib_content_area, 10, 0);
    lv_obj_set_style_pad_all(g_calib_content_area, 20, 0);
    lv_obj_clear_flag(g_calib_content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 懒加载：仅创建默认的流量校正表单 */
    g_calib_active_menu = -1;
    calib_switch_form(0);
}

/**
 * @brief 切换校正参数子表单（懒加载：销毁旧表单，创建新表单）
 */
static void calib_switch_form(int menu_index)
{
    if (menu_index == g_calib_active_menu) return;

    /* 销毁旧表单 */
    if (g_calib_active_form) {
        lv_obj_del(g_calib_active_form);
        g_calib_active_form = NULL;
    }

    /* 创建新表单 */
    g_calib_active_form = create_form_container(g_calib_content_area);

    switch (menu_index) {
        case 0: create_flow_calibration_form(g_calib_active_form); break;
        case 1: create_level_calibration_form(g_calib_active_form); break;
        case 2: create_ecph_calibration_form(g_calib_active_form); break;
        default: break;
    }
    g_calib_active_menu = menu_index;
}

/**
 * @brief 校正参数左侧菜单按钮回调
 */
static void calibration_menu_cb(lv_event_t *e)
{
    int menu_index = (int)(intptr_t)lv_event_get_user_data(e);

    /* 更新所有菜单按钮的颜色 */
    for (int i = 0; i < 3; i++) {
        if (g_calibration_menu_buttons[i]) {
            if (i == menu_index) {
                lv_obj_set_style_bg_color(g_calibration_menu_buttons[i], lv_color_hex(0x70c1d8), 0);
            } else {
                lv_obj_set_style_bg_color(g_calibration_menu_buttons[i], lv_color_hex(0xa8d8ea), 0);
            }
        }
    }

    /* 懒加载切换表单 */
    calib_switch_form(menu_index);
}

/**
 * @brief 创建流量校正表单内容
 */
static void create_flow_calibration_form(lv_obj_t *parent)
{
    int y_pos = 5;
    int label_x = 20;
    int input_x = 240;
    int input_width = 120;
    int btn_width = 80;
    int btn_height = 35;
    int row_height = 45;

    /* 定义参数列表 */
    const char *param_labels[] = {
        "1肥通道流量仪表系数:",
        "2肥通道流量仪表系数:",
        "3肥通道流量仪表系数:",
        "4肥通道流量仪表系数:",
        "5肥通道流量仪表系数:",
        "1水通道流量仪表系数:",
        "2水通道流量仪表系数:",
        "3水通道流量仪表系数:",
        "4水通道流量仪表系数:",
        "5水通道流量仪表系数:"
    };

    /* 创建10行参数 */
    for (int i = 0; i < 10; i++) {
        /* 参数标签 */
        mt_label(parent, label_x, y_pos + 8, param_labels[i]);

        /* 参数值输入框 */
        lv_obj_t *input = mt_textarea(parent, input_width, 35, input_x, y_pos, "0");

        /* 读取按钮 */
        lv_obj_t *btn_read = lv_btn_create(parent);
        lv_obj_set_size(btn_read, btn_width, btn_height);
        lv_obj_set_pos(btn_read, input_x + input_width + 20, y_pos);
        lv_obj_set_style_bg_color(btn_read, COLOR_PRIMARY, 0);
        lv_obj_set_style_radius(btn_read, 5, 0);

        lv_obj_t *label_read = lv_label_create(btn_read);
        lv_label_set_text(label_read, "读取");
        lv_obj_set_style_text_color(label_read, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_read, &my_font_cn_16, 0);
        lv_obj_center(label_read);

        /* 写入按钮 */
        lv_obj_t *btn_write = lv_btn_create(parent);
        lv_obj_set_size(btn_write, btn_width, btn_height);
        lv_obj_set_pos(btn_write, input_x + input_width + 110, y_pos);
        lv_obj_set_style_bg_color(btn_write, COLOR_PRIMARY, 0);
        lv_obj_set_style_radius(btn_write, 5, 0);

        lv_obj_t *label_write = lv_label_create(btn_write);
        lv_label_set_text(label_write, "写入");
        lv_obj_set_style_text_color(label_write, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_write, &my_font_cn_16, 0);
        lv_obj_center(label_write);

        /* 累计清零按钮 */
        lv_obj_t *btn_clear = lv_btn_create(parent);
        lv_obj_set_size(btn_clear, 110, btn_height);
        lv_obj_set_pos(btn_clear, input_x + input_width + 200, y_pos);
        lv_obj_set_style_bg_color(btn_clear, COLOR_PRIMARY, 0);
        lv_obj_set_style_radius(btn_clear, 5, 0);

        lv_obj_t *label_clear = lv_label_create(btn_clear);
        lv_label_set_text(label_clear, "累计清零");
        lv_obj_set_style_text_color(label_clear, lv_color_white(), 0);
        lv_obj_set_style_text_font(label_clear, &my_font_cn_16, 0);
        lv_obj_center(label_clear);

        y_pos += row_height;
    }
}

/**
 * @brief 创建液位校正表单内容
 */
static void create_level_calibration_form(lv_obj_t *parent)
{
    int y_pos = 10;
    int label_x = 20;
    int input_a_x = 420;
    int input_b_x = 600;
    int input_width = 120;
    int row_height = 50;

    /* 定义参数列表 - 5个母液桶,每个桶有液位和容量两个参数 */
    const char *param_labels[] = {
        "1号母液桶液位校准参数:",
        "1号母液桶容量校准参数:",
        "2号母液桶液位校准参数:",
        "2号母液桶容量校准参数:",
        "3号母液桶液位校准参数:",
        "3号母液桶容量校准参数:",
        "4号母液桶液位校准参数:",
        "4号母液桶容量校准参数:",
        "5号母液桶液位校准参数:",
        "5号母液桶容量校准参数:"
    };

    /* 默认值 - 液位参数为1.00,容量参数为1200.00 */
    const char *default_a[] = {"1.00", "1200.00", "1.00", "1200.00", "1.00", "1200.00", "1.00", "1200.00", "1.00", "1200.00"};
    const char *default_b[] = {"0.00", "0.00", "0.00", "0.00", "0.00", "0.00", "0.00", "0.00", "0.00", "0.00"};

    /* 创建10行参数 */
    for (int i = 0; i < 10; i++) {
        /* 参数标签 */
        mt_label(parent, label_x, y_pos + 8, param_labels[i]);

        /* a: 标签 */
        mt_label(parent, input_a_x - 30, y_pos + 8, "a:");

        /* a参数值输入框 */
        lv_obj_t *input_a = mt_textarea(parent, input_width, 35, input_a_x, y_pos, default_a[i]);

        /* b: 标签 */
        mt_label(parent, input_b_x - 30, y_pos + 8, "b:");

        /* b参数值输入框 */
        lv_obj_t *input_b = mt_textarea(parent, input_width, 35, input_b_x, y_pos, default_b[i]);

        y_pos += row_height;
    }

    /* 底部按钮 */
    int btn_y = 580;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建ECPH校正表单内容
 */
static void create_ecph_calibration_form(lv_obj_t *parent)
{
    /* 4个校准按钮垂直排列，居中显示 */
    int btn_width = 250;
    int btn_height = 50;
    int btn_spacing = 20;  /* 按钮之间的间距 */

    const char *btn_labels[] = {
        "开始EC1校准  >>",
        "开始PH1校准  >>",
        "开始EC2校准  >>",
        "开始PH2校准  >>"
    };

    /* 计算起始Y位置，使4个按钮垂直居中 */
    int total_height = 4 * btn_height + 3 * btn_spacing;
    int start_y = (660 - total_height) / 2;

    for (int i = 0; i < 4; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, btn_width, btn_height);
        lv_obj_align(btn, LV_ALIGN_CENTER, 0, start_y + i * (btn_height + btn_spacing) - 330);
        lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, 0);
        lv_obj_set_style_radius(btn, 5, 0);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, btn_labels[i]);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_center(label);
    }
}

/**
 * @brief 创建算法参数布局（左侧菜单 + 右侧内容）
 */
static void create_algorithm_params_layout(lv_obj_t *parent)
{
    /* 左侧浅蓝色菜单区域 */
    lv_obj_t *left_menu = lv_obj_create(parent);
    lv_obj_set_size(left_menu, 250, 660);
    lv_obj_set_pos(left_menu, 0, 0);
    lv_obj_set_style_bg_color(left_menu, lv_color_hex(0xa8d8ea), 0);
    lv_obj_set_style_border_width(left_menu, 0, 0);
    lv_obj_set_style_radius(left_menu, 10, 0);
    lv_obj_set_style_pad_all(left_menu, 0, 0);
    lv_obj_clear_flag(left_menu, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧菜单项 */
    const char *menu_items[] = {"PID设置", "通道设置", "滤波设置"};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *menu_btn = lv_btn_create(left_menu);
        lv_obj_set_size(menu_btn, 230, 50);
        lv_obj_set_pos(menu_btn, 10, 20 + i * 60);
        /* 第一个按钮默认选中为深蓝色，其他为浅蓝色 */
        if (i == 0) {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0x70c1d8), 0);
        } else {
            lv_obj_set_style_bg_color(menu_btn, lv_color_hex(0xa8d8ea), 0);
        }
        lv_obj_set_style_radius(menu_btn, 5, 0);

        /* 添加点击事件 */
        lv_obj_add_event_cb(menu_btn, algorithm_menu_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t *menu_label = lv_label_create(menu_btn);
        lv_label_set_text_fmt(menu_label, "%s         >", menu_items[i]);
        lv_obj_set_style_text_font(menu_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(menu_label, lv_color_black(), 0);
        lv_obj_align(menu_label, LV_ALIGN_LEFT_MID, 20, 0);

        /* 保存按钮引用 */
        g_algorithm_menu_buttons[i] = menu_btn;
    }

    /* 右侧白色内容区域 */
    g_algo_content_area = lv_obj_create(parent);
    lv_obj_set_size(g_algo_content_area, 913, 660);
    lv_obj_set_pos(g_algo_content_area, 255, 0);
    lv_obj_set_style_bg_color(g_algo_content_area, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_algo_content_area, 0, 0);
    lv_obj_set_style_radius(g_algo_content_area, 10, 0);
    lv_obj_set_style_pad_all(g_algo_content_area, 20, 0);
    lv_obj_clear_flag(g_algo_content_area, LV_OBJ_FLAG_SCROLLABLE);

    /* 懒加载：仅创建默认的PID设置表单 */
    g_algo_active_menu = -1;
    algo_switch_form(0);
}

/**
 * @brief 切换算法参数子表单（懒加载：销毁旧表单，创建新表单）
 */
static void algo_switch_form(int menu_index)
{
    if (menu_index == g_algo_active_menu) return;

    /* 销毁旧表单 */
    if (g_algo_active_form) {
        lv_obj_del(g_algo_active_form);
        g_algo_active_form = NULL;
    }

    /* 创建新表单 */
    g_algo_active_form = create_form_container(g_algo_content_area);

    switch (menu_index) {
        case 0: create_pid_settings_form(g_algo_active_form); break;
        case 1: create_channel_settings_form(g_algo_active_form); break;
        case 2: create_filter_settings_form(g_algo_active_form); break;
        default: break;
    }
    g_algo_active_menu = menu_index;
}

/**
 * @brief 算法参数左侧菜单按钮回调
 */
static void algorithm_menu_cb(lv_event_t *e)
{
    int menu_index = (int)(intptr_t)lv_event_get_user_data(e);

    /* 更新所有菜单按钮的颜色 */
    for (int i = 0; i < 3; i++) {
        if (g_algorithm_menu_buttons[i]) {
            if (i == menu_index) {
                lv_obj_set_style_bg_color(g_algorithm_menu_buttons[i], lv_color_hex(0x70c1d8), 0);
            } else {
                lv_obj_set_style_bg_color(g_algorithm_menu_buttons[i], lv_color_hex(0xa8d8ea), 0);
            }
        }
    }

    /* 懒加载切换表单 */
    algo_switch_form(menu_index);
}

/**
 * @brief 创建PID设置表单内容
 */
static void create_pid_settings_form(lv_obj_t *parent)
{
    int y_pos = 10;
    int col1_x = 30;   /* 参数项列 */
    int col2_x = 280;  /* 比例积分参数(%) */
    int col3_x = 500;  /* EC控制参数(ms) */
    int col4_x = 720;  /* PH控制参数 */
    int col_width = 180;
    int row_height = 60;

    /* 表头 */
    lv_obj_t *header_param = mt_label(parent, col1_x, y_pos, "参数项");
    lv_obj_set_style_text_color(header_param, lv_color_hex(0x333333), 0);

    lv_obj_t *header_ratio = mt_label(parent, col2_x, y_pos, "比例积分参数(%)");
    lv_obj_set_style_text_color(header_ratio, lv_color_hex(0x333333), 0);

    lv_obj_t *header_ec = mt_label(parent, col3_x, y_pos, "EC控制参数(ms)");
    lv_obj_set_style_text_color(header_ec, lv_color_hex(0x333333), 0);

    lv_obj_t *header_ph = mt_label(parent, col4_x, y_pos, "PH控制参数");
    lv_obj_set_style_text_color(header_ph, lv_color_hex(0x333333), 0);

    y_pos += 40;

    /* 参数行定义 */
    const char *param_names[] = {"P", "I", "D", "F", "积分限幅(ms)", "输出滤波系数", "容忍差值", "控制周期(S)"};
    const char *col2_defaults[] = {"1.000", "0.150", "0.000", "0.000", "150", "0.000", "0.01", "2"};
    const char *col3_defaults[] = {"0.200", "0.068", "0.070", "0.000", "80", "0.000", "0.01", "2"};
    const char *col4_defaults[] = {"0.010", "0.010", "0.100", "0.000", "80", "0.000", "0.02", "2"};

    /* 创建8行参数 */
    for (int i = 0; i < 8; i++) {
        /* 参数名称 */
        lv_obj_t *label_param = mt_label(parent, col1_x, y_pos + 8, param_names[i]);
        lv_obj_set_style_text_color(label_param, lv_color_hex(0x333333), 0);

        /* 比例积分参数输入框 */
        lv_obj_t *input_col2 = mt_textarea(parent, col_width, 35, col2_x, y_pos, col2_defaults[i]);
        lv_obj_set_style_bg_color(input_col2, lv_color_hex(0xf5f5f5), 0);

        /* EC控制参数输入框 */
        lv_obj_t *input_col3 = mt_textarea(parent, col_width, 35, col3_x, y_pos, col3_defaults[i]);
        lv_obj_set_style_bg_color(input_col3, lv_color_hex(0xf5f5f5), 0);

        /* PH控制参数输入框 */
        lv_obj_t *input_col4 = mt_textarea(parent, col_width, 35, col4_x, y_pos, col4_defaults[i]);
        lv_obj_set_style_bg_color(input_col4, lv_color_hex(0xf5f5f5), 0);

        y_pos += row_height;
    }

    y_pos += 20;

    /* 底部按钮 */
    int btn_y = 560;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建通道设置表单内容
 */
static void create_channel_settings_form(lv_obj_t *parent)
{
    int y_pos = 10;
    int label_x = 20;
    int input_x_left = 220;   /* 左列输入框X位置 */
    int input_x_right = 570;  /* 右列输入框X位置 */
    int input_width = 150;
    int row_height = 60;

    /* 第一行：模块通信地址 + 第1通道地址 */
    mt_label(parent, label_x, y_pos + 10, "模块通信地址:");

    lv_obj_t *input_module = mt_textarea(parent, input_width, 40, input_x_left, y_pos, "10");

    mt_label(parent, 420, y_pos + 10, "第1通道地址:");

    lv_obj_t *input_ch1 = mt_textarea(parent, input_width, 40, input_x_right, y_pos, "0");

    y_pos += row_height;

    /* 第二行：第2通道地址 + 第3通道地址 */
    mt_label(parent, label_x, y_pos + 10, "第2通道地址:");

    lv_obj_t *input_ch2 = mt_textarea(parent, input_width, 40, input_x_left, y_pos, "1");

    mt_label(parent, 420, y_pos + 10, "第3通道地址:");

    lv_obj_t *input_ch3 = mt_textarea(parent, input_width, 40, input_x_right, y_pos, "2");

    y_pos += row_height;

    /* 第三行：第4通道地址 + 第5通道地址 */
    mt_label(parent, label_x, y_pos + 10, "第4通道地址:");

    lv_obj_t *input_ch4 = mt_textarea(parent, input_width, 40, input_x_left, y_pos, "3");

    mt_label(parent, 420, y_pos + 10, "第5通道地址:");

    lv_obj_t *input_ch5 = mt_textarea(parent, input_width, 40, input_x_right, y_pos, "4");

    y_pos += row_height;

    /* 第四行：通道数 + 启用酸通道复选框 */
    mt_label(parent, label_x, y_pos + 10, "通道数:");

    lv_obj_t *input_ch_count = mt_textarea(parent, input_width, 40, input_x_left, y_pos, "1");

    lv_obj_t *cb_enable_acid = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_enable_acid, "启用酸通道");
    lv_obj_set_style_text_font(cb_enable_acid, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_enable_acid, 480, y_pos + 8);

    /* 底部按钮 */
    int btn_y = 550;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建滤波设置表单内容
 */
static void create_filter_settings_form(lv_obj_t *parent)
{
    int y_pos = 30;
    int col1_x = 80;   /* 左列X位置 */
    int col2_x = 500;  /* 右列X位置 */
    int row_height = 60;

    /* 第一行：EC1启用滤波 + EC2启用滤波 */
    lv_obj_t *cb_ec1 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_ec1, "EC1启用滤波");
    lv_obj_set_style_text_font(cb_ec1, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_ec1, col1_x, y_pos);
    lv_obj_set_style_text_font(cb_ec1, &my_font_cn_16, 0);
    lv_obj_add_state(cb_ec1, LV_STATE_CHECKED);  /* 默认选中 */

    lv_obj_t *cb_ec2 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_ec2, "EC2启用滤波");
    lv_obj_set_style_text_font(cb_ec2, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_ec2, col2_x, y_pos);
    lv_obj_set_style_text_font(cb_ec2, &my_font_cn_16, 0);

    y_pos += row_height;

    /* 第二行：PH1启用滤波 + PH2启用滤波 */
    lv_obj_t *cb_ph1 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_ph1, "PH1启用滤波");
    lv_obj_set_style_text_font(cb_ph1, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_ph1, col1_x, y_pos);
    lv_obj_set_style_text_font(cb_ph1, &my_font_cn_16, 0);
    lv_obj_add_state(cb_ph1, LV_STATE_CHECKED);  /* 默认选中 */

    lv_obj_t *cb_ph2 = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_ph2, "PH2启用滤波");
    lv_obj_set_style_text_font(cb_ph2, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_ph2, col2_x, y_pos);
    lv_obj_set_style_text_font(cb_ph2, &my_font_cn_16, 0);

    y_pos += row_height;

    /* 第三行：主水管流量启用滤波 */
    lv_obj_t *cb_main_flow = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_main_flow, "主水管流量启用滤波");
    lv_obj_set_style_text_font(cb_main_flow, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_main_flow, col1_x, y_pos);
    lv_obj_set_style_text_font(cb_main_flow, &my_font_cn_16, 0);
    lv_obj_add_state(cb_main_flow, LV_STATE_CHECKED);  /* 默认选中 */

    y_pos += row_height;

    /* 第四行：肥通道流量启用滤波 */
    lv_obj_t *cb_fert_flow = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_fert_flow, "肥通道流量启用滤波");
    lv_obj_set_style_text_font(cb_fert_flow, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_fert_flow, col1_x, y_pos);
    lv_obj_set_style_text_font(cb_fert_flow, &my_font_cn_16, 0);
    lv_obj_add_state(cb_fert_flow, LV_STATE_CHECKED);  /* 默认选中 */

    y_pos += row_height;

    /* 第五行：EC调配参考流量计数据 */
    lv_obj_t *cb_ec_ref = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb_ec_ref, "EC调配参考流量计数据");
    lv_obj_set_style_text_font(cb_ec_ref, &my_font_cn_16, 0);
    lv_obj_set_pos(cb_ec_ref, col1_x, y_pos);
    lv_obj_set_style_text_font(cb_ec_ref, &my_font_cn_16, 0);
    lv_obj_add_state(cb_ec_ref, LV_STATE_CHECKED);  /* 默认选中 */

    /* 底部按钮 */
    int btn_y = 550;

    /* 取消设置按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(parent);
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
    lv_obj_t *btn_save = lv_btn_create(parent);
    lv_obj_set_size(btn_save, 150, 45);
    lv_obj_set_pos(btn_save, 450, btn_y);
    lv_obj_set_style_bg_color(btn_save, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_save, 22, 0);

    lv_obj_t *label_save = lv_label_create(btn_save);
    lv_label_set_text(label_save, "保存设置");
    lv_obj_set_style_text_color(label_save, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_save, &my_font_cn_16, 0);
    lv_obj_center(label_save);
}

/**
 * @brief 创建顶部标签按钮
 */
static void create_tab_buttons(lv_obj_t *parent)
{
    const char *tab_names[] = {"通信参数", "校正参数", "算法参数"};
    int btn_width = 150;
    int btn_height = 50;
    int x_start = 10;
    int y_pos = 10;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(parent);
        lv_obj_set_size(btn, btn_width, btn_height);
        lv_obj_set_pos(btn, x_start + i * (btn_width + 10), y_pos);

        /* 第一个按钮默认选中 - 蓝色背景 */
        if (i == 0) {
            lv_obj_set_style_bg_color(btn, COLOR_PRIMARY, 0);
        } else {
            lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
        }

        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xcccccc), 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_add_event_cb(btn, tab_btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        /* 按钮文字 */
        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, tab_names[i]);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, i == 0 ? lv_color_white() : lv_color_hex(0x333333), 0);
        lv_obj_center(label);

        /* 保存按钮引用 */
        g_tab_buttons[i] = btn;
    }
}

/**
 * @brief 标签按钮回调
 */
static void tab_btn_cb(lv_event_t *e)
{
    int tab_index = (int)(intptr_t)lv_event_get_user_data(e);

    /* 更新所有标签按钮的颜色和文字颜色 */
    for (int i = 0; i < 3; i++) {
        if (g_tab_buttons[i]) {
            if (i == tab_index) {
                /* 选中：蓝色背景 */
                lv_obj_set_style_bg_color(g_tab_buttons[i], COLOR_PRIMARY, 0);

                /* 更新文字颜色为白色 */
                lv_obj_t *label = lv_obj_get_child(g_tab_buttons[i], 0);
                if (label) lv_obj_set_style_text_color(label, lv_color_white(), 0);
            } else {
                /* 未选中：白色背景 */
                lv_obj_set_style_bg_color(g_tab_buttons[i], lv_color_white(), 0);

                /* 更新文字颜色为深色 */
                lv_obj_t *label = lv_obj_get_child(g_tab_buttons[i], 0);
                if (label) lv_obj_set_style_text_color(label, lv_color_hex(0x333333), 0);
            }
        }
    }

    /* 清空内容容器 */
    if (g_content_container) {
        lv_obj_clean(g_content_container);
    }

    /* 重置子表单状态（旧对象已被 lv_obj_clean 销毁） */
    g_comm_content_area = NULL;
    g_comm_active_form = NULL;
    g_comm_active_menu = -1;
    g_calib_content_area = NULL;
    g_calib_active_form = NULL;
    g_calib_active_menu = -1;
    g_algo_content_area = NULL;
    g_algo_active_form = NULL;
    g_algo_active_menu = -1;

    /* 根据选中的标签创建对应的布局 */
    if (tab_index == 0) {
        /* 通信参数 */
        create_comm_params_layout(g_content_container);
    } else if (tab_index == 1) {
        /* 校正参数 */
        create_calibration_params_layout(g_content_container);
    } else if (tab_index == 2) {
        /* 算法参数 */
        create_algorithm_params_layout(g_content_container);
    }
}

/**
 * @brief 输入框点击回调 - 显示数字键盘
 */
static void textarea_click_cb(lv_event_t *e)
{
    lv_obj_t *textarea = lv_event_get_target(e);
    ui_main_t *ui_main = ui_get_main();

    if (textarea && ui_main && ui_main->screen) {
        ui_numpad_show(textarea, ui_main->screen);
    }
}
