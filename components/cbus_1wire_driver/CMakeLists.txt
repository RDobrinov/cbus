idf_component_register(
    SRCS "src/cbus_1wire_driver.c"
    "src/onewire/onewire_bus_api.c"
    "src/onewire/onewire_bus_impl_rmt.c"
    "src/onewire/onewire_crc.c"
    "src/onewire/onewire_device.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event driver
)
