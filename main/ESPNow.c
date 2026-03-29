#include "ESPNow.h"
#include "Lap_Time.h"

static const char *TAG = "ESPNow";


// Global state variables
//static QueueHandle_t rx_queue = NULL;  // Queue for received packets (non-blocking callback)

//Main: f0:9e:9e:55:15:48
esp_now_peer_info_t mainInfo = 
{
    .peer_addr[0] = 0xf0,
    .peer_addr[1] = 0x9e,
    .peer_addr[2] = 0x9e,
    .peer_addr[3] = 0x55,
    .peer_addr[4] = 0x15,
    .peer_addr[5] = 0x48,
    .channel = 11,
    .ifidx = ESP_IF_WIFI_STA,
    .encrypt = false
};

//Segment: f0:9e:9e:55:15:08
esp_now_peer_info_t segInfo = 
{
    .peer_addr[0] = 0xf0,
    .peer_addr[1] = 0x9e,
    .peer_addr[2] = 0x9e,
    .peer_addr[3] = 0x55,
    .peer_addr[4] = 0x15,
    .peer_addr[5] = 0x08,
    .channel = 11,
    .ifidx = ESP_IF_WIFI_STA,
    .encrypt = false
};


// Paddock: 70:04:1d:b7:01:68
esp_now_peer_info_t paddockInfo = 
{
    .peer_addr[0] = 0x70,
    .peer_addr[1] = 0x04,
    .peer_addr[2] = 0x1d,
    .peer_addr[3] = 0xb7,
    .peer_addr[4] = 0x01,
    .peer_addr[5] = 0x68,
    .channel = 11,
    .ifidx = ESP_IF_WIFI_STA,
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
    ESP_LOGW(TAG, "Segment tripped. Starting lap...");

    lap_triggered(NULL);
}

/**
 * @brief Setup ESP-NOW for Segment Timer
 */
void Seg_Timer_Setup() {
    ESP_LOGI(TAG, "Adding Main Timer as ESP-NOW peer: " MACSTR, MAC2STR(mainInfo.peer_addr));
    ESP_ERROR_CHECK(esp_now_add_peer(&mainInfo));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
}

/**
 * @brief Setup ESP-NOW for Main Timer
 */
void Main_Timer_Setup() {
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
    
    // Promiscuous mode must be temporarily enabled to change the channel in STA mode
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE));
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

/**
 * @brief Send a trigger via ESP-NOW to start lap timing
 */
void ESPNow_Send_Trigger() {
    uint8_t trigger_msg[1] = {0x01}; // Simple trigger message
    esp_err_t result = espnow_send(mainInfo.peer_addr, trigger_msg, sizeof(trigger_msg));
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "ESP-NOW trigger sent to Main Timer");
    }
    else {
        ESP_LOGE(TAG, "Failed to send ESP-NOW trigger: %s", esp_err_to_name(result));
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // Short delay to prevent spamming
}

/**
 * @brief Sends data via ESP-NOW to whatever its supposed to send it to whatever ig
 * @param params Task parameters (unused)
 */
void espnow_Ritual(void* params) {
    (void) params; // Unused parameter

    TickType_t curr_ticks;
    uint8_t data[MAX_MSG_LEN];
    uint16_t len;
    esp_err_t err;

    while (1) {
        curr_ticks = xTaskGetTickCount();

        err = compose_LoRa_msg(data, &len);

        printf("lap: %d, minutes %d, sec %d, millisec %d\n", ((data[8] << 8) | data[9]), data[10], data[11], (data[12] << 8) | data[13]);

        if (err == ESP_OK){
            ESP_ERROR_CHECK(espnow_send(paddockInfo.peer_addr, data, len)); // Send to Paddock ig?
            xTaskDelayUntil(&curr_ticks, pdMS_TO_TICKS(1000));
        }
        else {
            ESP_LOGE(TAG, "Failed to compose LoRa message: %s", esp_err_to_name(err));
        }
    }
}