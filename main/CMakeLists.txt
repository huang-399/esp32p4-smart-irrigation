idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES smart_irrigation_ui wifi_manager display_manager event_recorder
    PRIV_REQUIRES esp32_p4_wifi6_touch_lcd_x esp_netif lwip
)
