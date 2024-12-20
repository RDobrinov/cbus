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
#include "cbus_1wire_driver.h"
#include "cbus_i2c_driver.h"
#include "cbus_spi_driver.h"
#include "esp_event.h"

/**
 * Type of driver bus types
 */
typedef enum cbus_bus_types {
    CBUS_BUS_I2C,   /*!< I2C bus type */
    CBUS_BUS_SPI,   /*!< SPI bus type */
    CBUS_BUS_1WIRE  /*!< Dallas 1-Wire bus type */
} cbus_bus_t;

/**
 * Type of driver event/handlers commands 
 */
typedef enum {
    CBUSCMD_RESET,      /*!< Reset bus */
    CBUSCMD_READ,       /*!< Read trasaction to device */
    CBUSCMD_WRITE,      /*!< Write trasaction to device */
    CBUSCMD_RW,         /*!< Write-Read trasaction with device */
    CBUSCMD_PROBE,      /*!< Probe single device presence */
    CBUSCMD_SCAN,       /*!< Scan bus for devices */
    CBUSCMD_INFO,       /*!< Get device description  */
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
    CCBUSDATA_UINT64,   /*!< Quad word type - 8 bytes */
    CBUSDATA_BLOB,      /*!< BLOB type */
    CBUSDATA_MAX        /*!< MAX */
} cbus_data_t;

/**
 * Type of driver result/error codes
 */
typedef enum {
    CBUS_OK,                    /*!< No error */
    //CBUS_ERR_NOT_FOUND,       /*!<  */
    CBUS_ERR_TIMEOUT,           /*!< Bus timeout */
    CBUS_ERR_BAD_ARGS,          /*!< Bad arguments passed */
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
 * Type of common bus device confiuration
 */
typedef struct cbus_device_config {
    cbus_bus_t bus_type;                    /*!< Device bus type */
    union {
        /** I2C device configuration */
        struct {
            uint32_t dev_addr_length:1;     /*!< I2C address length */
            uint32_t device_address:10;     /*!< I2C device address */
            uint32_t scl_gpio:7;            /*!< I2C device clock GPIO */
            uint32_t sda_gpio:7;            /*!< I2C device data GPIO */
            uint32_t disable_ack_check:1;   /*!< I2C disable ack check */
            uint32_t rlsb:6;                /*!< Not used */
            uint32_t scl_speed_hz:20;       /*!< I2C device clock speed */
            uint32_t xfer_timeout_ms:4;     /*!< I2C device timeout */
            uint32_t rmsb:8;                /*!< Not used */
        } i2c_device;
        /** 1-Wire device configuration */
        struct {
            uint64_t rom_code;              /*!< 1-Wire ROM code */
            struct {
                uint32_t data_gpio:7;       /*!< 1-Wire data GPIO */
                uint32_t reserved:25;       /*!< Not used */
            };
        } ow_device;
        struct {
            uint32_t miso_gpio:7;
            uint32_t mosi_gpio:7;
            uint32_t sclk_gpio:7;
            uint32_t cs_gpio:7;
            uint32_t dummy_bits:4;
            uint32_t cmd_bits:5;
            uint32_t addr_bits:7;            
            uint32_t mode:2;
            uint32_t pretrans:5;
            uint32_t postrans:5;
            uint32_t input_delay:8;
            uint32_t clock_speed;
            uint32_t flags;
        } spi_device;
    };
} cbus_device_config_t;

/* New strutures */

typedef struct cbus_device_transaction {
    uint32_t device_id;     /*!< Target device ID */
    uint32_t device_cmd;    /*!< Command send to device */
    uint64_t reg_address;   /*!< Device register/memory address */
} cbus_device_transaction_t;

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

typedef struct {
    cbus_event_command_t cmd;
    cbus_device_transaction_t transaction;
    uint32_t event_it;      /*!< User event ID */
    uint8_t payload[128];   /*!< Command payload */
} cbus_event_data_t;

/* END New strutures */

//typedef struct cbus_driver *cbus_driver_t;

esp_event_loop_handle_t cbus_initialize(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_DRIVER_H_ */