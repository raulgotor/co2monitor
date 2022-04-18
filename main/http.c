/*!
 *******************************************************************************
 * @file http.c
 *
 * @brief 
 *
 * @author Raúl Gotor (raulgotor@gmail.com)
 * @date 10.04.22
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Raúl Gotor
 * All rights reserved.
 * *******************************************************************************
 */

/*
 *******************************************************************************
 * #include Statements                                                         *
 *******************************************************************************
 */

#include <stdint.h>
#include <stdbool.h>

#include "esp_http_client.h"
#include "wifi.h"

#include "esp_log.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define TAG                                 "http"

#define MAX_HTTP_OUTPUT_BUFFER              (100)

#define SERVER_URL                          CONFIG_CO2_MONITOR_DEVICE_URL
#define TOKEN                               CONFIG_CO2_MONITOR_DEVICE_TOKEN
#define ENDPOINT                            "/api/v1/" TOKEN "/telemetry"
#define URL                                 SERVER_URL ENDPOINT

#define HEADER_KEY                          "Content-Type"
#define HEADER_VALUE                        "application/json"

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

_Noreturn static void http_task(void *pvParameter);

static esp_err_t http_event_handler(esp_http_client_event_t *evt);

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

QueueHandle_t http_q = NULL;

extern TaskHandle_t sensor_task_h;

/*
 *******************************************************************************
 * Static Data Declarations                                                    *
 *******************************************************************************
 */

char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};

static esp_http_client_config_t config = {
                .host = "httpbin.org",
                .path = "/get",
                .query = "esp",
                .event_handler = http_event_handler,
                .user_data = local_response_buffer,
                .disable_auto_redirect = true,
};

static esp_http_client_handle_t m_client;

static const char * m_post_data_template = "{\"co2_concentration\": %d}";

/*
 *******************************************************************************
 * Public Function Bodies                                                      *
 *******************************************************************************
 */

bool http_init(void) {

        bool success;

        BaseType_t task_result;
        TaskHandle_t http_task_h = NULL;

        http_q = xQueueCreate(3, sizeof(uint32_t *));

        success = (NULL != http_q);

        if (success) {
                // Tell `sensor_task_h` that our queue is ready to be used
                (void)xTaskNotifyIndexed(sensor_task_h, 1, 0, eNoAction);

                task_result = xTaskCreate((TaskFunction_t)http_task,
                                          "http_task",
                                          8000,
                                          NULL,
                                          2,
                                          &http_task_h);

                success = (pdPASS == task_result);
        }

        if (success) {
                m_client = esp_http_client_init(&config);

                success = (NULL != m_client);
        }

        return success;
}

/*
 *******************************************************************************
 * Private Function Bodies                                                     *
 *******************************************************************************
 */

void http_send_data(uint32_t const data)
{

        ESP_LOGI(TAG,"Sending data to %s", URL);
        esp_err_t esp_result;
        bool success;
        char post_data[30];

        esp_result = esp_http_client_set_url(
                        m_client,
                        URL);

        success = (ESP_OK == esp_result);

        if (success) {
                esp_result = esp_http_client_set_method(
                                m_client,
                                HTTP_METHOD_POST);

                success = (ESP_OK == esp_result);
        }

        if (success) {
                esp_result = esp_http_client_set_header(
                                m_client,
                                HEADER_KEY,
                                HEADER_VALUE);

                success = (ESP_OK == esp_result);
        }

        if (success) {
                sprintf(post_data, m_post_data_template, data);
                esp_result = esp_http_client_set_post_field(
                                m_client,
                                post_data,
                                (int)strlen(post_data));

                success = (ESP_OK == esp_result);
        }

        if (success) {
                esp_result = esp_http_client_perform(m_client);

                success = (ESP_OK == esp_result);
        }

        if (success) {
                ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %i",
                         esp_http_client_get_status_code(m_client),
                         esp_http_client_get_content_length(m_client));
        } else {
                ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(esp_result));
        }
}

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */

_Noreturn static void http_task(void *pvParameter)
{
        (void)pvParameter;
        uint32_t co2_ppm;
        BaseType_t queue_result;
        wifi_status_t wifi_status;

        for (;;) {
                queue_result = xQueueReceive(http_q, &co2_ppm, 500);

                wifi_status = wifi_get_status();

                if (pdTRUE == queue_result && (WIFI_STATUS_CONNECTED == wifi_status)) {
                        (void)http_send_data(co2_ppm);
                }
        }
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
        static char *output_buffer;  // Buffer to store response of http request from event handler
        static int output_len;       // Stores number of bytes read
        switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
                ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
                break;
        case HTTP_EVENT_ON_CONNECTED:
                ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
                break;
        case HTTP_EVENT_HEADER_SENT:
                ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
                break;
        case HTTP_EVENT_ON_HEADER:
                ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
                break;
        case HTTP_EVENT_ON_DATA:
                ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                /*
                 *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
                 *  However, event handler can also be used in case chunked encoding is used.
                 */
                if (!esp_http_client_is_chunked_response(evt->client)) {
                        // If user_data buffer is configured, copy the response into the buffer
                        if (evt->user_data) {
                                memcpy(evt->user_data + output_len, evt->data, evt->data_len);
                        } else {
                                if (output_buffer == NULL) {
                                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                                        output_len = 0;
                                        if (output_buffer == NULL) {
                                                ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                                                return ESP_FAIL;
                                        }
                                }
                                memcpy(output_buffer + output_len, evt->data, evt->data_len);
                        }
                        output_len += evt->data_len;
                }

                break;
        case HTTP_EVENT_ON_FINISH:
                ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
                if (output_buffer != NULL) {
                        // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                        // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                        free(output_buffer);
                        output_buffer = NULL;
                }
                output_len = 0;
                break;
        case HTTP_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
                int mbedtls_err = 0;
                //TODO:
                /* esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
                if (err != 0) {
                        ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                        ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
                }
                if (output_buffer != NULL) {
                        free(output_buffer);
                        output_buffer = NULL;
                }
                output_len = 0;
                                  */

                break;
        }
        return ESP_OK;
}
