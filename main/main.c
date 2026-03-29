#include "Lap_Time.h"
#include "ESPNow.h"

#define LoRaLED 6

#define GPIO_PIN 7

#define NSS 10 
#define MOSI 11
#define SPI_CLK 12
#define MISO 13
#define RST 48
#define DIO0PIN 21
#define DIO1 47
#define SPI_SPEED 10000000
#define LORA_DEFAULT_FREQUENCY 915000000

#define MAIN_TIMER 1

//Declare global variable
params globalParams;

void app_main(void)
{
    // SPI is initialized in SPI module if needed
    // Pins are defined as macros above (NSS, MOSI, SPI_CLK, MISO)
    espnow_init();
    start_timer();

    lt_timeval lastTime;
    gettimeofday(&lastTime, NULL);
    globalParams.lastTime = lastTime;
    globalParams.pin = GPIO_PIN;

    xTaskCreate(laser_processing_task, "Laser Processing Task", 4096, NULL, 5, NULL);

    #if (MAIN_TIMER == 1)

    gpio_set_up(&globalParams);
    
    xTaskCreate(lap_processing_task, "Lap Processing Task", 4096, &globalParams, 5, NULL);

    Main_Timer_Setup();

    xTaskCreatePinnedToCore(espnow_Ritual, "ESP Now Ritual Task", 8192, NULL, 2, NULL, 0);

    // #elif (MAIN_TIMER == 2) 
    // Paddock_Timer_Setup();

    #else
    gpio_set_up(&globalParams);

    Seg_Timer_Setup();

    xTaskCreate(espnow_seg, "Segment Shit", 4096, NULL, 2, NULL);    

    #endif

    while(1) {
            vTaskDelay(1000);
    }
}
    