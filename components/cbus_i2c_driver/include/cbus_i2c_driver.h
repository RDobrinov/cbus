#ifndef _CBUS_I2C_DRIVER_H
#define _CBUS_I2C_DRIVER_H

#include <inttypes.h>
#include "idf_gpio_driver.h"
#include "driver/i2c_master.h"
#include "../i2c_private.h"

#include "cbus_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

void *i2cbus_get_bus(void);
//cbus_driver_t *i2cbus_get_bus(void);

void i2cbus_dump_devices(void);

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_I2C_DRIVER_H */
