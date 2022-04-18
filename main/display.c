#include <sys/_default_fcntl.h>
#include <sys/cdefs.h>
/*!
 *******************************************************************************
 * @file display.c
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

#include <stdint.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "esp_freertos_hooks.h"
#include "esp_log.h"

#include "lvgl.h"
#include "st7789.h"

#include "display.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

#define DISPLAY_NO_IP_TEXT                  "No IP"

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

static char const * TAG = __FILE__;

xQueueHandle display_q;

/*
 *******************************************************************************
 * Private Function Prototypes                                                 *
 *******************************************************************************
 */

_Noreturn static void display_task(void * pvParameters);

static void lv_tick_task(void);

static bool display_send_message(display_msg_type_t const type, void * value);

static void backlight_timer_cb(TimerHandle_t const timer_handle);

static void display_enable_backlight(bool const enable);

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

extern TaskHandle_t sensor_task_h;

TaskHandle_t display_task_h = NULL;

/*
 *******************************************************************************
 * Static Data Declarations                                                    *
 *******************************************************************************
 */

static lv_style_t m_ppm_style;

static lv_style_t m_units_style;

static lv_style_t m_bg_style;

static uint16_t  const m_wifi_sign_width = 30;

static uint16_t  const m_wifi_sign_height = (m_wifi_sign_width / 2);

static lv_color_t buffer[LV_CANVAS_BUF_SIZE_TRUE_COLOR(30, 15)];

static TimerHandle_t m_backlight_timer_h = NULL;

static uint32_t m_display_task_refresh_rate = pdMS_TO_TICKS(1000);

static lv_color_t m_buffer_1[DISP_BUF_SIZE];

static lv_color_t m_buffer_2[DISP_BUF_SIZE];

static lv_disp_buf_t m_display_buffer;

/*
 *******************************************************************************
 * Public Function Bodies                                                      *
 *******************************************************************************
 */

bool display_init(void)
{
        bool success = true;
        lv_disp_drv_t disp_drv;
        lv_disp_t * p_display;
        esp_err_t esp_result;
        BaseType_t task_result;
        BaseType_t timer_result;

        lv_init();
        lvgl_driver_init();

        lv_disp_buf_init(&m_display_buffer, m_buffer_1, m_buffer_2, DISP_BUF_SIZE);

        lv_disp_drv_init(&disp_drv);
        disp_drv.flush_cb = st7789_flush;
        disp_drv.buffer = &m_display_buffer;

        p_display = lv_disp_drv_register(&disp_drv);

        if (NULL == p_display) {
                success = false;
        }

        if (success) {
                esp_result = esp_register_freertos_tick_hook(lv_tick_task);

                success = (ESP_OK == esp_result);
        }

        if ((success) && (0 != DISPLAY_BACKLIGHT_TIMEOUT_MS)) {
                m_backlight_timer_h = xTimerCreate("backlight_timer",
                                                   pdMS_TO_TICKS(DISPLAY_BACKLIGHT_TIMEOUT_MS),
                                                   pdFALSE,
                                                   NULL,
                                                   backlight_timer_cb);

                success = (NULL != m_backlight_timer_h);
        }

        if ((success) && (NULL != m_backlight_timer_h)) {
                timer_result = xTimerStart(m_backlight_timer_h, 0);

                success = (pdPASS == timer_result);
        }

        if (success) {
                display_q = xQueueCreate(10U, sizeof(display_msg_t *));

                if (NULL == display_q) {
                        success = false;
                }
        }

        if (success) {
                // Tell `sensor_task_h` that our queue is ready to be used
                (void)xTaskNotifyIndexed(sensor_task_h, 0, 0, eNoAction);

                task_result = xTaskCreate((TaskFunction_t)display_task,
                            "display_task",
                            8000,
                            NULL,
                            1,
                            &display_task_h);

                success = (pdPASS == task_result);
        }

        return success;
}

bool display_set_wifi_status(display_wifi_status_t const display_wifi_status)
{
        return display_send_message(DISPLAY_MSG_WIFI_STATUS,
                                    (void *)(&display_wifi_status));
}

bool display_set_concentration(uint32_t const concentration)
{
        return display_send_message(DISPLAY_MSG_CO2_PPM,
                                    (void *)(concentration));
}

bool display_is_active(void)
{
        return (portMAX_DELAY != m_display_task_refresh_rate);
}

/*
 *******************************************************************************
 * Private Function Bodies                                                     *
 *******************************************************************************
 */

static void IRAM_ATTR lv_tick_task(void)
{
        //TODO: 1 or tick_rate_ms?
        lv_tick_inc(1);
        // lv_tick_inc(portTICK_RATE_MS);
}

static bool display_send_message(display_msg_type_t const type, void * value)
{
        display_msg_t * p_message = NULL;
        BaseType_t queue_result;
        bool success;

        p_message = pvPortMalloc(sizeof(display_msg_t));

        if (NULL == p_message) {
                ESP_LOGE(TAG, "Ran out of memory");
                success = false;

        } else {
                p_message->type = type;

                switch (type) {

                case DISPLAY_MSG_CO2_PPM:
                        p_message->numeric_value = (uint32_t)value;
                        break;

                case DISPLAY_MSG_WIFI_STATUS:
                        p_message->wifi_status = *((display_wifi_status_t *)value);
                        break;

                case DISPLAY_MSG_COUNT:
                        // TODO: fail here
                        break;
                }

                queue_result = xQueueSend(display_q, &p_message, 0);

                success = (pdPASS == queue_result);
        }

        if (!success) {
                ESP_LOGV(TAG, "Message couldn't be sent. Deleting it...");
                vPortFree(p_message);
                p_message = NULL;
        }

        return success;
}

static void backlight_timer_cb(TimerHandle_t const timer_handle)
{
        (void)timer_handle;
        display_enable_backlight(false);
}

static void display_enable_backlight(bool const enable)
{

        if (enable) {
                (void)xTimerReset(m_backlight_timer_h, 0);
                m_display_task_refresh_rate = pdMS_TO_TICKS(1000);
        } else {
                m_display_task_refresh_rate = portMAX_DELAY;
        }

        st7789_enable_backlight(enable);


}

static void draw_network_symbol(lv_obj_t * canvas, int8_t strength)
{
        int32_t const start_angle = 225;
        int32_t const end_angle = 315;

        int8_t const min_strength = -70;
        int8_t const med_strength = -60;
        int8_t const max_strength = -50;

        // TODO: round
        uint8_t const min_radius = 1 * (m_wifi_sign_height / 3);
        uint8_t const med_radius = 2 * (m_wifi_sign_height / 3);
        uint8_t const max_radius = 3 * (m_wifi_sign_height / 3);

        lv_draw_line_dsc_t draw_dsc;

        lv_canvas_set_buffer(canvas,
                             buffer,
                             (lv_coord_t)m_wifi_sign_width,
                             (lv_coord_t)m_wifi_sign_height,
                             LV_IMG_CF_TRUE_COLOR);

        lv_canvas_fill_bg(canvas, LV_COLOR_BLACK, LV_OPA_COVER);
        lv_draw_line_dsc_init(&draw_dsc);

        draw_dsc.color = LV_COLOR_WHITE;
        draw_dsc.width = 2;

        if (DISPLAY_RSSI_NO_IP_VALUE == strength) {
                // Todo:
        }

        if (min_strength < strength) {
                lv_canvas_draw_arc(canvas,
                                   (lv_coord_t)(m_wifi_sign_width / 2),
                                   (lv_coord_t)(m_wifi_sign_height),
                                   min_radius,
                                   start_angle,
                                   end_angle,
                                   &draw_dsc);
        }

        if (med_strength < strength) {
                lv_canvas_draw_arc(canvas,
                                   (lv_coord_t)(m_wifi_sign_width / 2),
                                   (lv_coord_t)(m_wifi_sign_height),
                                   med_radius,
                                   start_angle,
                                   end_angle,
                                   &draw_dsc);
        }

        if (max_strength < strength) {
                lv_canvas_draw_arc(canvas,
                                   (lv_coord_t)(m_wifi_sign_width / 2),
                                   (lv_coord_t)(m_wifi_sign_height),
                                   max_radius,
                                   start_angle,
                                   end_angle,
                                   &draw_dsc);
        }
}


/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */

_Noreturn static void display_task(void * pvParameters)
{
        uint32_t const low_concentration_max = 1000;
        uint32_t const high_concentration_min = 1500;
        char const * ip_label_default_text = "No IP";

        lv_obj_t * const scr = lv_scr_act();
        lv_obj_t * const co2_value = lv_label_create(scr, NULL);
        lv_obj_t * const units = lv_label_create(scr, NULL);
        lv_obj_t * const ip_label = lv_label_create(scr, NULL);
        lv_obj_t * const canvas = lv_canvas_create(scr, NULL);

        display_msg_t * p_message = NULL;

        int8_t rssi;
        uint32_t ip_addr;
        uint8_t octets[4];
        uint32_t co2_ppm;
        char text[20];
        bool style_changed;
        BaseType_t queue_result;
        BaseType_t notification_result;
        lv_color_t co2_value_color;


        /* configure CO2 concentration value style */
        lv_style_set_text_font(&m_ppm_style, LV_STATE_DEFAULT, &lv_font_montserrat_48);
        lv_style_set_text_color(&m_ppm_style, LV_STATE_DEFAULT, LV_COLOR_NAVY);

        /* configure CO2 concentration units style */
        lv_style_set_text_font(&m_units_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
        lv_style_set_text_color(&m_units_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);

        lv_style_set_bg_color(&m_bg_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

        lv_obj_add_style(units, LV_LABEL_PART_MAIN, &m_units_style);
        lv_obj_set_pos(units, 80, 85);
        lv_label_set_text(units, "ppm CO2");

        lv_obj_add_style(scr, LV_OBJ_PART_MAIN, &m_bg_style);

        lv_obj_add_style(co2_value, LV_LABEL_PART_MAIN, &m_ppm_style);
        lv_label_set_text(co2_value, ip_label_default_text);
        lv_label_set_long_mode(co2_value, LV_LABEL_LONG_EXPAND);
        lv_obj_align(co2_value, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_align(co2_value, LV_LABEL_ALIGN_CENTER);
        lv_label_set_text(co2_value, "-");

        lv_obj_add_style(ip_label, LV_LABEL_PART_MAIN, &m_units_style);
        lv_label_set_text(ip_label, ip_label_default_text);
        lv_label_set_long_mode(ip_label, LV_LABEL_LONG_EXPAND);
        lv_obj_align(ip_label, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 0);
        lv_label_set_align(ip_label, LV_LABEL_ALIGN_RIGHT);


        while (1) {

                notification_result = xTaskNotifyWait(0xFFFFFFFFUL,
                                                      0xFFFFFFFFUL,
                                                      NULL,
                                                      m_display_task_refresh_rate);

                // Reset the backlight timer only if it's configured
                if ((pdPASS == notification_result) && (NULL != m_backlight_timer_h)) {
                        display_enable_backlight(true);

                        // We're not interested in out-dated messages
                        xQueueReset(display_q);
                }

                // Don't receive any messages if the display won't show them
                if (portMAX_DELAY == m_display_task_refresh_rate) {
                        // Code style exception for readability
                        continue;
                }

                queue_result = xQueueReceive(display_q, &p_message, 0);

                if ((pdTRUE == queue_result) && (NULL != p_message)) {

                        switch (p_message->type) {

                        case DISPLAY_MSG_CO2_PPM:
                                co2_ppm = p_message->numeric_value;

                                style_changed = true;

                                if (low_concentration_max > co2_ppm) {
                                        co2_value_color = LV_COLOR_GREEN;

                                } else if ((low_concentration_max < co2_ppm) &&
                                           (high_concentration_min > co2_ppm)) {

                                        co2_value_color =  LV_COLOR_ORANGE;

                                } else if (high_concentration_min < co2_ppm) {
                                        co2_value_color = LV_COLOR_RED;

                                } else {
                                        style_changed = false;
                                }

                                if (style_changed) {
                                        lv_style_set_text_color(
                                                        &m_ppm_style,
                                                        LV_STATE_DEFAULT,
                                                        co2_value_color);

                                        lv_obj_add_style(
                                                        co2_value,
                                                        LV_LABEL_PART_MAIN,
                                                        &m_ppm_style);
                                }

                                sprintf(text, "%u", co2_ppm);
                                lv_label_set_text(co2_value, text);
                                lv_label_set_long_mode(co2_value, LV_LABEL_LONG_EXPAND);
                                lv_obj_align(co2_value, NULL, LV_ALIGN_CENTER, 0, 0);

                                break;
                        case DISPLAY_MSG_WIFI_STATUS:
                                ip_addr = p_message->wifi_status.ip;
                                rssi = p_message->wifi_status.rssi;

                                if (0 == ip_addr) {
                                        sprintf(text, DISPLAY_NO_IP_TEXT);
                                } else {
                                        octets[3] = (uint8_t)((ip_addr >> 3) & 0x000000FF);
                                        octets[2] = (uint8_t)((ip_addr >> 2) & 0x000000FF);
                                        octets[1] = (uint8_t)((ip_addr >> 1) & 0x000000FF);
                                        octets[0] = (uint8_t)(ip_addr & 0x000000FF);

                                        sprintf(text, "%u.%u.%u.%u",
                                                octets[0],
                                                octets[1],
                                                octets[2],
                                                octets[3]);
                                }

                                lv_label_set_text(ip_label, text);
                                lv_label_set_long_mode(ip_label, LV_LABEL_LONG_EXPAND);
                                lv_obj_align(ip_label, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 0);

                                draw_network_symbol(canvas, rssi);

                                break;

                        default:
                                break;
                        }

                        vPortFree(p_message);
                        p_message = NULL;
                }

                lv_task_handler();
        }
}
