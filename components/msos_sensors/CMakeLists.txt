idf_component_register(
    SRCS "src/msos_sensors.c"
    SRCS "src/ds18b20/msos_ds18b20.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_timer cbus_driver
)
