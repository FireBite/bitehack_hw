#include "comm.h"
#include "net.h"

static const char* TAG = "comm";

QueueHandle_t comm_packet_queue;

void comm_init() {
    comm_packet_queue = xQueueCreate(8, sizeof(frame_t));
}

void comm_send_packet(frame_t packet) {
    net_
}

void comm_parse_packet(uint8_t* buf) {
    frame_t packet = {
        .cmd = buf[0],
        .payload = buf + 1,
    };

    xQueueSend(comm_packet_queue, &packet, 0);
}

void comm_execute_task() {
    while (1) {
        frame_t packet;
        xQueueReceive(comm_packet_queue, &packet, portMAX_DELAY);

        ESP_LOGI(TAG, "CMD: %c Payload: %s", packet.cmd, packet.payload);

        packet.cmd = 'x';
        comm_send_packet(packet);

        free(packet.payload - 1); // oh no, anyway
    }
}