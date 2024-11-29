#ifndef _ONEWIRE_ROMCODE_H_
#define _ONEWIRE_ROMCODE_H_

#include <inttypes.h>

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

#ifdef __cplusplus
}
#endif 
#endif /* _ONEWIRE_ROMCODE_H_ */