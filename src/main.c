#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sniffer.h"
#include "comm.h"
#include "net.h"
#include "motor.h"

static const char* TAG = "main";

void memory_task() {
    while (1) {
        ESP_LOGI("memory", "Free %.2f KB / Minimum: %.2f KB", xPortGetFreeHeapSize() / 1024.f, xPortGetMinimumEverFreeHeapSize() / 1024.f);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void packet_process_task() {
    frame_t packet = {
        .cmd = FRAME_CMD_SNIFFER
    };

    packet.payload = malloc(128);

    while(1) {
        probe_req_data_t data;
        xQueueReceive(sniffer_captured_queue, &data, portMAX_DELAY);

        //ESP_LOGI(TAG, "PROBE_REQUEST %02x:%02x:%02x:%02x:%02x:%02x", data.source[0], data.source[1], data.source[2], data.source[3], data.source[4], data.source[5]);
        //ESP_LOGI(TAG, "    RSSI %d", data.rssi);
        //ESP_LOGI(TAG, "    NF   %d", data.noise_floor);

        packet.len = snprintf((char*)packet.payload, 127, ";1;%02x:%02x:%02x:%02x:%02x:%02x;%d;%d#", data.source[0], data.source[1], data.source[2], data.source[3], data.source[4], data.source[5], data.rssi, data.noise_floor);

        comm_send_packet(packet);
    }
}

void app_main() {
    //esp_log_level_set("*", ESP_LOG_INFO);

    // Fix bootstrap GPIO
    gpio_config_t gpio_conf = {
        .pin_bit_mask = 1ULL << GPIO_NUM_14,
        .mode = GPIO_MODE_OUTPUT
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));
    gpio_set_level(GPIO_NUM_14, 0);

    ESP_ERROR_CHECK(nvs_flash_init());

    // Communication init
    comm_init();
    net_init();
	sniffer_init();

    // Motor init
    motor_init();
    
    // Task creation
    xTaskCreate(motor_pos_task,      "motor_pos",      2048, NULL, 9,  NULL);
    xTaskCreate(memory_task,         "memory",         2048, NULL, 5,  NULL);
    xTaskCreate(packet_process_task, "packet_process", 2048, NULL, 15, NULL);
    xTaskCreate(comm_execute_task,   "comm_execute",   2048, NULL, 10, NULL);
}