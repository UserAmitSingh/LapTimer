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

typedef struct timeval lt_timeval;

typedef struct {
    uint64_t microSeconds;
    uint16_t minutes;
    uint16_t lap;
    uint8_t seconds;
} timer;

typedef struct {
    timer laptimer;
    lt_timeval lastTime;
    int pin;
} params;

esp_err_t gpio_set_up (params*);

void start_timer ();

void lap_triggered (void* par);

void espnow_seg();



void IRAM_ATTR gpioHandler(void* );

void lap_processing_task (void *pvParameter);

void laser_processing_task (void *pvParameter);

esp_err_t compose_LoRa_msg(uint8_t * data, uint16_t * len); 

#endif //LAP_TIME_H