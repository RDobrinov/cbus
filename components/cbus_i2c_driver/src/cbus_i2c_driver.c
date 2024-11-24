
//#include "cbus_driver.h"
#include "cbus_i2c_driver.h"
#include "esp_log.h"

/**
 * @brief Type of unique ID for attached i2c device
*/
typedef union {
    struct {
        uint32_t i2caddr:10;    /*!< Device I2C Address */
        uint32_t i2cbus:2;      /*!< Controller bus number */
        uint32_t gpiosda:7;     /*!< Bus SDA GPIO */
        uint32_t gpioscl:7;     /*!< Bus SDA GPIO */
        uint32_t reserved:6;    /*!< Not used */
    };
    uint32_t id;                /*!< Single i2c device ID */
} i2cbus_device_id_t;

typedef struct i2cbus_device_list {
    uint32_t device_id;
    uint32_t xfer_timeout_ms;
    i2c_master_dev_handle_t handle;
    struct i2cbus_device_list *next;
} i2cbus_device_list_t;

/*
typedef struct i2cbus_master_list {
    i2c_master_bus_handle_t master;
    struct i2cbus_master_list *next;
} i2cbus_master_list_t;
*/

static cbus_common_id_t cbus_i2c_attach(cbus_device_config_t *payload);
static cbus_common_id_t cbus_i2c_deattach(uint32_t id);
static cbus_common_id_t cbus_i2c_command(cbus_common_cmd_t *payload);

static i2c_master_bus_handle_t i2cbus_find_master(gpio_num_t scl, gpio_num_t sda);
static i2cbus_device_list_t *i2cbus_find_device(uint32_t id);
static void i2cbus_release_master(i2c_master_bus_handle_t bus);

static i2cbus_device_list_t *i2c_devices = NULL;
//static i2cbus_master_list_t *i2c_masters = NULL;
static cbus_driver_t *cbus_i2c = NULL;

static cbus_common_id_t cbus_i2c_attach(cbus_device_config_t *payload) {
    uint64_t pinmask = (BIT64(payload->i2c_device.scl_gpio) | BIT64(payload->i2c_device.sda_gpio));
    /* -> I2C Controller acquisition */
    i2c_master_bus_handle_t port_handle = i2cbus_find_master(payload->i2c_device.scl_gpio, payload->i2c_device.sda_gpio);
    if(!port_handle) {
        if(!gpio_drv_reserve_pins(pinmask)) return (cbus_common_id_t) { .error = CBUS_ERR_PIN_IN_USE, .id = 0x00000000UL };
        i2c_master_bus_config_t i2c_bus_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1,
            .scl_io_num = payload->i2c_device.scl_gpio,
            .sda_io_num = payload->i2c_device.sda_gpio, 
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true
        };
        esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &port_handle);
        if(ESP_OK != err) {
            free(port_handle);
            gpio_drv_free_pins(pinmask);
            return (cbus_common_id_t) { .error = ( ESP_ERR_NOT_FOUND == err ? CBUS_ERR_NO_MORE_BUSES : CBUS_ERR_UNKNOWN ), .id = 0x00000000UL };
        }
    } /* <- I2C Controller acquisition */

    /* -> Attach device */
    i2cbus_device_id_t new_id = (i2cbus_device_id_t) {
        .gpioscl = payload->i2c_device.scl_gpio,
        .gpiosda = payload->i2c_device.sda_gpio,
        .i2caddr = payload->i2c_device.device_address,
        .i2cbus = port_handle->base->port_num
    };

    if(i2cbus_find_device(new_id.id)) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_EXIST, .id = new_id.id };

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
        return (cbus_common_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = 0x00000000UL };
    }
    i2cbus_device_list_t *new_device_entry = (i2cbus_device_list_t *)calloc(1, sizeof(i2cbus_device_list_t));
    if(!new_device_entry) {
        i2c_master_bus_rm_device(*new_device_handle);
        free(new_device_handle);
        i2cbus_release_master(port_handle);
        return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
    }
    *new_device_entry = (i2cbus_device_list_t) {
        .handle = *new_device_handle,
        .device_id = new_id.id,
        .xfer_timeout_ms = payload->i2c_device.xfer_timeout_ms,
        .next = i2c_devices
    }; 
    i2c_devices = new_device_entry; /* <- END Attach device */

    return (cbus_common_id_t) { .error = CBUS_OK, .id = new_device_entry->device_id };
}

static cbus_common_id_t cbus_i2c_deattach(uint32_t id) {
    i2cbus_device_list_t *device = i2cbus_find_device(id);
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };
    i2c_master_bus_handle_t master_handle = device->handle->master_bus;
    if( ESP_OK == i2c_master_bus_rm_device(device->handle) ) {
        if( device == i2c_devices ) i2c_devices = device->next;
        else {
            i2cbus_device_list_t *prev = i2c_devices;
            while( prev && (prev->next != device) ) prev = prev->next;
            if(prev) prev->next = device->next;
        }
        free(device);
    } else {
        return (cbus_common_id_t) { .error = CBUS_ERR_UNKNOWN, .id = id };
    }
    i2cbus_release_master(master_handle);
    return (cbus_common_id_t) { .error = CBUS_OK, .id = id };
}

static cbus_common_id_t cbus_i2c_command(cbus_common_cmd_t *payload) {
    i2cbus_device_list_t *device = i2cbus_find_device(payload->device_id);
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = payload->device_id };
    esp_err_t err = ESP_OK;
    switch (payload->command) {
        case CBUSCMD_READ:
            err = i2c_master_transmit(device->handle, payload->data, payload->inDataLen, device->xfer_timeout_ms);
            break;
        case CBUSCMD_WRITE:
            err = i2c_master_receive(device->handle, payload->data, payload->outDataLen, device->xfer_timeout_ms);
            break;
        case CBUSCMD_RW:
            err = i2c_master_transmit_receive(device->handle, payload->data, payload->inDataLen, payload->data, payload->outDataLen, device->xfer_timeout_ms);
            break;
    }
    return (cbus_common_id_t) { .error = ( (ESP_OK == err) ? CBUS_OK : ((ESP_ERR_TIMEOUT == err) ? CBUS_ERR_TIMEOUT : CBUS_ERR_UNKNOWN) ), .id = payload->device_id }; 
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
    i2cbus_device_list_t *found = NULL;
    for(i2cbus_device_list_t *dev = i2c_devices; dev && !found; dev=dev->next) { 
        if(dev->device_id == id) found = dev;
    }
    return found;
}

static void i2cbus_release_master(i2c_master_bus_handle_t handle) {
    if(handle->device_list.slh_first) return;
    gpio_drv_free_pins( BIT64(handle->base->scl_num) | BIT64(handle->base->sda_num));
    if(ESP_OK != i2c_del_master_bus(handle)){
        /* Error state - I2C Controller remains locked / unusable */
    }
}

//cbus_driver_t *i2cbus_get_bus(void) {
void *i2cbus_get_bus(void) {
    if(!cbus_i2c) { 
        cbus_i2c = (cbus_driver_t *)calloc(1, sizeof(cbus_driver_t));
        if(cbus_i2c) {
            cbus_i2c->attach = &cbus_i2c_attach;
            cbus_i2c->deattach = &cbus_i2c_deattach;
            cbus_i2c->read = &cbus_i2c_command;
            cbus_i2c->write = &cbus_i2c_command;
            cbus_i2c->rw = &cbus_i2c_command;
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
