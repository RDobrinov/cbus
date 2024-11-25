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

#include "esp_log.h"

void hexdump(const uint8_t *buf, size_t len) {
    if( !len ) return;
    ESP_LOGI("hexdump", "%p", buf);
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

void app_main(void)
{
    printf("Hello world!\n");
    cbus_driver_t *cbus = (cbus_driver_t *)i2cbus_get_bus();
    cbus_device_config_t dev_conf = (cbus_device_config_t) {
        .bus_type = CBUS_BUS_I2C,
        .i2c_device = { 
            .scl_gpio = GPIO_NUM_26,
            .sda_gpio = GPIO_NUM_18,
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .scl_speed_hz = 400000U,
            .xfer_timeout_ms = 10,
            .device_address = 0x76,
            .disable_ack_check = false
        }
    };
    printf("sizeof(cbus_device_config_t) %d\n", sizeof(cbus_device_config_t));

    test_channles();
    
    /*uint32_t id_i2c0d0 = cbus->attach(&dev_conf).id;

    dev_conf.i2c_device.scl_gpio = GPIO_NUM_22;
    dev_conf.i2c_device.sda_gpio = GPIO_NUM_21;
    uint32_t id_i2c1d0 = cbus->attach(&dev_conf).id;
    
    i2cbus_dump_devices();
    uint8_t buf[127];

    cbus_common_cmd_t cmd = (cbus_common_cmd_t) {
        .command = CBUSCMD_RW,
        .device_id = id_i2c0d0,
        .inDataLen = 1,
        .outDataLen = 15,
        .data = buf
    };
    buf[0] = 0xE1;
    cbus->rw(&cmd);
    hexdump(buf, 15);
    cmd.device_id = id_i2c1d0;
    buf[0] = 0xE1;
    cbus->rw(&cmd);
    hexdump(buf, 15);

    cmd.outDataLen = 25;
    buf[0] = 0x88;
    cbus->rw(&cmd);
    hexdump(buf, 25);

    cmd.device_id = id_i2c0d0;
    buf[0] = 0x88;
    cbus->rw(&cmd);
    hexdump(buf, 25);
   
    i2cbus_dump_devices(); */
    //printf("%p, cbus_i2c->attach:%p cbus_i2c->deattach:%p [%04lx][%ld]\n", cbus, cbus->attach, cbus->deattach, cbus->attach(0), cbus->deattach(123));
    fflush(stdout);
}
