#ifndef LAP_TIME_H
#define LAP_TIME_H

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "hal/gpio_types.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_sntp.h"

#define LASER_PIN 7
#define LT_data_len 12

typedef struct timeval lt_timeval;

typedef struct {
    uint64_t microSeconds;
    uint16_t minutes;
    uint8_t seconds;
} timer;



esp_err_t Lap_Time_init(QueueHandle_t lt_espnow_queue);

//Lap Time handling functions
void start_timer();

void lap_triggered();

void lap_processing_task();

esp_err_t compose_LT_msg(uint8_t * data); 


//Laser handling functions
void IRAM_ATTR laserHandler(void* params);

esp_err_t laser_setup(gpio_num_t pin);

void laser_processing_task (void* pvParameter);

#endif //LAP_TIME_H