idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_wifi
        esp_netif
        esp_http_server
        fatfs
        sdmmc
        nvs_flash
        esp_event
        log
)
