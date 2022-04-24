#include <sys/_default_fcntl.h>
#include <sys/cdefs.h>
/*!
 *******************************************************************************
 * @file battery.c
 *
 * @brief 
 *
 * @author Raúl Gotor (raulgotor@gmail.com)
 * @date 18.04.22
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

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc.h"

#include "driver/adc_common.h"

#include "display.h"

#include "battery.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define BATTERY_ADC_AVERAGE_SAMPLES         (10)
#define BATTERY_ADC_CHANNEL                 ADC1_CHANNEL_6
#define BATTERY_ADC_ATTENUATION             ADC_ATTEN_DB_11
#define BATTERY_ADC_WIDTH                   ADC_WIDTH_BIT_12

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

uint16_t const m_max_voltage = 2045;
uint16_t const m_max_raw = 4095;
uint16_t const m_scale_factor = 3;

/*
 *******************************************************************************
 * Private Function Prototypes                                                 *
 *******************************************************************************
 */

_Noreturn static void battery_task(void * pvParameters);

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

TaskHandle_t battery_task_h = NULL;

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

bool battery_init(void)
{
        esp_err_t esp_result;
        bool success;
        BaseType_t task_result;

        esp_result = adc1_config_width(BATTERY_ADC_WIDTH);

        success = (ESP_OK == esp_result);

        if (success) {

                esp_result = adc1_config_channel_atten(BATTERY_ADC_CHANNEL,
                                                       BATTERY_ADC_ATTENUATION);

                success = (ESP_OK == esp_result);
        }

        if (success) {

                task_result = xTaskCreate((TaskFunction_t)battery_task,
                                          "battery_task",
                                          8000,
                                          NULL,
                                          1,
                                          &battery_task_h);

                success = (pdPASS == task_result);
        }


        return success;
}

/*
 *******************************************************************************
 * Private Function Bodies                                                     *
 *******************************************************************************
 */

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */

_Noreturn static void battery_task(void * pvParameters)
{
        uint32_t battery_level;
        uint16_t raw_value;
        size_t i;

        (void)pvParameters;

        // Wait for `display_q` to be ready
        xTaskNotifyWaitIndexed(0,0,0,0, portMAX_DELAY);

        while(1) {

                vTaskDelay(pdMS_TO_TICKS(3000));

                raw_value = 0;

                for (i = 0; BATTERY_ADC_AVERAGE_SAMPLES > i; ++i) {
                        raw_value += adc1_get_raw(BATTERY_ADC_CHANNEL);
                }

                raw_value += BATTERY_ADC_AVERAGE_SAMPLES / 2;
                raw_value /= BATTERY_ADC_AVERAGE_SAMPLES;

                battery_level = raw_value * m_max_voltage / m_max_raw;
                battery_level *= m_scale_factor;

                (void)display_set_battery_level(battery_level);
        }
}
