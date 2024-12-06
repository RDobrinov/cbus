/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CBUS_1WIRE_DRIVER_H_
#define _CBUS_1WIRE_DRIVER_H_

#include <inttypes.h>
#include "idf_gpio_driver.h"
#include "esp_event.h"

#include "cbus_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create new 1-Wire bus handle
 *
 * @return
 *      - Created handle
 */
void *ow_get_bus(void);

/**
 * Not used
 */
void owbus_dump_devices(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_1WIRE_DRIVER_H_ */