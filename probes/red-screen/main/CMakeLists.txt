idf_component_register(SRCS "red_screen.c"
                            "../../../../TinyPetz/firmware/bringup/main/board.c"
                            "../../../../TinyPetz/firmware/bringup/main/panel.c"
                       INCLUDE_DIRS "."
                                    "../../../../TinyPetz/firmware/bringup/main"
                       REQUIRES driver esp_driver_i2c esp_driver_spi esp_driver_gpio esp_timer)
