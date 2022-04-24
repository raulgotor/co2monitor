set(SOURCES "main.c" "sensor.c" "display.c" "lv_conf.h" "winsen_mh_z19.c" "battery.c" "wifi.c" "http.c")
idf_component_register(SRCS ${SOURCES}
        INCLUDE_DIRS .
        REQUIRES lvgl_esp32_drivers lvgl_touch lvgl_tft lvgl lv_examples esp_http_client)

target_compile_definitions(${COMPONENT_LIB} PUBLIC "-DLV_LVGL_H_INCLUDE_SIMPLE")

