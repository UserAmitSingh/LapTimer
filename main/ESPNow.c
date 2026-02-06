#include "ESPNow.h"
#include "Lap_Time.h"

static const char *TAG = "ESPNow";


// Global state variables
//static QueueHandle_t rx_queue = NULL;  // Queue for received packets (non-blocking callback)

//Segment: 0x70, 0x04, 0x1d, 0xb7, 0x00, 0xd8
//Main_mac: 0x70, 0x04, 0x1d, 0xb7, 0x01, 0x30

esp_now_peer_info_t mainInfo = 
{
    .peer_addr[0] = 0x70,
    .peer_addr[1] = 0x04,
    .peer_addr[2] = 0x1d,
    .peer_addr[3] = 0xb7,
    .peer_addr[4] = 0x01,
    .peer_addr[5] = 0x30,
    .channel = 0,
    .ifidx = WIFI_IF_STA,
    .encrypt = false
};

esp_now_peer_info_t segInfo = 
{
    .peer_addr[0] = 0x70,
    .peer_addr[1] = 0x04,
    .peer_addr[2] = 0x1d,
    .peer_addr[3] = 0xb7,
    .peer_addr[4] = 0x00,
    .peer_addr[5] = 0xd8,
    .channel = 0,
    .ifidx = WIFI_IF_STA,
    .encrypt = false
};

//Paddock: 70:04:1d:b7:01:68
esp_now_peer_info_t paddockInfo = 
{
    .peer_addr[0] = 0x70,
    .peer_addr[1] = 0x04,
    .peer_addr[2] = 0x1d,
    .peer_addr[3] = 0xb7,
    .peer_addr[4] = 0x01,
    .peer_addr[5] = 0x68,
    .channel = 0,
    .ifidx = WIFI_IF_STA,
    .encrypt = false
};

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
    } else {
        // Transmission failed (no ACK from peer) - status=1 means ESP_NOW_SEND_FAIL
        ESP_LOGV(TAG, "ESP-NOW ACK failed to " MACSTR " (status=%d, likely no ACK from peer)",
                 MAC2STR(tx_info->src_addr), status);
    }
}

/**
 * @brief Receive callback - called when ESP-NOW data is received
 * Note: Called from WiFi task context - keep this fast to prevent watchdog issues
 */
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    ESP_LOGI(TAG, "Segment tripped. Starting lap...");

    lap_triggered(NULL);
}

/**
 * @brief Setup ESP-NOW for Segment Timer
 */
void Seg_Timer_Setup()
{
    ESP_ERROR_CHECK(esp_now_add_peer(&mainInfo));

    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
}

/**
 * @brief Setup ESP-NOW for Main Timer
 */
void Main_Timer_Setup()
{
    ESP_ERROR_CHECK(esp_now_add_peer(&segInfo));
    ESP_ERROR_CHECK(esp_now_add_peer(&paddockInfo));
    //Register the receive callback
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));   
}

/**
 * @brief Initialize ESP-NOW wireless communication
 * @return ESP_OK on success, error code on failure
 */
int espnow_init() {

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
    
    // Step 3: Initialize RX queue for non-blocking serial output (Receiving from other timer)
    //rx_queue = xQueueCreate(RX_QUEUE_SIZE, sizeof(espnow_rx_packet_t));
    // if (rx_queue == NULL) {
    //     ESP_LOGE(TAG, "Failed to create RX queue");
    //     return ESP_ERR_NO_MEM;
    // }

    //Initialize ESPNow
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI(TAG,"ESPNow init success!!");

    // Step 5: Register callbacks
    // ESP_LOGI(TAG, "Registering ESP-NOW callbacks");
    // ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    // ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    // Step 5: Add peers
    // esp_now_add_peer(&mainInfo); // Add Main Timer
    // esp_now_add_peer(&segInfo);  // Add Segment Timer
    // esp_now_add_peer(&paddockInfo);

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

    ESP_LOGI(TAG, "ESP-NOW data sent successfully");
    return ESP_OK;
}

/**
 * @brief Send a trigger via ESP-NOW to start lap timing
 */
void ESPNow_Send_Trigger() {
    uint8_t trigger_msg[1] = {0x01}; // Simple trigger message
    esp_err_t result = espnow_send(segInfo.peer_addr, trigger_msg, sizeof(trigger_msg));
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "ESP-NOW trigger sent to Segment Timer");
    }
}

/**
 * @brief Sends data via ESP-NOW to whatever its supposed to send it to whatever ig
 * @param params Task parameters (unused)
 */
void espnow_Ritual(void* params) {
    (void) params; // Unused parameter

    TickType_t curr_ticks;

    while (1)
    {
        curr_ticks = xTaskGetTickCount();

        uint8_t data[MAX_MSG_LEN];
        uint16_t len;
        esp_err_t err = compose_LoRa_msg(data, &len); // Car number was set to -1 anyways for LoRa

        //printf("minutes %d, sec %d, millisec %d\n", data[6], data[7], (data[8] << 8) | data[9]);

        if (err == ESP_OK)
        {
            ESP_ERROR_CHECK(espnow_send(paddockInfo.peer_addr, data, len)); // Send to Paddock ig?
        }
    }
}