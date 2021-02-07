//
// Created by Raúl Gotor on 12/31/20.
//

#include "display.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>
#include "lvgl.h"
#include "st7789.h"
#include "esp_freertos_hooks.h"

static const char *TAG = "main.c";
xQueueHandle sensor_do_q;
lv_disp_drv_t disp_drv;

static void lv_tick_task(void *arg) {
    (void) arg;

    lv_tick_inc(1);
}

static void display_init() {
    lv_init();
    lvgl_driver_init();

    static lv_color_t buf1[DISP_BUF_SIZE];
    static lv_color_t buf2[DISP_BUF_SIZE];
    static lv_disp_buf_t disp_buf;
    lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = st7789_flush;
    disp_drv.buffer = &disp_buf;

    lv_disp_drv_register(&disp_drv);

    esp_register_freertos_tick_hook(lv_tick_task);

}

void display_task(void *pvParameters) {

    display_init();

    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *co2_value = lv_label_create(scr, NULL);
    lv_obj_t *units = lv_label_create(scr, NULL);

    uint32_t rec_val;
    char text[20];

    static lv_style_t ppm_style;
    static lv_style_t units_style;
    static lv_style_t bg_style;

    /* configure CO2 concentration value style */
    lv_style_set_text_font(&ppm_style, LV_STATE_DEFAULT, &lv_font_montserrat_48);
    lv_style_set_text_color(&ppm_style, LV_STATE_DEFAULT, LV_COLOR_NAVY);

    /* configure CO2 concentration units style */
    lv_style_set_text_font(&units_style, LV_STATE_DEFAULT, &lv_font_montserrat_16);
    lv_style_set_text_color(&units_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);

    lv_style_set_bg_color(&bg_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);

    lv_obj_add_style(units, LV_LABEL_PART_MAIN, &units_style);
    lv_obj_add_style(scr, LV_OBJ_PART_MAIN, &bg_style);
    lv_obj_add_style(co2_value, LV_LABEL_PART_MAIN, &ppm_style);

    lv_obj_set_pos(co2_value, 50, 40);
    lv_obj_set_pos(units, 80, 85);


    //lv_obj_set_pos(button, 0, 0);
    //lv_obj_set_size(button, 240, 135);


    while (1) {
        if (pdTRUE == xQueueReceive(sensor_do_q, &rec_val, 100)) {

            if (rec_val < 1000) {
                lv_style_set_text_color(&ppm_style, LV_STATE_DEFAULT, LV_COLOR_GREEN);
            } else if (rec_val > 1000 && rec_val < 1500) {
                lv_style_set_text_color(&ppm_style, LV_STATE_DEFAULT, LV_COLOR_ORANGE);
            } else if (rec_val > 1500) {
                lv_style_set_text_color(&ppm_style, LV_STATE_DEFAULT, LV_COLOR_RED);
            }
            lv_obj_add_style(co2_value, LV_LABEL_PART_MAIN, &ppm_style);
            sprintf(text, "%u", rec_val);
            lv_label_set_text(co2_value, text);
            lv_label_set_text(units, "ppm CO2");
        }
        lv_task_handler();

    }
    vTaskDelete(NULL);
}
