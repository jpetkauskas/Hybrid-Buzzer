idf_component_register(SRCS "webserver.c" "buzz.c" "wireless.c" "button.c" "config.c" "main.c"
                    PRIV_REQUIRES spi_flash driver freertos esp_timer packet esp_wifi nvs_flash esp_http_server esp_netif
                    INCLUDE_DIRS "")
