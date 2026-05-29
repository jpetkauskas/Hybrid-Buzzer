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


typedef struct buzz_profile {
  uint32_t length;
  uint8_t buzzes;
} buzz_profile;

extern buzz_profile ba;
extern buzz_profile bb;

extern buzz_profile bn[2];

void buzz(void *arg);