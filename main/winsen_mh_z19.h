/*!
 *******************************************************************************
 * @file winsen_mh_z19.h
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

#ifndef WINSEN_MH_Z19_H
#define WINSEN_MH_Z19_H

/*
 *******************************************************************************
 * Public Macros                                                               *
 *******************************************************************************
 */


/*
 *******************************************************************************
 * Public Data Types                                                           *
 *******************************************************************************
 */

//! @brief Different operation results thrown by the module
typedef enum {
        MH_Z19_ERROR_SUCCESS = 0,
        MH_Z19_ERROR_ALREADY_INITIALIZED,
        MH_Z19_ERROR_NOT_INITIALIZED,
        MH_Z19_ERROR_BAD_PARAMETER,
        MH_Z19_ERROR_GENERAL_ERROR,
        MH_Z19_ERROR_IO_ERROR,
        MH_Z19_ERROR_COUNT,
} mh_z19_error_t;

/*!
 * @brief UART transfer function prototype
 *
 * The transfer function that will be registered with the `mh_z19_init` function
 * must follow this prototype
 *
 * @note If the operation to perform is a read operation:
 *          - `p_tx_buffer` must be null
 *          - `tx_buffer_size` must be 0
 *
 * @note If the operation to perform is a write operation:
 *          - `p_rx_buffer` must be null
 *          - `rx_buffer_size` must be 0
 *
 * @note `p_rx_buffer` and `p_tx_buffer` cannot be both null
 *
 * @param[out]          p_rx_buffer         Pointer to the buffer where to read
 *                                          the data to
 * @param[in]           rx_buffer_size      Size of the read buffer
 * @param[in]           p_tx_buffer         Pointer to the buffer where to read
 *                                          the data from
 * @param[in]           tx_buffer_size      Size of the write buffer
 *
 * @return              mh_z19_error_t      Operation result
 * @retval              MH_Z19_ERROR_SUCCESS
 *                                          Everything went well
 * @retval              MH_Z19_ERROR_BAD_PARAMETER
 *                                          Parameter is null
 */
typedef mh_z19_error_t (*mh_z19_xfer_func )(uint8_t * const p_rx_buffer,
                                            size_t const rx_buffer_size,
                                            uint8_t const * const p_tx_buffer,
                                            size_t const tx_buffer_size);

//! @brief Allowed detection range settings
typedef enum {
        MH_Z19B_RANGE_0_2000_PPM = 0,
        MH_Z19B_RANGE_0_5000_PPM,
        MH_Z19B_RANGE_0_10000_PPM,
        MH_Z19B_RANGE_COUNT,
} mh_z19b_range_t;

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

//! @brief Initialize the module
mh_z19_error_t mh_z19_init(mh_z19_xfer_func const xfer_func);

//! @brief Get gas concentration
mh_z19_error_t mh_z19_get_gas_concentration(uint32_t * const p_concentration);

//! @brief Calibrate zero point
mh_z19_error_t mh_z19_calibrate_zero_point(void);

//! @brief Calibrate span point
mh_z19_error_t mh_z19_calibrate_span_point(uint16_t const span_point);

//! @brief Enable / disable ABC
mh_z19_error_t mh_z19_enable_abc(bool enabled);

//! @brief Set detection range
mh_z19_error_t mh_z19_set_range(uint32_t const range);


#endif //WINSEN_MH_Z19_H