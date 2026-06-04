#pragma once

#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "packet.h"
#include "portmacro.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "button.h"

static const uint8_t DEFAULT_MAC_VALUE[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static packet handshake = {3,3, {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};

void pairing_recv_callback(const esp_now_recv_info_t *info, const uint8_t *data, int len);

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len);

void receiver_init_wireless(void);

void receiver_hardware_init(void);

static SemaphoreHandle_t pairing_complete;

static bool transmitter_a_paired = false;

static bool transmitter_b_paired = false;

static bool *transmitter_flags[2] = {&transmitter_a_paired, &transmitter_b_paired};

extern uint8_t transmitter_mac_addresses[2][6];