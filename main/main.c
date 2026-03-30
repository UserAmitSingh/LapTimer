#include "Lap_Time.h"
#include "ESPNow.h"

// #define LoRaLED 6
// #define NSS 10 
// #define MOSI 11
// #define SPI_CLK 12
// #define MISO 13
// #define RST 48
// #define DIO0PIN 21
// #define DIO1 47
// #define SPI_SPEED 10000000
// #define LORA_DEFAULT_FREQUENCY 915000000
// SPI is initialized in SPI module if needed
// Pins are defined as macros above (NSS, MOSI, SPI_CLK, MISO)


void app_main(void) {
    QueueHandle_t shared_queue = xQueueCreate(5, LT_data_len); // Queue for sharing lap times between tasks
    //Setup ESP-NOW and Lap Time modules
    espnow_init(shared_queue);
    Lap_Time_init(shared_queue);


    //Now logic for ESP-NOW communication and lap timing is handled in their respective tasks and callbacks
    Main_Timer_Setup();

    xTaskCreatePinnedToCore(espnow_Ritual, "ESP Now Ritual Task", 8192, NULL, 2, NULL, 0);

    Seg_Timer_Setup();

    while(1) {
            vTaskDelay(1000);
    }
}
    