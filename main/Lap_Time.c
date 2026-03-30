#include "Lap_Time.h"

#define TAG ((const char *const) "Lap_Time")

//Laser State Variables
static SemaphoreHandle_t laser_signal;
StaticSemaphore_t laser_signal_buffer;
static volatile bool laser_tripped = false;
static volatile uint32_t last_interrupt_tick = 0;

//Timer State Variables
timer laptimer;
lt_timeval current_time;
lt_timeval last_time;


//Queue for sending lap times to ESP-NOW task
static QueueHandle_t lap_time_queue = NULL; //Queue for sending lap times to ESP-NOW task (non-blocking)

//Initialization function for Lap Time module
esp_err_t Lap_Time_init(QueueHandle_t lt_espnow_queue) {
    if(lt_espnow_queue == NULL) {
        ESP_LOGE(TAG, "ESP-NOW queue handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    lap_time_queue = lt_espnow_queue; //Set the global queue handle for sending lap times to ESP-NOW task

    // Initialize laser handling
    esp_err_t err = laser_setup(LASER_PIN); 
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to setup laser: %s", esp_err_to_name(err));
        return err;
    }

    // Initialize timer variables
    start_timer();

    // Create tasks for processing lap times and laser signals
    xTaskCreate(lap_processing_task, "LapProcessingTask", 4096, NULL, 5, NULL);
    xTaskCreate(laser_processing_task, "LaserProcessingTask", 4096, NULL, 5, NULL);

    return ESP_OK;
}

//Lap Time handling functions
void start_timer() {
    laptimer.minutes = 0;
    laptimer.seconds = 0;
    laptimer.microSeconds = 0;

    gettimeofday(&current_time, NULL);
    gettimeofday(&last_time, NULL);
}

void lap_triggered() {    
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
        ESP_LOGW ("Lap too short, ignoring", "Time: %d:%02d.%06llu",
            (totalMicroseconds / 60000000), ((totalMicroseconds % 60000000) / 1000000), (totalMicroseconds % 1000000));
        return;
    }

    //Get minutes and seconds from totalMicroseconds
    uint16_t minutes = totalMicroseconds / 60000000; 
    uint8_t seconds = (totalMicroseconds % 60000000) / 1000000;

    //Update the timer
    laptimer.microSeconds = totalMicroseconds % 1000000; //Remainder as microseconds
    laptimer.minutes = minutes;
    laptimer.seconds = seconds;

    //Update lastTime for the next lap
    last_time.tv_sec = current_time.tv_sec;
    last_time.tv_usec = current_time.tv_usec;

    ESP_LOGW ("Updated lap time", "Time: %d:%02d.%06llu",
        (minutes), (seconds), (uint64_t) laptimer.microSeconds);
}

void lap_processing_task(void *pvParameter) {  
   while (1) {
        vTaskDelay(pdMS_TO_TICKS(25));
    
        int current_tick = xTaskGetTickCount();
        // DEBUG: printf("Last Tick: %ld, Current Tick: %d, pdMS_TO_TICKS: %ld\n", last_interrupt_tick, current_tick, pdMS_TO_TICKS(100));
        if (current_tick - last_interrupt_tick > pdMS_TO_TICKS(100)) { //100 -> 2000 maybe?
            if (!laser_tripped) {
                lap_triggered(pvParameter);
                laser_tripped = true;

                // Send lap time to ESP-NOW task via queue (non-blocking)
                if (lap_time_queue != NULL) { //Redundant check, but just in case
                    uint8_t data[LT_data_len];
                    if(compose_LoRa_msg(data) != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to compose LoRa message");
                        continue;
                    }
                    if (xQueueSend(lap_time_queue, &data, 0) != pdPASS) {
                        ESP_LOGE(TAG, "Failed to send lap time to ESP-NOW task");
                    }
                }
           }
        }
    }
}

esp_err_t compose_LT_data(uint8_t* data) {
    if(data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // "LT": <index>, <min>:<sec>::<ms>

    // TimeStamp Data 
    uint32_t* timeStamp = (uint32_t*) data; //4 bytes
    *(timeStamp) = 0; //We aren't using this

    // "Car_Number" 
    uint32_t* carNum = (timeStamp + 1);  //4 bytes
    *carNum = -1;
    
    //Lap Time Data
    data[8] = laptimer.minutes % 256;
    data[9] = laptimer.seconds;

    int milliSec = laptimer.microSeconds/1000;
    data[10] = milliSec/256;
    data[11] = milliSec % 256;
    return ESP_OK;
}


//Laser handling functions
void IRAM_ATTR laserHandler(void *params) {
    xSemaphoreGive(laser_signal);
}

esp_err_t laser_setup(gpio_num_t pin) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE; //???
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    esp_err_t err = gpio_config(&io_conf);
    if(err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(err));
        return err;
    }

    gpio_install_isr_service(ESP_INTR_FLAG_EDGE); // Install ISR service
    gpio_isr_handler_add((gpio_num_t)pin, laserHandler, NULL); // Add ISR handler

    return err;
}

void laser_processing_task(void *pvParameter) {
    laser_signal = xSemaphoreCreateBinaryStatic(&laser_signal_buffer);
    while (1) {
        xSemaphoreTake(laser_signal, portMAX_DELAY);
        last_interrupt_tick = xTaskGetTickCount(); 
        laser_tripped = false; 
    }
}

void espnow_seg() {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(25));
        
        int current_tick = xTaskGetTickCount();

        // DEBUG: 
        // printf("Last Tick: %ld, Current Tick: %d, pdMS_TO_TICKS: %ld\n", last_interrupt_tick, current_tick, pdMS_TO_TICKS(100));
        
        if (current_tick - last_interrupt_tick > pdMS_TO_TICKS(25)) { //100 -> 2000 maybe?
            if(!laser_tripped) {
                ESPNow_Send_Trigger();
                laser_tripped = true;
            }
        }
    }
}