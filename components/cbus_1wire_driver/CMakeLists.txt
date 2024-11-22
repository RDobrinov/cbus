idf_component_register(
    SRCS "src/cbus_1wire_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event driver
)
