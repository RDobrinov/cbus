idf_component_register(
    SRCS "src/cbus_i2c_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event driver cbus_driver idf_gpio_driver
)
