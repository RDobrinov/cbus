
#include "cbus_1wire_driver.h"
#include "onewire/onewire_bus.h"
#include "onewire/onewire_device.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "../src/rmt_private.h"

#include "esp_log.h"

typedef union {
    struct {
        uint32_t gpio:7;
        uint32_t tx_channel:3;
        uint32_t rx_channel:3;
        uint32_t reserved:3;
        uint32_t intr_id:16;
    };
    uint32_t id;
} ow_device_id_t;

typedef struct owbus_device_list {
    uint32_t device_id;
    uint64_t address;
    onewire_bus_handle_t handle;
    struct owbus_device_list *next;
} owbus_device_list_t;

static owbus_device_list_t *ow_devices = NULL;
static cbus_driver_t *cbus_ow = NULL;

static cbus_common_id_t cbus_ow_attach(cbus_device_config_t *payload);
static cbus_common_id_t cbus_ow_deattach(uint32_t id);
static cbus_common_id_t cbus_ow_command(cbus_common_cmd_t *payload);

static onewire_bus_handle_t owbus_find_handle(gpio_num_t gpio);
static owbus_device_list_t *owbus_find_device(uint32_t id);
static uint16_t _genid(uint8_t *data);
static uint64_t _swap_romcode(uint64_t val);

/* Bus handlers */
static cbus_common_id_t cbus_ow_attach(cbus_device_config_t *payload) {
    if(payload->bus_type != CBUS_BUS_1WIRE) return (cbus_common_id_t) { .error = CBUS_ERR_BAD_ARGS, .id = 0x00000000UL };
    onewire_bus_handle_t handle = owbus_find_handle(payload->ow_device.data_gpio);
    if(!handle) {
        /* -> RMT channel acquisition & creat OneWire bus */
        if(!gpio_drv_reserve(payload->ow_device.data_gpio)) return (cbus_common_id_t) { .error = CBUS_ERR_PIN_IN_USE, .id = 0x00000000UL };
        onewire_bus_config_t bus_config = { .bus_gpio_num = payload->ow_device.data_gpio};
        onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes = 10};
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
        onewire_bus_del(handle);
        gpio_drv_free(payload->ow_device.data_gpio);
        return (cbus_common_id_t) { .error = CBUS_ERR_NO_MEM, .id = 0x00000000UL };
    }

    new_device_entry->address = _swap_romcode(*((uint64_t *)(payload->ow_device.rom_code.raw_address)));
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
    printf("ow_id %08lX, %016llx, %016llx\n", new_device_entry->device_id, *((uint64_t*)(payload->ow_device.rom_code.raw_address)), new_device_entry->address);
    if(owbus_find_device(new_id.id)) {
        free(new_device_entry);
        return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_EXIST, .id = new_id.id };
    }

    ow_devices = new_device_entry;
    return (cbus_common_id_t) { .error = CBUS_OK, .id = new_device_entry->device_id };
}

static cbus_common_id_t cbus_ow_deattach(uint32_t id) {
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

static cbus_common_id_t cbus_ow_command(cbus_common_cmd_t *payload) {
    owbus_device_list_t *device = owbus_find_device(payload->device_id);
    if(!device) return (cbus_common_id_t) { .error = CBUS_ERR_DEVICE_NOT_FOUND, .id = payload->device_id };
    esp_err_t err = ESP_OK;
    switch (payload->command) {
        case CBUSCMD_READ:
            err = onewire_bus_read_bytes(device->handle, payload->data, payload->outDataLen);
            //err = i2c_master_transmit(device->handle, payload->data, payload->inDataLen, device->xfer_timeout_ms);
            break;
        case CBUSCMD_WRITE:
            err = onewire_bus_write_bytes(device->handle, payload->data, payload->inDataLen);
            //err = i2c_master_receive(device->handle, payload->data, payload->outDataLen, device->xfer_timeout_ms);
            break;
        case CBUSCMD_RW:
            err = onewire_bus_write_bytes(device->handle, payload->data, payload->inDataLen);
            err = onewire_bus_read_bytes(device->handle, payload->data, payload->outDataLen);
            //err = i2c_master_transmit_receive(device->handle, payload->data, payload->inDataLen, payload->data, payload->outDataLen, device->xfer_timeout_ms);
            break;
        case CBUSCMD_RESET:
            err = onewire_bus_reset(device->handle);
            break;
        default:
            return (cbus_common_id_t) { .error = CBUS_ERR_NOT_USED, .id = payload->device_id };
            break;
    }
    return (cbus_common_id_t) { .error = ( (ESP_OK == err) ? CBUS_OK : ((ESP_ERR_TIMEOUT == err) ? CBUS_ERR_TIMEOUT : CBUS_ERR_UNKNOWN) ), .id = payload->device_id }; 
}

static onewire_bus_handle_t owbus_find_handle(gpio_num_t gpio) {
    if(!ow_devices) return NULL;
    owbus_device_list_t *found = NULL;
    onewire_bus_rmt_obj_t *bus_rmt = NULL;
    for(owbus_device_list_t *dev = ow_devices; dev && !found; dev=dev->next) { 
        bus_rmt = __containerof(dev->handle, onewire_bus_rmt_obj_t, base);
        if(bus_rmt->tx_channel->gpio_num == gpio) found=dev;
    }
    return (found) ? found->handle : NULL;
}
static owbus_device_list_t *owbus_find_device(uint32_t id) {
    owbus_device_list_t *found = NULL;
    for(owbus_device_list_t *dev = ow_devices; dev && !found; dev=dev->next) { 
        if(dev->device_id == id) found = dev;
    }
    return found;
}

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

uint16_t cbus_1wire_init(void) {
    OneWire_ROMCode rcode = (OneWire_ROMCode) { .raw_address = { 0x28, 0x02, 0x01, 0x03, 0x04, 0x05, 0x06, 0x99} };
    printf("%p\n", rcode.raw_address);
    uint16_t id = _genid(rcode.raw_address);
    printf("%p\n", rcode.raw_address);
    return id;
}

void test_channles() {
    onewire_bus_handle_t bus;
    onewire_bus_config_t bus_config = { .bus_gpio_num = GPIO_NUM_22};
    onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes = 10};
    onewire_new_bus_rmt(&bus_config, &rmt_config, &bus);
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(bus, onewire_bus_rmt_obj_t, base);
    printf("TXRMTCH%2d, RXRMTCH%2d\n", bus_rmt->tx_channel->channel_id, bus_rmt->rx_channel->channel_id);
    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next;
    esp_err_t sresult = ESP_OK;
    if(ESP_OK == onewire_new_device_iter(bus, &iter)) printf("Device iterator created\n");
    do {
        sresult = onewire_device_iter_get_next(iter, &next);
        if( ESP_OK == sresult ){
            printf("%016llX\n", next.address);
        }
    } while (ESP_ERR_NOT_FOUND != sresult);
}

void *ow_get_bus(void) {
    if(!cbus_ow) { 
        cbus_ow = (cbus_driver_t *)calloc(1, sizeof(cbus_driver_t));
        if(cbus_ow) {
            cbus_ow->attach = &cbus_ow_attach;
            cbus_ow->deattach = &cbus_ow_deattach;
            cbus_ow->read = &cbus_ow_command;
            cbus_ow->write = &cbus_ow_command;
            cbus_ow->rw = &cbus_ow_command;
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