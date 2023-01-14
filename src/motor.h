#pragma once

#include "esp_system.h"

typedef struct {
    
} motor_t;

void motor_init();
void motor_set_wheels(int32_t L, int32_t R);