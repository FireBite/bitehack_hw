#include "comm.h"
#include "net.h"
#include "motor.h"

static const char* TAG = "comm";

QueueHandle_t comm_packet_queue;

void comm_init() {
    comm_packet_queue = xQueueCreate(8, sizeof(frame_t));
}

void comm_send_packet(frame_t packet) {
    uint8_t* buf = malloc(packet.len);

    buf[0] = packet.cmd;
    memcpy(buf + 1, packet.payload, packet.len);

    net_broadcast_ws_msg(buf, packet.len + 1);
}

void comm_parse_packet(uint8_t* buf, size_t len) {
    frame_t packet = {
        .cmd = buf[0],
        .payload = buf + 1,
        .len = len
    };

    xQueueSend(comm_packet_queue, &packet, 0);
}

void comm_execute_task() {
    int8_t L = 0;
    int8_t R = 0;

    while (1) {
        frame_t packet;
        xQueueReceive(comm_packet_queue, &packet, portMAX_DELAY);

        switch (packet.cmd) {
            case FRAME_CMD_WHEELS:
                sscanf((char*) packet.payload, ";%hhd;%hhd", &L, &R);
                motor_set_wheels(L, R);
                break;
            default:
                ESP_LOGW(TAG, "Unknown packet CMD: %c Payload: %s Len: %d", packet.cmd, packet.payload, packet.len);
                break;
        }

        free(packet.payload - 1); // oh no, anyway
    }
}