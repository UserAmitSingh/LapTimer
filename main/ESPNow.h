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
#define PADDOCK_CHANNEL 11
#define LT_data_len 12

// Function declarations
/**
 * @brief Initialize ESP-NOW wireless communication
 * @return ESP_OK on success, error code on failure
 */
int espnow_init(QueueHandle_t lt_espnow_queue);

/**
 * @brief Send ESP-NOW data to a specific peer
 * @param dest_mac Destination MAC address (6 bytes)
 * @param data Pointer to data buffer
 * @param length Data length in bytes (max 250)
 * @return ESP_OK on success, error code on failure
 */
int espnow_send(const uint8_t *dest_mac, uint8_t* data, size_t length);

/**
 * @brief Send a trigger via ESP-NOW to start lap timing
 */
void send_SegData();

/**
 * @brief Sends data via ESP-NOW to whatever its supposed to send it to whatever ig
 * @param params Task parameters (unused)
 */
void espnow_Ritual(void* params);

#endif // ESPNow_H