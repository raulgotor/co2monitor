/*!
 *******************************************************************************
 * @file main.c
 *
 * @brief
 *
 * @author Raúl Gotor (raulgotor@gmail.com)
 * @date 03.04.22
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Raúl Gotor
 * All rights reserved.
 *******************************************************************************
 */

/*
 *******************************************************************************
 * #include Statements                                                         *
 *******************************************************************************
 */

#include <limits.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "driver/gpio.h"

#include "lvgl.h"
#include "st7789.h"

#include "disp_spi.h"

#include "http.h"
#include "wifi.h"
#include "display.h"
#include "sensor.h"

#include "main.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define LV_LVGL_H_INCLUDE_SIMPLE            (1)

/*
 *******************************************************************************
 * Data types                                                                  *
 *******************************************************************************
 */

/*
 *******************************************************************************
 * Constants                                                                   *
 *******************************************************************************
 */

/*
 *******************************************************************************
 * Private Function Prototypes                                                 *
 *******************************************************************************
 */

static bool gpio_setup();

static void IRAM_ATTR gpio_isr_handler(void *parameters);

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

extern TaskHandle_t sensor_task_h;
extern TaskHandle_t display_task_h;

/*
 *******************************************************************************
 * Static Data Declarations                                                    *
 *******************************************************************************
 */

/*
 *******************************************************************************
 * Public Function Bodies                                                      *
 *******************************************************************************
 */

void app_main(void) {

        bool success;

        success = gpio_setup();

        success = success & sensor_init();

        success = success & display_init();

        success = success & wifi_init();

        success = success & http_init();

        if (!success) {
                assert(0 && "init failed");
        }

        while (1) {
                vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
}

/*
 *******************************************************************************
 * Private Function Bodies                                                     *
 *******************************************************************************
 */

static bool gpio_setup() {

        uint64_t const gpio_input_mask = (1ULL << BACKLIGHT_BUTTON) |
                                         (1ULL << CALIBRATION_BUTTON);

        gpio_config_t gpio_input_config;
        bool success = true;
        esp_err_t esp_result;

        gpio_input_config.mode = GPIO_MODE_INPUT;
        gpio_input_config.intr_type = GPIO_INTR_POSEDGE;
        gpio_input_config.pin_bit_mask = gpio_input_mask;
        gpio_input_config.pull_up_en = GPIO_PULLUP_ENABLE;

        esp_result = gpio_config(&gpio_input_config);

        if (ESP_OK != esp_result) {
                success = false;
        } else {
                esp_result = gpio_install_isr_service(0);

                success = (ESP_OK == esp_result);
        }

        if (success) {
                esp_result = gpio_isr_handler_add(
                                CALIBRATION_BUTTON,
                                gpio_isr_handler,
                                (void *) CALIBRATION_BUTTON);

                success = (ESP_OK == esp_result);
        }

        if (success) {
                esp_result = gpio_isr_handler_add(
                                BACKLIGHT_BUTTON,
                                gpio_isr_handler,
                                (void *) BACKLIGHT_BUTTON);


                success = (ESP_OK == esp_result);
        }

        return success;
}

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */

static void IRAM_ATTR gpio_isr_handler(void * parameters) {

        gpio_num_t const gpio_num = (gpio_num_t)parameters;

        switch (gpio_num) {
        case CALIBRATION_BUTTON:
                //xTaskNotifyFromISR(sensor_task_h, gpio_num, eSetValueWithOverwrite, NULL);
                break;
        case BACKLIGHT_BUTTON:

                xTaskNotifyFromISR(display_task_h, gpio_num, eSetValueWithOverwrite, NULL);
                break;
        default:
                break;
        }
}
