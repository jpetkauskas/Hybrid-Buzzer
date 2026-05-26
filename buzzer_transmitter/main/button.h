#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdint.h>

#define DEBOUNCE_US 50000

typedef struct {
    uint32_t pin;
    uint8_t  pin_id;
    uint8_t  transmitter_id;
    QueueHandle_t q;
} button_ctx_t;

void IRAM_ATTR button_isr(void *arg);
