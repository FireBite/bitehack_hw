#pragma once

#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_system.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "string.h"

#define FRAME_CONTROL_SUBTYPE_MASK           0b1111 << 4
#define FRAME_CONTROL_SUBTYPE_PROBE_REQUEST  0b0100 << 4
#define FRAME_CONTROL_SUBTYPE_PROBE_RESPONSE 0b0101 << 4

#define FRAME_CONTROL_MORE_SUBFRAGMENTS_MASK 1 << 10
#define FRAME_CONTROL_HT_ORDER_MASK          1 << 15

typedef struct __attribute__((__packed__)) {
    uint16_t frame_control;
    uint16_t duration;
    uint8_t destination[6];
    uint8_t source[6];
    uint8_t bssid[6];
    uint16_t seq_ctl;
    uint8_t* payload;
} ieee80211_mgmt_packet_t;

typedef struct {
    uint8_t source[6];
    int32_t rssi;
    int32_t noise_floor;
} probe_req_data_t;

extern QueueHandle_t sniffer_captured_queue;

void sniffer_init();
extern void sniffer_frame_callback();