/**
 * @file ui_device.c
 * @brief 设备控制界面实现
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include <stdio.h>
#include <string.h>

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_tab_buttons(lv_obj_t *parent);
static void create_control_panel(lv_obj_t *parent);
static void create_data_panel(lv_obj_t *parent);
static void create_device_card(lv_obj_t *parent, const char *title, int x, int y, bool is_double);
static void btn_device_control_cb(lv_event_t *e);
static void tab_btn_cb(lv_event_t *e);

/* 全局变量 */
static lv_obj_t *g_left_panel = NULL;   /* 左侧白色面板（主机控制视图） */
static lv_obj_t *g_right_panel = NULL;  /* 右侧白色面板（主机控制视图） */
static lv_obj_t *g_view_container = NULL; /* 视图容器（懒加载） */
static lv_obj_t *g_tab_buttons[4] = {NULL};   /* 标签按钮数组 */
static int g_active_tab = 0;             /* 当前活动标签页 */

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
    g_active_tab = 0;

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
 * @brief 创建设备卡片
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

    /* 创建卡片容器 - 浅绿色背景 */
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, card_width, card_height);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xd4edda), 0);  /* 浅绿色 */
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xc3e6cb), 0);
    lv_obj_set_style_radius(card, 5, 0);
    lv_obj_set_style_pad_all(card, 3, 0);  /* 减小padding到3px */
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 - 左上角 */
    lv_obj_t *label_title = lv_label_create(card);
    lv_label_set_text(label_title, title);
    lv_obj_set_pos(label_title, 3, 3);
    lv_obj_set_style_text_font(label_title, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x333333), 0);

    /* 左下角：关闭文字 */
    lv_obj_t *label_close = lv_label_create(card);
    lv_label_set_text(label_close, "关闭");
    lv_obj_set_pos(label_close, 3, 43);  /* 固定在43px位置 */
    lv_obj_set_style_text_font(label_close, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_close, lv_color_hex(0x333333), 0);

    /* 右下角图标 */
    if (is_double) {
        /* 上面两个：打印图标 */
        lv_obj_t *icon_print = lv_label_create(card);
        lv_label_set_text(icon_print, LV_SYMBOL_IMAGE);
        lv_obj_set_pos(icon_print, 115, 43);  /* 固定在右下角 */
        lv_obj_set_style_text_font(icon_print, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(icon_print, lv_color_hex(0x666666), 0);
    } else {
        /* 下面三个：设备图标 */
        lv_obj_t *icon = lv_label_create(card);
        if (strstr(title, "出肥阀") != NULL) {
            lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
        } else if (strstr(title, "注水阀") != NULL) {
            lv_label_set_text(icon, LV_SYMBOL_REFRESH);
        } else if (strstr(title, "搅拌机") != NULL) {
            lv_label_set_text(icon, LV_SYMBOL_SETTINGS);
        } else {
            lv_label_set_text(icon, LV_SYMBOL_SHUFFLE);
        }
        lv_obj_set_pos(icon, 115, 43);  /* 固定在右下角 */
        lv_obj_set_style_text_font(icon, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x666666), 0);
    }
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
 * @brief 设备控制按钮回调
 */
static void btn_device_control_cb(lv_event_t *e)
{
    (void)e;
    /* TODO: 实现设备控制逻辑 */
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
 * @brief 创建阀门控制视图
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

    /* 顶部左侧：统计信息 - 使用多个标签实现不同颜色 */
    lv_obj_t *stats_container = lv_obj_create(panel);
    lv_obj_set_size(stats_container, 600, 40);
    lv_obj_set_pos(stats_container, 10, 10);
    lv_obj_set_style_bg_opa(stats_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats_container, 0, 0);
    lv_obj_set_style_pad_all(stats_container, 0, 0);
    lv_obj_clear_flag(stats_container, LV_OBJ_FLAG_SCROLLABLE);

    int x_offset = 0;

    /* "全部阀门个数: " */
    lv_obj_t *label1 = lv_label_create(stats_container);
    lv_label_set_text(label1, "全部阀门个数: ");
    lv_obj_set_style_text_font(label1, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label1, lv_color_hex(0x333333), 0);
    lv_obj_set_pos(label1, x_offset, 0);
    x_offset += 140;

    /* 蓝色的 "0" */
    lv_obj_t *label2 = lv_label_create(stats_container);
    lv_label_set_text(label2, "0");
    lv_obj_set_style_text_font(label2, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label2, lv_color_hex(0x2196F3), 0);  /* 蓝色 */
    lv_obj_set_pos(label2, x_offset, 0);
    x_offset += 30;

    /* "    开启阀门个数: " */
    lv_obj_t *label3 = lv_label_create(stats_container);
    lv_label_set_text(label3, "    开启阀门个数: ");
    lv_obj_set_style_text_font(label3, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label3, lv_color_hex(0x333333), 0);
    lv_obj_set_pos(label3, x_offset, 0);
    x_offset += 170;

    /* 绿色的 "0" */
    lv_obj_t *label4 = lv_label_create(stats_container);
    lv_label_set_text(label4, "0");
    lv_obj_set_style_text_font(label4, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label4, lv_color_hex(0x4CAF50), 0);  /* 绿色 */
    lv_obj_set_pos(label4, x_offset, 0);

    /* 顶部右侧：一键关闭按钮（橙色） */
    lv_obj_t *close_all_btn = lv_btn_create(panel);
    lv_obj_set_size(close_all_btn, 120, 40);
    lv_obj_set_pos(close_all_btn, 1020, 5);
    lv_obj_set_style_bg_color(close_all_btn, lv_color_hex(0xff9800), 0);  /* 橙色 */
    lv_obj_set_style_border_width(close_all_btn, 0, 0);
    lv_obj_set_style_radius(close_all_btn, 5, 0);

    lv_obj_t *btn_label = lv_label_create(close_all_btn);
    lv_label_set_text(btn_label, "一键关闭");
    lv_obj_set_style_text_color(btn_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(btn_label, &my_font_cn_16, 0);
    lv_obj_center(btn_label);

    /* 底部：分页控件 */
    int pagination_y = 620;

    /* 首页按钮 */
    lv_obj_t *first_page_btn = lv_btn_create(panel);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, 350, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    lv_obj_t *prev_page_btn = lv_btn_create(panel);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, 430, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    lv_obj_t *page_label = lv_label_create(panel);
    lv_label_set_text(page_label, "0/0");
    lv_obj_set_pos(page_label, 540, pagination_y + 8);
    lv_obj_set_style_text_font(page_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    lv_obj_t *next_page_btn = lv_btn_create(panel);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, 600, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    lv_obj_t *last_page_btn = lv_btn_create(panel);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, 690, pagination_y);
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
 * @brief 创建灌区控制视图
 */
static void create_zone_control_view(lv_obj_t *parent)
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

    /* 顶部：统计信息 - 使用多个标签实现不同颜色 */
    lv_obj_t *stats_container = lv_obj_create(panel);
    lv_obj_set_size(stats_container, 600, 40);
    lv_obj_set_pos(stats_container, 10, 10);
    lv_obj_set_style_bg_opa(stats_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats_container, 0, 0);
    lv_obj_set_style_pad_all(stats_container, 0, 0);
    lv_obj_clear_flag(stats_container, LV_OBJ_FLAG_SCROLLABLE);

    int x_offset = 0;

    /* "全部灌区个数: " */
    lv_obj_t *label1 = lv_label_create(stats_container);
    lv_label_set_text(label1, "全部灌区个数: ");
    lv_obj_set_style_text_font(label1, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label1, lv_color_hex(0x333333), 0);
    lv_obj_set_pos(label1, x_offset, 0);
    x_offset += 140;

    /* 蓝色的 "0" */
    lv_obj_t *label2 = lv_label_create(stats_container);
    lv_label_set_text(label2, "0");
    lv_obj_set_style_text_font(label2, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label2, lv_color_hex(0x2196F3), 0);  /* 蓝色 */
    lv_obj_set_pos(label2, x_offset, 0);
    x_offset += 30;

    /* "    开启灌区个数: " */
    lv_obj_t *label3 = lv_label_create(stats_container);
    lv_label_set_text(label3, "    开启灌区个数: ");
    lv_obj_set_style_text_font(label3, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label3, lv_color_hex(0x333333), 0);
    lv_obj_set_pos(label3, x_offset, 0);
    x_offset += 170;

    /* 绿色的 "0" */
    lv_obj_t *label4 = lv_label_create(stats_container);
    lv_label_set_text(label4, "0");
    lv_obj_set_style_text_font(label4, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label4, lv_color_hex(0x4CAF50), 0);  /* 绿色 */
    lv_obj_set_pos(label4, x_offset, 0);

    /* 底部：分页控件 */
    int pagination_y = 620;

    /* 首页按钮 */
    lv_obj_t *first_page_btn = lv_btn_create(panel);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, 350, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    lv_obj_t *prev_page_btn = lv_btn_create(panel);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, 430, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    lv_obj_t *page_label = lv_label_create(panel);
    lv_label_set_text(page_label, "0/0");
    lv_obj_set_pos(page_label, 540, pagination_y + 8);
    lv_obj_set_style_text_font(page_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    lv_obj_t *next_page_btn = lv_btn_create(panel);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, 600, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    lv_obj_t *last_page_btn = lv_btn_create(panel);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, 690, pagination_y);
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
 * @brief 创建传感监测视图
 */
static void create_sensor_monitor_view(lv_obj_t *parent)
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

    /* 顶部：统计信息 - 使用多个标签实现不同颜色 */
    lv_obj_t *stats_container = lv_obj_create(panel);
    lv_obj_set_size(stats_container, 300, 40);
    lv_obj_set_pos(stats_container, 10, 10);
    lv_obj_set_style_bg_opa(stats_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats_container, 0, 0);
    lv_obj_set_style_pad_all(stats_container, 0, 0);
    lv_obj_clear_flag(stats_container, LV_OBJ_FLAG_SCROLLABLE);

    int x_offset = 0;

    /* "传感器个数: " */
    lv_obj_t *label1 = lv_label_create(stats_container);
    lv_label_set_text(label1, "传感器个数: ");
    lv_obj_set_style_text_font(label1, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label1, lv_color_hex(0x333333), 0);
    lv_obj_set_pos(label1, x_offset, 0);
    x_offset += 120;

    /* 蓝色的 "0" */
    lv_obj_t *label2 = lv_label_create(stats_container);
    lv_label_set_text(label2, "0");
    lv_obj_set_style_text_font(label2, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(label2, lv_color_hex(0x2196F3), 0);  /* 蓝色 */
    lv_obj_set_pos(label2, x_offset, 0);

    /* 底部：分页控件 */
    int pagination_y = 620;

    /* 首页按钮 */
    lv_obj_t *first_page_btn = lv_btn_create(panel);
    lv_obj_set_size(first_page_btn, 70, 35);
    lv_obj_set_pos(first_page_btn, 350, pagination_y);
    lv_obj_set_style_bg_color(first_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(first_page_btn, 0, 0);
    lv_obj_set_style_radius(first_page_btn, 5, 0);

    lv_obj_t *first_label = lv_label_create(first_page_btn);
    lv_label_set_text(first_label, "首页");
    lv_obj_set_style_text_color(first_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(first_label, &my_font_cn_16, 0);
    lv_obj_center(first_label);

    /* 上一页按钮 */
    lv_obj_t *prev_page_btn = lv_btn_create(panel);
    lv_obj_set_size(prev_page_btn, 80, 35);
    lv_obj_set_pos(prev_page_btn, 430, pagination_y);
    lv_obj_set_style_bg_color(prev_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(prev_page_btn, 0, 0);
    lv_obj_set_style_radius(prev_page_btn, 5, 0);

    lv_obj_t *prev_label = lv_label_create(prev_page_btn);
    lv_label_set_text(prev_label, "上一页");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(prev_label, &my_font_cn_16, 0);
    lv_obj_center(prev_label);

    /* 页码显示 */
    lv_obj_t *page_label = lv_label_create(panel);
    lv_label_set_text(page_label, "0/0");
    lv_obj_set_pos(page_label, 540, pagination_y + 8);
    lv_obj_set_style_text_font(page_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(page_label, lv_color_hex(0x333333), 0);

    /* 下一页按钮 */
    lv_obj_t *next_page_btn = lv_btn_create(panel);
    lv_obj_set_size(next_page_btn, 80, 35);
    lv_obj_set_pos(next_page_btn, 600, pagination_y);
    lv_obj_set_style_bg_color(next_page_btn, lv_color_hex(0xe0e0e0), 0);
    lv_obj_set_style_border_width(next_page_btn, 0, 0);
    lv_obj_set_style_radius(next_page_btn, 5, 0);

    lv_obj_t *next_label = lv_label_create(next_page_btn);
    lv_label_set_text(next_label, "下一页");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0x333333), 0);
    lv_obj_set_style_text_font(next_label, &my_font_cn_16, 0);
    lv_obj_center(next_label);

    /* 尾页按钮 */
    lv_obj_t *last_page_btn = lv_btn_create(panel);
    lv_obj_set_size(last_page_btn, 70, 35);
    lv_obj_set_pos(last_page_btn, 690, pagination_y);
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
