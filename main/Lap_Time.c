#include "Lap_Time.h"
#include "ESPNow.h" 

static SemaphoreHandle_t laser_signal;
StaticSemaphore_t laser_signal_buffer;
timer laptimer;

lt_timeval current_time;
lt_timeval last_time;

static volatile bool laser_tripped = false;
static volatile uint32_t last_interrupt_tick = 0;

void start_timer() 
{
    laptimer.minutes = 0;
    laptimer.seconds = 0;
    laptimer.microSeconds = 0;
    laptimer.lap = 0;

    gettimeofday(&current_time, NULL);
    gettimeofday(&last_time, NULL);
}

void lap_triggered(void *param) 
{    
    gettimeofday(&current_time, NULL);
    
    //Convert current and last times to uint64_t
    uint64_t currentTimeUs = (uint64_t) current_time.tv_sec * 1000000 + current_time.tv_usec;
    uint64_t lastTimeUs = (uint64_t) last_time.tv_sec * 1000000 + last_time.tv_usec;

    //Total time elapsed in microseconds (Us)
    uint64_t totalMicroseconds = currentTimeUs - lastTimeUs;

    // DEBUG: printf("%lld\n", (uint64_t) totalMicroseconds);
    if (totalMicroseconds < 2000000) { //Check if lap is valid
        last_time.tv_sec = current_time.tv_sec;
        last_time.tv_usec = current_time.tv_usec;
        ESP_LOGW ("Lap too short, ignoring", "Lap: %d, Time: %d:%02d.%06llu",
                (laptimer.lap), (totalMicroseconds / 60000000), ((totalMicroseconds % 60000000) / 1000000), (totalMicroseconds % 1000000));
        return;
    }

    //Get minutes and seconds from totalMicroseconds
    uint16_t minutes = totalMicroseconds / 60000000; 
    uint8_t seconds = (totalMicroseconds % 60000000) / 1000000;

    //Update the timer
    laptimer.microSeconds = totalMicroseconds % 1000000; //Remainder as microseconds
    laptimer.lap++;
    laptimer.minutes = minutes;
    laptimer.seconds = seconds;

    //Update lastTime for the next lap
    last_time.tv_sec = current_time.tv_sec;
    last_time.tv_usec = current_time.tv_usec;

    ESP_LOGW ("Updated lap time", "Lap: %d, Time: %d:%02d.%06llu",
    (laptimer.lap), (minutes), (seconds), (uint64_t) laptimer.microSeconds);
}

void IRAM_ATTR gpioHandler(void *params) 
{
    xSemaphoreGive(laser_signal);
}


//GPIO Setup
esp_err_t gpio_set_up(params *parameters) 
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE; //???
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << parameters->pin);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    esp_err_t error = gpio_config(&io_conf);
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE); // Install ISR service
    gpio_isr_handler_add((gpio_num_t)parameters->pin, gpioHandler, (void *)parameters); // Add ISR handler

    return error;
}

void lap_processing_task(void *pvParameter) 
{
   lap_triggered(pvParameter);

   while (true) {
        vTaskDelay(pdMS_TO_TICKS(25));
    
        int current_tick = xTaskGetTickCount();
        // DEBUG: printf("Last Tick: %ld, Current Tick: %d, pdMS_TO_TICKS: %ld\n", last_interrupt_tick, current_tick, pdMS_TO_TICKS(100));
        if (current_tick - last_interrupt_tick > pdMS_TO_TICKS(100)) { //100 -> 2000 maybe?
           if (!laser_tripped)
           {
              lap_triggered(pvParameter);
              laser_tripped = true;
           }
        }
    }
}

void laser_processing_task(void *pvParameter) 
{
    laser_signal = xSemaphoreCreateBinaryStatic(&laser_signal_buffer);
    while (true) {
        xSemaphoreTake(laser_signal, portMAX_DELAY);
        last_interrupt_tick = xTaskGetTickCount(); 
        laser_tripped = false; 
    }
}


esp_err_t compose_LoRa_msg(uint8_t * data, uint16_t * len) {
    // "LT": <index>, <min>:<sec>::<ms>

    //TimeStamp Data 
    uint32_t* timeStamp = (uint32_t*) data; //4 bytes
    *(timeStamp) = 0; //We aren't using this

    //"Car_Number" 
    uint32_t* carNum = (timeStamp + 1);  //4 bytes
    *carNum = -1;

    data[8] = laptimer.lap /256;
    data[9] = laptimer.lap % 256; 
    data[10] = laptimer.minutes % 256;
    data[11] = laptimer.seconds;

    int milliSec = laptimer.microSeconds/1000;
    data[12] = milliSec/256;
    data[13] = milliSec % 256;
    //8 onward - room for segment times

    *(len) = 14;

    return ESP_OK;
}

void espnow_seg()
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(25));
        
        int current_tick = xTaskGetTickCount();
 
        // DEBUG: 
        // printf("Last Tick: %ld, Current Tick: %d, pdMS_TO_TICKS: %ld\n", last_interrupt_tick, current_tick, pdMS_TO_TICKS(100));
        
        if (current_tick - last_interrupt_tick > pdMS_TO_TICKS(25)) { //100 -> 2000 maybe?
            if(!laser_tripped)
            {
                ESPNow_Send_Trigger();
                laser_tripped = true;
            }
        }
    }
}