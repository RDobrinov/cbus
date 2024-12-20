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

/*---*/




/*---*/
uint32_t my_device_id;
esp_event_loop_handle_t cbus_event_loop;
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

static void data_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    cbus_event_data_t *event = (cbus_event_data_t *)event_data;
    ESP_LOGW(evtag, "data received for device_id %08lX", event->transaction.device_id);
    if(event->cmd.command == CBUSCMD_ATTACH) {
        my_device_id = event->transaction.device_id;
        event->cmd.command = CBUSCMD_INFO;
        esp_event_post_to(cbus_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        /**/
        event->cmd.inDataLen = 0;
        event->cmd.command = CBUSCMD_WRITE;
        esp_event_post_to(cbus_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->cmd.command = CBUSCMD_RW;
        esp_event_post_to(cbus_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->cmd.inDataLen = 2;
        event->cmd.data_type = CBUSDATA_BLOB;
        event->cmd.outDataLen = 0;
        esp_event_post_to(cbus_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);

        event->cmd.command = CBUSCMD_READ;
        event->cmd.data_type = CBUSDATA_UINT16;
        esp_event_post_to(cbus_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, event_data, sizeof(cbus_event_data_t), 1);
        return;
    }
    if(event->cmd.command == CBUSCMD_INFO) ESP_LOGW(evtag,"[%08lX] - %s", event->transaction.device_id, event->payload);
    return;
}

static void error_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    ESP_LOGE(ertag, "Error %04X received", ((cbus_event_data_t *)event_data)->cmd.status);
    return;
}

void app_main(void)
{
    printf("Hello world!\n");
    cbus_event_loop = cbus_initialize();

    esp_event_handler_instance_register_with(cbus_event_loop, CBUS_EVENT, CBUS_EVENT_DATA, data_event_handler, NULL, NULL );
    esp_event_handler_instance_register_with(cbus_event_loop, CBUS_EVENT, CBUS_EVENT_ERROR, error_event_handler, NULL, NULL );

    /* Test START */
    cbus_event_data_t evt;
    evt.cmd.command = CBUSCMD_ATTACH;
    *((cbus_device_config_t *)evt.payload) = (cbus_device_config_t){
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
    esp_event_post_to(cbus_event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &evt, sizeof(cbus_event_data_t), 1);
    ESP_LOGE("exit", "");
    /* Test END */
    return;
    
    fflush(stdout);
}
