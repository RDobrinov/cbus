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
#include "onewire/onewire_crc.h"
#include "onewire/onewire_cmd.h"
#include "cbus_driver.h"
#include "driver/spi_master.h"

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

    uint8_t scanbuf[128];
    cbus_driver_t *spidrv = (cbus_driver_t *)spi_get_bus();
    cbus_common_id_t spidev = spidrv->deattach(0x11E3868CUL);
    ESP_LOGW("spidrv", "ID:%08lX [%04X]\n", spidev.id, spidev.error);
    cbus_device_config_t spi_dev_conf = {
        .bus_type = CBUS_BUS_SPI,
        .spi_device = {
            .mosi_gpio = GPIO_NUM_13,
            .miso_gpio = GPIO_NUM_12,
            .sclk_gpio = GPIO_NUM_14,
            .cs_gpio = GPIO_NUM_15,
            .addr_bits = 8,
            .cmd_bits = 0,
            .dummy_bits = 0,
            .mode = 1,
            .pretrans = 8,
            .clock_speed = 2500000,
            .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY
        }
    };

    spidev = spidrv->attach(&spi_dev_conf);
    //ESP_LOGW("spidrv", "ID:%08lX [%04X]\n", spidev.id, spidev.error);
    spidev = spidrv->desc(spidev.id, scanbuf, 35);
    printf("desc: %s\n", scanbuf);
    cbus_common_cmd_t spi_read_all = {
        .command = CBUSCMD_READ,
        .device_id = spidev.id,
        .inDataLen = 0,
        .outDataLen = 8,
        .data = scanbuf
    };
    ((spibus_cmdaddr_t *)(spi_read_all.data))->address = 0x00;

    spidev = spidrv->execute(&spi_read_all);
    //ESP_LOGW("spidrv", "ID:%08lX [%04X]\n", spidev.id, spidev.error);
    //hexdump(scanbuf, 16);

    cbus_common_cmd_t spi_write_conf = {
        .command = CBUSCMD_WRITE,
        .device_id = spidev.id,
        .inDataLen = 1,
        .outDataLen = 0,
        .data = scanbuf
    };
    scanbuf[sizeof(spibus_cmdaddr_t)] = 0b10110001;
    ((spibus_cmdaddr_t *)(spi_write_conf.data))->address = 0x80;
    ((spibus_cmdaddr_t *)(spi_write_conf.data))->command = 0x4554;
    spidev = spidrv->execute(&spi_write_conf);
    ESP_LOGW("spidrv", "ID:%08lX [%04X]\n", spidev.id, spidev.error);
    vTaskDelay(100);
    //scanbuf[sizeof(spibus_cmdaddr_t)] = 0b10110001;
    //spidev = spidrv->execute(&spi_write_conf);
    //ESP_LOGW("spidrv", "ID:%08lX [%04X]\n", spidev.id, spidev.error);
    //vTaskDelay(100);
    ((spibus_cmdaddr_t *)(spi_read_all.data))->address = 0x00;
    spidev = spidrv->execute(&spi_read_all);
    //ESP_LOGW("spidrv", "ID:%08lX [%04X]\n", spidev.id, spidev.error);
    hexdump(scanbuf, 16);
    return;
    //Blocked
    cbus_common_id_t desc = cbus->attach(&dev_conf);

    //uint8_t scanbuf[128];
    cbus->desc(desc.id, scanbuf, 35);
    printf("%s\n", scanbuf);

    cbus_driver_t *owbus = (cbus_driver_t *)ow_get_bus();
    cbus_device_config_t ow_device_config = {
        .bus_type = CBUS_BUS_1WIRE,
        .ow_device = {
            .data_gpio = GPIO_NUM_32,
            .rom_code = 0x28FF8CA7741604DBLLU,
            //.rom_code = {
                //.raw_address = {0xDB, 0x04, 0x16, 0x74, 0xA7, 0x8C, 0xFF, 0x28}
                //.raw_address = {0x28, 0xFF, 0x8C, 0xA7, 0x74, 0x16, 0x04, 0xDB}
            //},
            .reserved = 0
        }
    };

    //uint8_t scanbuf[128];

    cbus_common_cmd_t scancmd = (cbus_common_cmd_t) {
        .command = CBUSCMD_SCAN,
        .device_id = 0,
        .inDataLen = 0,
        .outDataLen = 0,
        .data = scanbuf
    };
    
    *((gpio_num_t *)scanbuf) = GPIO_NUM_32;
    cbus_common_id_t codes = owbus->execute(&scancmd);
    //hexdump(scancmd.data, 8);
    printf("Return code 0x%02X - Device(s): %lu %016llX\n", codes.error, codes.id, *((uint64_t *)scancmd.data));

    //*((uint64_t *)ow_device_config.ow_device.rom_code.raw_address) = *((uint64_t *)scancmd.data);
    
    cbus_common_id_t retcode = owbus->attach(&ow_device_config);
    owbus->desc(retcode.id, scanbuf, 35);
    printf("\n%s\n", scanbuf);

    ow_device_config.ow_device.data_gpio = GPIO_NUM_33;
    owbus->attach(&ow_device_config);

    ow_device_config.ow_device.data_gpio = GPIO_NUM_22;
    owbus->attach(&ow_device_config);
    //printf("cbus_common_id_t %02X / %08lX\n", retcode.error, retcode.id);
    /*
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
    printf("cbus_common_id_t %02X / %08lX\n", retcode.error, retcode.id); */

    //owbus_dump_devices();

    uint8_t buf[128];

    cbus_common_cmd_t cmd = (cbus_common_cmd_t) {
        .command = CBUSCMD_RESET,
        .device_id = retcode.id,
        .inDataLen = 4,
        .outDataLen = 9,    //Scratchpad
        .data = buf
    };
    owbus->execute(&cmd);
    cmd.data[0] = 0x4E;
    cmd.data[1] = 0x53;
    cmd.data[2] = 0x21;
    cmd.data[3] = 0x5F;
    cmd.command = CBUSCMD_WRITE;
    owbus->execute(&cmd);

    cmd.command = CBUSCMD_RESET;
    owbus->execute(&cmd);

    cmd.data[0] = 0x44;
    cmd.inDataLen = 1;
    cmd.command = CBUSCMD_WRITE;
    owbus->execute(&cmd);

    vTaskDelay(pdMS_TO_TICKS(800));

    cmd.command = CBUSCMD_RESET;
    owbus->execute(&cmd);

    cmd.command = CBUSCMD_READ;
    cmd.data[0] = 0xBE;
    cmd.inDataLen = 1;
    owbus->execute(&cmd);

    hexdump(cmd.data, 9);
    printf("0x%02X, 0x%02X\n", onewire_crc8(0, cmd.data, 8), onewire_crc8(0, cmd.data, 9));

    cmd.command = CBUSCMD_SCAN;
    retcode = owbus->execute(&cmd);
    printf("Return code 0x%02X - Device(s): %lu %016llX\n", retcode.error, retcode.id, *((uint64_t *)cmd.data));

    //**/owbus->getaddress(&cmd);
    //**/uint64_t rom = *((uint64_t *)cmd.data);
    //**/cmd.data[0] = ONEWIRE_CMD_MATCH_ROM;
    ///**/cmd.data[9] = 0x4E;
    //**/*((uint64_t *)(&cmd.data[1])) = rom;
    /*owbus->execute(&cmd);

    cmd.data[10] = 0x52;
    cmd.data[11] = 0x35;
    cmd.data[12] = 0x3F;
    cmd.inDataLen = 13;

    cmd.command = CBUSCMD_WRITE;
    hexdump(cmd.data, 10);
    owbus->execute(&cmd);

    cmd.data[9] = 0xBE;
    cmd.inDataLen = 10;
    cmd.command = CBUSCMD_RESET;
    owbus->execute(&cmd);
    //hexdump(cmd.data, cmd.inDataLen);
    cmd.command = CBUSCMD_WRITE;
    owbus->execute(&cmd);

    cmd.command = CBUSCMD_READ;
    owbus->execute(&cmd);
    */

    //payload->data[9] = payload->data[0];
    //payload->data[0] = ONEWIRE_CMD_MATCH_ROM;
    //memcpy(&payload->data[1], &(device->address), 8);

    //#define DS18B20_CMD_CONVERT_TEMP      0x44
    //#define DS18B20_CMD_WRITE_SCRATCHPAD  0x4E
    //#define DS18B20_CMD_READ_SCRATCHPAD   0xBE
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
