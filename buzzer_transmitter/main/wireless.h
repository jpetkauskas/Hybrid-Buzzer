#pragma once

#include <stdint.h>
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <string.h>
#include "led.h"
#include "freertos/semphr.h"

static const uint8_t DEFAULT_MAC_VALUE[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

extern uint8_t receiver_mac[6];

extern esp_now_peer_info_t peer;

SemaphoreHandle_t received_sem;

void init_transmitter_wireless(void);

static void wireless_hardware_init(void);

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len);

