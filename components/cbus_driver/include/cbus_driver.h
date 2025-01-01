/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CBUS_DRIVER_H_
#define _CBUS_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include "cbus_devices.h"
#include "cbus_1wire_driver.h"
#include "cbus_i2c_driver.h"
#include "cbus_spi_driver.h"
#include "esp_event.h"


/**
 * Type of driver event/handlers commands 
 */
typedef enum {
    CBUSCMD_RESET,      /*!< Reset bus */
    CBUSCMD_READ,       /*!< Read transaction to device */
    CBUSCMD_WRITE,      /*!< Write transaction to device */
    CBUSCMD_RW,         /*!< Write-Read transaction with device */
    CBUSCMD_PROBE,      /*!< Probe single device presence */
    CBUSCMD_SCAN,       /*!< Scan bus for devices */
    CBUSCMD_INFO,       /*!< Get device description  */
    CBUSCMD_STATS,
    CBUSCMD_ATTACH,     /*!< Attach device to bus */
    CBUSCMD_DEATTACH    /*!< Deattach device from bus */
} cbus_command_t;

/**
 * Type of driver data types
 */
typedef enum {
    CBUSDATA_UINT8,     /*!< BYTE type */
    CBUSDATA_UINT16,    /*!< WORD type - 2 bytes */
    CBUSDATA_UINT32,    /*!< Double word type - 4 bytes */
    CBUSDATA_UINT64,    /*!< Quad word type - 8 bytes */
    CBUSDATA_BLOB,      /*!< BLOB type */
    CBUSDATA_MAX        /*!< MAX */
} cbus_data_t;

/**
 * Type of driver result/error codes
 */
typedef enum {
    CBUS_OK,                    /*!< No error */
    CBUS_ERR_TIMEOUT,           /*!< Bus timeout */
    CBUS_ERR_BAD_ARGS,          /*!< Bad arguments passed */
    CBUS_ERR_BAD_CRC,           /*!< CRC Check failed */
    CBUS_ERR_UNKNOWN,           /*!< Unknown or not determinated error */
    CBUS_ERR_NO_MEM,            /*!< No memory left */
    CBUS_ERR_NO_MORE_BUSES,     /*!< No free buses left */
    CBUS_ERR_PIN_IN_USE,        /*!< Requested PIN/GPIO already in use */
    CBUS_ERR_DEVICE_EXIST,      /*!< Device already exist on same bus */
    CBUS_ERR_DEVICE_NOT_FOUND,  /*!< Device not found in bus device list */
    CBUS_ERR_DEVICE_NOT_ACK,    /*!< Device not acknowledge */
    CBUS_ERR_NOT_USED           /*!< Requested operation not implemented */
} cbus_opcodes_t;

/**
 * Type of common bus events
 */
typedef enum {
    CBUS_EVENT_EXEC,    /*!< Command event */
    CBUS_EVENT_DATA,    /*!< Responce event */
    CBUS_EVENT_ERROR    /*!< Error indication event */
} i2cdrv_resp_event_t;

ESP_EVENT_DECLARE_BASE(CBUS_EVENT);

/**
 * Type of command for event to cbus driver
 */
typedef struct cbus_event_command {
    union {
        struct {
            uint32_t command:4;     /*!< Command to execute */
            uint32_t inDataLen:7;   /*!< Data length send to device */
            uint32_t outDataLen:7;  /*!< Data length received from device */
            uint32_t data_type:3;   /*!< Data type */
            uint32_t status:4;      /*!< Bus status code */
            uint32_t reserved:7;    /*!< Not used */
        };
        uint32_t event_command;
    };
} cbus_event_command_t;

/**
 * Type of event data to cbus driver
 */
typedef struct {
    cbus_event_command_t cmd;               /*!< Command parameters */
    cbus_device_transaction_t transaction;  /*!< Device transaction parameters */
    union {
        struct {
            uint32_t sender_id:8;   /*!< Event sender ID */
            uint32_t bus_type:4;    /*!< Return a bus type executed on */
            uint32_t event_id:20;   /*!< User event ID */
        };
        uint32_t val;
    };
    void *user_device_handle;
    uint8_t payload[128];       /*!< Command payload */
} cbus_event_data_t;

/**
 * @brief Find device in internal list
 *
 * @param[out] handle Event loop handle to interact with
 * @param[out] sender_id Event sender ID. Can be used in event data
 * @return
 *      - ESP_OK - Valid handle and sender ID
 *      - ESP_FAIL - No more senders allowed
 *      - ESP_ERR_NO_MEM - No memory for internal config
 *      - Other - Error from event loop or handler registration
 */
esp_err_t cbus_register(esp_event_loop_handle_t **handle, uint32_t *sender_id);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_DRIVER_H_ */