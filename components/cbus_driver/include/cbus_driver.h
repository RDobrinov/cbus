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
    CBUSCMD_ATTACH,     /*!< Attach device to bus */
    CBUSCMD_DEATTACH    /*!< Deattach device from bus */
} cbus_command_t;

/**
 * Type of driver data types
 */
typedef enum {
    BUSDATA_BLOB = 0,   /*!< BLOB type */
    BUSDATA_UINT8,      /*!< BYTE type */
    BUSDATA_UINT16,     /*!< WORD type - 2 bytes */
    BUSDATA_UINT32,     /*!< Double word type - 4 bytes */
    BUSDATA_UINT64,     /*!< Quad word type - 8 bytes */
    BUSDATA_MAX         /*!< MAX */
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
        } spi_device;
    };
} cbus_device_config_t;

/** Common bus event data structure */
typedef struct {
    struct {
        uint32_t command:4;     /*!< Command to execute */
        uint32_t data_type:3;   /*!< Data type */
        uint32_t inDataLen:7;   /*!< Data length send to device */
        uint32_t outDataLen:7;  /*!< Data length received from device */
        uint32_t status:4;      /*!< Bus status code */
        uint32_t reserved:7;    /*!< Not used */
    };
    uint32_t device_id;     /*!< Target device ID */
    uint32_t event_it;      /*!< User event ID */
    uint8_t payload[128];   /*!< Command payload */
} cbus_event_data_t;

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
} cbus_common_id_t;

/** Common bus command structure */
typedef struct {
    struct {
        uint32_t command:4;     /*!< Command code */
        uint32_t inDataLen:7;   /*!< Data length send to device */
        uint32_t outDataLen:7;  /*!< Data length received from device */
        uint32_t reserved:14;   /*!< Not used */
    };
    uint32_t device_id; /*!< Target device id */
    uint8_t *data;      /*!< Pointer to data buffer */
} cbus_common_cmd_t;

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
    cbus_common_id_t (*attach)(cbus_device_config_t *payload);

    /**
     * @brief Deattach device from bus driver
     *
     * @param[in] id Device ID
     * @return
     *      - Common bus id structure
     */
    cbus_common_id_t (*deattach)(uint32_t id);

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
    cbus_common_id_t (*desc)(uint32_t id, uint8_t *desc, size_t len);

    /**
     * @brief Execute command
     *
     * @param[in] payload Pointer to cbus commom command structure
     * 
     * @return
     *      - Common bus id structure
     */
    cbus_common_id_t (*execute)(cbus_common_cmd_t *payload);
    //void * (*deinit_bus)(void *payload);
} cbus_driver_t;

//typedef struct cbus_driver *cbus_driver_t;

//esp_event_loop_handle_t *cbus_initialize(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_DRIVER_H_ */