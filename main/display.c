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

/*
 *******************************************************************************
 * Public Data Declarations                                                    *
 *******************************************************************************
 */

extern TaskHandle_t sensor_task_h;

/*
 *******************************************************************************
 * Static Data Declarations                                                    *
 *******************************************************************************
 */

static TaskHandle_t display_task_h = NULL;

static lv_style_t m_ppm_style;

static lv_style_t m_units_style;

static lv_style_t m_bg_style;

/*
 *******************************************************************************
 * Public Function Bodies                                                      *
 *******************************************************************************
 */

bool display_init()
{
        bool success = true;
        lv_disp_drv_t disp_drv;
        lv_disp_t * p_display;
        esp_err_t esp_result;
        BaseType_t task_result;

        static lv_color_t buf1[DISP_BUF_SIZE];
        static lv_color_t buf2[DISP_BUF_SIZE];
        static lv_disp_buf_t disp_buf;

        lv_init();
        lvgl_driver_init();

        lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

        lv_disp_drv_init(&disp_drv);
        disp_drv.flush_cb = st7789_flush;
        disp_drv.buffer = &disp_buf;

        p_display = lv_disp_drv_register(&disp_drv);

        if (NULL == p_display) {
                success = false;
        }

        if (success) {
                esp_result = esp_register_freertos_tick_hook(lv_tick_task);

                success = (ESP_OK == esp_result);
        }

        if (success) {
                display_q = xQueueCreate(10U, sizeof(uint32_t));

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

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */

_Noreturn static void display_task(void * pvParameters)
{
        uint32_t co2_ppm;
        char text[20];
        bool style_changed;

        lv_obj_t * scr = lv_scr_act();
        lv_obj_t * co2_value = lv_label_create(scr, NULL);
        lv_obj_t * units = lv_label_create(scr, NULL);

        /* configure CO2 concentration value style */
        lv_style_set_text_font(&m_ppm_style, LV_STATE_DEFAULT, &lv_font_montserrat_48);
        lv_style_set_text_color(&m_ppm_style, LV_STATE_DEFAULT, LV_COLOR_NAVY);

        /* configure CO2 concentration units style */
        lv_style_set_text_font(&m_units_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
        lv_style_set_text_color(&m_units_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);

        lv_style_set_bg_color(&m_bg_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

        lv_obj_add_style(units, LV_LABEL_PART_MAIN, &m_units_style);
        lv_obj_add_style(scr, LV_OBJ_PART_MAIN, &m_bg_style);
        lv_obj_add_style(co2_value, LV_LABEL_PART_MAIN, &m_ppm_style);

        lv_obj_set_pos(co2_value, 50, 40);
        lv_obj_set_pos(units, 80, 85);

        //lv_obj_set_pos(button, 0, 0);
        //lv_obj_set_size(button, 240, 135);

        while (1) {
                if (pdTRUE == xQueueReceive(display_q, &co2_ppm, 100)) {

                        style_changed = true;
                        if (co2_ppm < 1000) {
                                lv_style_set_text_color(&m_ppm_style, LV_STATE_DEFAULT, LV_COLOR_GREEN);
                        } else if (co2_ppm > 1000 && co2_ppm < 1500) {
                                lv_style_set_text_color(&m_ppm_style, LV_STATE_DEFAULT, LV_COLOR_ORANGE);
                        } else if (co2_ppm > 1500) {
                                lv_style_set_text_color(&m_ppm_style, LV_STATE_DEFAULT, LV_COLOR_RED);
                        } else {
                                style_changed = false;
                        }

                        if (style_changed) {
                                lv_obj_add_style(co2_value, LV_LABEL_PART_MAIN, &m_ppm_style);
                        }

                        sprintf(text, "%u", co2_ppm);
                        lv_label_set_text(co2_value, text);
                        lv_label_set_text(units, "ppm CO2");
                }
                lv_task_handler();

        }
}
