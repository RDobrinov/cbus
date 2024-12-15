/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "cbus_1wire_driver.h"
#include "onewire/onewire_bus.h"
#include "onewire/onewire_device.h"
#include "onewire/onewire_cmd.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "../src/rmt_private.h"

#include "esp_log.h"

/* Type for 1-Wire device id generator */
typedef union {
    struct {
        uint32_t gpio:7;        /*!< 1-Wire bus pin */
        uint32_t tx_channel:3;  /*!< RMT TX Channel */
        uint32_t rx_channel:3;  /*!< RMT RX Channel */
        uint32_t reserved:3;    /*!< Not used */
        uint32_t intr_id:16;    /*!< CRC-16/MCRF4XX Seed is part of MAC */
    };
    uint32_t id;                /*!< Device ID wrapper */
} ow_device_id_t;

/* Type of internal device list */
typedef struct owbus_device_list {
    uint32_t device_id;             /*!< 1-Wire device id */
    uint64_t address;               /*!< 1-Wire ROM address */
    onewire_bus_handle_t handle;    /*!< 1-Wire bus handle */
    struct owbus_device_list *next; /*!< Pointer to next element */
} owbus_device_list_t;

static owbus_device_list_t *ow_devices = NULL;  /*!< First device list element */
static cbus_driver_t *cbus_ow = NULL;           /*!< cbus driver handler */

/**
 * @brief Attach new 1-Wire device to bus
 *
 * @param[in] payload pointer to device configuration
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_common_id_t cbus_ow_attach(cbus_device_config_t *payload);

/**
 * @brief Deatach 1-Wire device from bus
 *
 * @param[in] id Device ID
 * @return
 *      - Common ID structure filled with result code and Device ID
 * @note
 *      - Remove device from list, free device handler and free I2C Port
 *      if no devices left
 */
static cbus_common_id_t cbus_ow_deattach(uint32_t id);

/**
 * @brief Generate 1-Wire device description
 *
 * @param[in] id Device ID
 * @param[in] len Maximum length for device description
 * @param[out] desc Pointer to char array for description
 * 
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_common_id_t cbus_ow_description(uint32_t id, uint8_t *desc, size_t len);

/**
 * @brief Execute command on 1-Wire bus
 *
 * @param[in] payload Pointer to Common CBUS command structure
 * 
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_common_id_t cbus_ow_command(cbus_common_cmd_t *payload);

/**
 * @brief Find onewire bus handle for given GPIO
 *
 * @param[in] gpio 1-Wire bus pin
 * @return
 *      - onewire bus handle or NULL if no handle created for given GPIO
 */
static onewire_bus_handle_t owbus_find_handle(gpio_num_t gpio);

/**
 * @brief Find device in internal list
 *
 * @param[in] id Device id
 * @return
 *      - Pointer to internal device list element
 */
static owbus_device_list_t *owbus_find_device(uint32_t id);

/**
 * @brief Generate internal 16bit ID
 *
 * @param[in] data Pointer to 1-Wire device ROM Code
 * @return
 *      - 16bit internal ID
 */
static uint16_t _genid(uint8_t *data);

/**
 * @brief Swap 1-Wire ROM Code
 *
 * @param[in] val ROM Code
 * @return
 *      - Swapped ROM Code
 */
static uint64_t _swap_romcode(uint64_t val);

/* Bus handlers */
static cbus_common_id_t cbus_ow_attach(cbus_device_config_t *payload) {
    if(payload->bus_type != CBUS_BUS_1WIRE) return (cbus_common_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = 0x00000000UL };
    onewire_bus_handle_t handle = owbus_find_handle(payload->ow_device.data_gpio);
    if(!handle) {
        /* -> RMT channel acquisition & creat OneWire bus */
        if(!gpio_drv_reserve(payload->ow_device.data_gpio)) return (cbus_common_id_t) { .error = CBUS_ERR_PIN_IN_USE, .id = 0x00000000UL };
        onewire_bus_config_t bus_config = { .bus_gpio_num = payload->ow_device.data_gpio};
        /* *> Keep in mind *<
         * max_rx_bytes is allocate memory for rx channel. Same time this value blocks creating new rmt channel due lack of memory
         * For example max_rx_bytes = 32 allow only one 1-wire bus to be created.
         * 8 bytes per channel do not overlap channels. (ESP32 has 64 blocks, S3 48 blocks)
        */
        onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes = 16};
        if(ESP_OK != onewire_new_bus_rmt(&bus_config, &rmt_config, &handle)) {
            free(handle); /* DO YOU NEED this ??? */
            gpio_drv_free(payload->ow_device.data_gpio);
            return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
        }
        /* <- RMT channel acquisition & creat OneWire bus */
    }
    /* -> Attach device */
    owbus_device_list_t *new_device_entry = (owbus_device_list_t *)calloc(1, sizeof(owbus_device_list_t));
    if(!new_device_entry) {
        onewire_bus_del(handle);    /* Not safe delete. Do not check for new created handle */
        gpio_drv_free(payload->ow_device.data_gpio);
        return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
    }

    new_device_entry->address = payload->ow_device.rom_code;
    new_device_entry->handle = handle;
    new_device_entry->next = ow_devices;

    onewire_bus_rmt_obj_t *bus_rmt = __containerof(handle, onewire_bus_rmt_obj_t, base);
    ow_device_id_t new_id = {
        .gpio = payload->ow_device.data_gpio,
        .tx_channel = bus_rmt->tx_channel->channel_id,
        .rx_channel = bus_rmt->rx_channel->channel_id,
        .reserved = 0x00,
        .intr_id = _genid((uint8_t *)(&(new_device_entry->address))),
    };
    new_device_entry->device_id = new_id.id;
    if(owbus_find_device(new_id.id)) {
        free(new_device_entry);
        return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_EXIST, .id = new_id.id };
    }

    ow_devices = new_device_entry;
    return (cbus_common_id_t) { .error = CBUS_OK, .id = new_device_entry->device_id };
}

static cbus_common_id_t cbus_ow_deattach(uint32_t id) {
    owbus_device_list_t *device = ow_devices, *target = NULL;
    while(device && (device->device_id != id)) {
        target = device;
        device = device->next;
    }
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(device->handle, onewire_bus_rmt_obj_t, base);
    gpio_num_t gpio_num = bus_rmt->tx_channel->gpio_num;
    if(target) target->next = device->next;
    else ow_devices = device->next;
    target = NULL;
    for(owbus_device_list_t *dev = ow_devices; dev && !target; dev=dev->next) { 
        if(dev->handle == device->handle) target = dev;
    }
    if(!target) {
        onewire_bus_del(device->handle);
        gpio_drv_free(gpio_num);
    }
    free(device);
    return (cbus_common_id_t) { .error = CBUS_OK, .id = id };
}
/*
    owbus_device_list_t *device = owbus_find_device(id);
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(device->handle, onewire_bus_rmt_obj_t, base);
    gpio_num_t gpio_num = bus_rmt->tx_channel->gpio_num;
    owbus_device_list_t *prev = NULL;
    if( device == ow_devices ) ow_devices = device->next;
    else {
        prev = ow_devices;
        while( prev && (prev->next != device) ) prev = prev->next;
        if(prev) prev->next = device->next;
    }
    prev = NULL;
    for(owbus_device_list_t *dev = ow_devices; dev && !prev; dev=dev->next) { 
        if(dev->handle == device->handle) prev = dev;
    }
    if(!prev) {
        onewire_bus_del(device->handle);
        gpio_drv_free(gpio_num);
        free(device);
    }
    return (cbus_common_id_t) { .error = CBUS_OK, .id = id };
}
*/

static cbus_common_id_t cbus_ow_description(uint32_t id, uint8_t *desc, size_t len) {
    owbus_device_list_t *device = owbus_find_device(id);
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(device->handle, onewire_bus_rmt_obj_t, base);
    char *buf = (char *)calloc(40, sizeof(uint8_t));
    sprintf(buf, "%016llX @ owb/p%02ut%02ur%02u", _swap_romcode(device->address), 
            bus_rmt->tx_channel->gpio_num, bus_rmt->tx_channel->channel_id, bus_rmt->rx_channel->channel_id);
    memcpy(desc, buf, len);
    free(buf);
    desc[len-1] = 0x00;
    return (cbus_common_id_t) { .error = CBUS_OK, .id = id };
}

static cbus_common_id_t cbus_ow_command(cbus_common_cmd_t *payload) {
    /* -> SCAN 1-Wire bus for devices */
    /* Valid device number passed to cbus_common_cmd_t device_id
    *  - Scan registred device 1-Wire bus 
    *  GPIO passed in cbus_common_cmd_t data
    *  - Search for active bus for that GPIO or create new one for scan
    *  
    * Return array of uint64_t in cbus_common_cmd_t data 
    * with number of elements (max 8) in cbus_common_cmd_t device_id
    */
    esp_err_t err = ESP_OK;
    if(payload->command == CBUSCMD_SCAN) {
        bool new_handle = false;
        onewire_bus_handle_t handle = NULL;
        gpio_num_t gpio = *((gpio_num_t *)(payload->data));
        owbus_device_list_t *device = owbus_find_device(payload->device_id);
        if(device) handle = device->handle;
        else {
            handle = owbus_find_handle(gpio);
            if(!handle) {
                onewire_bus_config_t bus_config = { .bus_gpio_num = gpio};
                onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes = 10};
                if(ESP_OK != onewire_new_bus_rmt(&bus_config, &rmt_config, &handle)) {
                    return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
                }
                new_handle = true;
            }
        }
        uint64_t *found = (uint64_t *)payload->data;
        cbus_common_id_t ret = { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = 0 };
        onewire_device_iter_handle_t iter = NULL;
        onewire_device_t next;
        if(ESP_OK == onewire_new_device_iter(handle, &iter)) {
            do {
                err = onewire_device_iter_get_next(iter, &next);
                if( ESP_OK == err ){
                    *found = next.address;
                    found++;
                    ret.id++;
                    ret.error = CBUS_OK;
                }
            } while ( (ESP_ERR_NOT_FOUND != err) || ((sizeof(uint64_t)*8) <= (found - (uint64_t *)payload->data)) );
        }
        if(new_handle) onewire_bus_del(handle);
        return ret;
    } /* <- SCAN 1-Wire bus for devices */

    owbus_device_list_t *device = owbus_find_device(payload->device_id);
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = payload->device_id };
    if(payload->command == CBUSCMD_RESET) {
        err = onewire_bus_reset(device->handle);
        return (cbus_common_id_t) { .error = ( (ESP_OK == err) ? CBUS_OK : CBUS_ERR_UNKNOWN), .id = payload->device_id }; 
    }

    /* Prepare command for 1-Wire device */
    /* MATCH ROM:ROM CODE:COMMAND */
    memcpy((payload->data)+9, payload->data, payload->inDataLen);
    payload->data[0] = ONEWIRE_CMD_MATCH_ROM;
    *(uint64_t *)(&payload->data[1]) = device->address;
    err = ESP_OK;
    switch (payload->command) {
        case CBUSCMD_WRITE:
            err = onewire_bus_write_bytes(device->handle, payload->data, payload->inDataLen+9);
            break;
        case CBUSCMD_READ:
        case CBUSCMD_RW:
            err = onewire_bus_write_bytes(device->handle, payload->data, payload->inDataLen+9);
            err = onewire_bus_read_bytes(device->handle, payload->data, payload->outDataLen);
            break;
        default:
            return (cbus_common_id_t) { .error = CBUS_ERR_NOT_USED, .id = payload->device_id };
            break;
    }
    return (cbus_common_id_t) { .error = ( (ESP_OK == err) ? CBUS_OK : ((ESP_ERR_TIMEOUT == err) ? CBUS_ERR_TIMEOUT : CBUS_ERR_UNKNOWN) ), .id = payload->device_id }; 
}

static onewire_bus_handle_t owbus_find_handle(gpio_num_t gpio) {
    onewire_bus_rmt_obj_t *bus_rmt;
    for(owbus_device_list_t *device = ow_devices; device != NULL; device = device->next) { 
        bus_rmt = __containerof(device->handle, onewire_bus_rmt_obj_t, base);
        if(bus_rmt->tx_channel->gpio_num == gpio) return device->handle;
    }
    return NULL;
}

static owbus_device_list_t *owbus_find_device(uint32_t id) {
    for(owbus_device_list_t *device = ow_devices; device != NULL; device = device->next) { 
        if(device->device_id == id) return device;
    }
    return NULL;
}

/*
static owbus_device_list_t *owbus_find_device(uint32_t id) {
    for (owbus_device_list_t *dev = ow_devices; dev != NULL; dev = dev->next) {
        if (dev->device_id == id) {
            return dev;
        }
    }
    return NULL;
}
*/

static uint16_t _genid(uint8_t *data) //crc16_mcrf4xx
{
    uint8_t len = 8;
    uint8_t t;
    uint8_t L;
    uint64_t mac = 0;
    esp_efuse_mac_get_default((uint8_t *)(&mac));
    uint16_t id = *(((int16_t *)(&mac))+2);
    if (data) {
        while (len--) {
            id ^= *data++;
            L = id ^ (id << 4);
            t = (L << 3) | (L >> 5);
            L ^= (t & 0x07);
            t = (t & 0xF8) ^ (((t << 1) | (t >> 7)) & 0x0F) ^ (uint8_t)(id >> 8);
            id = (L << 8) | t;
        }
    }
    return id;
}

static uint64_t _swap_romcode(uint64_t val)
{
    val = ((val << 8) & 0xFF00FF00FF00FF00ULL ) | ((val >> 8) & 0x00FF00FF00FF00FFULL );
    val = ((val << 16) & 0xFFFF0000FFFF0000ULL ) | ((val >> 16) & 0x0000FFFF0000FFFFULL );
    return (val << 32) | (val >> 32);
}

void *ow_get_bus(void) {
    if(!cbus_ow) { 
        cbus_ow = (cbus_driver_t *)calloc(1, sizeof(cbus_driver_t));
        if(cbus_ow) {
            cbus_ow->attach = &cbus_ow_attach;
            cbus_ow->deattach = &cbus_ow_deattach;
            cbus_ow->desc = &cbus_ow_description;
            cbus_ow->execute = &cbus_ow_command;
        }
    }
    return cbus_ow;
}

void owbus_dump_devices(void) {
    for(owbus_device_list_t *device = ow_devices; device; device = device->next) {
        onewire_bus_rmt_obj_t *bus_rmt = __containerof(device->handle, onewire_bus_rmt_obj_t, base);
        ESP_LOGI("dump", "LstHandle: %p [ID:0x%08lX, Addr:0x%016llx DHandle:%p on TX%02d RX%02d with SCL_GPIO%d BHandle:%p] NxDev:%p", 
            device, device->device_id, _swap_romcode(device->address), device->handle, bus_rmt->tx_channel->channel_id, bus_rmt->rx_channel->channel_id,
            bus_rmt->tx_channel->gpio_num, bus_rmt, device->next);
    }
}