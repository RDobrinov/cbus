/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CBUS_I2C_DRIVER_H_
#define _CBUS_I2C_DRIVER_H_

#include <inttypes.h>
#include "idf_gpio_driver.h"
#include "driver/i2c_master.h"
#include "../i2c_private.h"

#include "cbus_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create new i2c bus handle
 *
 * @return
 *      - Created handle
 */
void *i2cbus_get_bus(void);

/**
 * Not used
 */
void i2cbus_dump_devices(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_I2C_DRIVER_H_ */
