idf_component_register (
    SRCS "Spi.c" "ESPNow.c" "Lap_Time.c" "main.c"
    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi esp_driver_spi driver esp_timer
    INCLUDE_DIRS ".")
