
//#include "cbus_driver.h"
#include "cbus_i2c_driver.h"

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
    i2c_master_dev_handle_t handle;
    struct i2cbus_device_list *next;
} i2cbus_device_list_t;

typedef struct i2cbus_master_list {
    i2c_master_bus_handle_t master;
    struct i2cbus_master_list *next;
} i2cbus_master_list_t;

static i2cbus_master_list_t *i2cbus_find_master(gpio_num_t scl, gpio_num_t sda);
static i2cbus_device_list_t *i2cbus_find_device(uint32_t id);
static void i2cbus_kill_master(i2c_master_bus_handle_t bus);

static i2cbus_device_list_t *i2c_devices = NULL;
static i2cbus_master_list_t *i2c_masters = NULL;
static cbus_driver_t *cbus_i2c = NULL;

cbus_common_id_t cbus_i2c_attach(cbus_device_config_t *payload) {
    uint64_t pinmask = (BIT64(payload->i2c_device.scl_gpio) | BIT64(payload->i2c_device.sda_gpio));
    i2cbus_master_list_t *master_bus = i2cbus_find_master(payload->i2c_device.scl_gpio, payload->i2c_device.sda_gpio);
    if(!master_bus) {
        if(!gpio_drv_reserve_pins(pinmask)) return (cbus_common_id_t) { .error = CBUS_ERR_PIN_IN_USE, .id = 0x00000000UL };
        i2c_master_bus_config_t i2c_bus_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = -1,
            .scl_io_num = payload->i2c_device.scl_gpio,
            .sda_io_num = payload->i2c_device.sda_gpio, 
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true
        };
        master_bus = (i2cbus_master_list_t *)calloc(1, sizeof(i2cbus_master_list_t));
        if(!master_bus) {
            gpio_drv_free_pins(pinmask);
            return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
        }
        esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &(master_bus->master));
        if(ESP_OK != err) {
            free(master_bus);
            gpio_drv_free_pins(pinmask);
            return (cbus_common_id_t) { .error = ( ESP_ERR_NOT_FOUND == err ? CBUS_ERR_NO_MORE_BUSES : CBUS_ERR_UNKNOWN ), .id = 0x00000000UL };
        }
        if(i2c_masters) master_bus->next = i2c_masters;
        i2c_masters = master_bus;
    }
    /* Attach device */
    i2cbus_device_id_t new_id = (i2cbus_device_id_t) {
        .gpioscl = payload->i2c_device.scl_gpio,
        .gpiosda = payload->i2c_device.sda_gpio,
        .i2caddr = payload->i2c_device.device_address,
        .i2cbus = master_bus->master->base->port_num
    };

    if(i2cbus_find_device(new_id.id)) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_EXIST, .id = new_id.id };

    i2c_master_dev_handle_t *new_device_handle = (i2c_master_dev_handle_t *)calloc(1, sizeof(i2c_master_dev_handle_t));
    i2c_device_config_t device_conf = (i2c_device_config_t) { 
        .dev_addr_length = payload->i2c_device.dev_addr_length, 
        .device_address = payload->i2c_device.device_address,
        .scl_speed_hz = 10000 * payload->i2c_device.scl_speed_hz,
        .flags.disable_ack_check = payload->i2c_device.disable_ack_check
    };
    esp_err_t err = i2c_master_bus_add_device(master_bus->master, &device_conf, new_device_handle);
    if( ESP_OK != err ) 
    {
        free(new_device_handle);
        i2cbus_kill_master(master_bus->master);
        return (cbus_common_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = 0x00000000UL };
    }
    i2cbus_device_list_t *new_device_entry = (i2cbus_device_list_t *)calloc(1, sizeof(i2cbus_device_list_t));
    if(!new_device_entry) {
        i2c_master_bus_rm_device(*new_device_handle);
        free(new_device_handle);
        i2cbus_kill_master(master_bus->master);
        return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
    }
    *new_device_entry = (i2cbus_device_list_t) {
        .handle = new_device_handle,
        .device_id = new_id.id,
        .next = i2c_devices
    };
    new_device_entry->handle = new_device_handle;
    new_device_entry->device_id = new_id.id;
    new_device_entry->next = 
    /* END Attach device */
}
cbus_common_id_t cbus_i2c_deattach(uint32_t id) {
    //return id;
}


static i2cbus_master_list_t *i2cbus_find_master(gpio_num_t scl, gpio_num_t sda) {
    i2cbus_master_list_t *master_element = i2c_masters;
    bool found = false;
    while(master_element && !found) {
        if(master_element->master->base->scl_num == scl && master_element->master->base->sda_num == sda) found=true;
        else master_element = master_element->next;
    }
    return master_element;
}

static i2cbus_device_list_t *i2cbus_find_device(uint32_t id){
    i2cbus_device_list_t *found = NULL;
    for(i2cbus_device_list_t *dev = i2c_devices; dev && !found; dev=dev->next) { 
        if(dev->device_id == id) found = dev;
    }
    return found;
}

static void i2cbus_kill_master(i2c_master_bus_handle_t bus) {
    if(!bus->device_list.slh_first) {
        i2cbus_master_list_t *last_master = i2c_masters;
        i2cbus_master_list_t *free_master = NULL;
        if(last_master->master == bus) { 
            i2c_masters = last_master->next;
            free_master = last_master;
        }
        else { 
            while( last_master && (last_master->next->master != bus)) last_master = last_master->next;
            if(last_master) {
                free_master = last_master->next;
                last_master->next = last_master->next->next;
                gpio_drv_free_pins( BIT64(bus->base->scl_num) | BIT64(bus->base->sda_num));
                if(ESP_OK != i2c_del_master_bus(bus)) {

                } else {

                }
            }
        }
        free(free_master);
    }
}

//cbus_driver_t *i2cbus_get_bus(void) {
void *i2cbus_get_bus(void) {
    if(!cbus_i2c) { 
        cbus_i2c = (cbus_driver_t *)calloc(1, sizeof(cbus_driver_t));
        if(cbus_i2c) {
            cbus_i2c->attach = &cbus_i2c_attach;
            cbus_i2c->deattach = &cbus_i2c_deattach;
        }
    }
    return cbus_i2c;
}
