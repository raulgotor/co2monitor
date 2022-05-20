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
#include "tasks_config.h"

#include "driver/adc_common.h"
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
#define DISPLAY_NO_AP_TEXT                  "No AP"
#define NO_WAIT                             (0)

#define TASK_REFRESH_RATE_TICKS     (pdMS_TO_TICKS(TASKS_CONFIG_DISPLAY_REFRESH_RATE_MS))
#define TASK_STACK_DEPTH            TASKS_CONFIG_DISPLAY_STACK_DEPTH
#define TASK_PRIORITY               TASKS_CONFIG_DISPLAY_PRIORITY
#define TAG                                 "display"

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

static void display_enable_backlight(bool const is_enabled);

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

extern TaskHandle_t sensor_task_h;

extern TaskHandle_t battery_task_h;

TaskHandle_t display_task_h = NULL;

/*
 *******************************************************************************
 * Static Data Declarations                                                    *
 *******************************************************************************
 */

static lv_style_t m_concentration_style;

static lv_style_t m_units_style;

static lv_style_t m_bg_style;

static int16_t  const m_wifi_sign_width = 30;

static int16_t  const m_wifi_sign_height = (m_wifi_sign_width / 2);

static lv_color_t m_wifi_sign_buffer[LV_CANVAS_BUF_SIZE_TRUE_COLOR(30, 15)];

static int16_t  const m_battery_sign_width = 30;

static int16_t  const m_battery_sign_height = (m_battery_sign_width / 2);

static lv_color_t m_battery_sign_buffer[LV_CANVAS_BUF_SIZE_TRUE_COLOR(30, 15)];

static lv_color_t m_link_sign_buffer[LV_CANVAS_BUF_SIZE_TRUE_COLOR(30, 30)];

static TimerHandle_t m_backlight_timer_h = NULL;

static uint32_t m_display_task_refresh_rate = TASK_REFRESH_RATE_TICKS;

static bool m_display_bckl_is_enabled = true;

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
                // Tell `sensor_task_h` and `battery_task_h` that our queue is ready to be used
                (void)xTaskNotifyIndexed(sensor_task_h, 0, 0, eNoAction);
                (void)xTaskNotifyIndexed(battery_task_h, 0, 0, eNoAction);

                task_result = xTaskCreate((TaskFunction_t)display_task,
                            "display_task",
                            TASK_STACK_DEPTH,
                            NULL,
                            TASK_PRIORITY,
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

bool display_set_battery_level(uint32_t const battery_level)
{
        return display_send_message(DISPLAY_MSG_BATTERY_LEVEL,
                                    (void *)(battery_level));
}

bool display_set_link_status(bool const linked)
{
        return display_send_message(DISPLAY_MSG_LINK_STATUS,
                                    (void *)(linked));
}

bool display_is_enabled(void)
{
        return m_display_bckl_is_enabled;
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
                case DISPLAY_MSG_BATTERY_LEVEL:
                        p_message->numeric_value = (uint32_t)value;
                        break;
                case DISPLAY_MSG_LINK_STATUS:
                        p_message->flag = (bool)value;
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

static void display_enable_backlight(bool const is_enabled)
{

        if (is_enabled) {
                (void)xTimerReset(m_backlight_timer_h, 0);
                m_display_task_refresh_rate = TASK_REFRESH_RATE_TICKS;
        } else {
                (void)xTimerStop(m_backlight_timer_h, 0);
                m_display_task_refresh_rate = portMAX_DELAY;
        }

        m_display_bckl_is_enabled = is_enabled;
        st7789_enable_backlight(is_enabled);
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
                             m_wifi_sign_buffer,
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

static void draw_battery_symbol(lv_obj_t * canvas, int level)
{
        int8_t const min_level = 1;
        int8_t const med_level = 2;
        int8_t const max_level = 3;

        int16_t const connector_width = 2;
        int16_t const battery_divisions = 3;
        int16_t const internal_margin = 2;
        int16_t const active_zone_border_height = m_battery_sign_height;
        int16_t const active_zone_border_width = m_battery_sign_width - connector_width;
        int16_t const active_zone_width = active_zone_border_width - (internal_margin * 2);
        int16_t const active_rect_width = active_zone_width / battery_divisions;
        int16_t const active_rect_heigh = active_zone_border_height - (internal_margin * 2);
        int16_t const active_rect_pos_x = 0 + internal_margin;
        int16_t const active_rect_pos_y = 0 + internal_margin;
        int16_t const active_rect_1_width = active_rect_width * 1;
        int16_t const active_rect_2_width = active_rect_width * 2;
        int16_t const active_rect_3_width = active_rect_width * 3;
        int16_t const connector_height = 5;
        int16_t const connector_margin = (m_battery_sign_height - connector_height) / 2;

        lv_draw_rect_dsc_t draw_dsc;
        lv_color_t border_color;
        lv_color_t fill_color;
        lv_coord_t rect_width;

        level = 4;


        lv_canvas_set_buffer(canvas,
                             m_battery_sign_buffer,
                             (lv_coord_t)m_battery_sign_width,
                             (lv_coord_t)m_battery_sign_height,
                             LV_IMG_CF_TRUE_COLOR);

        lv_canvas_fill_bg(canvas, LV_COLOR_BLACK, LV_OPA_COVER);
        lv_draw_rect_dsc_init(&draw_dsc);


        border_color = LV_COLOR_WHITE;
        fill_color = LV_COLOR_RED;


        if (max_level < level) {
                fill_color = LV_COLOR_GREEN;
                rect_width = active_rect_3_width;
        } else if (med_level < level) {
                fill_color = LV_COLOR_ORANGE;
                rect_width = active_rect_2_width;
        } else if (min_level < level) {
                fill_color = LV_COLOR_RED;
                rect_width = active_rect_1_width;
        } else {
                border_color = LV_COLOR_RED;
                rect_width = 0;
        }

        draw_dsc.border_color = border_color;
        draw_dsc.bg_opa = LV_OPA_TRANSP;
        draw_dsc.border_width = 2;
        draw_dsc.radius = 3;
        draw_dsc.border_width = 1;

        lv_canvas_draw_rect(canvas,
                            0,
                            0,
                            active_zone_border_width,
                            active_zone_border_height,
                            &draw_dsc);

        lv_canvas_draw_rect(canvas,
                            active_zone_border_width,
                            connector_margin,
                            connector_width,
                            connector_margin,
                            &draw_dsc);

        draw_dsc.border_color = fill_color;
        draw_dsc.bg_color = fill_color;
        draw_dsc.bg_opa = LV_OPA_COVER;

        lv_canvas_draw_rect(canvas,
                            active_rect_pos_x,
                            active_rect_pos_y,
                            rect_width,
                            active_rect_heigh,
                            &draw_dsc);
}

static void draw_backend_link_symbol(lv_obj_t * canvas, bool linked)
{

        lv_draw_rect_dsc_t draw_dsc;
        lv_color_t border_color;

        uint16_t const backend_link_sign_width = 10;
        uint16_t const backend_link_sign_height = 10;


        lv_canvas_set_buffer(canvas,
                             m_link_sign_buffer,
                             (lv_coord_t)backend_link_sign_width,
                             (lv_coord_t)backend_link_sign_height,
                             LV_IMG_CF_TRUE_COLOR);

        lv_canvas_fill_bg(canvas, LV_COLOR_BLACK, LV_OPA_COVER);
        lv_draw_rect_dsc_init(&draw_dsc);


        if (linked) {
                border_color = LV_COLOR_GREEN;
        } else {
                border_color = LV_COLOR_RED;
        }

        draw_dsc.bg_color = border_color;
        draw_dsc.border_color = border_color;
        draw_dsc.radius = backend_link_sign_width / 2;

        lv_canvas_draw_rect(canvas,
                            0,
                            0,
                            backend_link_sign_width,
                            backend_link_sign_height,
                            &draw_dsc);
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
        char const * battery_label_default_text = "";
        size_t const ssid_str_size = 33;
        size_t const ip_str_size = 16;
        size_t const separator_size = 3;
        bool const backlight_automatic = (NULL != m_backlight_timer_h);

        lv_obj_t * const scr = lv_scr_act();
        lv_obj_t * const co2_value = lv_label_create(scr, NULL);
        lv_obj_t * const units = lv_label_create(scr, NULL);
        lv_obj_t * const ip_label = lv_label_create(scr, NULL);
        lv_obj_t * const battery_label = lv_label_create(scr, NULL);
        lv_obj_t * const wifi_sign_canvas = lv_canvas_create(scr, NULL);
        lv_obj_t * const battery_sign_canvas = lv_canvas_create(scr, NULL);
        lv_obj_t * const link_sign_canvas = lv_canvas_create(scr, NULL);

        display_msg_t * p_message = NULL;

        int8_t rssi;
        uint32_t ip_addr;
        bool linked;
        uint32_t battery_level;
        uint8_t octets[4];
        uint32_t co2_ppm;
        char label_buffer[ssid_str_size + separator_size + ip_str_size];
        char ip_str[ip_str_size];
        bool style_changed;
        BaseType_t queue_result;
        BaseType_t notification_result;
        lv_color_t co2_value_color;
        bool is_enabled;

        /* configure CO2 concentration value style */
        lv_style_set_text_font(&m_concentration_style, LV_STATE_DEFAULT, &lv_font_montserrat_48);
        lv_style_set_text_color(&m_concentration_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);

        /* configure CO2 concentration units style */
        lv_style_set_text_font(&m_units_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
        lv_style_set_text_color(&m_units_style, LV_STATE_DEFAULT, LV_COLOR_WHITE);

        lv_style_set_bg_color(&m_bg_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

        lv_obj_add_style(units, LV_LABEL_PART_MAIN, &m_units_style);
        lv_obj_set_pos(units, 80, 85);
        lv_label_set_text(units, "ppm CO2");

        lv_obj_add_style(scr, LV_OBJ_PART_MAIN, &m_bg_style);

        lv_obj_add_style(co2_value, LV_LABEL_PART_MAIN, &m_concentration_style);
        lv_label_set_text(co2_value, "-");
        lv_label_set_long_mode(co2_value, LV_LABEL_LONG_EXPAND);
        lv_obj_align(co2_value, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_align(co2_value, LV_LABEL_ALIGN_CENTER);

        lv_obj_add_style(ip_label, LV_LABEL_PART_MAIN, &m_units_style);
        lv_label_set_long_mode(ip_label, LV_LABEL_LONG_SROLL_CIRC);
        lv_obj_set_width(ip_label, 100);
        lv_label_set_text(ip_label, ip_label_default_text);
        lv_obj_align(ip_label, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 0);
        lv_label_set_anim_speed(ip_label, 100);

        lv_obj_align(battery_sign_canvas, NULL, LV_ALIGN_IN_BOTTOM_LEFT, 0, -20);
        lv_obj_align(link_sign_canvas, wifi_sign_canvas, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

        lv_obj_add_style(battery_label, LV_LABEL_PART_MAIN, &m_units_style);
        lv_label_set_text(battery_label, battery_label_default_text);
        lv_label_set_long_mode(battery_label, LV_LABEL_LONG_EXPAND);
        lv_obj_align(battery_label, battery_sign_canvas, LV_ALIGN_OUT_RIGHT_MID, 0, 0);
        lv_label_set_align(battery_label, LV_LABEL_ALIGN_LEFT);

        while (1) {

                notification_result = xTaskNotifyWait(0xFFFFFFFFUL,
                                                      0xFFFFFFFFUL,
                                                      NULL,
                                                      m_display_task_refresh_rate);

                // Handle backlight state only if automatic mode is configured
                if ((pdPASS == notification_result) && (backlight_automatic)) {

                        is_enabled = display_is_enabled();

                        display_enable_backlight(!is_enabled);
                }

                // Don't receive any messages if the display won't show them
                if (portMAX_DELAY == m_display_task_refresh_rate) {
                        // Code style exception for readability
                        continue;
                }

                queue_result = xQueueReceive(display_q, &p_message, NO_WAIT);

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
                                                        &m_concentration_style,
                                                        LV_STATE_DEFAULT,
                                                        co2_value_color);

                                        lv_obj_add_style(
                                                        co2_value,
                                                        LV_LABEL_PART_MAIN,
                                                        &m_concentration_style);
                                }

                                sprintf(label_buffer, "%u", co2_ppm);
                                lv_label_set_text(co2_value, label_buffer);
                                lv_label_set_long_mode(co2_value, LV_LABEL_LONG_EXPAND);
                                lv_obj_align(co2_value, NULL, LV_ALIGN_CENTER, 0, 0);

                                break;

                        case DISPLAY_MSG_WIFI_STATUS:
                                ip_addr = p_message->wifi_status.ip;
                                rssi = p_message->wifi_status.rssi;

                                if (0 == strlen(p_message->wifi_status.ap_ssid)) {
                                        strcpy(label_buffer, DISPLAY_NO_AP_TEXT);
                                } else {
                                        strcpy(label_buffer,
                                                p_message->wifi_status.ap_ssid);
                                }

                                if (0 == ip_addr) {
                                        sprintf(ip_str, DISPLAY_NO_IP_TEXT);
                                } else {
                                        octets[3] = (uint8_t)((ip_addr >> 3) & 0x000000FF);
                                        octets[2] = (uint8_t)((ip_addr >> 2) & 0x000000FF);
                                        octets[1] = (uint8_t)((ip_addr >> 1) & 0x000000FF);
                                        octets[0] = (uint8_t)(ip_addr & 0x000000FF);

                                        sprintf(ip_str, "%u.%u.%u.%u",
                                                octets[0],
                                                octets[1],
                                                octets[2],
                                                octets[3]);
                                }

                                strcat(label_buffer, ", ");
                                strcat(label_buffer, ip_str);

                                /* Only refresh label if it changed, so scroll
                                 * animation is not spoiled
                                 */
                                if (0 != strcmp(label_buffer,
                                                lv_label_get_text(ip_label))) {
                                        lv_label_set_text(ip_label, label_buffer);
                                }

                                draw_network_symbol(wifi_sign_canvas, rssi);

                                break;

                        case DISPLAY_MSG_BATTERY_LEVEL:
                                battery_level = p_message->numeric_value;

                                sprintf(label_buffer, "%.2f V", ((float)battery_level) / 1000);

                                draw_battery_symbol(battery_sign_canvas, 5);
                                lv_label_set_text(battery_label, label_buffer);
                                lv_label_set_long_mode(battery_label, LV_LABEL_LONG_EXPAND);
                                lv_obj_align(battery_label, battery_sign_canvas, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

                                break;

                        case DISPLAY_MSG_LINK_STATUS:
                                linked = p_message->flag;

                                draw_backend_link_symbol(link_sign_canvas, linked);
                                lv_obj_align(link_sign_canvas, wifi_sign_canvas, LV_ALIGN_OUT_RIGHT_MID, 0, 0);

                                draw_battery_symbol(battery_sign_canvas, 5);
                                break;

                        default:
                                break;

                        }

                        vPortFree(p_message);
                        p_message = NULL;

                        ESP_LOGI(TAG,"Max stack usage: %d of %d bytes", TASK_STACK_DEPTH - uxTaskGetStackHighWaterMark(NULL), TASK_STACK_DEPTH);
                }

                lv_task_handler();
        }
}
