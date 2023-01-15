#include "motor.h"
#include "comm.h"

static const char* TAG = "motor";

motor_t motor;

void motor_init() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = MOTOR_BRIDGE_RES,
        .freq_hz          = MOTOR_BRIDGE_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    for (int i = 0; i < 4; i++) {
        motor_bridge_inst_t* inst = &motor.bridge_inst[i];

        ledc_channel_config_t ledc_channel = {
            .speed_mode     = LEDC_HIGH_SPEED_MODE,
            .channel        = inst->channel,
            .timer_sel      = LEDC_TIMER_0,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = inst->gpio,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }
}

static uint32_t motor_calculate_wheel_pwm(int8_t target) {
    uint32_t res = 0;

    // Clip invalid values
    if (target > 100)
        target = 100;

    if (target < -100)
        target = -100;

    // Map to the timer address space
    if (target < 100 && target > -100) {
        res = ((2 ^ MOTOR_BRIDGE_RES) - 1) * (float) target / 100.f;
    } else {
        res = (target / 100) * ((2 ^ MOTOR_BRIDGE_RES) - 1);
    }

    return res;
}

void motor_set_wheels(int8_t L, int8_t R) {

    ESP_LOGI(TAG, "Set wheels: L %hhd R %hhd", L, R);
}

void motor_pos_task() {
    frame_t packet = {
        .cmd = FRAME_CMD_POSITION
    };

    packet.payload = malloc(16);

    while(1) {
        packet.len = snprintf((char*)packet.payload, 15, ";%d;%d#", motor.x, motor.y);
        comm_send_packet(packet);

        vTaskDelay(500 / portTICK_RATE_MS);
    }
}