idf_component_register(SRCS "wifi_app.c"
                    INCLUDE_DIRS "include"
                    REQUIRES bmp280 esp_wifi esp_netif esp_http_server esp_http_client nvs_flash spi_flash lwip)
