idf_component_register(
    SRCS "src/cbus_spi_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event driver cbus_driver idf_gpio_driver esp_driver_spi
)
