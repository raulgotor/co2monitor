/*!
 *******************************************************************************
 * @file tasks_config.h
 *
 * @brief 
 *
 * @author Raúl Gotor (raulgotor@gmail.com)
 * @date 24.04.22
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Raúl Gotor
 * All rights reserved.
 *******************************************************************************
 */

#ifndef TASKS_CONFIG_H
#define TASKS_CONFIG_H

/*
 *******************************************************************************
 * Public Macros                                                               *
 *******************************************************************************
 */

#define TASKS_CONFIG_DISPLAY_STACK_DEPTH        (1024 * 16)
#define TASKS_CONFIG_SENSOR_STACK_DEPTH         (1024 * 8)
#define TASKS_CONFIG_HTTP_STACK_DEPTH           (1024 * 8)

#define TASKS_CONFIG_DISPLAY_PRIORITY           (1)
#define TASKS_CONFIG_SENSOR_PRIORITY            (2)
#define TASKS_CONFIG_HTTP_PRIORITY              (2)

#define TASKS_CONFIG_DISPLAY_REFRESH_RATE_MS    (10)
#define TASKS_CONFIG_SENSOR_REFRESH_RATE_MS     (10000)
#define TASKS_CONFIG_HTTP_REFRESH_RATE_MS       (5000)

/*
 *******************************************************************************
 * Public Data Types                                                           *
 *******************************************************************************
 */

/*
 *******************************************************************************
 * Public Constants                                                            *
 *******************************************************************************
 */


/*
 *******************************************************************************
 * Public Function Prototypes                                                  *
 *******************************************************************************
 */

#endif //TASKS_CONFIG_H