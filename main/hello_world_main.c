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

uint64_t swap_uint64( uint64_t val )
{
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL ) | ((val >> 8) & 0x00FF00FF00FF00FFULL );
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL ) | ((val >> 16) & 0x0000FFFF0000FFFFULL );
    return (val << 32) | (val >> 32);
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

    cbus_driver_t *owbus = (cbus_driver_t *)ow_get_bus();
    cbus_device_config_t ow_device_config = {
        .bus_type = CBUS_BUS_1WIRE,
        .ow_device = {
            .data_gpio = GPIO_NUM_22,
            .rom_code = {
                .raw_address = {0xDB, 0x04, 0x16, 0x74, 0xA7, 0x8C, 0xFF, 0x28}
            },
            .reserved = 0
        }
    };

    cbus_common_id_t retcode = owbus->attach(&ow_device_config);
    printf("cbus_common_id_t %02X / %08lX\n", retcode.error, retcode.id);

    ow_device_config.ow_device.data_gpio = GPIO_NUM_18;
    retcode = owbus->attach(&ow_device_config);
    uint32_t deattach = retcode.id;
    printf("cbus_common_id_t %02X / %08lX\n", retcode.error, retcode.id);

    ow_device_config.ow_device.data_gpio = GPIO_NUM_26;
    retcode = owbus->attach(&ow_device_config);
    printf("cbus_common_id_t %02X / %08lX\n", retcode.error, retcode.id);
    owbus_dump_devices();
    retcode = owbus->deattach(deattach);
    owbus_dump_devices();
    ow_device_config.ow_device.data_gpio = GPIO_NUM_21;
    retcode = owbus->attach(&ow_device_config);
    printf("cbus_common_id_t %02X / %08lX\n", retcode.error, retcode.id);
    owbus_dump_devices();
    ow_device_config.ow_device.data_gpio = GPIO_NUM_21;
    retcode = owbus->attach(&ow_device_config);
    printf("cbus_common_id_t %02X / %08lX\n", retcode.error, retcode.id);

    ow_device_config.ow_device.rom_code.serial_number[6] = 0x55;
    retcode = owbus->attach(&ow_device_config);
    printf("cbus_common_id_t %02X / %08lX\n", retcode.error, retcode.id);
    owbus_dump_devices();
    //test_channles();
    
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
    //uint64_t ROMCode = 0xDB041674A78CFF28LLU;
    //printf("swaped %016llX\n", swap_uint64(ROMCode));
    fflush(stdout);
}
