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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "button.h"
#include "wireless.h"

#define LED_1 3
#define LED_2 46
#define LED_3 9
#define LED_4 10
#define LED_5 11
#define LED_6 12
#define LED_7 13
#define LED_8 14
#define BUZZ 8
#define CLEAR 37

#define DEBOUNCE_US 50000

extern bool latch_state;

extern QueueHandle_t q;

static uint32_t led_array[] = {LED_1, LED_2, LED_3, LED_4,
                               LED_5, LED_6, LED_7, LED_8};

extern gpio_config_t io_conf;

extern gpio_config_t output_conf;

void receiver_init_gpio(void);


void receiver_init_wireless(void);
