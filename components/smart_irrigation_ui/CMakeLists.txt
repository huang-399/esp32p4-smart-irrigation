file(GLOB UI_SOURCES src/*.c)

idf_component_register(
    SRCS ${UI_SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES lvgl nvs_flash log
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-missing-field-initializers
    -Wno-unused-variable
)
