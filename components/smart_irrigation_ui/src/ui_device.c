/**
 * @file ui_device.c
 * @brief 设备控制界面实现
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_tab_buttons(lv_obj_t *parent);
static void create_control_panel(lv_obj_t *parent);
static void create_data_panel(lv_obj_t *parent);
static void create_device_card(lv_obj_t *parent, const char *title, int x, int y, bool is_double);
static void tab_btn_cb(lv_event_t *e);
static void device_card_click_cb(lv_event_t *e);
static void valve_btn_cb(lv_event_t *e);
static void zone_switch_cb(lv_event_t *e);
static void show_device_confirm_dialog(const char *dev_name, bool to_on);
static void device_dialog_confirm_cb(lv_event_t *e);
static void device_dialog_cancel_cb(lv_event_t *e);
static void update_valve_open_count(void);

/* 全局变量 */
static lv_obj_t *g_left_panel = NULL;   /* 左侧白色面板（主机控制视图） */
static lv_obj_t *g_right_panel = NULL;  /* 右侧白色面板（主机控制视图） */
static lv_obj_t *g_view_container = NULL; /* 视图容器（懒加载） */
static lv_obj_t *g_tab_buttons[4] = {NULL};   /* 标签按钮数组 */
static int g_active_tab = 0;             /* 当前活动标签页 */

/* 设备控制回调 */
static ui_device_control_cb_t g_device_control_cb = NULL;

/* 主机控制视图：设备状态标签 */
static lv_obj_t *g_dev_status_labels[5] = {NULL};  /* 主水泵/施肥泵/出肥阀/注水阀/搅拌机 */

/* 右侧数据面板值标签 */
static lv_obj_t *g_main_data_vals[8] = {NULL};  /* EC1/PH1/EC2/PH2 + 流量/压力等 */

/* 阀门控制视图标签 */
static lv_obj_t *g_valve_total_label = NULL;
static lv_obj_t *g_valve_open_label = NULL;
static lv_obj_t *g_valve_container = NULL;

/* 灌区控制视图标签 */
static lv_obj_t *g_zone_container = NULL;

/* 传感监测视图标签 */
static lv_obj_t *g_sensor_container = NULL;

/* 确认对话框 */
static lv_obj_t *g_device_dialog = NULL;
static uint8_t g_pending_dev_type = 0;
static uint8_t g_pending_dev_id = 0;
static bool g_pending_on = false;
static int g_pending_label_type = 0;  /* 0=设备卡片, 1=阀门卡片 */
static int g_pending_label_idx = 0;

/* 设备开关状态 */
static bool g_dev_states[5] = {false, false, false, false, false};

/* 阀门状态及状态标签 */
static bool g_valve_states[7] = {false, false, false, false, false, false, false};
static lv_obj_t *g_valve_status_labels[7] = {NULL};
static lv_obj_t *g_valve_btns[7] = {NULL};       /* 阀门开关按钮 */
static lv_obj_t *g_valve_btn_labels[7] = {NULL};  /* 按钮上的文字 */

/* 设备名称/ID 映射 */
static const uint8_t s_dev_id_map[5] = {2, 3, 4, 5, 1};
static const char *s_dev_name_map[5] = {"主水泵", "施肥泵", "出肥阀", "注水阀", "搅拌机"};

/* 前向声明 */
static void create_main_control_view(lv_obj_t *parent);
static void create_valve_control_view(lv_obj_t *parent);
static void create_zone_control_view(lv_obj_t *parent);
static void create_sensor_monitor_view(lv_obj_t *parent);
static void switch_to_tab(int tab_index);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 创建设备控制页面
 */
void ui_device_create(lv_obj_t *parent)
{
    /* 重置静态指针（旧对象已被 ui_switch_nav 中的 lv_obj_clean 销毁） */
    g_left_panel = NULL;
    g_right_panel = NULL;
    g_view_container = NULL;
    for (int i = 0; i < 4; i++) g_tab_buttons[i] = NULL;
    for (int i = 0; i < 5; i++) g_dev_status_labels[i] = NULL;
    for (int i = 0; i < 8; i++) g_main_data_vals[i] = NULL;
    g_valve_total_label = NULL;
    g_valve_open_label = NULL;
    g_valve_container = NULL;
    g_zone_container = NULL;
    g_sensor_container = NULL;
    g_active_tab = 0;
    for (int i = 0; i < 7; i++) g_valve_status_labels[i] = NULL;
    if (g_device_dialog) { lv_obj_del(g_device_dialog); g_device_dialog = NULL; }

    /* 顶部标签页按钮 */
    create_tab_buttons(parent);

    /* 创建主容器 - 用于懒加载不同的视图 */
    g_view_container = lv_obj_create(parent);
    lv_obj_set_size(g_view_container, 1168, 660);
    lv_obj_set_pos(g_view_container, 5, 70);
    lv_obj_set_style_bg_opa(g_view_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_view_container, 0, 0);
    lv_obj_set_style_pad_all(g_view_container, 0, 0);
    lv_obj_clear_flag(g_view_container, LV_OBJ_FLAG_SCROLLABLE);

    /* 懒加载：只创建默认视图（主机控制） */
    create_main_control_view(g_view_container);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

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

    /* 第二行：1号出肥阀 + 1号注水阀 + 1号搅拌机 */
    create_device_card(parent, "1号出肥阀", 0, 75, false);
    create_device_card(parent, "1号注水阀", 150, 75, false);
    create_device_card(parent, "1号搅拌机", 300, 75, false);
}

/**
 * @brief 设备卡片点击回调 - 弹出确认对话框
 * user_data = status_idx (0~4)
 */
static void device_card_click_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= 5) return;

    g_pending_dev_type = 0x04;
    g_pending_dev_id = s_dev_id_map[idx];
    g_pending_on = !g_dev_states[idx];
    g_pending_label_type = 0;
    g_pending_label_idx = idx;

    show_device_confirm_dialog(s_dev_name_map[idx], g_pending_on);
}

/**
 * @brief 阀门按钮回调 - 弹出确认对话框（单按钮切换）
 * user_data = valve_idx (0~6)
 */
static void valve_btn_cb(lv_event_t *e)
{
    int valve_idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (valve_idx < 0 || valve_idx >= 7) return;

    static const char *valve_names[] = {"主管道阀", "副管道1阀", "副管道2阀", "副管道3阀",
                                         "副管道4阀", "副管道5阀", "副管道6阀"};

    g_pending_dev_type = 0x02;
    g_pending_dev_id = (uint8_t)valve_idx;
    g_pending_on = !g_valve_states[valve_idx];
    g_pending_label_type = 1;
    g_pending_label_idx = valve_idx;

    show_device_confirm_dialog(valve_names[valve_idx], g_pending_on);
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
    /* 主通道数据标题 */
    lv_obj_t *main_data_label = lv_label_create(parent);
    lv_label_set_text(main_data_label, "主通道数据");
    lv_obj_set_pos(main_data_label, 0, 0);
    lv_obj_set_style_text_font(main_data_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(main_data_label, lv_color_hex(0x333333), 0);

    /* 主通道数据容器 - 不需要额外的卡片，直接在白色面板上显示 */
    int y_pos = 35;

    /* 第一行数据：EC1, PH1, EC2, PH2 */
    const char *row1_labels[] = {"EC1(ms/cm)", "PH1酸碱度", "EC2(ms/cm)", "PH2酸碱度"};
    int col_width = 160;
    for (int i = 0; i < 4; i++) {
        /* 数值 */
        lv_obj_t *value = lv_label_create(parent);
        lv_label_set_text(value, "---");
        lv_obj_set_pos(value, i * col_width + 10, y_pos);
        lv_obj_set_style_text_font(value, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(value, lv_color_hex(0x333333), 0);

        /* 标签 */
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, row1_labels[i]);
        lv_obj_set_pos(label, i * col_width + 10, y_pos + 30);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
    }

    /* 分隔线 */
    y_pos += 80;
    lv_obj_t *line1 = lv_obj_create(parent);
    lv_obj_set_size(line1, 650, 1);
    lv_obj_set_pos(line1, 0, y_pos);
    lv_obj_set_style_bg_color(line1, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(line1, 0, 0);
    lv_obj_clear_flag(line1, LV_OBJ_FLAG_SCROLLABLE);

    /* 第二行数据：本次用水, 瞬时流量, 累计流量, 肥管压力 */
    y_pos += 20;
    const char *row2_labels[] = {"本次用水(m³)", "瞬时流量(m³/h)", "累计流量(m³)", "肥管压力 (Mpa)"};
    for (int i = 0; i < 4; i++) {
        /* 数值 */
        lv_obj_t *value = lv_label_create(parent);
        lv_label_set_text(value, "---");
        lv_obj_set_pos(value, i * col_width + 10, y_pos);
        lv_obj_set_style_text_font(value, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(value, lv_color_hex(0x333333), 0);

        /* 标签 */
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, row2_labels[i]);
        lv_obj_set_pos(label, i * col_width + 10, y_pos + 30);
        lv_obj_set_style_text_font(label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(0x666666), 0);
    }

    /* 注肥通道数据标题 */
    y_pos += 90;
    lv_obj_t *fert_data_label = lv_label_create(parent);
    lv_label_set_text(fert_data_label, "注肥通道数据");
    lv_obj_set_pos(fert_data_label, 0, y_pos);
    lv_obj_set_style_text_font(fert_data_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(fert_data_label, lv_color_hex(0x333333), 0);

    /* 注肥通道数据表格 */
    y_pos += 30;
    lv_obj_t *fert_table = lv_obj_create(parent);
    lv_obj_set_size(fert_table, 650, 280);
    lv_obj_set_pos(fert_table, 0, y_pos);
    lv_obj_set_style_bg_color(fert_table, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(fert_table, 1, 0);
    lv_obj_set_style_border_color(fert_table, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_radius(fert_table, 10, 0);
    lv_obj_set_style_pad_all(fert_table, 0, 0);
    lv_obj_clear_flag(fert_table, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头 */
    lv_obj_t *table_header = lv_obj_create(fert_table);
    lv_obj_set_size(table_header, 650, 45);
    lv_obj_set_pos(table_header, 0, 0);
    lv_obj_set_style_bg_color(table_header, lv_color_hex(0xe8f4f8), 0);
    lv_obj_set_style_border_width(table_header, 0, 0);
    lv_obj_set_style_radius(table_header, 0, 0);
    lv_obj_set_style_pad_all(table_header, 0, 0);
    lv_obj_clear_flag(table_header, LV_OBJ_FLAG_SCROLLABLE);

    const char *headers[] = {"管道", "液位(m)", "剩余容量(L)", "瞬时流量(L/h)", "本次用肥(L)", "累计流量(L)"};
    int header_x[] = {20, 90, 180, 300, 430, 550};

    for (int i = 0; i < 6; i++) {
        lv_obj_t *h_label = lv_label_create(table_header);
        lv_label_set_text(h_label, headers[i]);
        lv_obj_set_pos(h_label, header_x[i], 15);
        lv_obj_set_style_text_font(h_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(h_label, lv_color_hex(0x333333), 0);
    }

    /* 数据行 - 1号通道 */
    int row_y = 55;

    /* 管道 */
    lv_obj_t *label_pipe = lv_label_create(fert_table);
    lv_label_set_text(label_pipe, "1号");
    lv_obj_set_pos(label_pipe, 30, row_y);
    lv_obj_set_style_text_font(label_pipe, &my_font_cn_16, 0);

    /* 其他数据列 - 都显示 --- */
    const int data_x[] = {100, 210, 330, 460, 570};
    for (int i = 0; i < 5; i++) {
        lv_obj_t *label_data = lv_label_create(fert_table);
        lv_label_set_text(label_data, "---");
        lv_obj_set_pos(label_data, data_x[i], row_y);
        lv_obj_set_style_text_font(label_data, &my_font_cn_16, 0);
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
}

/**
 * @brief 懒加载切换视图：销毁旧视图，创建新视图
 */
static void switch_to_tab(int tab_index)
{
    if (!g_view_container) return;

    /* 清空视图容器（销毁当前视图的所有子对象） */
    lv_obj_clean(g_view_container);
    g_left_panel = NULL;
    g_right_panel = NULL;
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
    /* 创建白色背景面板 */
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 1168, 660);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 15, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶部统计 */
    lv_obj_t *label1 = lv_label_create(panel);
    lv_label_set_text(label1, "全部阀门个数: ");
    lv_obj_set_style_text_font(label1, &my_fontbd_16, 0);
    lv_obj_set_pos(label1, 10, 10);

    g_valve_total_label = lv_label_create(panel);
    lv_label_set_text(g_valve_total_label, "7");
    lv_obj_set_style_text_font(g_valve_total_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(g_valve_total_label, lv_color_hex(0x2196F3), 0);
    lv_obj_set_pos(g_valve_total_label, 150, 10);

    lv_obj_t *label3 = lv_label_create(panel);
    lv_label_set_text(label3, "    开启阀门个数: ");
    lv_obj_set_style_text_font(label3, &my_fontbd_16, 0);
    lv_obj_set_pos(label3, 180, 10);

    g_valve_open_label = lv_label_create(panel);
    lv_label_set_text(g_valve_open_label, "0");
    lv_obj_set_style_text_font(g_valve_open_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(g_valve_open_label, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_pos(g_valve_open_label, 350, 10);

    /* 阀门网格：7 个阀门卡片 */
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

    /* 重置阀门状态标签和按钮 */
    for (int i = 0; i < 7; i++) {
        g_valve_status_labels[i] = NULL;
        g_valve_btns[i] = NULL;
        g_valve_btn_labels[i] = NULL;
    }

    const char *valve_names[] = {"主管道阀", "副管道1阀", "副管道2阀", "副管道3阀",
                                  "副管道4阀", "副管道5阀", "副管道6阀"};
    for (int i = 0; i < 7; i++) {
        lv_obj_t *card = lv_obj_create(g_valve_container);
        lv_obj_set_size(card, 260, 100);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xf0f8ff), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xd0e0f0), 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 10, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, valve_names[i]);
        lv_obj_set_style_text_font(name, &my_fontbd_16, 0);
        lv_obj_set_pos(name, 5, 5);

        /* 状态标签（根据当前状态显示） */
        lv_obj_t *status = lv_label_create(card);
        lv_label_set_text(status, g_valve_states[i] ? "开启  流量:---  压力:---" : "关闭  流量:---  压力:---");
        lv_obj_set_style_text_font(status, &my_font_cn_16, 0);
        lv_obj_set_pos(status, 5, 40);
        g_valve_status_labels[i] = status;

        /* 单个切换按钮：关闭时红色显示"关闭"，开启时绿色显示"开启" */
        lv_obj_t *btn = lv_btn_create(card);
        lv_obj_set_size(btn, 55, 26);
        lv_obj_set_pos(btn, 180, 5);
        lv_obj_set_style_bg_color(btn, g_valve_states[i] ? lv_color_hex(0xE53935) : lv_color_hex(0x4CAF50), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        lv_obj_add_event_cb(btn, valve_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        g_valve_btns[i] = btn;

        lv_obj_t *btn_label = lv_label_create(btn);
        lv_label_set_text(btn_label, g_valve_states[i] ? "关闭" : "开启");
        lv_obj_set_style_text_font(btn_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
        lv_obj_center(btn_label);
        g_valve_btn_labels[i] = btn_label;
    }

    /* 刷新开启计数 */
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

    /* 发送控制指令 */
    if (g_device_control_cb) {
        g_device_control_cb(g_pending_dev_type, g_pending_dev_id, g_pending_on);
    }

    /* 更新 UI 状态 */
    if (g_pending_label_type == 0 && g_pending_label_idx >= 0 && g_pending_label_idx < 5) {
        /* 设备卡片 */
        g_dev_states[g_pending_label_idx] = g_pending_on;
        if (g_dev_status_labels[g_pending_label_idx]) {
            lv_label_set_text(g_dev_status_labels[g_pending_label_idx],
                              g_pending_on ? "开启" : "关闭");
        }
    } else if (g_pending_label_type == 1 && g_pending_label_idx >= 0 && g_pending_label_idx < 7) {
        /* 阀门卡片 */
        g_valve_states[g_pending_label_idx] = g_pending_on;
        if (g_valve_status_labels[g_pending_label_idx]) {
            lv_label_set_text(g_valve_status_labels[g_pending_label_idx],
                              g_pending_on ? "开启  流量:---  压力:---" : "关闭  流量:---  压力:---");
        }
        /* 更新按钮颜色和文字 */
        if (g_valve_btns[g_pending_label_idx]) {
            lv_obj_set_style_bg_color(g_valve_btns[g_pending_label_idx],
                                      g_pending_on ? lv_color_hex(0xE53935) : lv_color_hex(0x4CAF50), 0);
        }
        if (g_valve_btn_labels[g_pending_label_idx]) {
            lv_label_set_text(g_valve_btn_labels[g_pending_label_idx],
                              g_pending_on ? "关闭" : "开启");
        }
        update_valve_open_count();
    }

    /* 关闭对话框 */
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
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
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
 * @brief 灌区视图中的开关回调（直接发送控制指令）
 * user_data 编码: (dev_type << 8) | dev_id
 */
static void zone_switch_cb(lv_event_t *e)
{
    uint32_t code = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    uint8_t dev_type = (code >> 8) & 0xFF;
    uint8_t dev_id   = code & 0xFF;
    lv_obj_t *sw = lv_event_get_target(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    if (g_device_control_cb) {
        g_device_control_cb(dev_type, dev_id, on);
    }
}

/**
 * @brief 创建灌区控制视图 - 显示 6 个田地灌区 + 3 个储料罐
 */
static void create_zone_control_view(lv_obj_t *parent)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 1168, 660);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_pad_all(panel, 15, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "灌区概览（6 田地 + 3 储料罐）");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
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

    /* 6 个田地卡片 */
    for (int i = 0; i < 6; i++) {
        lv_obj_t *card = lv_obj_create(g_zone_container);
        lv_obj_set_size(card, 360, 100);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xf0fff0), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xc3e6cb), 0);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 8, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        char buf[32];
        snprintf(buf, sizeof(buf), "田地%d", i + 1);
        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, buf);
        lv_obj_set_style_text_font(name, &my_fontbd_16, 0);
        lv_obj_set_pos(name, 5, 5);

        lv_obj_t *info = lv_label_create(card);
        lv_label_set_text(info, "N:--- P:--- K:---\n温:--- 湿:--- 光:---");
        lv_obj_set_style_text_font(info, &my_font_cn_16, 0);
        lv_obj_set_style_text_line_space(info, 6, 0);
        lv_obj_set_pos(info, 5, 35);

        /* 副管道阀门开关 */
        lv_obj_t *sw = lv_switch_create(card);
        lv_obj_set_size(sw, 45, 22);
        lv_obj_set_pos(sw, 295, 5);
        uint32_t cb_code = (0x02 << 8) | (uint8_t)(i + 1);
        lv_obj_add_event_cb(sw, zone_switch_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)cb_code);
    }

    /* 3 个储料罐卡片 */
    const char *tank_names[] = {"储料罐N", "储料罐P", "储料罐K"};
    for (int i = 0; i < 3; i++) {
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
        lv_obj_set_style_text_font(name, &my_fontbd_16, 0);
        lv_obj_set_pos(name, 5, 5);

        lv_obj_t *info = lv_label_create(card);
        lv_label_set_text(info, "液位: ---L  状态: 关闭");
        lv_obj_set_style_text_font(info, &my_font_cn_16, 0);
        lv_obj_set_pos(info, 5, 40);

        lv_obj_t *sw = lv_switch_create(card);
        lv_obj_set_size(sw, 45, 22);
        lv_obj_set_pos(sw, 295, 5);
        /* dev_type=0x03(控制系统/储料罐), dev_id=1~3 */
        uint32_t cb_code = (0x03 << 8) | (uint8_t)(i + 1);
        lv_obj_add_event_cb(sw, zone_switch_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)cb_code);
    }
}

/**
 * @brief 创建传感监测视图 - 显示所有在线传感器实时值
 */
static void create_sensor_monitor_view(lv_obj_t *parent)
{
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
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_pos(title, 10, 10);

    /* 可滚动的传感器列表容器 */
    g_sensor_container = lv_obj_create(panel);
    lv_obj_set_size(g_sensor_container, 1138, 590);
    lv_obj_set_pos(g_sensor_container, 0, 45);
    lv_obj_set_style_bg_opa(g_sensor_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_sensor_container, 0, 0);
    lv_obj_set_style_pad_all(g_sensor_container, 0, 0);
    lv_obj_set_flex_flow(g_sensor_container, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(g_sensor_container, 8, 0);
    lv_obj_set_style_pad_column(g_sensor_container, 8, 0);

    /* 6 个田地传感器组 */
    static const char *sensor_labels[] = {"N", "P", "K", "温", "湿", "光"};
    for (int f = 0; f < 6; f++) {
        lv_obj_t *card = lv_obj_create(g_sensor_container);
        lv_obj_set_size(card, 555, 80);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xf5f5f5), 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0xe0e0e0), 0);
        lv_obj_set_style_radius(card, 6, 0);
        lv_obj_set_style_pad_all(card, 6, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        char buf[16];
        snprintf(buf, sizeof(buf), "田地%d", f + 1);
        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, buf);
        lv_obj_set_style_text_font(name, &my_fontbd_16, 0);
        lv_obj_set_pos(name, 5, 5);

        for (int s = 0; s < 6; s++) {
            int sx = 5 + s * 88;
            lv_obj_t *slabel = lv_label_create(card);
            snprintf(buf, sizeof(buf), "%s:---", sensor_labels[s]);
            lv_label_set_text(slabel, buf);
            lv_obj_set_style_text_font(slabel, &my_font_cn_16, 0);
            lv_obj_set_pos(slabel, sx, 45);
        }
    }

    /* 管道传感器 */
    lv_obj_t *pipe_card = lv_obj_create(g_sensor_container);
    lv_obj_set_size(pipe_card, 1130, 80);
    lv_obj_set_style_bg_color(pipe_card, lv_color_hex(0xf0f8ff), 0);
    lv_obj_set_style_border_width(pipe_card, 1, 0);
    lv_obj_set_style_border_color(pipe_card, lv_color_hex(0xd0e0f0), 0);
    lv_obj_set_style_radius(pipe_card, 6, 0);
    lv_obj_set_style_pad_all(pipe_card, 6, 0);
    lv_obj_clear_flag(pipe_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pipe_title = lv_label_create(pipe_card);
    lv_label_set_text(pipe_title, "管道传感器");
    lv_obj_set_style_text_font(pipe_title, &my_fontbd_16, 0);
    lv_obj_set_pos(pipe_title, 5, 5);

    for (int p = 0; p < 7; p++) {
        char buf[32];
        lv_obj_t *plabel = lv_label_create(pipe_card);
        if (p == 0)
            snprintf(buf, sizeof(buf), "主:---");
        else
            snprintf(buf, sizeof(buf), "P%d:---", p);
        lv_label_set_text(plabel, buf);
        lv_obj_set_style_text_font(plabel, &my_font_cn_16, 0);
        lv_obj_set_pos(plabel, 5 + p * 155, 45);
    }
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

void ui_device_register_control_cb(ui_device_control_cb_t cb)
{
    g_device_control_cb = cb;
}
