#pragma once

#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/task.h"


void IRAM_ATTR button_isr(void *arg);
