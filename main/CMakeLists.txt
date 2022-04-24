set(SOURCES "main.c" "sensor.c" "display.c" "lv_conf.h" "winsen_mh_z19.c" "battery.c" "wifi.c" "http.c")
idf_component_register(SRCS ${SOURCES}
        INCLUDE_DIRS .
        REQUIRES ${EXTRA_COMPONENT_DIRS})

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_LVGL_H_INCLUDE_SIMPLE")

