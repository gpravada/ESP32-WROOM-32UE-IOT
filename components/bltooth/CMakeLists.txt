idf_component_register(SRCS "ble_app.c"
                            "gatt_svr.c"
                       INCLUDE_DIRS "include" "../bmp280/include"
                       REQUIRES bmp280 driver bt nvs_flash)
