#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "packet.h"
#include <stdint.h>
#include "button.h"

#define SW_1 3
#define SW_2 46
#define SW_3 9
#define SW_4 10
#define LED 8

extern QueueHandle_t q;

extern packet data;

extern uint8_t team;

void init_transmitter_gpio(QueueHandle_t queue);