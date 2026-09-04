idf_component_register(
    SRCS "main.c" "display.c" "display_st7789.c" "status_led.c" "board.c"
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
        esp_lcd
    PRIV_REQUIRES esp_app_format esp_timer led_strip esp_driver_tsens
)
