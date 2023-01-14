#include "sniffer.h"

static const char* TAG = "sniffer";
QueueHandle_t sniffer_captured_queue;

void IRAM_ATTR sniffer_rx_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
    // Reject non management frames
    if (type != WIFI_PKT_MGMT)
        return;

    wifi_promiscuous_pkt_t* raw_packet = (wifi_promiscuous_pkt_t*) buf;
    ieee80211_mgmt_packet_t* packet = (ieee80211_mgmt_packet_t*) raw_packet->payload;

    // Filter management frame subtype
    if ((packet->frame_control & FRAME_CONTROL_SUBTYPE_MASK) != FRAME_CONTROL_SUBTYPE_PROBE_REQUEST)
        return;

    // Abort on fragmented packets
    if (packet->frame_control & FRAME_CONTROL_MORE_SUBFRAGMENTS_MASK) {
        ESP_LOGW(TAG, "Packet fragmented");
        return;
    }

    // Enqueue valid packet
    probe_req_data_t probe_req_data = {
        .rssi = raw_packet->rx_ctrl.rssi,
        .noise_floor = raw_packet->rx_ctrl.noise_floor
    };
    memcpy(probe_req_data.source, packet->source, 6);

    BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(sniffer_captured_queue, &probe_req_data, &pxHigherPriorityTaskWoken);

    portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
}

void sniffer_init() {
    // Initialize wifi peripheral
    ESP_LOGI(TAG, "Initializing WiFi");
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Setup promiscuous mode
    ESP_LOGI(TAG, "Turning on promiscuous mode");
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&sniffer_rx_callback));
    sniffer_captured_queue = xQueueCreate(32, sizeof(probe_req_data_t));

    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));

    // Start wifi
    ESP_LOGI(TAG, "Starting WiFi peripheral");
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Init done");
}

void sniffer_set_channel(uint8_t channel) {
    ESP_LOGI(TAG, "Set channel to %d", channel);
    ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));
}