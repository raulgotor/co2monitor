/*!
 *******************************************************************************
 * @file wifi.c
 *
 * @brief 
 *
 * @author Raúl Gotor (raulgotor@gmail.com)
 * @date 08.04.22
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
//#include <freertos/event_groups.h>
#include <esp_log.h>
#include "esp_system.h"
#include "esp_freertos_hooks.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "display.h"
#include "esp_http_client.h"

#include "wifi.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define TAG "WiFi"

#define WIFI_SSID                           CONFIG_CO2_MONITOR_WIFI_SSID
#define WIFI_PASSWORD                       CONFIG_CO2_MONITOR_WIFI_PASSWORD
#define WIFI_RECONNECTION_TIMEOUT_MS        (CONFIG_CO2_MONITOR_AUTO_RECONNECT_TIME_S * 1000)

#if EVENT_BITS
/* FreeRTOS event group to signal when we are connected*/
//static EventGroupHandle_t s_wifi_event_group;
#endif // EVENT_BITS

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

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);

static void wifi_report_status(void);

static void wifi_ap_reconnection(void);

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

extern xQueueHandle display_q;

/*
 *******************************************************************************
 * Static Data Declarations                                                    *
 *******************************************************************************
 */

static uint32_t m_connect_retries = 0;

static wifi_status_t m_wifi_status = WIFI_STATUS_DISCONNECTED;

static TimerHandle_t m_wifi_status_timer = NULL;

static TimerHandle_t m_wifi_reconnect_timer = NULL;

/*
 *******************************************************************************
 * Public Function Bodies                                                      *
 *******************************************************************************
 */

bool wifi_init()
{
        wifi_config_t wifi_config = {
                        .sta.ssid = WIFI_SSID,
                        .sta.password = WIFI_PASSWORD,
                        .sta.threshold.authmode = WIFI_AUTH_WPA2_PSK,
        };

        BaseType_t timer_result;
        bool success;

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        init_config.nvs_enable = false;
        ESP_ERROR_CHECK(esp_wifi_init(&init_config));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_got_ip));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        m_wifi_status_timer = xTimerCreate("wifi_status",
                                           pdMS_TO_TICKS(3000),
                                           pdTRUE,
                                           NULL,
                                           wifi_report_status);


        success = (NULL != m_wifi_status_timer);

        if (success) {
                timer_result = xTimerStart(m_wifi_status_timer, 0);

                success = (pdPASS == timer_result);
        }

        if (success) {

                m_wifi_reconnect_timer = xTimerCreate("wifi_reconnect",
                                                      pdMS_TO_TICKS(WIFI_RECONNECTION_TIMEOUT_MS),
                                                      pdFALSE,
                                                      NULL,
                                                      wifi_ap_reconnection);

                success = (NULL != m_wifi_reconnect_timer );
        }


#if EVENT_BITS
        /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
         * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
        EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                               WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               portMAX_DELAY);

        /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
         * happened. */
        if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                         EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
        } else if (bits & WIFI_FAIL_BIT) {
                ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                         EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
        } else {
                ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
#endif // EVENT_BITS

        return success;
}

wifi_status_t wifi_get_status(void) {
        return m_wifi_status;
}

/*
 *******************************************************************************
 * Private Function Bodies                                                     *
 *******************************************************************************
 */

static void wifi_ap_reconnection(void)
{
        ESP_LOGI(TAG, "Trying to reconnect!");
        esp_wifi_connect();
}

static void event_handler(void * arg,
                          esp_event_base_t event_base,
                          int32_t event_id,
                          void * event_data) {

        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {

                esp_wifi_connect();
                ESP_LOGI(TAG, "Connected to the AP\n");

        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {

                m_wifi_status = WIFI_STATUS_DISCONNECTED;

                if (m_connect_retries < 10) {
                        esp_wifi_connect();
                        m_connect_retries++;
                        ESP_LOGI(TAG, "Retrying to connect to the AP. Retry number %d.", m_connect_retries);

                } else {
#if EVENT_BITS
                        m_wifi_status = WIFI_STATUS_FAILED;

                        //xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
#else
                        ESP_LOGI(TAG, "Connection to the AP failed. Retrying again in %d seconds", WIFI_RECONNECTION_TIMEOUT_MS / 1000);
                        xTimerStart(m_wifi_reconnect_timer, pdMS_TO_TICKS(100));
                        m_connect_retries = 0;

#endif// EVENT_BITS
                }

        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {

                m_connect_retries = 0;

                ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));

#if EVENT_BITS
                //xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
#else
                m_wifi_status = WIFI_STATUS_CONNECTED;
#endif// EVENT_BITS

        }

        wifi_report_status();
}

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */

static void wifi_report_status(void)
{
        wifi_ap_record_t ap;
        tcpip_adapter_ip_info_t ip_info;

        display_wifi_status_t disp_wifi_status;
        esp_err_t wifi_result;

        wifi_result = esp_wifi_sta_get_ap_info(&ap);

        if (ESP_OK != wifi_result) {
                disp_wifi_status.ip = 0;
                disp_wifi_status.rssi = DISPLAY_RSSI_NO_IP_VALUE;
        } else {
                tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);

                disp_wifi_status.ip = ip_info.ip.addr;
                disp_wifi_status.rssi = ap.rssi;
        }

        display_set_wifi_status(disp_wifi_status);

        ESP_LOGI(TAG, "IP: " IPSTR ", RSSI: %d", IP2STR(&ip_info.ip), disp_wifi_status.rssi);
}

