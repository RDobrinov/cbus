#ifndef _CBUS_DRIVER_H_
#define _CBUS_DRIVER_H_

#include <inttypes.h>
#include "cbus_1wire_driver.h"
#include "cbus_i2c_driver.h"
#include "esp_event.h"
//#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cbus_bus_types {
    CBUS_BUS_I2C,
    CBUS_BUS_SPI,
    CBUS_BUS_1WIRE
} cbus_bus_t;

typedef enum {
    CBUSCMD_READ,
    CBUSCMD_WRITE,
    CBUSCMD_RW,
    CBUSCMD_PROBE,
    CBUSCMD_ATTACH,
    CBUSCMD_DEATTACH
} cbus_command_t;

typedef enum {
    BUSDATA_BLOB = 0,
    BUSDATA_UINT8,
    BUSDATA_UINT16,
    BUSDATA_UINT32,
    BUSDATA_UINT64,
    BUSDATA_MAX
} cbus_data_t;

typedef enum {
    CBUS_OK,
    CBUS_ERR_NOT_FOUND,
    CBUS_ERR_TIMEOUT,
    CBUS_ERR_BAD_ARGS,
    CBUS_ERR_UNKNOWN,
    CBUS_ERR_NO_MEM,
    CBUS_ERR_NO_MORE_BUSES,
    CBUS_ERR_PIN_IN_USE,
    CBUS_ERR_DEVICE_EXIST,
    CBUS_ERR_DEVICE_NOT_FOUND,
    CBUS_ERR_DEVICE_NOT_ACK
} cbus_opcodes_t;

typedef enum {
    CBUS_EVENT_EXEC,
    CBUS_EVENT_DATA,
    CBUS_EVENT_ERROR
} i2cdrv_resp_event_t;

ESP_EVENT_DECLARE_BASE(CBUS_EVENT);

typedef struct cbus_device_config {
    cbus_bus_t bus_type;
    union {
        struct {
            uint32_t dev_addr_length:1;
            uint32_t device_address:10;
            uint32_t scl_gpio:7;
            uint32_t sda_gpio:7;
            uint32_t disable_ack_check:1;
            uint32_t rlsb:6;
            uint32_t scl_speed_hz:20;
            uint32_t xfer_timeout_ms:4;
            uint32_t rmsb:8;
        } i2c_device;
        struct {
            OneWire_ROMCode rom_code;
            struct {
                uint32_t data_gpio:7;
                uint32_t reserved:25;
            };
        } ow_device;
    };
} cbus_device_config_t;

typedef struct {
    struct {
        uint32_t command:3;
        uint32_t data_type:3;
        uint32_t inDataLen:7;
        uint32_t outDataLen:7;
        uint32_t status:4;
        uint32_t reserved:8;
    };
    uint32_t device_id;
    uint32_t event_it;
    uint8_t payload[127];
} cbus_event_data_t;

typedef struct {
    union {
        struct {
            uint32_t error:4;
            uint32_t reserved:28;
        };
        uint32_t val;
    };
    uint32_t id;
} cbus_common_id_t;

typedef struct {
    struct {
        uint32_t command:3;
        uint32_t inDataLen:7;
        uint32_t outDataLen:7;
        uint32_t reserved:15;
    };
    uint32_t device_id;
    uint8_t *data;
} cbus_common_cmd_t;

typedef struct cbus_driver {
    cbus_common_id_t (*attach)(cbus_device_config_t *payload);
    cbus_common_id_t (*deattach)(uint32_t id);
    void * (*deinit_bus)(void *payload);
    cbus_common_id_t (*read)(cbus_common_cmd_t *payload);
    cbus_common_id_t (*write)(cbus_common_cmd_t *payload);
    cbus_common_id_t (*rw)(cbus_common_cmd_t *payload);
} cbus_driver_t;

//typedef struct cbus_driver *cbus_driver_t;

esp_event_loop_handle_t *cbus_initialize(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_DRIVER_H_ */