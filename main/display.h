/*!
 *******************************************************************************
 * @file display.h
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

#ifndef MAIN_MAIN_DISPLAY_H_
#define MAIN_MAIN_DISPLAY_H_

/*
 *******************************************************************************
 * Public Macros                                                               *
 *******************************************************************************
 */

#define DISPLAY_BACKLIGHT_TIMEOUT_MS        (CONFIG_CO2_MONITOR_DISPLAY_BACKLIGHT_TIMEOUT_S * 1000)
#define DISPLAY_RSSI_NO_IP_VALUE            INT8_MIN

/*
 *******************************************************************************
 * Public Data Types                                                           *
 *******************************************************************************
 */

typedef enum {
        DISPLAY_MSG_CO2_PPM = 0,
        DISPLAY_MSG_BATTERY_LEVEL,
        DISPLAY_MSG_WIFI_STATUS,
        DISPLAY_MSG_LINK_STATUS,
        DISPLAY_MSG_COUNT
} display_msg_type_t;

typedef struct {
        int8_t rssi;
        uint32_t ip;
} display_wifi_status_t;

typedef struct {
        display_msg_type_t type;
        union {
                uint32_t numeric_value;
                bool flag;
                display_wifi_status_t wifi_status;
        };
} display_msg_t;


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

bool display_init(void);

bool display_set_wifi_status(display_wifi_status_t const display_wifi_status);

bool display_set_concentration(uint32_t const concentration);

bool display_set_battery_level(uint32_t const battery_level);

bool display_set_link_status(bool const linked);

bool display_is_active(void);

#endif //MAIN_MAIN_DISPLAY_H_
