#ifndef _CBUS_1WIRE_DRIVER_H
#define _CBUS_1WIRE_DRIVER_H

#include <inttypes.h>
#include "idf_gpio_driver.h"
#include "esp_event.h"
//#include "driver/i2c_master.h"

#include "cbus_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

void *ow_get_bus(void);
void owbus_dump_devices(void);
uint16_t cbus_1wire_init(void);

void test_channles();

#ifdef __cplusplus
}
#endif 
#endif /* _CBUS_1WIRE_DRIVER_H */