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

#include "wifi_manager.h"
#include "wifi.h"

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

static void wifi_report_status(void);

static void wifi_ap_reconnection(void);

static void wifi_set_connection_status_active(void * p_param);

static void wifi_set_connection_status_inactive(void * p_param);
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
        BaseType_t timer_result;
        bool success;

        wifi_manager_start();

        wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, wifi_set_connection_status_active);
        wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, wifi_set_connection_status_inactive);

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

static void wifi_set_connection_status_inactive(void * p_param)
{
        (void)p_param;
        m_wifi_status = WIFI_STATUS_DISCONNECTED;

        wifi_report_status();
}

static void wifi_set_connection_status_active(void * p_param)
{
        (void)p_param;
        m_wifi_status = WIFI_STATUS_CONNECTED;

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
                disp_wifi_status.ap_ssid[0]='|';



        } else {
                tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);

                disp_wifi_status.ip = ip_info.ip.addr;
                disp_wifi_status.rssi = ap.rssi;
                memcpy(disp_wifi_status.ap_ssid, ap.ssid, strlen((char *)ap.ssid));
        }


        display_set_wifi_status(disp_wifi_status);

        ESP_LOGI(TAG, "IP: " IPSTR ", RSSI: %d", IP2STR(&ip_info.ip), disp_wifi_status.rssi);
}

