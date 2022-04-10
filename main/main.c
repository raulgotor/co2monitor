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

#include "display.h"
#include "sensor.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define LV_LVGL_H_INCLUDE_SIMPLE            (1)
#define CALIBRATION_BUTTON                  (35)

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

        uint64_t const calibration_button_mask = (1ULL << CALIBRATION_BUTTON);
        gpio_config_t io_config;
        bool success = true;
        esp_err_t esp_result;

        io_config.mode = GPIO_MODE_INPUT;
        io_config.intr_type = GPIO_INTR_POSEDGE;
        io_config.pin_bit_mask = calibration_button_mask;
        io_config.pull_up_en = GPIO_PULLUP_ENABLE;

        esp_result = gpio_config(&io_config);

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

        return success;
}

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */

static void IRAM_ATTR gpio_isr_handler(void * parameters) {

    uint32_t const gpio_num = (uint32_t)parameters;

    if (CALIBRATION_BUTTON == gpio_num) {
        xTaskNotifyFromISR(sensor_task_h, gpio_num, eSetValueWithOverwrite, NULL);
    }
}
