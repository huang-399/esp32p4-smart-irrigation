idf_component_register(
    SRCS
        "src/my_font_cn_16.c"
        "src/my_fontbd_16.c"
        "src/ui_alarm.c"
        "src/ui_alarm_records.c"
        "src/ui_device.c"
        "src/ui_display.c"
        "src/ui_home.c"
        "src/ui_keyboard.c"
        "src/ui_log.c"
        "src/ui_log_records.c"
        "src/ui_main.c"
        "src/ui_maintenance.c"
        "src/ui_network.c"
        "src/ui_numpad.c"
        "src/ui_program.c"
        "src/ui_settings.c"
        "src/ui_wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl nvs_flash log irrigation_scheduler
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers
    -Wno-unused-variable
)
