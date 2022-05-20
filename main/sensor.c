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
#include "freertos/semphr.h"
#include "tasks_config.h"

#include "http.h"
#include "display.h"
#include "wifi.h"
#include "main.h"

#include "sensor.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define TAG                                 "sensor"

#define TASK_REFRESH_RATE_TICKS             (pdMS_TO_TICKS(TASKS_CONFIG_SENSOR_REFRESH_RATE_MS))
#define TASK_STACK_DEPTH                    TASKS_CONFIG_SENSOR_STACK_DEPTH
#define TASK_PRIORITY                       TASKS_CONFIG_SENSOR_PRIORITY

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

static inline bool sensor_lock(bool lock);

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

static QueueHandle_t m_uart_mutex_q = NULL;
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
                m_uart_mutex_q = xSemaphoreCreateMutex();

                success = (NULL != m_uart_mutex_q);
        }

        if (success) {

                (void)sensor_lock(true);

                mh_z19_result = mh_z19_enable_abc(false);

                (void)sensor_lock(false);

                success = (MH_Z19_ERROR_SUCCESS == mh_z19_result);
        }

        if (success) {
                task_result = xTaskCreate((TaskFunction_t)sensor_task,
                                          "sensor_task",
                                          TASK_STACK_DEPTH,
                                          NULL,
                                          TASK_PRIORITY,
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

static inline bool sensor_lock(bool lock)
{
        BaseType_t semaphore_result;

        if (lock) {
                semaphore_result = xSemaphoreTake(m_uart_mutex_q, portMAX_DELAY);
        } else {
                semaphore_result = xSemaphoreGive(m_uart_mutex_q);
        }

        return (pdTRUE == semaphore_result);
}

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

_Noreturn static void sensor_task(void * pvParameter) {

        uint32_t co2_ppm;
        uint32_t io_pressed = 0;
        mh_z19_error_t mh_z19_result;
        BaseType_t task_notify_result;

        (void)pvParameter;

        // Wait for `http_q` and `display_q` to be ready
        xTaskNotifyWaitIndexed(0,0,0,0, portMAX_DELAY);
        xTaskNotifyWaitIndexed(1,0,0,0, portMAX_DELAY);

        while (1) {

                task_notify_result = xTaskNotifyWait(0, 0,
                                                     &io_pressed,
                                                     TASK_REFRESH_RATE_TICKS);

                /*
                 * Don't even read the sensor if there is no one interested in
                 * the output
                 */
                if ((!display_is_enabled()) &&
                    (WIFI_STATUS_CONNECTED != wifi_get_status())) {

                        // Code style exception for the shake of readability
                        continue;
                }

                (void)sensor_lock(true);
                mh_z19_result = mh_z19_get_gas_concentration(&co2_ppm);
                (void)sensor_lock(false);

                if (MH_Z19_ERROR_SUCCESS == mh_z19_result) {

                        // Don't sent info to display if it isn't active
                        if ((NULL != display_q) && (display_is_enabled())) {
                                (void)display_set_concentration(co2_ppm);
                        }

                        // Don't attempt to post to server if there is no wifi
                        if ((NULL != http_q) &&
                            (WIFI_STATUS_CONNECTED == wifi_get_status())) {

                                (void)xQueueSend(http_q, &co2_ppm, 0);
                        }

                        ESP_LOGI(TAG,"CO2 concentration %d ppm", co2_ppm);
                }

                if ((MH_Z19_ERROR_SUCCESS == mh_z19_result) &&
                    (pdPASS == task_notify_result) &&
                    (display_is_enabled())) {

                        if (CALIBRATION_BUTTON == io_pressed) {
                                (void)sensor_lock(true);
                                (void)mh_z19_calibrate_zero_point();
                                (void)sensor_lock(false);
                        }
                }

                ESP_LOGI(TAG,"Max stack usage: %d of %d bytes", TASK_STACK_DEPTH - uxTaskGetStackHighWaterMark(NULL), TASK_STACK_DEPTH);
        }
}


