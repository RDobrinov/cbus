/*
 * SPDX-FileCopyrightText: 2024 No Company name
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "cbus_i2c_driver.h"
#include "i2c_private_def.h"
#include "esp_log.h"


/** Type of device list element for attached i2c devices */
typedef struct i2cbus_device_list {
    uint32_t device_id;                 /*!< Device ID */
    struct {
        uint32_t cmd_bytes:2;       /*!< Device command bytes 0-2 */
        uint32_t addr_bytes:4;      /*!< Device command bytes 0-8 */
        uint32_t xfer_timeout_ms:4; /*!< Device timeout */
        uint32_t not_used:22;       /*!< Not used */
    };
    cbus_statistic_t stats;             /*!< Device stats counters */
    i2c_master_dev_handle_t handle;     /*!< I2C master device handle */
    struct i2cbus_device_list *next;    /*!< Pointer to next elements */
} i2cbus_device_list_t;

/**
 * @brief Attach new I2C device to bus
 *
 * @param[in] payload pointer to device configuration
 * @return
 *      - Common ID structure filled with result code and Device ID
 * @note
 *      - Search for existing i2c master bus or create new one
 *        and attach device to bus
 */
static cbus_id_t cbus_i2c_attach(cbus_device_config_t *payload);

/**
 * @brief Deattach new I2C device to bus
 *
 * @param[in] id Device ID
 * @return
 *      - Common ID structure filled with result code and Device ID
 * @note
 *      - Deattach device from bus and release bus if no devices left
 */
static cbus_id_t cbus_i2c_deattach(uint32_t id);

/**
 * @brief Generate i2c device description
 *
 * @param[in] id Device ID
 * @param[in] len Maximum length for device description
 * @param[out] desc Pointer to char array for description
 * 
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_id_t cbus_i2c_description(uint32_t id, uint8_t *desc, size_t len);

/**
 * @brief Get device statistics
 *
 * @param[in] id Device ID
 * @param[out] stats Pointer to device statistics to be filled
 * 
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_id_t cbus_i2c_statistcis(uint32_t id, cbus_stats_data_t *stats);

/**
 * @brief Execute command on i2c bus
 *
 * @param[in] payload Pointer to Common CBUS command structure
 * 
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_id_t cbus_i2c_command(cbus_cmd_t *payload);

/**
 * @brief Find for master bus handle
 * 
 * @param[in] scl I2C master bus clock GPIO
 * @param[in] sda I2C master bus data GPIO
 * 
 * @return
 *      - i2c master bus handle or NULL if not found
 */
static i2c_master_bus_handle_t i2cbus_find_master(gpio_num_t scl, gpio_num_t sda);

/**
 * @brief Find device in internal list
 *
 * @param[in] id Device id
 * @return
 *      - Pointer to internal device list element
 */
static i2cbus_device_list_t *i2cbus_find_device(uint32_t id);

/**
 * @brief Release master bus handle
 * 
 * @param[in] bus I2C master bus handle
 * @note
 *      - Check for existing devices on bus and release
 *        master bus in no devices left
 */
static void i2cbus_release_master(i2c_master_bus_handle_t bus);

static i2cbus_device_list_t *i2c_devices = NULL;    /*!< Head of internal device list */
static cbus_driver_t *cbus_i2c = NULL;  /*!< cbus driver handle */

void i2c_hex(const uint8_t *buf, size_t len) {
    if( !len ) return;
    //ESP_LOGI("hexdump", "%p", buf);
    printf("i2c_hex: [%p] ", buf);
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

static cbus_id_t cbus_i2c_attach(cbus_device_config_t *payload) {
    if(payload->bus_type != CBUS_BUS_I2C) return (cbus_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = 0x00000000UL };
    uint64_t pinmask = (BIT64(payload->i2c_device.scl_gpio) | BIT64(payload->i2c_device.sda_gpio));
    /* -> I2C Controller acquisition */
    i2c_master_bus_handle_t port_handle = i2cbus_find_master(payload->i2c_device.scl_gpio, payload->i2c_device.sda_gpio);
    if(!port_handle) {
        if(!gpio_drv_reserve_pins(pinmask)) return (cbus_id_t) { .error = CBUS_ERR_PIN_IN_USE, .id = 0x00000000UL };
        i2c_master_bus_config_t i2c_bus_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1,
            .scl_io_num = payload->i2c_device.scl_gpio,
            .sda_io_num = payload->i2c_device.sda_gpio, 
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true
        };
        esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &port_handle);
        /* Error ??? */
        if(ESP_OK != err) {
            free(port_handle);
            gpio_drv_free_pins(pinmask);
            return (cbus_id_t) { .error = ( ESP_ERR_NOT_FOUND == err ? CBUS_ERR_NO_MORE_BUSES : CBUS_ERR_UNKNOWN ), .id = 0x00000000UL };
        }
    } /* <- I2C Controller acquisition */

    /* -> Attach device */
    i2cbus_device_id_t new_id = (i2cbus_device_id_t) {
        .gpioscl = payload->i2c_device.scl_gpio,
        .gpiosda = payload->i2c_device.sda_gpio,
        .i2caddr = payload->i2c_device.device_address,
        .i2cbus = port_handle->base->port_num
    };

    if(i2cbus_find_device(new_id.id)) return (cbus_id_t) { .error = CBUS_ERR_DEVICE_EXIST, .id = new_id.id };

    i2c_master_dev_handle_t *new_device_handle = (i2c_master_dev_handle_t *)calloc(1, sizeof(i2c_master_dev_handle_t));
    i2c_device_config_t device_conf = (i2c_device_config_t) { 
        .dev_addr_length = payload->i2c_device.dev_addr_length, 
        .device_address = payload->i2c_device.device_address,
        //.scl_speed_hz = 10000 * payload->i2c_device.scl_speed_hz,
        .scl_speed_hz = payload->i2c_device.scl_speed_hz,
        .flags.disable_ack_check = payload->i2c_device.disable_ack_check
    };
    esp_err_t err = i2c_master_bus_add_device(port_handle, &device_conf, new_device_handle);
    if( ESP_OK != err ) 
    {
        free(new_device_handle);
        i2cbus_release_master(port_handle);
        return (cbus_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = 0x00000000UL };
    }
    i2cbus_device_list_t *new_device_entry = (i2cbus_device_list_t *)calloc(1, sizeof(i2cbus_device_list_t));
    if(!new_device_entry) {
        i2c_master_bus_rm_device(*new_device_handle);
        free(new_device_handle);
        i2cbus_release_master(port_handle);
        return (cbus_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
    }
    *new_device_entry = (i2cbus_device_list_t) {
        .handle = *new_device_handle,
        .device_id = new_id.id,
        .xfer_timeout_ms = payload->i2c_device.xfer_timeout_ms,
        .stats = new_device_entry->stats = (cbus_statistic_t){},
        .cmd_bytes = (payload->i2c_device.cmd_bytes > 2) ? 2 : payload->i2c_device.cmd_bytes,
        .addr_bytes = (payload->i2c_device.addr_bytes > 8) ? 8 : payload->i2c_device.addr_bytes,
        .next = i2c_devices
    }; 
    i2c_devices = new_device_entry; /* <- END Attach device */

    return (cbus_id_t) { .error = CBUS_OK, .id = new_device_entry->device_id };
}

static cbus_id_t cbus_i2c_deattach(uint32_t id) {
    i2cbus_device_list_t *device = i2c_devices, *target = NULL;
    while(device && (device->device_id != id)) {
        target = device;
        device = device->next;
    }
    if(!device) return (cbus_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };

    i2c_master_bus_handle_t master_handle = device->handle->master_bus;
    if( ESP_OK == i2c_master_bus_rm_device(device->handle) ) {
        if(target) target->next = device->next;
        else i2c_devices = device->next;
        free(device);
    } else {
        return (cbus_id_t) { .error = CBUS_ERR_UNKNOWN, .id = id };
    }
    i2cbus_release_master(master_handle);
    return (cbus_id_t) { .error = CBUS_OK, .id = id };
}

static cbus_id_t cbus_i2c_description(uint32_t id, uint8_t *desc, size_t len)
{   
    i2cbus_device_list_t *device = i2cbus_find_device(id);
    if(!device) return (cbus_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };
    char *buf = (char *)calloc(40, sizeof(uint8_t));
    sprintf(buf, "0x%02X @ i2c/p%02ucl%02uda%02ucs%03u", device->handle->device_address, device->handle->master_bus->base->port_num,
            device->handle->master_bus->base->scl_num, device->handle->master_bus->base->sda_num, (unsigned int)device->handle->scl_speed_hz/1000);
    memcpy(desc, buf, len);
    free(buf);
    desc[len-1] = 0x00;
    return (cbus_id_t) { .error = CBUS_OK, .id = id };
}

static cbus_id_t cbus_i2c_statistcis(uint32_t id, cbus_stats_data_t *stats) {
    i2cbus_device_list_t *device = i2cbus_find_device(id);
    if(!device) return (cbus_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };
    stats->stats = device->stats; 
    return (cbus_id_t) { .error = CBUS_OK, .id = id };
}

static cbus_id_t cbus_i2c_command(cbus_cmd_t *payload) {
    i2cbus_device_list_t *device = i2cbus_find_device(payload->device_transaction.device_id);
    if(!device) return (cbus_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = payload->device_transaction.device_id };
    uint8_t shift = device->cmd_bytes + device->addr_bytes;
    if((CBUSCMD_READ != payload->device_command.command) && (( shift + payload->device_command.inDataLen) > 128)) {
        return (cbus_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = payload->device_transaction.device_id };
    } else {
        if(payload->device_command.inDataLen) {
            for(int i = payload->device_command.inDataLen -1; i >= 0; i--) { payload->data[i+shift] = payload->data[i]; }
        }
    }
    if(device->cmd_bytes > 0) {
        if(device->cmd_bytes == 2) *((uint16_t *)payload->data) = (uint16_t)( 0xFFFF & payload->device_transaction.device_cmd);
        else payload->data[0] = (uint8_t)( 0xFF & payload->device_transaction.device_cmd);
    }
    if(device->addr_bytes > 0) {
        memcpy(&payload->data[device->cmd_bytes], &(payload->device_transaction.reg_address), device->addr_bytes);
    }
    
    esp_err_t err = ESP_OK;
    switch (payload->device_command.command) {
        case CBUSCMD_READ:
            if(shift > 0) err = i2c_master_transmit_receive(device->handle, payload->data, shift, payload->data, payload->device_command.outDataLen, device->xfer_timeout_ms);
            else err = i2c_master_receive(device->handle, payload->data, payload->device_command.outDataLen, device->xfer_timeout_ms);
            break;
        case CBUSCMD_WRITE:
            err = i2c_master_transmit(device->handle, payload->data, payload->device_command.inDataLen+shift, device->xfer_timeout_ms);
            break;
        case CBUSCMD_RW:
            err = i2c_master_transmit_receive(device->handle, payload->data, payload->device_command.inDataLen+shift, payload->data, payload->device_command.outDataLen, device->xfer_timeout_ms);
            break;
        default:
            return (cbus_id_t) { .error = CBUS_ERR_NOT_USED, .id = payload->device_transaction.device_id };
            break;
    }
    if(ESP_OK == err) {
        if(CBUSCMD_WRITE != payload->device_command.command) {
            device->stats.rcv += payload->device_command.outDataLen;
        }
        if(shift > 0 && (CBUSCMD_READ == payload->device_command.command)) device->stats.snd += shift;
        else {
             if(CBUSCMD_READ != payload->device_command.command) device->stats.snd += payload->device_command.inDataLen+shift;
        }
    } else { 
        if(ESP_ERR_TIMEOUT == err) device->stats.timeouts++;
        else device->stats.other++;
    }
    return (cbus_id_t) { .error = ( (ESP_OK == err) ? CBUS_OK : ((ESP_ERR_TIMEOUT == err) ? CBUS_ERR_TIMEOUT : CBUS_ERR_UNKNOWN) ), .id = payload->device_transaction.device_id }; 
}

static i2c_master_bus_handle_t i2cbus_find_master(gpio_num_t scl, gpio_num_t sda) {
    if(!i2c_devices) return NULL;
    i2cbus_device_list_t *found = NULL;
    for(i2cbus_device_list_t *dev = i2c_devices; dev && !found; dev=dev->next) { 
        if(dev->handle->master_bus->base->scl_num == scl && dev->handle->master_bus->base->sda_num == sda) found = dev;
    }
    return (found) ? found->handle->master_bus : NULL;
}

static i2cbus_device_list_t *i2cbus_find_device(uint32_t id){
    for(i2cbus_device_list_t *device = i2c_devices; device != NULL; device = device->next)
        if(device->device_id == id) return device;
    return NULL;
}

static void i2cbus_release_master(i2c_master_bus_handle_t handle) {
    if(handle->device_list.slh_first) return;
    gpio_drv_free_pins( BIT64(handle->base->scl_num) | BIT64(handle->base->sda_num));
    if(ESP_OK != i2c_del_master_bus(handle)){
        /* Error state - I2C Controller remains locked / unusable */
    }
}

void *i2cbus_get_bus(void) {
    if(!cbus_i2c) { 
        cbus_i2c = (cbus_driver_t *)calloc(1, sizeof(cbus_driver_t));
        if(cbus_i2c) {
            cbus_i2c->attach = &cbus_i2c_attach;
            cbus_i2c->deattach = &cbus_i2c_deattach;
            cbus_i2c->info = &cbus_i2c_description;
            cbus_i2c->execute = &cbus_i2c_command;
            cbus_i2c->stats = &cbus_i2c_statistcis;
        }
    }
    return cbus_i2c;
}

void i2cbus_dump_devices(void) {
    for(i2cbus_device_list_t *device = i2c_devices; device; device = device->next) {
        ESP_LOGI("dump", "LstHandle: %p [ID:0x%08lX, Addr:0x%02x, DHandle:%p on I2C%d with SCL_GPIO%d SDA_GPIO%d at %lukHz, BHandle:%p] NxDev:%p", 
            device, device->device_id, device->handle->device_address, device->handle ,device->handle->master_bus->base->port_num,
            device->handle->master_bus->base->scl_num, device->handle->master_bus->base->sda_num, device->handle->scl_speed_hz / 1000, device->handle->master_bus, device->next);
    }
}
