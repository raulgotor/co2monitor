//
// Created by Raúl Gotor on 12/31/20.
//

#include "sensor.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "driver/uart.h"

char sensor_read_cmd[9] = {
    0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79
};

char sensor_calibrate_cmd[9] = {
    0xFF, 0x01, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78
};

char sensor_abd_off_cmd[9] = {
    0xFF, 0x01, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80
};

xQueueHandle sensor_do_q;

static void sensor_calibrate();

static int8_t sensor_init(void);

static int8_t sensor_init(void) {
        int8_t result = 0;
    sensor_do_q = xQueueCreate(10U, sizeof(uint32_t));
        if (NULL == sensor_do_q) {
            assert(0);
            result = -1;
        }

        uart_config_t uart_config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            };

        const int uart_num = UART_NUM_2;

        ESP_ERROR_CHECK(uart_set_pin(
            UART_NUM_2,
            33,
            32,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE));

        ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

        const int uart_buffer_size = (1024 * 2);
        QueueHandle_t uart_queue;
        ESP_ERROR_CHECK(uart_driver_install(
            UART_NUM_2,
            uart_buffer_size,
            uart_buffer_size,
            10,
            &uart_queue,
            0
            ));

        /* switch off auto-calibration every 24h */
        uart_write_bytes(UART_NUM_2, (const char *) sensor_abd_off_cmd, 9);

        return result;
}

static void sensor_calibrate() {
    uart_write_bytes(UART_NUM_2, (const char *) sensor_calibrate_cmd, 9);
}

void sensor_task(void *pvParameter) {
    uint32_t i = 0;
    uint32_t result = 0;
    uint8_t in_buffer[9 * 10];
    uint32_t io_pressed = 0;

    if (0 != sensor_init()) {
        assert(0);
    }

    while (1) {

        int length = 9;

        uart_write_bytes(UART_NUM_2, (const char *) sensor_read_cmd, 9);
        uart_read_bytes(UART_NUM_2, in_buffer, length, 100);

        i = (uint32_t) in_buffer[2] * 256 + (uint32_t) in_buffer[3];

        xQueueSend(sensor_do_q, &i, 500 / portTICK_PERIOD_MS);
        i++;
        if (pdPASS == xTaskNotifyWait(
            0,
            0,
            &io_pressed,
            0)) {
            if (35 == io_pressed) {
                sensor_calibrate();
            }
        };
    }
    vTaskDelete(NULL);
}

