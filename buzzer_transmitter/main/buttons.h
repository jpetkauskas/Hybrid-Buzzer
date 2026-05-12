#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

void buttons_init(QueueHandle_t q);
