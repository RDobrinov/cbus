/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ds18b20/msos_ds18b20.h"
#include "esp_timer.h"
#include "esp_log.h"

#define DS18B20_FUNC_CONVERT            0x44    /*!< Start single temperature conversion */
#define DS18B20_FUNC_READ_SCRATCHPAD    0xBE    /*!< Read device scratchpad ( 9 Bytes ) */
#define DS18B20_FUNC_WRITE_SCRATCHPAD   0x4E    /*!< Write device scratchpad ( Bytes 2 to 4 )*/
#define DS18B20_MAX_MEASURE_TIME        750     /*!< Maximum measure time in milliseconds */
#define DS18B20_RESERVE_MEASURE_TIME    10000   /*!< Protection measure time in microseconds */

void hd(const uint8_t *buf, size_t len) {
    if( !len ) return;
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

typedef enum {
    DS18B20_OP_NONE = 0,
    DS18B20_OP_ATTACH,
    DS18B20_OP_DEATTACH,
    DS18B20_OP_WRITE_CONF,
    DS18B20_OP_READ_REGS,
    DS18B20_OP_MEASURE
} ds18b20_ops_t;

typedef struct {
    msos_sensor_interface_t interface;
    esp_event_loop_handle_t *event_loop;
    msos_sensor_api api;
    uint8_t sender_id;
} ds18b20_run_config_t;

typedef struct {
    uint32_t id;
    struct {
        uint32_t init:1;
        uint32_t ready:1;
        uint32_t busy:1;
        uint32_t use_crc:1;
        uint32_t resolution:2;
        uint32_t ops:4;
        uint32_t temp:16;
        uint32_t reserved:6;
    };
    esp_timer_handle_t measure_timer;
    msos_sensor_api_t base;
} ds18b20_base;

typedef ds18b20_base *ds18b20_handle_t;

esp_err_t ds18b20_init(void *handle);
esp_err_t ds18b20_prepare(void *handle);
esp_err_t ds18b20_measure(void *handle);
esp_err_t ds18b20_magnitudes(void *handle, msos_sensor_magnitude_t *magnitudes, size_t *len);
esp_err_t ds18b20_get(void *handle, msos_sensor_magnitude_t *magnitude, float *value);
bool ds18b20_ready(void *handle);

static void ds18b20_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ds18b20_error_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void ds18b20_timer_cb(void *handle);

ds18b20_run_config_t _conf = {NULL, NULL, 
    {   ._init = &ds18b20_init, 
        ._prepare = &ds18b20_prepare,
        ._measure = &ds18b20_measure,
        ._magnitudes = &ds18b20_magnitudes,
        ._get = &ds18b20_get,
        ._ready = &ds18b20_ready
    }, 0xFF};

esp_err_t ds18b20_init(void *handle) {
    ds18b20_handle_t ds18b20 = __containerof(handle, ds18b20_base, base);
    if(ds18b20->init) return ESP_OK;
    cbus_event_data_t evt;

    ds18b20->ops = DS18B20_OP_WRITE_CONF;
    ds18b20->busy = true;
    evt.event_id = DS18B20_OP_WRITE_CONF;
    evt.sender_id = _conf.sender_id;
    evt.user_device_handle = ds18b20;
    
    evt.cmd = (cbus_event_command_t) {
        .command = CBUSCMD_WRITE,
        .data_type = CBUSDATA_BLOB,
        .inDataLen = 3,
        .outDataLen = 0
    };
    evt.transaction.device_cmd = DS18B20_FUNC_WRITE_SCRATCHPAD;
    evt.transaction.device_id = ds18b20->id;
    *((uint32_t *)(evt.payload)) = 0xA55ALU;
    evt.payload[2] = ((uint8_t)(ds18b20->resolution)) << 5;
    hd(evt.payload,9);
    return esp_event_post_to(*_conf.event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &evt, sizeof(cbus_event_data_t), 1);
}

esp_err_t ds18b20_prepare(void *handle) {
    //ds18b20_handle_t ds18b20 = __containerof(handle, ds18b20_base, base);
    return ESP_OK;
}

esp_err_t ds18b20_measure(void *handle) {
    ds18b20_handle_t ds18b20 = __containerof(handle, ds18b20_base, base);
    cbus_event_data_t evt;

    ds18b20->ops = DS18B20_OP_MEASURE;
    ds18b20->busy = true;
    evt.event_id = DS18B20_OP_MEASURE;
    evt.sender_id = _conf.sender_id;
    evt.user_device_handle = ds18b20;
    
    evt.cmd = (cbus_event_command_t) {
        .command = CBUSCMD_WRITE,
        .data_type = CBUSDATA_UINT8,
        .inDataLen = 0,
        .outDataLen = 0
    };
    evt.transaction.device_cmd = DS18B20_FUNC_CONVERT;
    evt.transaction.device_id = ds18b20->id;
    esp_err_t ret = esp_event_post_to(*_conf.event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &evt, sizeof(cbus_event_data_t), 1);
    esp_timer_start_once(ds18b20->measure_timer, DS18B20_RESERVE_MEASURE_TIME + (1000 * (DS18B20_MAX_MEASURE_TIME >> (3 - ds18b20->resolution))));
    return ESP_OK;
}

bool ds18b20_ready(void *handle) {
    ds18b20_handle_t ds18b20 = __containerof(handle, ds18b20_base, base);
    printf("ds18b20_ready [%08lX:%p] init:%u, ready:%u, busy:%u res:%u, ops:%u\n", 
        ds18b20->id, ds18b20, ds18b20->init, ds18b20->ready, ds18b20->busy, ds18b20->resolution, ds18b20->ops);
    return (ds18b20->init && ds18b20->ready && !ds18b20->busy);
}

esp_err_t ds18b20_magnitudes(void *handle, msos_sensor_magnitude_t *magnitudes, size_t *len) {
    ds18b20_handle_t ds18b20 = __containerof(handle, ds18b20_base, base);
    if(*len != 1) return ESP_ERR_INVALID_ARG;
    magnitudes[0] = (msos_sensor_magnitude_t){ .magnitude_index = 0, .magnitude = MAGNITUDE_TEMPERATURE, .decimals = 1+((ds18b20_handle_t)handle)->resolution};
    return ESP_OK;
}

esp_err_t ds18b20_get(void *handle, msos_sensor_magnitude_t *magnitude, float *value) {
    ds18b20_handle_t ds18b20 = __containerof(handle, ds18b20_base, base);
    *value = 0.0f;
    printf("ds18b20_get magnitude index %d\n", magnitude->magnitude_index);
    if(magnitude->magnitude_index != 0) return ESP_ERR_INVALID_ARG;
    *value = (float)((int16_t)(-1 & ds18b20->temp)/16.0);
    return ESP_OK;
}

esp_err_t ds18b20_add(void *data, msos_sensor_api_t **handle) {
    ds18b20_handle_t ds18b20 = (ds18b20_handle_t)calloc(1, sizeof(ds18b20_base));
    ds18b20->base = &_conf.api;
    //ds18b20->id = *((uint32_t *)data);
    //printf("ds18b20_add ds18b20->base: %p\n", &(ds18b20->base));
    //printf("ds18b20_add ds18b20_handle_t: %p\n", ds18b20);
    ds18b20->resolution = ((ds18b20_config_t *)data)->resolution;
    *handle = &ds18b20->base;
    cbus_event_data_t evt;

    ds18b20->ops = DS18B20_OP_ATTACH;
    evt.cmd.command = CBUSCMD_ATTACH;
    evt.sender_id = _conf.sender_id;
    evt.event_id = DS18B20_OP_ATTACH;
    evt.user_device_handle = ds18b20;
    
    *((cbus_device_config_t *)evt.payload) = (cbus_device_config_t){
        .bus_type = CBUS_BUS_1WIRE,
        .ow_device = {
            .data_gpio = ((ds18b20_config_t *)data)->data_gpio,
            .rom_code = ((ds18b20_config_t *)data)->rom_code,
            //.rom_code = 0xDB041674A78CFF28LLU,
            .reserved = 0,
            .cmd_bytes = 1,
            .addr_bytes = 0,
            .crc_check = ((ds18b20_config_t *)data)->use_crc
        }
    };
    const esp_timer_create_args_t measure_timer_args = {
            .callback = &ds18b20_timer_cb,
            /* Send handle to this device */
            .arg = (void*) ds18b20, 
    };
    if(ESP_OK != esp_timer_create(&measure_timer_args, &(ds18b20->measure_timer))) {
        free(ds18b20);
        return ESP_FAIL;
    }
    //printf("ds18b20[%p] %p, %p [%p]\n", ds18b20, handle, *handle, ds18b20->base);
    return esp_event_post_to(*_conf.event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &evt, sizeof(cbus_event_data_t), 1);
}

esp_err_t ds18b20_remove(void *handle) {
    return ESP_OK;
}

msos_sensor_interface_t ds18b20_get_interface(void) {
    if(!_conf.interface) {
        _conf.interface =  (msos_sensor_interface_t)calloc(1, sizeof(msos_sensor_interface));
        if(_conf.interface) {
            uint32_t sender_id = 0xFF;
            if(ESP_OK == cbus_register(&_conf.event_loop, &sender_id)) {
                _conf.sender_id = (uint8_t)(0xFF & sender_id);
                esp_event_handler_instance_register_with(*_conf.event_loop, CBUS_EVENT, CBUS_EVENT_DATA, ds18b20_event_handler, NULL, NULL );
                esp_event_handler_instance_register_with(*_conf.event_loop, CBUS_EVENT, CBUS_EVENT_ERROR, ds18b20_error_handler, NULL, NULL );
                _conf.interface->_add = &ds18b20_add;
                _conf.interface->_remove = &ds18b20_remove;
            }
            else {
                free(_conf.interface);
                _conf.interface = NULL;
            }
        }
    }
    return _conf.interface;
}

static void ds18b20_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(((cbus_event_data_t *)event_data)->sender_id != _conf.sender_id) return;
    cbus_event_data_t *event = (cbus_event_data_t *)event_data;
    ds18b20_handle_t ds18b20 = event->user_device_handle;
    switch(event->event_id) {
        case DS18B20_OP_ATTACH:
            ds18b20->id = event->transaction.device_id;
            ds18b20->ready = true;
            
            break;
        case DS18B20_OP_WRITE_CONF:
            ds18b20->init = true;
            ds18b20->busy = false;
            break;
        case DS18B20_OP_READ_REGS:
            static const uint8_t spad_mask[4] = { ~0x07, ~0x03, ~0x01, ~0x00 };
            ds18b20->temp = (uint16_t)(event->payload[1] << 8 | (spad_mask[ds18b20->resolution] & event->payload[0]));
            hd(event->payload, 9);
            ds18b20->busy = false;
            break;
    }
    printf("Event: [%04lX]:%02lX\n", (uint32_t)event->cmd.command, (uint32_t)(event->event_id));
    //printf("ds18b20_event_handler: [%08lX] init:%u, ready:%u, res:%u, ops:%u\n", 
    //    ds18b20->id, ds18b20->init, ds18b20->ready, ds18b20->resolution, ds18b20->ops);
    return;
}

static void ds18b20_error_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(((cbus_event_data_t *)event_data)->sender_id != _conf.sender_id) return;
    ESP_LOGE("error", "[%08lX] %04lX:%02lX", ((cbus_event_data_t *)event_data)->transaction.device_id, (uint32_t)(((cbus_event_data_t *)event_data)->cmd.status), (uint32_t)(((cbus_event_data_t *)event_data)->cmd.command));
    return;
}

static void ds18b20_timer_cb(void *handle) {
    ds18b20_handle_t ds18b20 = handle;
    printf("ds18b20_timer_cb Timer Expire\n");
    esp_timer_delete(ds18b20->measure_timer);
    
    cbus_event_data_t evt;
    ds18b20->ops = DS18B20_OP_READ_REGS;
    ds18b20->busy = true;
    evt.event_id = DS18B20_OP_READ_REGS;
    evt.sender_id = _conf.sender_id;
    evt.user_device_handle = ds18b20;
    
    evt.cmd = (cbus_event_command_t) {
        .command = CBUSCMD_READ,
        .data_type = CBUSDATA_BLOB,
        .inDataLen = 0,
        .outDataLen = 9
    };
    evt.transaction.device_cmd = DS18B20_FUNC_READ_SCRATCHPAD;
    evt.transaction.device_id = ds18b20->id;
    esp_err_t ret = esp_event_post_to(*_conf.event_loop, CBUS_EVENT, CBUS_EVENT_EXEC, &evt, sizeof(cbus_event_data_t), 1);
    //esp_timer_start_once(ds18b20->measure_timer, DS18B20_RESERVE_MEASURE_TIME + (1000 * (DS18B20_MAX_MEASURE_TIME >> (3 - ds18b20->resolution))));
    return;
    //((ds18b20_handle_t)handle)->busy = false;
}