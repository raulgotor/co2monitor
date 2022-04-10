/*!
 *******************************************************************************
 * @file sensor.c
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "esp_http_client.h"
#include "driver/uart.h"
#include "winsen_mh_z19.h"

#include "http.h"
#include "sensor.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define TAG                                 "Sensor"

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

_Noreturn static void sensor_task(void *pvParameter);

static mh_z19_error_t xfer_func(uint8_t const * const p_rx_buffer,
                                size_t const rx_buffer_size,
                                uint8_t * const p_tx_buffer,
                                size_t const tx_buffer_size);

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

TaskHandle_t sensor_task_h = NULL;

extern xQueueHandle display_q;
extern xQueueHandle http_q;

/*
 *******************************************************************************
 * Static Data Declarations                                                    *
 *******************************************************************************
 */

static uart_port_t const m_uart_instance = UART_NUM_2;
static int const m_uart_tx_pin = 33;
static int const m_uart_rx_pin = 32;

/*
 *******************************************************************************
 * Public Function Bodies                                                      *
 *******************************************************************************
 */

bool sensor_init(void) {

        uart_config_t const uart_config = {
                        .baud_rate = 9600,
                        .data_bits = UART_DATA_8_BITS,
                        .parity = UART_PARITY_DISABLE,
                        .stop_bits = UART_STOP_BITS_1,
                        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        };

        int const uart_buffer_size = (1024 * 2);
        bool success = true;

        esp_err_t esp_result;
        QueueHandle_t uart_queue;
        BaseType_t task_result;
        mh_z19_error_t mh_z19_result;

        esp_result = uart_set_pin(
                        m_uart_instance,
                        m_uart_tx_pin,
                        m_uart_rx_pin,
                        UART_PIN_NO_CHANGE,
                        UART_PIN_NO_CHANGE);

        success = (ESP_OK == esp_result);

        if (success) {
                esp_result = uart_param_config(m_uart_instance, &uart_config);

                success = (ESP_OK == esp_result);
        }

        if (success) {
                esp_result = uart_driver_install(
                                m_uart_instance,
                                uart_buffer_size,
                                uart_buffer_size,
                                10,
                                &uart_queue,
                                0);

                success = (ESP_OK == esp_result);
        }

        if (success) {
                mh_z19_result = mh_z19_init(xfer_func);

                success = (MH_Z19_ERROR_SUCCESS == mh_z19_result);
        }

        if (success) {
                mh_z19_result = mh_z19_enable_abc(false);

                success = (MH_Z19_ERROR_SUCCESS == mh_z19_result);
        }

        if (success) {
                task_result = xTaskCreate((TaskFunction_t)sensor_task,
                                          "sensor_task",
                                          8000,
                                          NULL,
                                          2,
                                          &sensor_task_h);

                success = (pdPASS == task_result);
        }

        return success;
}


/*
 *******************************************************************************
 * Private Function Bodies                                                     *
 *******************************************************************************
 */

static mh_z19_error_t xfer_func(uint8_t const * const p_rx_buffer,
                                size_t const rx_buffer_size,
                                uint8_t * const p_tx_buffer,
                                size_t const tx_buffer_size)
{

        mh_z19_error_t result = MH_Z19_ERROR_SUCCESS;
        bool is_rx_operation = true;
        int uart_result;

        if ((NULL == p_rx_buffer) != (0 == rx_buffer_size)) {
                result = MH_Z19_ERROR_BAD_PARAMETER;

        } else if ((NULL == p_tx_buffer) != (0 == tx_buffer_size)) {
                result = MH_Z19_ERROR_BAD_PARAMETER;

        } else if ((NULL == p_rx_buffer) && (NULL == p_tx_buffer)) {
                result = MH_Z19_ERROR_BAD_PARAMETER;
        } else if (0 != tx_buffer_size) {
                is_rx_operation = false;
        }

        if ((MH_Z19_ERROR_SUCCESS == result) && (is_rx_operation)) {
                uart_result = uart_read_bytes(m_uart_instance,
                                              (void *)p_rx_buffer,
                                              (uint32_t)rx_buffer_size, 100);

                if (-1 == uart_result) {
                        result = MH_Z19_ERROR_IO_ERROR;
                }

        } else if (MH_Z19_ERROR_SUCCESS == result) {
                uart_result = uart_write_bytes(m_uart_instance,
                                               (void *)p_tx_buffer,
                                               (uint32_t)tx_buffer_size);

                if (-1 == uart_result) {
                        result = MH_Z19_ERROR_IO_ERROR;
                }
        }

        return result;
}

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */

_Noreturn static void sensor_task(void *pvParameter) {

        uint32_t co2_ppm;
        uint32_t io_pressed = 0;
        mh_z19_error_t mh_z19_result;
        BaseType_t task_notify_result;

        (void)pvParameter;

        xTaskNotifyWaitIndexed(0,0,0,0, portMAX_DELAY);
        xTaskNotifyWaitIndexed(1,0,0,0, portMAX_DELAY);

        while (1) {

                mh_z19_result = mh_z19_get_gas_concentration(&co2_ppm);

                if (MH_Z19_ERROR_SUCCESS == mh_z19_result) {
                        if (NULL != display_q) {
                                xQueueSend(display_q, &co2_ppm, 0);
                        }

                        if (NULL != http_q) {
                                xQueueSend(http_q, &co2_ppm, 0);
                        }

                        ESP_LOGI(TAG,"CO2 concentration %d ppm", co2_ppm);
                }

                //TODO: to ms
                //vTaskDelay(500);
                task_notify_result = xTaskNotifyWait(0, 0, &io_pressed, 500);

                if (pdPASS == task_notify_result) {

                        if (35 == io_pressed) {
                                (void)mh_z19_calibrate_zero_point();
                        }
                }
        }
}


