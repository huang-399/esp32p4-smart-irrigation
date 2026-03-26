idf_component_register(
    SRCS "irrigation_scheduler.c" "irrigation_store.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash log device_registry zigbee_bridge event_recorder history_archive esp32_p4_wifi6_touch_lcd_x
)
