#include "ESPNow.h"

#define TAG ((const char *const) "ESPNow")

//No longer needed since we are using the receive callback to trigger lap times instead of polling the queue
    // esp_now_peer_info_t mainInfo = 
    // {
    //     .peer_addr[0] = 0xf0,
    //     .peer_addr[1] = 0x9e,
    //     .peer_addr[2] = 0x9e,
    //     .peer_addr[3] = 0x55,
    //     .peer_addr[4] = 0x15,
    //     .peer_addr[5] = 0x48,
    //     .channel = 11,
    //     .ifidx = ESP_IF_WIFI_STA,
    //     .encrypt = false
    // };

    //Segment: f0:9e:9e:55:15:08 
    // esp_now_peer_info_t segInfo = 
    // {
    //     .peer_addr[0] = 0xf0,
    //     .peer_addr[1] = 0x9e,
    //     .peer_addr[2] = 0x9e,
    //     .peer_addr[3] = 0x55,
    //     .peer_addr[4] = 0x15,
    //     .peer_addr[5] = 0x08,
    //     .channel = 11,
    //     .ifidx = ESP_IF_WIFI_STA,
    //     .encrypt = false
    // };


// Paddock: 70:04:1d:b7:01:68
esp_now_peer_info_t paddockInfo = {
    .peer_addr[0] = 0x70,
    .peer_addr[1] = 0x04,
    .peer_addr[2] = 0x1d,
    .peer_addr[3] = 0xb7,
    .peer_addr[4] = 0x01,
    .peer_addr[5] = 0x68,
    .channel = PADDOCK_CHANNEL,
    .ifidx = ESP_IF_WIFI_STA,
    .encrypt = false
};

// Global state variables
static QueueHandle_t ESPNow_queue = NULL;  // Queue for received packets (non-blocking callback):48

// Forward declarations for callbacks
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status);
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len);

/**
 * @brief Send callback - called when ESP-NOW transmission completes
 * Note: This is called AFTER esp_now_send() returns, indicates ACK status
 */
static void espnow_send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        // Transmission successful (got ACK from peer)
        ESP_LOGI(TAG, "ESP-NOW data sent successfully");
    } else {
        // Transmission failed (no ACK from peer) - status=1 means ESP_NOW_SEND_FAIL
        ESP_LOGW(TAG, "ESP-NOW ACK failed to " MACSTR " (status=%d, likely no ACK from peer)",
                 MAC2STR(tx_info->src_addr), status);
    }
}

/**
 * @brief Receive callback - called when ESP-NOW data is received
 * Note: Called from WiFi task context - keep this fast to prevent watchdog issues
 */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    //Functionality Chnanged
        // ESP_LOGW(TAG, "Segment tripped. Starting lap...");
        // lap_triggered(NULL);

    // Instead receive data from Paddock only (Not anything in mind yet though)
}

/**
 * @brief Initialize ESP-NOW wireless communication
 * @return ESP_OK on success, error code on failure
 */
int espnow_init(QueueHandle_t lt_espnow_queue) {
    if (lt_espnow_queue == NULL) {
        ESP_LOGE(TAG, "ESP-NOW queue handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    ESPNow_queue = lt_espnow_queue; // Set the global queue handle for use in callbacks

    // Step 1: Initialize NVS (required for WiFi)
    ESP_LOGI(TAG, "Initializing NVS flash");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    
    // Step 2: Initialize WiFI (required before ESP-NOW)
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //Initialize Wi-Fi in station mode
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    // Promiscuous mode must be temporarily enabled to change the channel in STA mode
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(PADDOCK_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));

    // Step 3: Initialize RX queue for non-blocking serial output (Receiving from other timer)
    //rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(espnow_rx_packet_t));
    // if (rx_queue == NULL) {
    //     ESP_LOGE(TAG, "Failed to create RX queue");
    //     return ESP_ERR_NO_MEM;
    // }

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGE(TAG, "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);


    //Initialize ESPNow
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI(TAG,"ESPNow init success!!");
    

    // Step 4: Register peers and callbacks (done in Seg_Timer_Setup and Main_Timer_Setup)
    ESP_ERROR_CHECK(esp_now_add_peer(&paddockInfo));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    // ESP_LOGI(TAG, "ESP-NOW initialization complete");
    return ESP_OK;
}

/**
 * @brief Send ESP-NOW data to a specific peer
 * @param dest_mac Destination MAC address (6 bytes)
 * @param data Pointer to data buffer
 * @param length Data length in bytes (max 250)
 * @return ESP_OK on success, error code on failure
 */
int espnow_send(const uint8_t *dest_mac, uint8_t* data, size_t length) {
    if (length > MAX_MSG_LEN) {
        ESP_LOGE(TAG, "Data length exceeds maximum ESP-NOW payload");
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t result = esp_now_send(dest_mac, data, length);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send ESP-NOW data: %s", esp_err_to_name(result));
        return result;
    }

    // ESP_LOGI(TAG, "ESP-NOW data sent successfully");
    return ESP_OK;
}

// /**
//  * @brief Send a trigger via ESP-NOW to start lap timing
//  */
// void send_SegData() {
//     uint8_t trigger_msg[1] = {0x01}; // Simple trigger message
//     esp_err_t result = espnow_send(mainInfo.peer_addr, trigger_msg, sizeof(trigger_msg));
//     if (result == ESP_OK) {
//         ESP_LOGI(TAG, "ESP-NOW trigger sent to Main Timer");
//     }
//     else {
//         ESP_LOGE(TAG, "Failed to send ESP-NOW trigger: %s", esp_err_to_name(result));
//     }
//     vTaskDelay(pdMS_TO_TICKS(1000)); // Short delay to prevent spamming
// }

/**
 * @brief Sends data via ESP-NOW to whatever its supposed to send it to whatever ig
 * @param params Task parameters (unused)
 */
void espnow_Ritual(void* params) {
    (void) params; // Unused parameter

    uint8_t data[LT_data_len];
    if(xQueueReceive(ESPNow_queue, &data, portMAX_DELAY) == pdTRUE) { // Wait indefinitely for data to send (we will trigger this from the lap time task when we get a new lap time)
        // In a real implementation, you would likely want to check the data received from the queue and send it to the appropriate peer (e.g., Main Timer or Segment Timer) based on the content of the data. For this example, we will just send a simple trigger message to the Main Timer when we receive any data from the queue.
        ESP_ERROR_CHECK(esp_now_send(paddockInfo.peer_addr, data, LT_data_len)); // Send the lap time data to the Paddock (or Main Timer if you want) via ESP-NOW
    }
}