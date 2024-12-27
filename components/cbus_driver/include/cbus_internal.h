/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CBUS_INTERNAL_H_
#define _CBUS_INTERNAL_H_

#include <inttypes.h>
#include "cbus_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Device command structure
* @brief Hold command and data params send to device
*/
typedef struct cbus_device_command {
    union {
        struct {
            uint32_t command:4;     /*!< Command to execute */
            uint32_t inDataLen:7;   /*!< Data length send to device */
            uint32_t outDataLen:7;  /*!< Data length received from device */
            uint32_t reserved:14;   /*!< Not used */
        };
        uint32_t uCommand;
    };
} cbus_device_command_t;

/** Common bus command structure */
typedef struct {
    cbus_device_command_t device_command;
    cbus_device_transaction_t device_transaction;
    uint8_t *data;          /*!< Pointer to data buffer */
} cbus_cmd_t;

/** 
 * Common bus result data structure 
 * @note
 *      - Data structure that hold result for last ops
 *        and targeted device ID. If no device targeted
 *        device ID holds 0x00000000UL
 */
typedef struct {
    union {
        struct {
            uint32_t error:4;       /*!< Last result code */
            uint32_t reserved:28;   /*!< Not used */
        };
        uint32_t val;
    };
    uint32_t id;    /*!< Target device id */
} cbus_id_t;

typedef struct {
    uint64_t timeouts:16;
    uint64_t crc_error:16;
    uint64_t other:16;
    uint64_t spi_corr:4;
    uint64_t notused:12;
    uint32_t snd;
    uint32_t rcv;
} cbus_statistic_t;

typedef struct {
    cbus_statistic_t stats;
    uint64_t other;
} cbus_stats_data_t;

/**
 * @brief Common bus interface definition
 */
typedef struct cbus_driver {
    /**
     * @brief Attach device to bus driver
     *
     * @param[in] payload Pointer to common driver device configuration
     * @return
     *      - Common bus id structure
     */
    cbus_id_t (*attach)(cbus_device_config_t *payload);

    /**
     * @brief Deattach device from bus driver
     *
     * @param[in] id Device ID
     * @return
     *      - Common bus id structure
     */
    cbus_id_t (*deattach)(uint32_t id);

    /**
     * @brief Device description
     *
     * @param[in] id Device ID
     * @param[in] len Maximum length for device description
     * @param[out] desc Pointer to char array for description
     * 
     * @return
     *      - Common bus id structure
     */
    cbus_id_t (*info)(uint32_t id, uint8_t *desc, size_t len);

    /**
     * @brief Execute command
     *
     * @param[in] payload Pointer to cbus commom command structure
     * 
     * @return
     *      - Common bus id structure
     */
    cbus_id_t (*execute)(cbus_cmd_t *payload);

    /**
     * @brief Execute command
     *
     * @param[in] payload Pointer to cbus commom command structure
     * 
     * @return
     *      - Common bus id structure
     */
    cbus_id_t (*stats)(uint32_t id, cbus_stats_data_t *stat_data);
    //void * (*deinit_bus)(void *payload);
} cbus_driver_t;

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_INTERNAL_H_ */