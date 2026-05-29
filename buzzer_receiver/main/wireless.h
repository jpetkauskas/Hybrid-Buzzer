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

#include "config.h"
#include "button.h"

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len);