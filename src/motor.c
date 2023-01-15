#include "motor.h"
#include "comm.h"

static const char* TAG = "motor";

motor_t motor;

static void IRAM_ATTR enc_isr_handler(void* arg) {
    uint32_t clk = (uint32_t) arg;

    if (clk == MOTOR_ENC_CLK1) {
        motor.enc_inst[0].pulses += gpio_get_level(MOTOR_ENC_DIR1) ? 1 : -1;
    } else if (clk == MOTOR_ENC_CLK2){
        motor.enc_inst[1].pulses += gpio_get_level(MOTOR_ENC_DIR2) ? 1 : -1;
    }
}

void motor_init() {
    motor.bridge_inst[MOTOR_BRIDGE_CH_1A].channel = MOTOR_BRIDGE_CH_1A;
    motor.bridge_inst[MOTOR_BRIDGE_CH_1A].gpio    = MOTOR_BRIDGE_CH_1A_GPIO;
    motor.bridge_inst[MOTOR_BRIDGE_CH_1B].channel = MOTOR_BRIDGE_CH_1B;
    motor.bridge_inst[MOTOR_BRIDGE_CH_1B].gpio    = MOTOR_BRIDGE_CH_1B_GPIO;
    motor.bridge_inst[MOTOR_BRIDGE_CH_2A].channel = MOTOR_BRIDGE_CH_2A;
    motor.bridge_inst[MOTOR_BRIDGE_CH_2A].gpio    = MOTOR_BRIDGE_CH_2A_GPIO;
    motor.bridge_inst[MOTOR_BRIDGE_CH_2B].channel = MOTOR_BRIDGE_CH_2B;
    motor.bridge_inst[MOTOR_BRIDGE_CH_2B].gpio    = MOTOR_BRIDGE_CH_2B_GPIO;

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER_0,
        .duty_resolution  = MOTOR_BRIDGE_RES,
        .freq_hz          = MOTOR_BRIDGE_FREQ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    for (int i = 0; i < 4; i++) {
        motor_bridge_inst_t* inst = &(motor.bridge_inst[i]);

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

    motor.enc_inst[0].gpioClk = MOTOR_ENC_CLK1;
    motor.enc_inst[0].gpioDir = MOTOR_ENC_DIR1;
    motor.enc_inst[1].gpioClk = MOTOR_ENC_CLK2;
    motor.enc_inst[1].gpioDir = MOTOR_ENC_DIR2;

    // Encoder interrupt init - clock
    gpio_config_t gpio_conf = {
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << MOTOR_ENC_CLK1) | (1ULL << MOTOR_ENC_CLK2),
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_ERROR_CHECK(gpio_isr_handler_add(MOTOR_ENC_CLK1, enc_isr_handler, (void*) MOTOR_ENC_CLK1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(MOTOR_ENC_CLK2, enc_isr_handler, (void*) MOTOR_ENC_CLK2));

    // Encoder dir pins init
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.pin_bit_mask = (1ULL << MOTOR_ENC_DIR1) | (1ULL << MOTOR_ENC_DIR2);
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));
}

static uint32_t motor_calculate_wheel_pwm(int8_t target) {
    uint32_t res = 0;

    // Clip invalid values
    if (target > 100)
        target = 100;

    if (target <= 0)
        target = 0;

    // Map to the timer address space
    if (target < 100) {
        res = MOTOR_BRIDGE_MAX * (float) target / 100.f;
    } else {
        res = MOTOR_BRIDGE_MAX;
    }

    return res;
}

void motor_set_wheels(int8_t L, int8_t R) {
    if (L > 0) {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_1A, motor_calculate_wheel_pwm(L));
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_1B, 0);
    } else if(L < 0) {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_1A, 0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_1B, motor_calculate_wheel_pwm(L));
    } else {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_1A, 0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_1B, 0);
    }

    if (R > 0) {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_2A, motor_calculate_wheel_pwm(R));
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_2B, 0);
    } else if(R < 0) {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_2A, 0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_2B, motor_calculate_wheel_pwm(R));
    } else {
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_2A, 0);
        ledc_set_duty(LEDC_HIGH_SPEED_MODE, MOTOR_BRIDGE_CH_2B, 0);
    }

    ESP_LOGI(TAG, "Set wheels: L %hhd R %hhd", L, R);

    for (int i = 0; i < 4; i++) {
        ledc_update_duty(LEDC_HIGH_SPEED_MODE, motor.bridge_inst[i].channel);
    }

}

void motor_pos_task() {
    frame_t packet = {
        .cmd = FRAME_CMD_POSITION
    };
    packet.payload = malloc(16);

    uint8_t output_prescaler = 0;

    while(1) {
        if (++output_prescaler == 1000 / MOTOR_ODO_DELAY / 10) { // 2Hz
            packet.len = snprintf((char*)packet.payload, 15, ";%d;%d#", motor.x, motor.y);
            comm_send_packet(packet);
            output_prescaler = 0;
        }

        ESP_LOGE(TAG, "%d %d", motor.enc_inst[0].pulses, motor.enc_inst[1].pulses);

        // Get current time
        struct timeval tv_now;
        gettimeofday(&tv_now, NULL);
        int64_t time = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec - motor.odo.lastTime;
        float dt = time / 1000000;

        // Calculate wheel velocity
        float dvl = (motor.enc_inst[0].pulses - motor.enc_inst[0].lastPulses) * MOTOR_ODO_DPR / dt;
        float dvr = (motor.enc_inst[1].pulses - motor.enc_inst[1].lastPulses) * MOTOR_ODO_DPR / dt;

        // Save pulses and time for the next calculation
        motor.enc_inst[0].lastPulses = motor.enc_inst[0].pulses;
        motor.enc_inst[1].lastPulses = motor.enc_inst[1].pulses;
        motor.odo.lastTime += time;

        // Calculate position vector
        float dphi = MOTOR_ODO_R / MOTOR_ODO_L * (dvr - dvl);
        float dx = MOTOR_ODO_R / 2 * cos(dphi);
        float dy = MOTOR_ODO_R / 2 * sin(dphi);

        // Update position
        motor.odo.phi += dphi;
        motor.odo.x += dx;
        motor.odo.y += dy;

        // Normalize resulting phi
        while (motor.odo.phi > 2*PI) {
            motor.odo.phi -= 2*PI;
        }

        while (motor.odo.phi < 0) {
            motor.odo.phi += 2*PI;
        }

        vTaskDelay(MOTOR_ODO_DELAY / portTICK_RATE_MS);
    }
}