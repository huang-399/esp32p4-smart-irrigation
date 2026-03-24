idf_component_register(
    SRCS "alarm_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash event_recorder
)
