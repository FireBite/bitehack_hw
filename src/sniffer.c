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
    // Setup promiscuous mode
    ESP_LOGI(TAG, "Turning on promiscuous mode");
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&sniffer_rx_callback));
    sniffer_captured_queue = xQueueCreate(32, sizeof(probe_req_data_t));

    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));

    ESP_LOGI(TAG, "Init done");
}