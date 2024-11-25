
#include "cbus_1wire_driver.h"
#include "onewire/onewire_bus.h"
#include "esp_mac.h"
#include "driver/gpio.h"
#include "../src/rmt_private.h"

//uint16_t _genid(uint8_t *data) //crc16_mcrf4xx
uint16_t _genid(uint8_t *data)
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

uint16_t cbus_1wire_init(void) {
    OneWire_ROMCode rcode = (OneWire_ROMCode) { .raw_address = { 0x28, 0x02, 0x01, 0x03, 0x04, 0x05, 0x06, 0x99} };
    printf("%p\n", rcode.raw_address);
    uint16_t id = _genid(rcode.raw_address);
    printf("%p\n", rcode.raw_address);
    return id;
}

void test_channles() {
    printf("test 39\n");
    onewire_bus_handle_t bus;
    onewire_bus_config_t bus_config = { .bus_gpio_num = GPIO_NUM_22};
    onewire_bus_rmt_config_t rmt_config = {.max_rx_bytes = 10};
    onewire_new_bus_rmt(&bus_config, &rmt_config, &bus);
    if(bus) printf("test 43\n");
    onewire_bus_rmt_obj_t *bus_rmt = __containerof(bus, onewire_bus_rmt_obj_t, base);
    printf("TXRMTCH%2d, RXRMTCH%2d\n", bus_rmt->tx_channel->channel_id, bus_rmt->rx_channel->channel_id);
}