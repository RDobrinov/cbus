/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "cbus_driver.h"

void app_main(void)
{
    printf("Hello world!\n");
    cbus_driver_t *cbus = (cbus_driver_t *)i2cbus_get_bus();
    printf("%p, cbus_i2c->attach:%p cbus_i2c->deattach%p [%04lx][%ld]\n", cbus, cbus->attach, cbus->deattach, cbus->attach(0), cbus->deattach(123));
    fflush(stdout);
}
