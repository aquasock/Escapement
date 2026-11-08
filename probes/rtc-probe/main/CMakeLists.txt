idf_component_register(SRCS "probe.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_driver_i2c esp_timer)
