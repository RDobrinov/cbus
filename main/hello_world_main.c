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

#include "msos_sensors.h"

uint32_t my_device_id;
cbus_cmd_t ow_cmd;
esp_event_loop_handle_t *ow_event_loop, *i2c_event_loop, *spi_event_loop;
const char *evtag = "evt_data";
const char *ertag = "evt_error";

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

static void spi_data_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    //return; //BLOCKED
    cbus_event_data_t *event = (cbus_event_data_t *)event_data;
    if(event->bus_type != CBUS_BUS_SPI) return;
    //ESP_LOGW(evtag, "Sender %u,%u [spi_data_event_handler %08lX] data received for device_id %08lX", 
    //        (uint8_t)event->sender_id, (uint8_t)event->bus_type, (uint32_t)event->event_id, event->transaction.device_id);
    
    if(event->cmd.command == CBUSCMD_ATTACH) {
        event->cmd.command = CBUSCMD_INFO;
        esp_event_post_to(*spi_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        event->cmd = (cbus_event_command_t) {
            .command = CBUSCMD_WRITE,
            .data_type = CBUSDATA_UINT64,
            .inDataLen = 1,
            .outDataLen = 0
        };
        event->transaction.reg_address = 0x80;
        event->payload[0] = 0b10110001;
        esp_event_post_to(*spi_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        vTaskDelay(100);
        event->cmd.command = CBUSCMD_READ;
        event->transaction.reg_address = 0x00;
        esp_event_post_to(*spi_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->cmd.command = CBUSCMD_STATS;
        esp_event_post_to(*spi_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        return;
    }
    
    if(event->cmd.command == CBUSCMD_READ) {
        ESP_LOGI("SPI", "%016llX", *((uint64_t *)event->payload));
        return;
    }
    if(event->cmd.command == CBUSCMD_INFO) {
        ESP_LOGW(evtag,"SPI [%08lX] - %s", event->transaction.device_id, event->payload);
    }

    if(event->cmd.command == CBUSCMD_STATS) {
        printf("%s\n", *(char **)event->payload);
        free(*(char **)event->payload);
    }
}

static void i2c_data_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    //return; //BLOCKED
    cbus_event_data_t *event = (cbus_event_data_t *)event_data;
    if(event->bus_type != CBUS_BUS_I2C) return;
    //ESP_LOGW(evtag, "Sender %u,%u [i2c_data_event_handler %08lX] data received for device_id %08lX", 
    //        (uint8_t)event->sender_id, (uint8_t)event->bus_type, (uint32_t)event->event_id, event->transaction.device_id);
    if(event->cmd.command == CBUSCMD_ATTACH) {
        event->cmd.command = CBUSCMD_INFO;
        esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        event->cmd = (cbus_event_command_t) {
            .command = CBUSCMD_READ,
            .data_type = CBUSDATA_UINT8,
            .inDataLen = 0,
            .outDataLen = 0
        };
        event->transaction.reg_address = 0xD0;
        esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->transaction.reg_address = 0xF2;
        event->cmd.command = CBUSCMD_WRITE;
        event->cmd.inDataLen = 1;
        event->payload[0] = 0b00000011;
        esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->transaction.reg_address = 0xF4;
        event->cmd.command = CBUSCMD_WRITE;
        event->cmd.inDataLen = 1;
        event->payload[0] = 0b10010011;
        esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->cmd.command = CBUSCMD_READ;
        esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->transaction.reg_address = 0xF2;
        esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->transaction.reg_address = 0xF7;
        event->cmd.data_type = CBUSDATA_UINT64;
        esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->cmd.command = CBUSCMD_STATS;
        esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        return;
    }
    if(event->cmd.command == CBUSCMD_READ) {
        ESP_LOGI("i2c", "%016llX", *((uint64_t *)event->payload));
        return;
    }
    if(event->cmd.command == CBUSCMD_INFO) {
        ESP_LOGW(evtag,"[%08lX] - %s", event->transaction.device_id, event->payload);
    }
    if(event->cmd.command == CBUSCMD_STATS) {
        printf("%s\n", *(char **)event->payload);
        free(*(char **)event->payload);
    }
    return;
}

static void ow_data_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    //return;     //Blocked
    cbus_event_data_t *event = (cbus_event_data_t *)event_data;
    if(event->bus_type != CBUS_BUS_1WIRE) return;
    //ESP_LOGW(evtag, "ow_data_event_handler [%08lX] data received for device_id %08lX", (uint32_t)event->event_id, event->transaction.device_id);
    if(event->cmd.command == CBUSCMD_ATTACH) {
        my_device_id = event->transaction.device_id;
        event->cmd.command = CBUSCMD_INFO;
        event->event_id++;
        esp_event_post_to(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        /**/
        event->cmd = (cbus_event_command_t) {
            .command = CBUSCMD_RESET
        };
        event->event_id++;
        esp_event_post_to(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        event->cmd = (cbus_event_command_t) {
            .command = CBUSCMD_WRITE,
            .data_type = CBUSDATA_BLOB,
            .inDataLen = 3,
            .outDataLen = 9
        };
        *((uint32_t *)(event->payload)) = 0x5F2153LU;
        event->transaction.device_cmd = 0x4E;      //WRITE_STRACHPAD
        esp_event_post_to(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->transaction.device_cmd = 0x44;      //CONVERT_T
        event->cmd.inDataLen = 0;
        esp_event_post_to(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        vTaskDelay(100);

        event->cmd.command = CBUSCMD_READ;
        /*event->cmd = (cbus_event_command_t) {
            .command = CBUSCMD_READ,
            .data_type = CBUSDATA_UINT64,
            .inDataLen = 0,
            .outDataLen = 0
        };*/
        event->transaction.device_cmd = 0xBE;      //READ_STRACHPAD
        event->event_id = 0x55;
        esp_event_post_to(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        event->cmd.command = CBUSCMD_STATS;
        esp_event_post_to(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        event->cmd.command = CBUSCMD_SCAN;
        esp_event_post_to(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        return;
    }
    if(event->cmd.command == CBUSCMD_INFO) ESP_LOGW(evtag,"[%08lX] - %s", event->transaction.device_id, event->payload);
    if(event->cmd.command == CBUSCMD_STATS) {
        printf("%s\n", *(char **)event->payload);
        free(*(char **)event->payload);
    }
    if(event->cmd.command == CBUSCMD_SCAN) {
        ESP_LOGI(evtag, "Found %lu device(s)", event->transaction.device_id);
        for(int i = 0; i<event->transaction.device_id; i++) ESP_LOGI(evtag, "%d - ROM %016llX", i, ((uint64_t *)event->payload)[i]);
    }
    return;
}

static void error_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ESP_LOGE(ertag, "Error %04X received for event_id %08lX", ((cbus_event_data_t *)event_data)->cmd.status, (uint32_t)((cbus_event_data_t *)event_data)->event_id);
    return;
}

void app_main(void)
{
    printf("Hello world!\n");

    msos_sensor_interface_t main = ds18b20_get_interface();
    msos_sensor_api_t *new_sensor;
    ds18b20_config_t conf;
    conf.data_gpio = GPIO_NUM_32;
    conf.resolution = DS18B20_RESOLUTION_12_BIT;
    conf.use_crc = true;
    conf.rom_code = 0x28FF8CA7741604DBLLU;
    //printf("main %p\n", main);
    main->_add(&conf, &new_sensor);
    printf("new_sensor %p, %p\n", new_sensor, *(new_sensor));
    //(*new_sensor)->_ready(new_sensor);
    (*new_sensor)->_init(new_sensor);
    while(!((*new_sensor)->_ready(new_sensor))) {
        vTaskDelay(10);
    }
    (*new_sensor)->_measure(new_sensor);
    while(!((*new_sensor)->_ready(new_sensor))) {
        vTaskDelay(10);
    }
    msos_sensor_magnitude_t magnitude;
    size_t len = 1;
    (*new_sensor)->_magnitudes(new_sensor, &magnitude, &len);
    printf("magnitude %d, %d, %d\n", magnitude.magnitude_index, magnitude.magnitude, magnitude.decimals);
    float value;
    (*new_sensor)->_get(new_sensor, &magnitude, &value);
    printf("Temperature %6.3f\n", value);

    return; //BLOCKED
    cbus_event_data_t ow_evt;
    uint32_t sender_id;
    //ow_event_loop = (esp_event_loop_handle_t *)calloc(1, sizeof(esp_event_loop_handle_t));
    //i2c_event_loop = (esp_event_loop_handle_t *)calloc(1, sizeof(esp_event_loop_handle_t));
    if(ESP_OK != cbus_register(&ow_event_loop, &sender_id)) return;
    ow_evt.sender_id = (uint8_t)(0xFF & sender_id);
    //cbus_event_loop = cbus_initialize();


    esp_event_handler_instance_register_with(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_DATA, ow_data_event_handler, NULL, NULL );
    esp_event_handler_instance_register_with(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_ERROR, error_event_handler, NULL, NULL );

    /* Test 1-Wire */
    ow_evt.cmd.command = CBUSCMD_ATTACH;
    
    ESP_LOGI("main", "Sender ID:0x%02X", ow_evt.sender_id);
    *((cbus_device_config_t *)ow_evt.payload) = (cbus_device_config_t){
        .bus_type = CBUS_BUS_1WIRE,
        .ow_device = {
            .data_gpio = GPIO_NUM_32,
            .rom_code = 0x28FF8CA7741604DBLLU,
            //.rom_code = 0xDB041674A78CFF28LLU,
            .reserved = 0,
            .cmd_bytes = 1,
            .addr_bytes = 0,
            .crc_check = true
        }
    };
    //esp_event_post_to(cbus_event_loop, CBUS_EVENT, CBUSCMD_ATTACH, &evt, sizeof(cbus_event_data_t), 1);
    esp_event_post_to(*ow_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &ow_evt, sizeof(cbus_event_data_t), 1);
    //ESP_LOGE("exit", "");
    /* Test END */
    /* Test I2C */
    cbus_event_data_t i2c_evt;

    if(ESP_OK != cbus_register(&i2c_event_loop, &sender_id)) return;
    i2c_evt.sender_id = (uint8_t)(0xFF & sender_id);
    ESP_LOGI("main", "Sender ID:0x%02X %p %p", i2c_evt.sender_id, ow_event_loop, i2c_event_loop);
    esp_event_handler_instance_register_with(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_DATA, i2c_data_event_handler, NULL, NULL );

    i2c_evt.cmd.command = CBUSCMD_ATTACH;
    i2c_evt.event_id = 0;
    *((cbus_device_config_t *)i2c_evt.payload) = (cbus_device_config_t){
        .bus_type = CBUS_BUS_I2C,
        .i2c_device = {
            .scl_gpio = GPIO_NUM_26,
            .sda_gpio = GPIO_NUM_18,
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .scl_speed_hz = 400000U,
            .xfer_timeout_ms = 10,
            .device_address = 0x76,
            .disable_ack_check = false,
            .addr_bytes = 1,
            .cmd_bytes = 0
        }
    };
    esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &i2c_evt, sizeof(cbus_event_data_t), 1);
    //((cbus_device_config_t *)i2c_evt.payload)->i2c_device.scl_gpio = GPIO_NUM_22;
    //((cbus_device_config_t *)i2c_evt.payload)->i2c_device.sda_gpio = GPIO_NUM_21;
    //esp_event_post_to(*i2c_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &i2c_evt, sizeof(cbus_event_data_t), 1);
    /* Test END*/

    /* Test SPI */
    cbus_event_data_t spi_evt;
    if(ESP_OK != cbus_register(&spi_event_loop, &spi_evt.val)) return;
    //spi_evt.sender_id = (uint8_t)(0xFF & sender_id);
    ESP_LOGI("main", "Sender ID:0x%02X %p %p %p", spi_evt.sender_id, ow_event_loop, i2c_event_loop, spi_event_loop);
    esp_event_handler_instance_register_with(*spi_event_loop, CBUS_EVENT, CBUS_EVENT_DATA, spi_data_event_handler, NULL, NULL );

    spi_evt.cmd.command = CBUSCMD_ATTACH;
    *((cbus_device_config_t *)spi_evt.payload) = (cbus_device_config_t){
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
    esp_event_post_to(*spi_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &spi_evt, sizeof(cbus_event_data_t), 1);
    /* Test END*/
    return;
    
    fflush(stdout);
}
