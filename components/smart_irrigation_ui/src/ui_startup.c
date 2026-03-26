#include "ui_startup.h"

#include "ui_common.h"

static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_stage_label = NULL;
static lv_obj_t *s_progress_bar = NULL;
static lv_obj_t *s_progress_label = NULL;

static int clamp_progress(int progress)
{
    if (progress < 0) {
        return 0;
    }
    if (progress > 100) {
        return 100;
    }
    return progress;
}

static void update_progress_label(int progress)
{
    static char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d%%", progress);
    if (s_progress_label) {
        lv_label_set_text(s_progress_label, buf);
    }
}

void ui_startup_show(const char *stage_text, int progress)
{
    progress = clamp_progress(progress);

    if (s_screen) {
        ui_startup_update(stage_text, progress);
        lv_scr_load(s_screen);
        return;
    }

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_size(s_screen, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_screen, COLOR_DARK_BG, 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *overlay = lv_obj_create(s_screen);
    lv_obj_remove_style_all(overlay);
    lv_obj_set_size(overlay, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_pos(overlay, 0, 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *frame = lv_obj_create(s_screen);
    lv_obj_set_size(frame, 520, 320);
    lv_obj_center(frame);
    lv_obj_set_style_bg_color(frame, COLOR_PRIMARY, 0);
    lv_obj_set_style_border_width(frame, 0, 0);
    lv_obj_set_style_radius(frame, 0, 0);
    lv_obj_set_style_pad_all(frame, 5, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *content = lv_obj_create(frame);
    lv_obj_set_size(content, 510, 310);
    lv_obj_center(content);
    lv_obj_set_style_bg_color(content, lv_color_white(), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_radius(content, 12, 0);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(content);
    lv_label_set_text(title, "系统启动中");
    lv_obj_set_style_text_font(title, &my_fontbd_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT_MAIN, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 28);

    lv_obj_t *spinner = lv_spinner_create(content);
    lv_obj_set_size(spinner, 96, 96);
    lv_obj_align(spinner, LV_ALIGN_TOP_MID, 0, 78);
    lv_spinner_set_anim_params(spinner, 1000, 270);
    lv_obj_set_style_arc_width(spinner, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(spinner, lv_color_hex(0xd9eaf7), LV_PART_MAIN);
    lv_obj_set_style_arc_width(spinner, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(spinner, COLOR_PRIMARY, LV_PART_INDICATOR);

    s_stage_label = lv_label_create(content);
    lv_label_set_text(s_stage_label, stage_text ? stage_text : "正在初始化...");
    lv_obj_set_width(s_stage_label, 410);
    lv_obj_set_style_text_font(s_stage_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(s_stage_label, COLOR_TEXT_MAIN, 0);
    lv_obj_set_style_text_align(s_stage_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_stage_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_stage_label, LV_ALIGN_TOP_MID, 0, 190);

    s_progress_bar = lv_bar_create(content);
    lv_obj_set_size(s_progress_bar, 360, 18);
    lv_obj_align(s_progress_bar, LV_ALIGN_BOTTOM_MID, 0, -56);
    lv_bar_set_range(s_progress_bar, 0, 100);
    lv_bar_set_value(s_progress_bar, progress, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0xe3edf5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_progress_bar, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress_bar, COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_progress_bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);

    s_progress_label = lv_label_create(content);
    lv_obj_set_style_text_font(s_progress_label, &my_font_cn_16, 0);
    lv_obj_set_style_text_color(s_progress_label, COLOR_TEXT_GRAY, 0);
    lv_obj_align(s_progress_label, LV_ALIGN_BOTTOM_MID, 0, -26);
    update_progress_label(progress);

    lv_scr_load(s_screen);
}

void ui_startup_update(const char *stage_text, int progress)
{
    progress = clamp_progress(progress);

    if (!s_screen) {
        ui_startup_show(stage_text, progress);
        return;
    }

    if (s_stage_label && stage_text) {
        lv_label_set_text(s_stage_label, stage_text);
    }
    if (s_progress_bar) {
        lv_bar_set_value(s_progress_bar, progress, LV_ANIM_ON);
    }
    update_progress_label(progress);
}

void ui_startup_close(void)
{
    if (!s_screen) {
        return;
    }

    lv_obj_delete(s_screen);
    s_screen = NULL;
    s_stage_label = NULL;
    s_progress_bar = NULL;
    s_progress_label = NULL;
}
