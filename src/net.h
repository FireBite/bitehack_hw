#pragma once

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_http_server.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define COMM_WIFI_SSID    "BITEhack_2023"
#define COMM_WIFI_PASSWD  "BITEhack2023"
#define COMM_WIFI_CHANNEL 5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

void net_init();
static void net_wifi_init();
static void net_server_init();