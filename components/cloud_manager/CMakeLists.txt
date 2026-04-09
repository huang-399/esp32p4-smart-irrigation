idf_component_register(
    SRCS "cloud_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt esp_event device_registry irrigation_scheduler zigbee_bridge wifi_manager app_update
)
