#pragma once

#include "driver/gpio.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "packet.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "button.h"

#define SW_1 3
#define SW_2 46
#define SW_3 9
#define SW_4 10
#define LED 8

#define DEBOUNCE_US 50000

extern QueueHandle_t q;

extern packet data;

extern uint8_t team;

extern uint8_t receiver_mac[6];

extern gpio_config_t led_conf;

extern gpio_config_t io_conf;

void init_transmitter_gpio(void);

void init_transmitter_wireless(void);