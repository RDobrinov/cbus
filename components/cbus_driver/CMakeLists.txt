idf_component_register(
    SRCS "src/cbus_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event driver cbus_i2c_driver cbus_spi_driver cbus_1wire_driver
)
