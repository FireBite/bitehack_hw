#pragma once

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

typedef enum {
    FRAME_CMD_WHEELS   = 'W',
    FRAME_CMD_POSITION = 'P',
    FRAME_CMD_SNIFFER  = 'S',

    FRAME_CMD_UNKNOWN  = '*',
} frame_cmd_t;

typedef struct {
    frame_cmd_t cmd;
    uint8_t* payload;
    size_t len;
} frame_t;

void comm_init();
void comm_send_packet(frame_t packet);
void comm_parse_packet(uint8_t* buf, size_t len);
void comm_execute_task();