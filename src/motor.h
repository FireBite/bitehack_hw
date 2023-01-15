#pragma once

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "math.h"
#include <sys/time.h>
#include "esp_intr_alloc.h"

#define MOTOR_BRIDGE_FREQ 5000
#define MOTOR_BRIDGE_RES  LEDC_TIMER_10_BIT
#define MOTOR_BRIDGE_MAX  ((1 << (MOTOR_BRIDGE_RES)) - 1)

#define MOTOR_BRIDGE_CH_1A      LEDC_CHANNEL_0
#define MOTOR_BRIDGE_CH_1A_GPIO 27
#define MOTOR_BRIDGE_CH_1B      LEDC_CHANNEL_1
#define MOTOR_BRIDGE_CH_1B_GPIO 14
#define MOTOR_BRIDGE_CH_2A      LEDC_CHANNEL_2
#define MOTOR_BRIDGE_CH_2A_GPIO 12
#define MOTOR_BRIDGE_CH_2B      LEDC_CHANNEL_3
#define MOTOR_BRIDGE_CH_2B_GPIO 13

#define MOTOR_ENC_CLK1 35
#define MOTOR_ENC_DIR1 34
#define MOTOR_ENC_CLK2 25
#define MOTOR_ENC_DIR2 33

#define PI              3.141592
#define MOTOR_ODO_R     3.3 
#define MOTOR_ODO_L     15.8
#define MOTOR_ODO_CPR   200
#define MOTOR_ODO_DELAY 50
#define MOTOR_ODO_DPR   (2 * PI * MOTOR_ODO_R / MOTOR_ODO_CPR)

typedef struct {
    ledc_channel_t channel;
    int gpio;
} motor_bridge_inst_t;

typedef struct {
    int gpioClk;
    int gpioDir;
    int32_t pulses;
    int32_t lastPulses;
} motor_enc_inst_t;

typedef struct {
    float x;
    float y;
    float phi;
    int64_t lastTime;
} motor_odo_t;

typedef struct {
    motor_bridge_inst_t bridge_inst[4];
    motor_enc_inst_t enc_inst[2];
    motor_odo_t odo;
    int32_t x;
    int32_t y;
} motor_t;

void motor_init();
void motor_set_wheels(int8_t L, int8_t R);
void motor_pos_task();