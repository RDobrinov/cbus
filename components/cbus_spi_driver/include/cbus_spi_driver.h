/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CBUS_SPI_DRIVER_H_
#define _CBUS_SPI_DRIVER_H_

#include <inttypes.h>
#include "idf_gpio_driver.h"
#include "driver/spi_master.h"
//#include "esp_event.h"

#include "cbus_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t command;
    uint64_t address;
} spibus_cmdaddr_t;
/**
 * @brief Create new SPI bus handle
 *
 * @return
 *      - Created handle
 */
void *spi_get_bus(void);

/**
 * Not used
 */
void spibus_dump_devices(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_SPI_DRIVER_H_ */
