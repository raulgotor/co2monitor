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
#include "esp_wifi.h"
#include "wifi.h"
#include "esp_netif.h"
#include "esp_http_client.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define TAG "WiFi"

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

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

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

bool wifi_init()
{


        wifi_config_t wifi_config = {
                        .sta.ssid = "-",
                        .sta.password = "-",
                        .sta.threshold.authmode = WIFI_AUTH_WPA2_PSK,
        };

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

        return true;
}

/*
 *******************************************************************************
 * Private Function Bodies                                                     *
 *******************************************************************************
 */


static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {

        static int s_retry_num = 0;
        if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
                esp_wifi_connect();
                ESP_LOGI(TAG, "connected to the AP\n");

        } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
                if (s_retry_num < 10) {
                        esp_wifi_connect();
                        s_retry_num++;
                        ESP_LOGI(TAG, "retry to connect to the AP");
                } else {
                        //xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                }
                ESP_LOGI(TAG,"connect to the AP fail");
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
                s_retry_num = 0;
#if EVENT_BITS
                //xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
#endif // EVENT_BITS

        }

}

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */
