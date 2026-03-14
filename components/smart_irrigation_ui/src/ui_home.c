/**
 * @file ui_home.c
 * @brief 智慧种植园监控系统 - 首页界面
 * 布局：左右分栏，直接在内容区绘制
 * - 左侧：圆环显示 + 禁用自动化按钮 + 设备状态列表
 * - 右侧：灌溉计划表格 + 按钮 + 状态文字
 */

#include "ui_common.h"
#include "ui_numpad.h"
#include <math.h>  /* 用于cosf和sinf函数 */

/*********************
 *  STATIC PROTOTYPES
 *********************/
static void create_mode_circle(lv_obj_t *parent);
static void create_status_list(lv_obj_t *parent);
static void create_schedule_area(lv_obj_t *parent);
static void btn_manual_program_cb(lv_event_t *e);
static void btn_manual_irrigation_cb(lv_event_t *e);
static void btn_disable_auto_cb(lv_event_t *e);
static void show_enable_auto_dialog(void);
static void show_warning_dialog(const char *title, const char *message);
static void show_manual_irrigation_dialog(void);
static void show_program_selection_dialog(void);
static void program_start_confirm_cb(lv_event_t *e);
static void program_start_cancel_cb(lv_event_t *e);
static void dialog_confirm_cb(lv_event_t *e);
static void dialog_cancel_cb(lv_event_t *e);
static void dialog_ok_cb(lv_event_t *e);
static void irrigation_confirm_cb(lv_event_t *e);
static void irrigation_cancel_cb(lv_event_t *e);
static void btn_uniform_set_cb(lv_event_t *e);
static void show_uniform_set_confirm_dialog(void);
static void uniform_set_confirm_cb(lv_event_t *e);
static void uniform_set_cancel_cb(lv_event_t *e);
static void enable_auto_mode(void);
static void disable_auto_mode(void);
static void textarea_click_cb(lv_event_t *e);

/*********************
 *  STATIC VARIABLES
 *********************/
static lv_obj_t *g_arc = NULL;           /* 圆环对象 */
static lv_obj_t *g_mode_label = NULL;    /* 模式文字 */
static lv_obj_t *g_btn_toggle = NULL;    /* 切换按钮 */
static lv_obj_t *g_btn_label = NULL;     /* 按钮文字 */
static lv_obj_t *g_tick_lines[90];       /* 刻度线数组 */
static int g_tick_count = 0;             /* 实际创建的刻度线数量 */
static bool g_auto_mode_enabled = true;  /* 自动模式状态：true=启用，false=禁用 */
static lv_obj_t *g_dialog = NULL;        /* 确认对话框 */
static lv_obj_t *g_program_checkboxes[100]; /* 程序选择复选框数组 */
static int g_program_checkbox_count = 0;    /* 实际创建的复选框数量 */

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 创建首页界面
 * @param parent 父容器（主内容区）
 */
void ui_home_create(lv_obj_t *parent)
{
    /* 重置静态指针（旧对象已被 ui_switch_nav 中的 lv_obj_clean 销毁） */
    g_arc = NULL;
    g_mode_label = NULL;
    g_btn_toggle = NULL;
    g_btn_label = NULL;
    g_tick_count = 0;
    g_program_checkbox_count = 0;
    /* g_dialog 由 ui_home_close_dialog() 在 ui_switch_nav 中处理 */

    /* 清空父容器 */
    lv_obj_clean(parent);

    /* 创建左侧：圆环模式 */
    create_mode_circle(parent);

    /* 创建左侧：设备状态列表 */
    create_status_list(parent);

    /* 创建右侧：灌溉计划区域 */
    create_schedule_area(parent);

    /* 同步自动模式状态到UI（g_auto_mode_enabled 在页面切换后仍保持） */
    if (!g_auto_mode_enabled) {
        disable_auto_mode();
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * @brief 创建模式圆环显示（左上角白色卡片）
 */
static void create_mode_circle(lv_obj_t *parent)
{
    /* 创建左上角白色卡片 - 约1/4宽度 */
    /* 内容区宽度1178，1/4约294，减去边距 = 284 */
    lv_obj_t *mode_card = lv_obj_create(parent);
    lv_obj_set_size(mode_card, 284, 362);  /* 高度：735/2-5-5 = 362 */
    lv_obj_set_pos(mode_card, 5, 5);
    lv_obj_set_style_bg_color(mode_card, lv_color_white(), 0);
    lv_obj_set_style_border_width(mode_card, 0, 0);
    lv_obj_set_style_radius(mode_card, 10, 0);
    lv_obj_set_style_pad_all(mode_card, 0, 0);
    lv_obj_clear_flag(mode_card, LV_OBJ_FLAG_SCROLLABLE);

    /* 圆弧（进度环） - 照片中看到是蓝色粗圆环，更大一些 */
    g_arc = lv_arc_create(mode_card);
    lv_obj_set_size(g_arc, 220, 220);
    lv_obj_set_pos(g_arc, 32, 30);  /* 居中：(284-220)/2 = 32 */
    lv_arc_set_range(g_arc, 0, 100);
    lv_arc_set_value(g_arc, 100);  /* 设置为100，让圆环显示满圈 */
    lv_arc_set_bg_angles(g_arc, 0, 360);
    lv_obj_set_style_arc_width(g_arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(g_arc, lv_color_hex(0xe0e0e0), LV_PART_MAIN);
    lv_obj_set_style_arc_color(g_arc, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_remove_flag(g_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_rounded(g_arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(g_arc, false, LV_PART_INDICATOR);
    /* 隐藏拖动的小球 knob */
    lv_obj_set_style_bg_opa(g_arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(g_arc, 0, LV_PART_KNOB);

    /* 在圆环外围添加刻度线 - 像时钟刻度那样 */
    int center_x = 32 + 110;  /* 圆心X: 卡片内的绝对位置 */
    int center_y = 30 + 110;  /* 圆心Y */
    int arc_outer_radius = 110;  /* 圆环外边缘半径 */
    int gap = 8;  /* 刻度线与圆环的间距 */
    int inner_radius = arc_outer_radius + gap;  /* 刻度线起点：110 + 8 = 118 */
    int outer_radius = inner_radius + 6;  /* 刻度线终点：118 + 6 = 124，刻度线长度6px */
    int tick_count = 90;      /* 刻度数量增加到90 */

    /* 为所有刻度线分配静态点数组 */
    static lv_point_precise_t tick_points[90][2];

    g_tick_count = 0;  /* 重置刻度线计数 */

    for (int i = 0; i < tick_count; i++) {
        float angle_deg = i * 360.0f / tick_count;  /* 角度（度数） */

        /* 跳过下方90度区域的刻度线 - LVGL中90度在底部，所以范围是45-135度 */
        if (angle_deg >= 45 && angle_deg <= 135) {
            continue;
        }

        float angle = angle_deg * 3.14159f / 180.0f;  /* 转换为弧度 */

        /* 计算刻度线的起点和终点 */
        int x1 = center_x + (int)(inner_radius * cosf(angle));
        int y1 = center_y + (int)(inner_radius * sinf(angle));
        int x2 = center_x + (int)(outer_radius * cosf(angle));
        int y2 = center_y + (int)(outer_radius * sinf(angle));

        /* 保存到独立的点数组 */
        tick_points[i][0].x = x1;
        tick_points[i][0].y = y1;
        tick_points[i][1].x = x2;
        tick_points[i][1].y = y2;

        /* 创建刻度线并保存引用 */
        g_tick_lines[g_tick_count] = lv_line_create(mode_card);
        lv_line_set_points(g_tick_lines[g_tick_count], tick_points[i], 2);
        lv_obj_set_style_line_width(g_tick_lines[g_tick_count], 2, 0);  /* 线宽2px */
        lv_obj_set_style_line_color(g_tick_lines[g_tick_count], COLOR_PRIMARY, 0);  /* 刻度线颜色与圆环一致 */
        g_tick_count++;
    }

    /* 圆环中心文字 "自动模式" */
    g_mode_label = lv_label_create(mode_card);
    lv_label_set_text(g_mode_label, "自动模式");
    lv_obj_set_style_text_font(g_mode_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_align(g_mode_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(g_mode_label, COLOR_PRIMARY, 0);
    lv_obj_align_to(g_mode_label, g_arc, LV_ALIGN_CENTER, 0, 0);  /* 相对于圆环居中 */

    /* "禁用自动化" 按钮（红色边框，圆角矩形） - 在圆环正下方 */
    g_btn_toggle = lv_btn_create(mode_card);
    lv_obj_set_size(g_btn_toggle, 150, 42);
    lv_obj_set_pos(g_btn_toggle, 67, 270);  /* 居中：(284-150)/2 = 67 */
    lv_obj_set_style_bg_color(g_btn_toggle, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_btn_toggle, 2, 0);
    lv_obj_set_style_border_color(g_btn_toggle, COLOR_ERROR, 0);
    lv_obj_set_style_radius(g_btn_toggle, 19, 0);
    lv_obj_add_event_cb(g_btn_toggle, btn_disable_auto_cb, LV_EVENT_CLICKED, NULL);

    g_btn_label = lv_label_create(g_btn_toggle);
    lv_label_set_text(g_btn_label, "禁用自动化");
    lv_obj_set_style_text_color(g_btn_label, COLOR_ERROR, 0);
    lv_obj_set_style_text_font(g_btn_label, &my_font_cn_16, 0);
    lv_obj_center(g_btn_label);
}

/**
 * @brief 创建设备状态和按钮区域（下半部分的白色卡片）
 */
static void create_status_list(lv_obj_t *parent)
{
    /* 创建白色卡片容器 - 下半部分 */
    /* 内容区高度735，上下对半，每半约367px */
    /* 卡片距离浅灰色边界5px */
    lv_obj_t *status_card = lv_obj_create(parent);
    lv_obj_set_size(status_card, 1168, 358);  /* 高度：735-362-5-5-5 = 358 */
    lv_obj_set_pos(status_card, 5, 372);  /* Y: 5 + 362 + 5 = 372 */
    lv_obj_set_style_bg_color(status_card, lv_color_white(), 0);
    lv_obj_set_style_border_width(status_card, 0, 0);
    lv_obj_set_style_radius(status_card, 10, 0);
    lv_obj_set_style_pad_all(status_card, 0, 0);
    lv_obj_clear_flag(status_card, LV_OBJ_FLAG_SCROLLABLE);

    /* 左侧：设备状态 */
    /* 标题 "当前设备状态" */
    lv_obj_t *title = lv_label_create(status_card);
    lv_label_set_text(title, "当前设备状态");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(title, 20, 20);

    /* EC - 使用两个标签实现对齐 */
    lv_obj_t *label_ec_name = lv_label_create(status_card);
    lv_label_set_text(label_ec_name, "EC(ms/cm):");
    lv_obj_set_style_text_font(label_ec_name, &my_font_cn_16, 0);
    lv_obj_set_pos(label_ec_name, 30, 80);

    lv_obj_t *label_ec_val = lv_label_create(status_card);
    lv_label_set_text(label_ec_val, "---");
    lv_obj_set_style_text_font(label_ec_val, &my_font_cn_16, 0);
    lv_obj_set_pos(label_ec_val, 240, 80);

    /* PH */
    lv_obj_t *label_ph_name = lv_label_create(status_card);
    lv_label_set_text(label_ph_name, "PH:");
    lv_obj_set_style_text_font(label_ph_name, &my_font_cn_16, 0);
    lv_obj_set_pos(label_ph_name, 30, 145);

    lv_obj_t *label_ph_val = lv_label_create(status_card);
    lv_label_set_text(label_ph_val, "---");
    lv_obj_set_style_text_font(label_ph_val, &my_font_cn_16, 0);
    lv_obj_set_pos(label_ph_val, 240, 145);

    /* Pressure */
    lv_obj_t *label_pressure_name = lv_label_create(status_card);
    lv_label_set_text(label_pressure_name, "Pressure(MPa):");
    lv_obj_set_style_text_font(label_pressure_name, &my_font_cn_16, 0);
    lv_obj_set_pos(label_pressure_name, 30, 210);

    lv_obj_t *label_pressure_val = lv_label_create(status_card);
    lv_label_set_text(label_pressure_val, "---");
    lv_obj_set_style_text_font(label_pressure_val, &my_font_cn_16, 0);
    lv_obj_set_pos(label_pressure_val, 240, 210);

    /* Flow */
    lv_obj_t *label_flow_name = lv_label_create(status_card);
    lv_label_set_text(label_flow_name, "Flow(m3/h):");
    lv_obj_set_style_text_font(label_flow_name, &my_font_cn_16, 0);
    lv_obj_set_pos(label_flow_name, 30, 275);

    lv_obj_t *label_flow_val = lv_label_create(status_card);
    lv_label_set_text(label_flow_val, "---");
    lv_obj_set_style_text_font(label_flow_val, &my_font_cn_16, 0);
    lv_obj_set_pos(label_flow_val, 240, 275);

    /* 右上角：按钮区域 - 更小的尺寸 */
    lv_obj_t *btn_program = lv_btn_create(status_card);
    lv_obj_set_size(btn_program, 140, 45);
    lv_obj_set_pos(btn_program, 850, 20);  /* 右上角 */
    lv_obj_set_style_bg_color(btn_program, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_program, 22, 0);
    lv_obj_add_event_cb(btn_program, btn_manual_program_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_prog = lv_label_create(btn_program);
    lv_label_set_text(label_prog, "手动运行程序");
    lv_obj_set_style_text_color(label_prog, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_prog, &my_font_cn_16, 0);
    lv_obj_center(label_prog);

    lv_obj_t *btn_irrigation = lv_btn_create(status_card);
    lv_obj_set_size(btn_irrigation, 140, 45);
    lv_obj_set_pos(btn_irrigation, 1000, 20);  /* 右上角，第二个按钮 */
    lv_obj_set_style_bg_color(btn_irrigation, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_irrigation, 22, 0);
    lv_obj_add_event_cb(btn_irrigation, btn_manual_irrigation_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_irr = lv_label_create(btn_irrigation);
    lv_label_set_text(label_irr, "手动进行轮灌");
    lv_obj_set_style_text_color(label_irr, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_irr, &my_font_cn_16, 0);
    lv_obj_center(label_irr);

    /* 中间状态文字 */
    lv_obj_t *status_text = lv_label_create(status_card);
    lv_label_set_text(status_text, "无手动轮灌&无程序运行");
    lv_obj_set_style_text_color(status_text, COLOR_TEXT_GRAY, 0);
    lv_obj_set_style_text_font(status_text, &my_font_cn_16, 0);
    lv_obj_set_pos(status_text, 520, 160);
}

/**
 * @brief 创建灌溉计划区域（右上角白色卡片）
 */
static void create_schedule_area(lv_obj_t *parent)
{
    /* 创建右上角白色卡片 - 约3/4宽度 */
    /* X = 5 + 284 + 5 = 294, 宽度 = 1178 - 294 - 5 = 879 */
    lv_obj_t *schedule_card = lv_obj_create(parent);
    lv_obj_set_size(schedule_card, 879, 362);  /* 高度：735/2-5-5 = 362 */
    lv_obj_set_pos(schedule_card, 294, 5);
    lv_obj_set_style_bg_color(schedule_card, lv_color_white(), 0);
    lv_obj_set_style_border_width(schedule_card, 0, 0);
    lv_obj_set_style_radius(schedule_card, 10, 0);
    lv_obj_set_style_pad_all(schedule_card, 0, 0);
    lv_obj_clear_flag(schedule_card, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 - 直接显示文字 */
    lv_obj_t *title = lv_label_create(schedule_card);
    lv_label_set_text(title, "今日剩余灌溉计划（启动条件：定时）");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_MAIN, 0);
    lv_obj_set_pos(title, 15, 15);

    /* 表格表头区域 */
    lv_obj_t *header_bg = lv_obj_create(schedule_card);
    lv_obj_set_size(header_bg, 850, 40);
    lv_obj_set_pos(header_bg, 15, 50);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f8ff), 0);  /* 非常淡的浅蓝色背景 */
    lv_obj_set_style_border_width(header_bg, 0, 0);  /* 去掉边框 */
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);
    lv_obj_clear_flag(header_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头文字 - 6列 */
    const char *headers[] = {"序号", "程序名称", "关联配方", "启动条件", "启动时间", "合计时长"};
    int col_widths[] = {60, 180, 140, 140, 140, 140};  /* 总计约850 */
    int x_pos = 5;

    for (int i = 0; i < 6; i++) {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_style_text_color(header_label, COLOR_TEXT_MAIN, 0);
        lv_obj_set_pos(header_label, x_pos, 12);
        x_pos += col_widths[i];
    }

    /* 数据行区域 - 4行空白，调整高度确保底部有15px间距 */
    /* 总高度计算：362 - 90(表头结束位置) - 15(底部间距) = 257 / 4 = 64.25px/行，取64px */
    for (int row = 0; row < 4; row++) {
        lv_obj_t *row_bg = lv_obj_create(schedule_card);
        lv_obj_set_size(row_bg, 850, 64);  /* 每行64px */
        lv_obj_set_pos(row_bg, 15, 90 + row * 64);  /* Y: 50 + 40 = 90 */
        lv_obj_set_style_bg_color(row_bg, lv_color_white(), 0);
        lv_obj_set_style_border_width(row_bg, 0, 0);  /* 去掉边框 */
        lv_obj_set_style_radius(row_bg, 0, 0);
        lv_obj_set_style_pad_all(row_bg, 0, 0);
        lv_obj_clear_flag(row_bg, LV_OBJ_FLAG_SCROLLABLE);
    }
    /* 4行总高度: 64*4=256, 最后一行结束于Y:346, 底部间距: 362-346=16px */
}

/**
 * @brief 手动运行程序按钮回调
 */
static void btn_manual_program_cb(lv_event_t *e)
{
    (void)e;

    /* 获取程序数量 */
    int program_count = ui_program_get_count();

    if (program_count == 0) {
        /* 显示告警弹窗 */
        show_warning_dialog("告警提示", "无法启动，当前灌溉程序数量为0，请先\n添加程序！");
    }
    else {
        /* 显示程序选择对话框 */
        show_program_selection_dialog();
    }
}

/**
 * @brief 手动进行轮灌按钮回调
 */
static void btn_manual_irrigation_cb(lv_event_t *e)
{
    (void)e;
    show_manual_irrigation_dialog();
}

/**
 * @brief 禁用自动化按钮回调
 */
static void btn_disable_auto_cb(lv_event_t *e)
{
    (void)e;

    if (g_auto_mode_enabled) {
        /* 当前是启用状态，点击后要禁用 - 直接禁用 */
        disable_auto_mode();
    }
    else {
        /* 当前是禁用状态，点击后要启用 - 显示确认对话框 */
        show_enable_auto_dialog();
    }
}

/**
 * @brief 启用自动模式
 */
static void enable_auto_mode(void)
{
    g_auto_mode_enabled = true;

    /* 圆环变为蓝色 */
    lv_obj_set_style_arc_color(g_arc, COLOR_PRIMARY, LV_PART_INDICATOR);

    /* 刻度线变为蓝色 */
    for (int i = 0; i < g_tick_count; i++) {
        lv_obj_set_style_line_color(g_tick_lines[i], COLOR_PRIMARY, 0);
    }

    /* 文字变为蓝色 "自动模式" */
    lv_label_set_text(g_mode_label, "自动模式");
    lv_obj_set_style_text_color(g_mode_label, COLOR_PRIMARY, 0);

    /* 按钮变为红色边框，白色背景 */
    lv_obj_set_style_border_color(g_btn_toggle, COLOR_ERROR, 0);
    lv_obj_set_style_bg_color(g_btn_toggle, lv_color_white(), 0);
    lv_label_set_text(g_btn_label, "禁用自动化");
    lv_obj_set_style_text_color(g_btn_label, COLOR_ERROR, 0);
    lv_obj_center(g_btn_label);
}

/**
 * @brief 禁用自动模式
 */
static void disable_auto_mode(void)
{
    g_auto_mode_enabled = false;

    /* 圆环变为红色 */
    lv_obj_set_style_arc_color(g_arc, COLOR_ERROR, LV_PART_INDICATOR);

    /* 刻度线变为红色 */
    for (int i = 0; i < g_tick_count; i++) {
        lv_obj_set_style_line_color(g_tick_lines[i], COLOR_ERROR, 0);
    }

    /* 文字变为红色 "自动模式\n已关闭" */
    lv_label_set_text(g_mode_label, "自动模式\n已关闭");
    lv_obj_set_style_text_color(g_mode_label, COLOR_ERROR, 0);

    /* 按钮变为绿色边框，白色背景 */
    lv_obj_set_style_border_color(g_btn_toggle, COLOR_SUCCESS, 0);
    lv_obj_set_style_bg_color(g_btn_toggle, lv_color_white(), 0);
    lv_label_set_text(g_btn_label, "启用自动化");
    lv_obj_set_style_text_color(g_btn_label, COLOR_SUCCESS, 0);
    lv_obj_center(g_btn_label);
}

/**
 * @brief 显示启用自动化确认对话框
 */
static void show_enable_auto_dialog(void)
{
    /* 如果对话框已存在，先删除 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }

    /* 创建外层蓝色背景（直角） */
    g_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dialog, 630, 390);
    lv_obj_center(g_dialog);
    lv_obj_set_style_bg_color(g_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_dialog, 0, 0);
    lv_obj_set_style_radius(g_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(g_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_dialog);
    lv_obj_set_size(content, 620, 380);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);  /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "启用全局自动化");
    lv_obj_set_style_text_font(title, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, "是否启用全局自动化");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 取消按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 140, 50);
    lv_obj_set_pos(btn_cancel, 180, 300);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);  /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, dialog_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消");
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_center(label_cancel);

    /* 确认按钮（蓝色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 140, 50);
    lv_obj_set_pos(btn_confirm, 340, 300);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);  /* 蓝色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, dialog_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认");
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 对话框确认按钮回调
 */
static void dialog_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* 启用自动模式 */
    enable_auto_mode();

    /* 关闭对话框 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 对话框取消按钮回调
 */
static void dialog_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* 关闭对话框，不做任何改变 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 对话框确定按钮回调（用于告警弹窗）
 */
static void dialog_ok_cb(lv_event_t *e)
{
    (void)e;

    /* 关闭对话框 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 显示告警对话框
 * @param title 标题文字
 * @param message 消息文字
 */
static void show_warning_dialog(const char *title, const char *message)
{
    /* 如果对话框已存在，先删除 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }

    /* 创建外层蓝色背景（直角） */
    g_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dialog, 630, 390);
    lv_obj_center(g_dialog);
    lv_obj_set_style_bg_color(g_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_dialog, 0, 0);
    lv_obj_set_style_radius(g_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(g_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_dialog);
    lv_obj_set_size(content, 620, 380);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);  /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题（红色） */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, COLOR_ERROR, 0);  /* 红色 */
    lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, message);
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 确定按钮（蓝色） */
    lv_obj_t *btn_ok = lv_btn_create(content);
    lv_obj_set_size(btn_ok, 160, 50);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -30);
    lv_obj_set_style_bg_color(btn_ok, COLOR_PRIMARY, 0);  /* 蓝色 */
    lv_obj_set_style_border_width(btn_ok, 0, 0);
    lv_obj_set_style_radius(btn_ok, 25, 0);
    lv_obj_add_event_cb(btn_ok, dialog_ok_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_ok = lv_label_create(btn_ok);
    lv_label_set_text(label_ok, "确定");
    lv_obj_set_style_text_font(label_ok, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_ok, lv_color_white(), 0);
    lv_obj_center(label_ok);
}

/**
 * @brief 轮灌确认启动按钮回调
 */
static void irrigation_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* TODO: 执行启动轮灌的逻辑 */

    /* 关闭对话框 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 轮灌取消启动按钮回调
 */
static void irrigation_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* 关闭对话框 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 程序启动确认按钮回调
 */
static void program_start_confirm_cb(lv_event_t *e)
{
    (void)e;

    /* TODO: 获取选中的程序并启动 */

    /* 关闭对话框 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 程序启动取消按钮回调
 */
static void program_start_cancel_cb(lv_event_t *e)
{
    (void)e;

    /* 关闭对话框 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 显示程序选择对话框
 */
static void show_program_selection_dialog(void)
{
    /* 如果对话框已存在，先删除 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }

    /* 创建外层蓝色背景（直角） */
    g_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dialog, 1050, 650);
    lv_obj_center(g_dialog);
    lv_obj_set_style_bg_color(g_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(g_dialog, 0, 0);
    lv_obj_set_style_radius(g_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(g_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(g_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(g_dialog);
    lv_obj_set_size(content, 1040, 640);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);  /* 圆角 */
    lv_obj_set_style_pad_all(content, 20, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题（黑色粗体） */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, "今日剩余灌溉计划(启动条件：定时)");
    lv_obj_set_style_text_font(title_label, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title_label, lv_color_black(), 0);
    lv_obj_set_pos(title_label, 0, 0);

    /* 表格区域 */
    lv_obj_t *table_bg = lv_obj_create(content);
    lv_obj_set_size(table_bg, 1000, 480);
    lv_obj_set_pos(table_bg, 0, 50);
    lv_obj_set_style_bg_color(table_bg, lv_color_white(), 0);
    lv_obj_set_style_border_width(table_bg, 0, 0);
    lv_obj_set_style_pad_all(table_bg, 0, 0);
    lv_obj_set_style_radius(table_bg, 0, 0);
    lv_obj_clear_flag(table_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* 表头背景 */
    lv_obj_t *header_bg = lv_obj_create(table_bg);
    lv_obj_set_size(header_bg, 1000, 50);
    lv_obj_set_pos(header_bg, 0, 0);
    lv_obj_set_style_bg_color(header_bg, lv_color_hex(0xf0f0f0), 0);
    lv_obj_set_style_border_width(header_bg, 0, 0);
    lv_obj_set_style_radius(header_bg, 0, 0);
    lv_obj_set_style_pad_all(header_bg, 0, 0);

    /* 表头列 */
    const char *headers[] = {"选定", "序号", "程序名称", "合计时长", "关联配方"};
    int header_widths[] = {120, 120, 300, 220, 240};
    int x_pos = 0;

    for (int i = 0; i < 5; i++) {
        lv_obj_t *header_label = lv_label_create(header_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_pos(header_label, x_pos + 20, 17);
        x_pos += header_widths[i];
    }

    /* 获取程序数量并动态添加数据行 */
    int program_count = ui_program_get_count();
    g_program_checkbox_count = 0;

    for (int i = 0; i < program_count && i < 8; i++) {  /* 最多显示8行 */
        int row_y = 60 + i * 50;  /* 表头50px，每行50px */
        x_pos = 0;

        /* 选定 - 复选框 */
        lv_obj_t *checkbox = lv_checkbox_create(table_bg);
        lv_checkbox_set_text(checkbox, "");
        lv_obj_set_pos(checkbox, x_pos + 40, row_y + 10);
        g_program_checkboxes[g_program_checkbox_count++] = checkbox;
        x_pos += header_widths[0];

        /* 序号 */
        lv_obj_t *label_no = lv_label_create(table_bg);
        lv_label_set_text_fmt(label_no, "%d", i + 1);
        lv_obj_set_pos(label_no, x_pos + 50, row_y + 15);
        lv_obj_set_style_text_font(label_no, &my_font_cn_16, 0);
        x_pos += header_widths[1];

        /* 程序名称 */
        lv_obj_t *label_name = lv_label_create(table_bg);
        const char *name = ui_program_get_name(i);
        lv_label_set_text(label_name, name ? name : "");
        lv_obj_set_pos(label_name, x_pos + 20, row_y + 15);
        lv_obj_set_style_text_font(label_name, &my_font_cn_16, 0);
        x_pos += header_widths[2];

        /* 合计时长 */
        lv_obj_t *label_duration = lv_label_create(table_bg);
        int duration = ui_program_get_duration(i);
        if (duration > 0) {
            lv_label_set_text_fmt(label_duration, "%d分", duration);
        } else {
            lv_label_set_text(label_duration, "--");
        }
        lv_obj_set_pos(label_duration, x_pos + 20, row_y + 15);
        lv_obj_set_style_text_font(label_duration, &my_font_cn_16, 0);
        x_pos += header_widths[3];

        /* 关联配方 */
        lv_obj_t *label_formula = lv_label_create(table_bg);
        const char *formula = ui_program_get_formula(i);
        lv_label_set_text(label_formula, formula ? formula : "");
        lv_obj_set_pos(label_formula, x_pos + 20, row_y + 15);
        lv_obj_set_style_text_font(label_formula, &my_font_cn_16, 0);
    }

    /* 取消启动按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 160, 50);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 320, -10);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);  /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, program_start_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消启动");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认启动按钮（蓝色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 160, 50);
    lv_obj_align(btn_confirm, LV_ALIGN_BOTTOM_RIGHT, -320, -10);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);  /* 蓝色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, program_start_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认启动");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 显示手动轮灌设置对话框
 */
static void show_manual_irrigation_dialog(void)
{
    /* 如果对话框已存在，先删除 */
    if (g_dialog != NULL) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }

    /* 创建外层蓝色边框 */
    g_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dialog, 1000, 630);
    lv_obj_center(g_dialog);
    lv_obj_set_style_bg_color(g_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_dialog, 3, 0);
    lv_obj_set_style_border_color(g_dialog, COLOR_PRIMARY, 0);  /* 蓝色边框 */
    lv_obj_set_style_radius(g_dialog, 0, 0);
    lv_obj_set_style_pad_all(g_dialog, 20, 0);
    lv_obj_clear_flag(g_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 肥前清水 */
    lv_obj_t *label_pre_water = lv_label_create(g_dialog);
    lv_label_set_text(label_pre_water, "肥前清水(分):");
    lv_obj_set_pos(label_pre_water, 20, 20);
    lv_obj_set_style_text_font(label_pre_water, &my_font_cn_16, 0);

    lv_obj_t *input_pre_water = lv_textarea_create(g_dialog);
    lv_obj_set_size(input_pre_water, 200, 40);
    lv_obj_set_pos(input_pre_water, 180, 15);
    lv_textarea_set_one_line(input_pre_water, true);
    lv_textarea_set_text(input_pre_water, "5");
    lv_obj_set_style_text_font(input_pre_water, &my_font_cn_16, 0);
    lv_obj_add_event_cb(input_pre_water, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 施肥配方 */
    lv_obj_t *label_formula = lv_label_create(g_dialog);
    lv_label_set_text(label_formula, "施肥配方:");
    lv_obj_set_pos(label_formula, 450, 20);
    lv_obj_set_style_text_font(label_formula, &my_font_cn_16, 0);

    lv_obj_t *dropdown_formula = lv_dropdown_create(g_dialog);
    lv_obj_set_size(dropdown_formula, 200, 40);
    lv_obj_set_pos(dropdown_formula, 580, 15);
    lv_dropdown_set_options(dropdown_formula, "无");
    lv_obj_set_style_text_font(dropdown_formula, &my_font_cn_16, 0);
    lv_obj_set_style_text_font(lv_dropdown_get_list(dropdown_formula), &my_font_cn_16, 0);

    /* 选择灌区按钮 */
    lv_obj_t *btn_select_zone = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_select_zone, 120, 40);
    lv_obj_set_pos(btn_select_zone, 850, 15);
    lv_obj_set_style_bg_color(btn_select_zone, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_select_zone, 20, 0);

    lv_obj_t *label_select_zone = lv_label_create(btn_select_zone);
    lv_label_set_text(label_select_zone, "选择灌区");
    lv_obj_set_style_text_color(label_select_zone, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_select_zone, &my_font_cn_16, 0);
    lv_obj_center(label_select_zone);

    /* 肥后清水 */
    lv_obj_t *label_post_water = lv_label_create(g_dialog);
    lv_label_set_text(label_post_water, "肥后清水(分):");
    lv_obj_set_pos(label_post_water, 20, 70);
    lv_obj_set_style_text_font(label_post_water, &my_font_cn_16, 0);

    lv_obj_t *input_post_water = lv_textarea_create(g_dialog);
    lv_obj_set_size(input_post_water, 200, 40);
    lv_obj_set_pos(input_post_water, 180, 65);
    lv_textarea_set_one_line(input_post_water, true);
    lv_textarea_set_text(input_post_water, "5");
    lv_obj_set_style_text_font(input_post_water, &my_font_cn_16, 0);
    lv_obj_add_event_cb(input_post_water, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 上移、下移按钮 */
    lv_obj_t *btn_up = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_up, 100, 40);
    lv_obj_set_pos(btn_up, 580, 65);
    lv_obj_set_style_bg_color(btn_up, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_up, 20, 0);

    lv_obj_t *label_up = lv_label_create(btn_up);
    lv_label_set_text(label_up, "上移");
    lv_obj_set_style_text_color(label_up, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_up, &my_font_cn_16, 0);
    lv_obj_center(label_up);

    lv_obj_t *btn_down = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_down, 100, 40);
    lv_obj_set_pos(btn_down, 700, 65);
    lv_obj_set_style_bg_color(btn_down, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_down, 20, 0);

    lv_obj_t *label_down = lv_label_create(btn_down);
    lv_label_set_text(label_down, "下移");
    lv_obj_set_style_text_color(label_down, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_down, &my_font_cn_16, 0);
    lv_obj_center(label_down);

    /* 表格区域 */
    lv_obj_t *table_bg = lv_obj_create(g_dialog);
    lv_obj_set_size(table_bg, 960, 370);
    lv_obj_set_pos(table_bg, 20, 120);
    lv_obj_set_style_bg_color(table_bg, lv_color_hex(0xf5f5f5), 0);
    lv_obj_set_style_border_width(table_bg, 0, 0);
    lv_obj_set_style_radius(table_bg, 0, 0);

    /* 表头 */
    const char *headers[] = {"灌溉顺序", "名称", "类型", "运行时长(分)", "操作"};
    int header_widths[] = {180, 240, 180, 240, 120};
    int header_x = 20;

    for (int i = 0; i < 5; i++) {
        lv_obj_t *header_label = lv_label_create(table_bg);
        lv_label_set_text(header_label, headers[i]);
        lv_obj_set_style_text_color(header_label, lv_color_black(), 0);
        lv_obj_set_style_text_font(header_label, &my_font_cn_16, 0);
        lv_obj_set_pos(header_label, header_x, 10);
        header_x += header_widths[i];
    }

    /* 底部运行时长 */
    lv_obj_t *label_duration = lv_label_create(g_dialog);
    lv_label_set_text(label_duration, "运行时长(分):");
    lv_obj_set_pos(label_duration, 20, 520);
    lv_obj_set_style_text_font(label_duration, &my_font_cn_16, 0);

    lv_obj_t *input_duration = lv_textarea_create(g_dialog);
    lv_obj_set_size(input_duration, 150, 40);
    lv_obj_set_pos(input_duration, 180, 515);
    lv_textarea_set_one_line(input_duration, true);
    lv_textarea_set_text(input_duration, "0");
    lv_obj_set_style_text_font(input_duration, &my_font_cn_16, 0);
    lv_obj_add_event_cb(input_duration, textarea_click_cb, LV_EVENT_CLICKED, NULL);

    /* 统一设置按钮 */
    lv_obj_t *btn_set_all = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_set_all, 140, 45);
    lv_obj_set_pos(btn_set_all, 450, 515);
    lv_obj_set_style_bg_color(btn_set_all, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_set_all, 22, 0);
    lv_obj_add_event_cb(btn_set_all, btn_uniform_set_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_set_all = lv_label_create(btn_set_all);
    lv_label_set_text(label_set_all, "统一设置");
    lv_obj_set_style_text_color(label_set_all, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_set_all, &my_font_cn_16, 0);
    lv_obj_center(label_set_all);

    /* 取消启动按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_cancel, 140, 45);
    lv_obj_set_pos(btn_cancel, 620, 515);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);
    lv_obj_set_style_radius(btn_cancel, 22, 0);
    lv_obj_add_event_cb(btn_cancel, irrigation_cancel_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消启动");
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_center(label_cancel);

    /* 确认启动按钮 */
    lv_obj_t *btn_confirm = lv_btn_create(g_dialog);
    lv_obj_set_size(btn_confirm, 140, 45);
    lv_obj_set_pos(btn_confirm, 790, 515);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);
    lv_obj_set_style_radius(btn_confirm, 22, 0);
    lv_obj_add_event_cb(btn_confirm, irrigation_confirm_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认启动");
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_center(label_confirm);
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

/**
 * @brief 关闭首页弹窗（如果存在）
 */
void ui_home_close_dialog(void)
{
    if (g_dialog) {
        lv_obj_del(g_dialog);
        g_dialog = NULL;
    }
}

/**
 * @brief 统一设置按钮回调
 */
static void btn_uniform_set_cb(lv_event_t *e)
{
    (void)e;
    show_uniform_set_confirm_dialog();
}

/**
 * @brief 显示统一设置确认对话框
 */
static void show_uniform_set_confirm_dialog(void)
{
    /* 注意：不删除原对话框，新对话框显示在上层 */

    /* 创建外层蓝色背景（直角） */
    lv_obj_t *confirm_dialog = lv_obj_create(lv_scr_act());
    lv_obj_set_size(confirm_dialog, 630, 390);
    lv_obj_center(confirm_dialog);
    lv_obj_set_style_bg_color(confirm_dialog, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(confirm_dialog, 0, 0);
    lv_obj_set_style_radius(confirm_dialog, 0, 0);  /* 直角 */
    lv_obj_set_style_pad_all(confirm_dialog, 5, 0);  /* 5px内边距 */
    lv_obj_clear_flag(confirm_dialog, LV_OBJ_FLAG_SCROLLABLE);

    /* 创建内层白色背景（圆角） */
    lv_obj_t *content = lv_obj_create(confirm_dialog);
    lv_obj_set_size(content, 620, 380);  /* 减去2×5px边距 */
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 10, 0);  /* 圆角 */
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    /* 标题 */
    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "统一修改");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* 内容文字 */
    lv_obj_t *msg = lv_label_create(content);
    lv_label_set_text(msg, "是否统一修改运行时长");
    lv_obj_set_style_text_font(msg, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(msg, lv_color_black(), 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);

    /* 取消修改按钮（灰色） */
    lv_obj_t *btn_cancel = lv_btn_create(content);
    lv_obj_set_size(btn_cancel, 140, 50);
    lv_obj_set_pos(btn_cancel, 180, 300);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x808080), 0);  /* 灰色 */
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_radius(btn_cancel, 25, 0);
    lv_obj_add_event_cb(btn_cancel, uniform_set_cancel_cb, LV_EVENT_CLICKED, confirm_dialog);

    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "取消修改");
    lv_obj_set_style_text_font(label_cancel, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_cancel, lv_color_white(), 0);
    lv_obj_center(label_cancel);

    /* 确认修改按钮（蓝色） */
    lv_obj_t *btn_confirm = lv_btn_create(content);
    lv_obj_set_size(btn_confirm, 140, 50);
    lv_obj_set_pos(btn_confirm, 340, 300);
    lv_obj_set_style_bg_color(btn_confirm, COLOR_PRIMARY, 0);  /* 蓝色 */
    lv_obj_set_style_border_width(btn_confirm, 0, 0);
    lv_obj_set_style_radius(btn_confirm, 25, 0);
    lv_obj_add_event_cb(btn_confirm, uniform_set_confirm_cb, LV_EVENT_CLICKED, confirm_dialog);

    lv_obj_t *label_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(label_confirm, "确认修改");
    lv_obj_set_style_text_font(label_confirm, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(label_confirm, lv_color_white(), 0);
    lv_obj_center(label_confirm);
}

/**
 * @brief 统一设置确认按钮回调
 */
static void uniform_set_confirm_cb(lv_event_t *e)
{
    lv_obj_t *confirm_dialog = lv_event_get_user_data(e);

    /* TODO: 执行统一设置运行时长的逻辑 */

    /* 关闭确认对话框 */
    if (confirm_dialog != NULL) {
        lv_obj_del(confirm_dialog);
    }
}

/**
 * @brief 统一设置取消按钮回调
 */
static void uniform_set_cancel_cb(lv_event_t *e)
{
    lv_obj_t *confirm_dialog = lv_event_get_user_data(e);

    /* 关闭确认对话框 */
    if (confirm_dialog != NULL) {
        lv_obj_del(confirm_dialog);
    }
}