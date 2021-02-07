#define LV_LVGL_H_INCLUDE_SIMPLE 1
#define CALIBRATION_BUTTON       35

#include <limits.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "st7789.h"
#include "disp_spi.h"

static TaskHandle_t sensor_task_h = NULL;
static TaskHandle_t display_task_h = NULL;
extern xQueueHandle sensor_do_q;
xQueueHandle gpio_event_q;

extern void display_task(void);
extern void sensor_task(void);
extern int test_val;

static void hardware_setup();

static void IRAM_ATTR gpio_isr_handler(void *parameters) {

    uint32_t gpio_num = (uint32_t) parameters;

    if (GPIO_NUM_35 == gpio_num) {
        xTaskNotifyFromISR(sensor_task_h, gpio_num, eSetValueWithOverwrite, NULL);
    }

}

void app_main(void) {

    hardware_setup();

    xTaskCreate(
        (TaskFunction_t) &display_task,
        "display_task",
        8000,
        NULL,
        1,
        &display_task_h);
    xTaskCreate(
        (TaskFunction_t) &sensor_task,
        "sensor_task",
        8000,
        NULL,
        2,
        &sensor_task_h);
    gpio_event_q = xQueueCreate(10, sizeof(uint32_t));

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void hardware_setup() {

    gpio_config_t io_config;

    io_config.mode = GPIO_MODE_INPUT;
    io_config.intr_type = GPIO_INTR_POSEDGE;
    io_config.pin_bit_mask = (1ULL << CALIBRATION_BUTTON);
    io_config.pull_up_en = GPIO_PULLUP_ENABLE;

    gpio_config(&io_config);

    gpio_install_isr_service(0);

    gpio_isr_handler_add(
        CALIBRATION_BUTTON,
        gpio_isr_handler,
        (void *) CALIBRATION_BUTTON);

}

static void IRAM_ATTR lv_tick_task(void) {
    lv_tick_inc(portTICK_RATE_MS);
}

