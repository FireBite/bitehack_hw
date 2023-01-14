#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sniffer.h"

static const char* TAG = "main";

void memory_task() {
    while (1) {
        ESP_LOGI("memory", "Free %.2f KB / Minimum: %.2f KB", xPortGetFreeHeapSize() / 1024.f, xPortGetMinimumEverFreeHeapSize() / 1024.f);
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

void packet_process_task() {
    while (1) {
        probe_req_data_t packet;
        xQueueReceive(sniffer_captured_queue, &packet, portMAX_DELAY);

        ESP_LOGI(TAG, "PROBE_REQUEST %02x:%02x:%02x:%02x:%02x:%02x", packet.source[0], packet.source[1], packet.source[2], packet.source[3], packet.source[4], packet.source[5]);
        ESP_LOGI(TAG, "    RSSI %d", packet.rssi);
        ESP_LOGI(TAG, "    NF   %d", packet.noise_floor);
    }
}

void app_main() {
    //esp_log_level_set("*", ESP_LOG_INFO);
	
    // Sniffer init
	sniffer_init();
    
    // Task creation
    xTaskCreate(memory_task,         "memory",         2048, NULL, 5,  NULL);
    xTaskCreate(packet_process_task, "packet_process", 2048, NULL, 10, NULL);
}