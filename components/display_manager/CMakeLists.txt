idf_component_register(
    SRCS "src/display_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES esp32_p4_wifi6_touch_lcd_x nvs_flash driver lvgl
)
