#ifndef ESPNow_H
#define ESPNow_H

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_random.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_crc.h"

#define ESP_NOW_ETH_ALEN 6
#define MAX_MSG_LEN 256

// Function declarations

/**
 * @brief Initialize ESP-NOW wireless communication
 * @return ESP_OK on success, error code on failure
 */
int espnow_init();

/**
 * @brief Send a trigger via ESP-NOW to start lap timing
 */
void ESPNow_Send_Trigger();

/**
 * @brief Setup ESP-NOW for Main Timer
 */
void Main_Timer_Setup();

/**
 * @brief Setup ESP-NOW for Segment Timer
 */
void Seg_Timer_Setup();

/**
 * @brief Send ESP-NOW data to a specific peer
 * @param dest_mac Destination MAC address (6 bytes)
 * @param data Pointer to data buffer
 * @param length Data length in bytes (max 250)
 * @return ESP_OK on success, error code on failure
 */
int espnow_send(const uint8_t *dest_mac, uint8_t* data, size_t length);

/**
 * @brief Sends data via ESP-NOW to whatever its supposed to send it to whatever ig
 * @param params Task parameters (unused)
 */
void espnow_Ritual(void* params);

#endif // ESPNow_H