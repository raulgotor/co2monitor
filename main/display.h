//
// Created by Raúl Gotor on 12/31/20.
//

#ifndef MAIN_MAIN_DISPLAY_H_
#define MAIN_MAIN_DISPLAY_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

extern xQueueHandle sensor_do_q;
extern void display_task(void *pvParameter);

#endif //MAIN_MAIN_DISPLAY_H_
