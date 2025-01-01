/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MSOS_DS18B20_H_
#define _MSOS_DS18B20_H_

#include <inttypes.h>
#include "msos_sensor_base.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create new 1-Wire bus handle
 *
 * @return
 *      - Created handle
 */

typedef enum {
    DS18B20_RESOLUTION_9_BIT = 0,   ///< 9-bit resolution, LSB bits 2,1,0 undefined
    DS18B20_RESOLUTION_10_BIT,      ///< 10-bit resolution, LSB bits 1,0 undefined
    DS18B20_RESOLUTION_11_BIT,      ///< 11-bit resolution, LSB bit 0 undefined
    DS18B20_RESOLUTION_12_BIT,      ///< 12-bit resolution (default)
} ds18b20_resolution_t;

typedef struct {
    struct {
        uint32_t resolution:2;
        uint32_t use_crc:1;
        uint32_t data_gpio:7;
        uint32_t reserved:22;
    };
    uint64_t rom_code;
} ds18b20_config_t;

msos_sensor_interface_t ds18b20_get_interface(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_1WIRE_DRIVER_H_ */
