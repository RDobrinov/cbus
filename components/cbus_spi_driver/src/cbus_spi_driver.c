#include "cbus_spi_driver.h"

#define SPIBUS_HOST_MAX (SPI_HOST_MAX - 1)

/**
 * @brief Type of unique ID for attached spi device
*/
typedef union {
    struct {
        uint32_t miso_gpio:7;   /*!< MISO IO Pin */
        uint32_t mosi_gpio:7;   /*!< MOSI IO Pin */
        uint32_t sclk_gpio:7;   /*!< SCLK IO Pin */
        uint32_t cs_gpio:7;     /*!< CS IO Pin */
        uint32_t host_id:4;     /*!< SPI Host number */
    };
    uint32_t id;                /*!< Single spi device ID */
} spibus_device_id_t;

typedef struct spibus_host {
    union {
        struct {
            uint32_t miso_gpio:7;
            uint32_t mosi_gpio:7;
            uint32_t sclk_gpio:7;
            uint32_t host_id:4;
            uint32_t free:1;
            uint32_t reserved:6;
        };
        uint32_t val;
    };
} spibus_host_t;

typedef struct spibus_device_list {
    spibus_device_id_t device_id;
    spi_device_handle_t handle;
    struct spibus_device_list *next;
} spibus_device_list_t;

static spibus_host_t spi_hosts[SPIBUS_HOST_MAX];
static cbus_driver_t *cbus_spi = NULL;  /*!< cbus driver handle */
static spibus_device_list_t *devices;

/**
 * @brief Attach new spi device to bus
 *
 * @param[in] payload pointer to device configuration
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_common_id_t cbus_spi_attach(cbus_device_config_t *payload);

/**
 * @brief Deatach spi device from bus
 *
 * @param[in] id Device ID
 * @return
 *      - Common ID structure filled with result code and Device ID
 * @note
 *      - Remove device from list, free device handler and free SPI HOST
 *      if no devices left
 */
static cbus_common_id_t cbus_spi_deattach(uint32_t id);

/**
 * @brief Generate SPI device description
 *
 * @param[in] id Device ID
 * @param[in] len Maximum length for device description
 * @param[out] desc Pointer to char array for description
 * 
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_common_id_t cbus_spi_description(uint32_t id, uint8_t *desc, size_t len);

/**
 * @brief Execute command on spi bus
 *
 * @param[in] payload Pointer to Common CBUS command structure
 * 
 * @return
 *      - Common ID structure filled with result code and Device ID
 */
static cbus_common_id_t cbus_spi_command(cbus_common_cmd_t *payload);

static spi_host_device_t spibus_get_host(uint32_t host_pins);


/**
 * @brief Find device in internal list
 *
 * @param[in] id Device id
 * @return
 *      - Pointer to internal device list element
 */
static spibus_device_list_t *spibus_find_device(uint32_t id);

static cbus_common_id_t cbus_spi_attach(cbus_device_config_t *payload) {
    if(payload->bus_type != CBUS_BUS_SPI) return (cbus_common_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = 0x00000000UL };
    spibus_device_id_t new_device_id = { 
        .miso_gpio = payload->spi_device.miso_gpio,
        .mosi_gpio = payload->spi_device.mosi_gpio, 
        .sclk_gpio = payload->spi_device.sclk_gpio,
        .cs_gpio = payload->spi_device.cs_gpio
    };
    new_device_id.host_id = spibus_get_host( new_device_id.id );
    if(spibus_find_device(new_device_id.id)) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_EXIST, .id = new_device_id.id };
    uint64_t bus_pinmask = (
            BIT64(payload->spi_device.miso_gpio) | BIT64(payload->spi_device.mosi_gpio) | 
            BIT64(payload->spi_device.sclk_gpio) | BIT64(payload->spi_device.cs_gpio));
    esp_err_t ret = ESP_OK;
    uint8_t bus_index = 0;
    bool new_bus = false;
    if(new_device_id.host_id == SPI_HOST_MAX) {
        while (bus_index < SPIBUS_HOST_MAX && !spi_hosts[bus_index].free) bus_index++;
        if(bus_index == SPIBUS_HOST_MAX) return (cbus_common_id_t) { .error = CBUS_ERR_NO_MORE_BUSES, .id = 0x00000000UL };
        if(gpio_drv_reserve_pins(bus_pinmask)) return (cbus_common_id_t) { .error = CBUS_ERR_PIN_IN_USE, .id = 0x00000000UL };
        new_device_id.host_id = spi_hosts[bus_index].host_id;
        /* Probe new host */
        spi_bus_config_t bus_config = {
            .mosi_io_num = new_device_id.mosi_gpio,
            .miso_io_num = new_device_id.miso_gpio,
            .sclk_io_num = new_device_id.sclk_gpio,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 32       //payload is max 128 TO CHANGE
        };
        ret = spi_bus_initialize(new_device_id.host_id, &bus_config, SPI_DMA_CH_AUTO);
        if(ESP_OK != ret) {
            gpio_drv_free_pins(bus_pinmask);
            return (cbus_common_id_t) { .error = ( ESP_ERR_INVALID_STATE == ret ? CBUS_ERR_NO_MORE_BUSES : CBUS_ERR_UNKNOWN ), .id = 0x00000000UL };
        }
        new_bus = true;
    }

    if(!new_bus && gpio_drv_reserve(payload->spi_device.cs_gpio)) {
        return (cbus_common_id_t) { .error = CBUS_ERR_PIN_IN_USE, .id = 0x00000000UL };
    }

    spibus_device_list_t *new_element = (spibus_device_list_t *)calloc(1, sizeof(spibus_device_list_t));
    if(!new_element) {
        if(new_bus) {
            spi_bus_free(new_device_id.host_id);
            gpio_drv_free_pins(bus_pinmask);
        }
        return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
    }

    spi_device_interface_config_t dev_config = {
        .spics_io_num = new_device_id.cs_gpio,
        .mode = payload->spi_device.mode,
        .command_bits = payload->spi_device.cmd_bits,
        .address_bits = payload->spi_device.addr_bits,
        .dummy_bits = payload->spi_device.dummy_bits,
        .clock_speed_hz = payload->spi_device.clock_speed,
        .cs_ena_pretrans = payload->spi_device.pretrans,
        .cs_ena_posttrans = payload->spi_device.postrans,
        .input_delay_ns = payload->spi_device.input_delay,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX | payload->spi_device.flags
    };

    ret = spi_bus_add_device(new_device_id.host_id, &dev_config, &(new_element->handle));
    if(ret != ESP_OK) {
        if(new_bus) {
            spi_bus_free(new_device_id.host_id);
            gpio_drv_free_pins(bus_pinmask);
        }
        else gpio_drv_free(dev_config.spics_io_num);
        free(new_element);
        return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
    }
    new_element->device_id = new_device_id;
    new_element->next = devices;
    devices = new_element;
    if(new_bus) {
        spi_hosts[bus_index].val = ( spi_hosts[bus_index].val & 0xFFE00000UL ) | ( new_element->device_id.id & 0x1FFFFFUL );
        spi_hosts[bus_index].free = 0;
    }
    return (cbus_common_id_t) { .error = CBUS_OK, .id = new_device_id.id };
}

static cbus_common_id_t cbus_spi_deattach(uint32_t id) {
    spibus_device_list_t *device = devices, *prev = NULL;
    while(device) {
        if(device->device_id.id == id) break;
        prev = device;
        device = device->next;
    }
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };
    esp_err_t ret = spi_bus_remove_device(device->handle);
    if(ESP_OK != ret && ESP_ERR_INVALID_STATE != ret) return (cbus_common_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = id }; //Strange state. Bad argument for handle
    if(prev) prev->next = device->next;
    else devices = device->next;
    ret = spi_bus_free(device->device_id.host_id);
    if(ret == ESP_OK) {
        for(int i = 0; i < SPIBUS_HOST_MAX; i++) {
            if(spi_hosts[i].host_id == device->device_id.host_id) {
                spi_hosts[i].free = true;
                spi_hosts[i].val = ( spi_hosts[i].val & 0xFFE00000UL );
                break;
            }
        }
    }

    gpio_drv_free_pins(BIT64(device->device_id.miso_gpio) | BIT64(device->device_id.mosi_gpio) |
            BIT64(device->device_id.sclk_gpio) | BIT64(device->device_id.cs_gpio) );
    free(device);
    return (cbus_common_id_t) { .error = CBUS_OK, .id = id };
}

static cbus_common_id_t cbus_spi_description(uint32_t id, uint8_t *desc, size_t len) {
    spibus_device_list_t *device = spibus_find_device(id);
    if(!device || !desc || !len) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = id };
    char *buf = (char *)calloc(30, sizeof(uint8_t));
    sprintf(buf, "CS%02X @ spi/p%02ucl%02udo%02udi%02u", device->device_id.cs_gpio, device->device_id.host_id, 
        device->device_id.sclk_gpio, device->device_id.mosi_gpio, device->device_id.miso_gpio);
    memcpy(desc, buf, len);
    free(buf);
    desc[len-1] = 0x00;
    return (cbus_common_id_t) { .error = CBUS_OK, .id = id };
}

static cbus_common_id_t cbus_spi_command(cbus_common_cmd_t *payload) {
    spibus_device_list_t *device = spibus_find_device(payload->device_id);
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = payload->device_id };
    esp_err_t ret = ESP_OK;
    uint8_t *tx_target;
    spi_transaction_t spibus_execute = {
        .addr = (*(spibus_cmdaddr_t *)payload->data).address,
        .cmd =  (*(spibus_cmdaddr_t *)payload->data).command,
        .rxlength = (payload->outDataLen) << 3,
        //.length = (payload->inDataLen + payload->outDataLen) << 3,
        .tx_buffer = NULL,
        .rx_buffer = NULL
    };

    switch (payload->command) {
        case CBUSCMD_READ:
            if(!(payload->outDataLen > 0)) return (cbus_common_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = payload->device_id };
            spibus_execute.length = spibus_execute.rxlength;
            spibus_execute.rx_buffer = payload->data;
            break;
        case CBUSCMD_WRITE:
            if(!(payload->inDataLen > 0)) return (cbus_common_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = payload->device_id };
            spibus_execute.length = (payload->inDataLen) << 3;      //What if in and out > 0?
            tx_target = spibus_execute.tx_data;
            if(payload->inDataLen > 4) {
                spibus_execute.tx_buffer = (uint8_t *)calloc(payload->inDataLen, sizeof(uint8_t));
                if(!spibus_execute.tx_buffer) return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = payload->device_id };
                tx_target = spibus_execute.tx_buffer;
                if(payload->inDataLen <= 4) spibus_execute.flags |= SPI_TRANS_USE_TXDATA;
                memcpy( tx_target, &(payload->data[sizeof(spibus_cmdaddr_t)]), payload->inDataLen);
            }
            break;
        default:
            return (cbus_common_id_t) { .error = CBUS_ERR_NOT_USED, .id = payload->device_id };
            break;
    }

    ret = spi_device_polling_transmit(device->handle, &spibus_execute);
    free(spibus_execute.tx_buffer);
    return (cbus_common_id_t) { .error = (ESP_OK == ret) ? CBUS_OK : CBUS_ERR_UNKNOWN, .id = payload->device_id };
}

/**/
static spi_host_device_t spibus_get_host(uint32_t host_pins) {
    for(uint8_t i=0; i< SPIBUS_HOST_MAX; i++) {
        if(!spi_hosts[i].free) {
            if( !((host_pins ^ spi_hosts[i].val) & 0x1FFFFUL))  return spi_hosts[i].host_id;
        }
    }
    return SPI_HOST_MAX;
}

static spibus_device_list_t *spibus_find_device(uint32_t id) {
    for(spibus_device_list_t *device = devices; device != NULL; device = device->next)
        if(device->device_id.id == id) return device;
    return NULL;
}

void *spi_get_bus(void) {
    if(!cbus_spi) { 
        cbus_spi = (cbus_driver_t *)calloc(1, sizeof(cbus_driver_t));
        if(cbus_spi) {
            cbus_spi->attach = &cbus_spi_attach;
            cbus_spi->deattach = &cbus_spi_deattach;
            cbus_spi->desc = &cbus_spi_description;
            cbus_spi->execute = &cbus_spi_command;
        }
        for(spi_host_device_t spi = SPI2_HOST; spi < SPI_HOST_MAX; spi++) spi_hosts[spi-1] = (spibus_host_t){ .host_id = spi, .free = 1};
        /* Test */
        //for(uint8_t i=0; i < (sizeof(spi_buses) / sizeof(spibus_hosts_t)); i++) printf("SPI%02d\n", spi_buses[i].host_id);
    }
    return cbus_spi;
}