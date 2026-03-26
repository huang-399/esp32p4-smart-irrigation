#ifndef UI_STARTUP_H
#define UI_STARTUP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 显示启动加载界面
 * @param stage_text 阶段提示文本
 * @param progress 初始进度 (0-100)
 */
void ui_startup_show(const char *stage_text, int progress);

/**
 * @brief 更新启动加载界面的阶段文本和进度
 * @param stage_text 阶段提示文本
 * @param progress 进度 (0-100)
 */
void ui_startup_update(const char *stage_text, int progress);

/**
 * @brief 关闭启动加载界面
 */
void ui_startup_close(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_STARTUP_H */
