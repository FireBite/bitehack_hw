#pragma once

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/ledc.h"

#define MOTOR_BRIDGE_FREQ 5000
#define MOTOR_BRIDGE_RES  LEDC_TIMER_10_BIT

#define MOTOR_BRIDGE_CH_1A 0
#define MOTOR_BRIDGE_CH_1B 1
#define MOTOR_BRIDGE_CH_2A 2
#define MOTOR_BRIDGE_CH_2B 3

typedef struct {
    ledc_channel_t channel;
    int gpio;
} motor_bridge_inst_t;

typedef struct {
    motor_bridge_inst_t bridge_inst[4];
    int32_t x;
    int32_t y;
} motor_t;

void motor_init();
void motor_set_wheels(int8_t L, int8_t R);
void motor_pos_task();