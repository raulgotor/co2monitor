/*!
 *******************************************************************************
 * @file winsen_mh_z19.c
 *
 * @brief 
 *
 * @author Raúl Gotor (raulgotor@gmail.com)
 * @date 04.04.22
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
#include <stdbool.h>
#include <stdlib.h>
#include <memory.h>
#include "winsen_mh_z19.h"

/*
 *******************************************************************************
 * Private Macros                                                              *
 *******************************************************************************
 */

/*!
 * @brief Sensor message format
 *
 * The sensor has different message format, even thought very similar, depending
 * if it is a reading (get) or writing (set) information.
 *
 * Write message
 *
 * Byte |   0   |    1   |     2   |  3  |  4  |  5  |  6  |  7  |      8      |
 *      |-------|--------|---------|-----|-----|-----|-----|-----|-------------|
 *      | Start |Reserved| Command | (MSByte)  Payload  (LSByte) | Check value |
 *      | 0xFF  | 0x01   |         |                             |             |
 *
 * Read message
 *
 * Byte |   0   |    1   |     2   |  3  |  4  |  5  |  6  |  7  |      8      |
 *      |-------|--------|---------|-----|-----|-----|-----|-----|-------------|
 *      | Start | Command|   (MSByte)    Payload      (LSByte)   | Check value |
 *      | 0xFF  |        |                                       |             |
 *
 *
 * The payload, when present is transmitted with the most significant byte first
 */

#define MH_Z19_MSG_START_VALUE_BYTE         (0)
#define MH_Z19_MSG_CHECK_VALUE_BYTE         (8)

#define MH_Z19_MSG_SET_COMMAND_BYTE         (2)
#define MH_Z19_MSG_SET_PAYLOAD_START_BYTE   (3)
#define MH_Z19_MSG_GET_PAYLOAD_START_BYTE   (2)

#define MH_Z19_MSG_START_VALUE              (0xFF)
#define MH_Z19_MSG_SENSOR_NUMBER            (0x01)

#define MH_Z19B_ABC_SETTING_ON              (0xA0)
#define MH_Z19B_ABC_SETTING_OFF             (0x00)

/*
 *******************************************************************************
 * Data types                                                                  *
 *******************************************************************************
 */

//! @brief Different available commands
typedef enum {
        //! @brief Get gas concentration
        MH_Z19_COMMAND_GAS_CONCENTRATION = 0x86,

        //! @brief Calibrate zero point
        MH_Z19_COMMAND_CAL_ZERO_POINT = 0x87,

        //! @brief Calibrate span point
        MH_Z19_COMMAND_CAL_SPAN_POINT = 0x88,

        //! @brief Set auto baseline correction on / off
        MH_Z19_COMMAND_SET_ABC = 0x79,

        //! @brief
        MH_Z19_COMMAND_SET_RANGE = 0x99,

        //! @brief Fence member
        MH_Z19_COMMAND_COUNT
} mh_z19_command_t;

/*
 *******************************************************************************
 * Constants                                                                   *
 *******************************************************************************
 */

//! @brief Size of the message transferred message
size_t const m_message_size = 9;

//! @brief Maximum payload size
size_t const m_max_payload_size = 5;

//! @brief Write message template to be filled in
uint8_t const m_tx_message_template[] = {
                MH_Z19_MSG_START_VALUE,
                MH_Z19_MSG_SENSOR_NUMBER,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00,
                0x00
};

/*
 *******************************************************************************
 * Private Function Prototypes                                                 *
 *******************************************************************************
 */

//! @brief Send command to the MH-Z19 sensor
static mh_z19_error_t send_command(mh_z19_command_t command,
                                   uint8_t const * const p_payload,
                                   size_t const payload_size);

//! @brief Calculate the check value
static uint8_t calculate_check_value(uint8_t const * const p_message);

//! @brief Validate message
static bool is_valid_message(uint8_t const * const p_message);

//! @brief Validate command
static bool is_valid_command(mh_z19_command_t const command);

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

//! @brief Whether the module is initialized or not
static bool m_is_initialized = false;

//! @brief UART transfer function
static mh_z19_xfer_func m_xfer_func = NULL;

/*
 *******************************************************************************
 * Public Function Bodies                                                      *
 *******************************************************************************
 */

/*!
 * @brief Initialize the module
 *
 * @param[in]           xfer_func           Pointer to an UART transfer function
 *
 * @return              mh_z19_error_t      Operation result
 * @retval              MH_Z19_ERROR_SUCCESS
 *                                          Everything went well
 * @retval              MH_Z19_ERROR_ALREADY_INITIALIZED
 *                                          Module is already initialized
 * @retval              MH_Z19_ERROR_BAD_PARAMETER
 *                                          Parameter is null
 */
mh_z19_error_t mh_z19_init(mh_z19_xfer_func const xfer_func)
{
        mh_z19_error_t result = MH_Z19_ERROR_SUCCESS;

        if (m_is_initialized) {
                result = MH_Z19_ERROR_ALREADY_INITIALIZED;
        } else if (NULL == xfer_func) {
                result = MH_Z19_ERROR_BAD_PARAMETER;
        } else {
                m_xfer_func = xfer_func;
                m_is_initialized = true;
        }

        return result;
}

/*!
 * @brief Get gas concentration
 *
 * Get the CO2 concentration in ppm
 *
 * @param[out]          p_concentration     Pointer where to store the gas
 *                                          concentration (in ppm CO2)
 *
 * @return              mh_z19_error_t      Operation result
 * @retval              MH_Z19_ERROR_SUCCESS
 *                                          Everything went well
 * @retval              MH_Z19_ERROR_NOT_INITIALIZED
 *                                          Module isn't initialized
 * @retval              MH_Z19_ERROR_BAD_PARAMETER
 *                                          Parameter is null
 */
mh_z19_error_t mh_z19_get_gas_concentration(uint32_t * const p_concentration)
{
        mh_z19_error_t result;
        uint8_t rx_buffer[m_message_size];
        uint16_t concentration;

        if (!m_is_initialized) {
                result = MH_Z19_ERROR_NOT_INITIALIZED;
        } else if (NULL == p_concentration) {
                result = MH_Z19_ERROR_BAD_PARAMETER;
        } else {
                result = send_command(MH_Z19_COMMAND_GAS_CONCENTRATION, NULL, 0);
        }

        if (MH_Z19_ERROR_SUCCESS == result) {
                result = m_xfer_func(rx_buffer, m_message_size, NULL, 0);
        }

        if ((MH_Z19_ERROR_SUCCESS == result) && (is_valid_message(rx_buffer))) {
                concentration =(rx_buffer[MH_Z19_MSG_GET_PAYLOAD_START_BYTE] << 8);
                concentration |= (rx_buffer[MH_Z19_MSG_GET_PAYLOAD_START_BYTE + 1]);

                *p_concentration = concentration;
        }

        return result;
}

/*!
 * @brief Calibrate zero point
 *
 * This function calibrates the current CO2 concentration as the lowest allowed
 * concentration, which is 400 ppm.
 *
 * Suggested calibration method is to let the sensor stabilize outdoors, and
 * where the CO2 concentration is the lowest, and then execute this function
 *
 * @param               -                   -
 *
 * @return              mh_z19_error_t      Operation result
 * @retval              MH_Z19_ERROR_SUCCESS
 *                                          Everything went well
 * @retval              MH_Z19_ERROR_NOT_INITIALIZED
 *                                          Module isn't initialized
 * @retval              *                   Any other error
 */
mh_z19_error_t mh_z19_calibrate_zero_point(void)
{
        mh_z19_error_t result;

        if (!m_is_initialized) {
                result = MH_Z19_ERROR_NOT_INITIALIZED;
        } else {
                result = send_command(MH_Z19_COMMAND_CAL_ZERO_POINT, NULL, 0);
        }

        return result;
}

/*!
 * @brief Calibrate span point
 *
 * Calibrate the gas concentration to a specific value.
 *
 * @note Zero calibration should be executed before this command
 *
 * @note Before issuing this command, the sensor should be 20 minutes at least
 *       in a stable CO2 atmosphere at the specified concentration.
 *
 * @note A span value of 2000 ppm is recommended. If this value cannot be
 *       achieved, use a concentration of at least 1000 ppm.
 *
 * @param[in]           span_point          Span point to pass to the
 *                                          calibrating function
 *
 * @return              mh_z19_error_t      Operation result
 * @retval              MH_Z19_ERROR_SUCCESS
 *                                          Everything went well
 * @retval              MH_Z19_ERROR_NOT_INITIALIZED
 *                                          Module isn't initialized
 * @retval              *                   Any other error
 */
mh_z19_error_t mh_z19_calibrate_span_point(uint16_t const span_point)
{
        size_t const payload_size = 2;
        uint8_t payload[payload_size];
        mh_z19_error_t result;

        if (!m_is_initialized) {
                result = MH_Z19_ERROR_NOT_INITIALIZED;
        } else {
                payload[0] = (0xFF & (span_point >> 8));
                payload[1] = (0xFF & (span_point));

                result = send_command(MH_Z19_COMMAND_CAL_SPAN_POINT,
                                      payload,
                                      payload_size);
        }

        return result;
}

/*!
 * @brief Enable / disable ABC
 *
 * Enable / disable automatic baseline correction, which is executed every 24h.
 * With ABC on, device tries to correct its baseline, but if the device is
 * indoors, the baseline correction can produce deviated results.
 *
 * @note ABC will be active next time device goes through a power cycle
 *
 * @param[in]           enabled             Whether the ABC should be enabled
 *                                          or disabled
 *
 * @return              mh_z19_error_t      Operation result
 * @retval              MH_Z19_ERROR_SUCCESS
 *                                          Everything went well
 * @retval              MH_Z19_ERROR_NOT_INITIALIZED
 *                                          Module isn't initialized
 * @retval              *                   Any other error
 */
mh_z19_error_t mh_z19_enable_abc(bool enabled)
{
        mh_z19_error_t result;
        uint8_t payload;

        if (!m_is_initialized) {
                result = MH_Z19_ERROR_NOT_INITIALIZED;
        } else {

                if (enabled) {
                        payload = MH_Z19B_ABC_SETTING_ON;
                } else {
                        payload = MH_Z19B_ABC_SETTING_OFF;
                }
                result = send_command(MH_Z19_COMMAND_SET_ABC, &payload, 1);
        }

        return result;
}

/*!
 * @brief Set detection range
 *
 * Sets a specific detection range. The allowed ranges are 0 - 2000, 0 - 5000
 * and 0 - 10000 ppm.
 *
 * @param[in]           range               Desired range to be set
 *
 * @return              mh_z19_error_t      Operation result
 * @retval              MH_Z19_ERROR_SUCCESS
 *                                          Everything went well
 * @retval              MH_Z19_ERROR_NOT_INITIALIZED
 *                                          Module isn't initialized
 * @retval              *                   Any other error
 */
mh_z19_error_t mh_z19_set_range(mh_z19b_range_t const range)
{
        uint32_t const ranges[] = {
                        2000,
                        5000,
                        10000
        };

        size_t const payload_size = 5;
        uint8_t payload[payload_size];
        mh_z19_error_t result;


        if (!m_is_initialized) {
                result = MH_Z19_ERROR_NOT_INITIALIZED;
        } else if (MH_Z19B_RANGE_COUNT <= range) {
                result = MH_Z19_ERROR_BAD_PARAMETER;
        } else {
                payload[0] = 0x00;
                payload[1] = (0xFF & (ranges[range] >> 24));
                payload[2] = (0xFF & (ranges[range] >> 16));
                payload[3] = (0xFF & (ranges[range] >> 8));
                payload[4] = (0xFF & (ranges[range]));

                result = send_command(MH_Z19_COMMAND_SET_RANGE,
                                      payload,
                                      payload_size);
        }

        return result;
}

/*
 *******************************************************************************
 * Private Function Bodies                                                     *
 *******************************************************************************
 */

/*!
 * @brief Send command to the MH-Z19 sensor
 *
 * @param[in]           command             Command to be sent
 * @param[in]           p_payload           Pointer to a payload to be sent
 *                                          (can be null if not needed)
 * @param[in]           payload_size        Size of the sent payload (set it to
 *                                          0 if payload is NULL)
 *
 * @return              mh_z19_error_t      Operation result
 * @retval              MH_Z19_ERROR_SUCCESS
 *                                          Everything went well
 * @retval              MH_Z19_ERROR_BAD_PARAMETER
 *                                          Parameter is null
 */
static mh_z19_error_t send_command(mh_z19_command_t command,
                                   uint8_t const * const p_payload,
                                   size_t const payload_size)
{
        uint8_t message[m_message_size];
        mh_z19_error_t result;
        uint8_t check_val;

        if (!is_valid_command(command)) {
                result = MH_Z19_ERROR_BAD_PARAMETER;

        } else if ((NULL == p_payload) != (0 == payload_size)) { // XOR
                result = MH_Z19_ERROR_BAD_PARAMETER;

        } else if (m_max_payload_size < payload_size) {
                result = MH_Z19_ERROR_BAD_PARAMETER;

        } else {
                memcpy(message, m_tx_message_template, m_message_size);
                message[MH_Z19_MSG_SET_COMMAND_BYTE] = command;

                // Will be ignored if `payload_size` is 0
                memcpy(&message[MH_Z19_MSG_SET_PAYLOAD_START_BYTE],
                       p_payload,
                       payload_size);

                check_val = calculate_check_value(message);

                message[MH_Z19_MSG_CHECK_VALUE_BYTE] = check_val;

                result = m_xfer_func(NULL, 0, message, m_message_size);
        }

        return result;
}

/*!
 * @brief Calculate the check value
 *
 * Check value follows the following formula:
 *
 * check_value = inv(byte_1 + byte_2 + ... + byte_7) + 1
 *
 * @note Byte 0 and byte 8 are excluded from the sum.
 *
 * @note The operation should be carried in a byte size variable
 *       (meant to overflow)
 *
 * @warning The function won't be checking for pointer validity. Is caller's
 *          responsibility to ensure a valid pointer.
 *
 * @param[in]           p_message          Pointer to the message to calculate
 *                                         the check value from
 *
 * @return              uint8_t            Calculation result
 */
static uint8_t calculate_check_value(uint8_t const * const p_message)
{
        uint8_t temp_check_val = 0x00;
        size_t i;

        if (NULL != p_message) {
                for (i = 1; MH_Z19_MSG_CHECK_VALUE_BYTE > i; ++i) {
                        // Overflow is meant to happen
                        temp_check_val += p_message[i];
                }

                temp_check_val = (~temp_check_val + 1);
        }

        return temp_check_val;
}

/*!
 * @brief Validate message
 *
 * The function compares the calculated and the message's check value to
 * validate the message integrity.
 *
 * @param[in]           p_message          Pointer to the message to validate
 *
 * @return              bool               Operation result
 */
static bool is_valid_message(uint8_t const * const p_message)
{
        uint8_t const start_val = p_message[MH_Z19_MSG_START_VALUE_BYTE];
        uint8_t const check_val = p_message[MH_Z19_MSG_CHECK_VALUE_BYTE];
        uint8_t const calculated_check_val = calculate_check_value(p_message);


        return ((check_val == calculated_check_val) &&
                (MH_Z19_MSG_START_VALUE == start_val));
}

/*!
 * @brief Validate command
 *
 * Check whether the command is a valid `mh_z19_command_t` command
 *
 * @param[in]           command            Command to validate
 *
 * @return              bool               Operation result
 */
static bool is_valid_command(mh_z19_command_t const command)
{
        bool success;
        switch (command) {

        // Intentional fall-through
        case MH_Z19_COMMAND_GAS_CONCENTRATION:
        case MH_Z19_COMMAND_CAL_ZERO_POINT:
        case MH_Z19_COMMAND_CAL_SPAN_POINT:
        case MH_Z19_COMMAND_SET_ABC:
                success = true;
                break;
        default:
                success = false;
                break;
        }

        return success;
}

/*
 *******************************************************************************
 * Interrupt Service Routines / Tasks / Thread Main Functions                  *
 *******************************************************************************
 */
