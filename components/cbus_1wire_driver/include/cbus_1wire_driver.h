#ifndef _CBUS_1WIRE_DRIVER_H
#define _CBUS_1WIRE_DRIVER_H

#include <inttypes.h>
#include "esp_event.h"
//#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union
{
    struct
    {
        uint8_t family[1];         ///< family identifier (1 byte, LSB - read/write first)
        uint8_t serial_number[6];  ///< serial number (6 bytes)
        uint8_t crc[1];            ///< CRC check byte (1 byte, MSB - read/write last)
    };
    uint8_t raw_address[8];              ///< Provides raw byte access

} OneWire_ROMCode;

uint16_t cbus_1wire_init(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_1WIRE_DRIVER_H */