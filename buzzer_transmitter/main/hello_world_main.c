/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

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

#define SW_1 3
#define SW_2 46
#define SW_3 9
#define SW_4 10
#define LED 8

#define DEBOUNCE_US 50000

static uint8_t team = 0;

packet data;

static uint8_t receiver_mac[6] = {0x20, 0x6E, 0xF1, 0xE1, 0xA0, 0xFC};

static QueueHandle_t q;
static int64_t last_fire[GPIO_NUM_MAX];

static void IRAM_ATTR button_isr(void *arg) {
  uint32_t pin = (uint32_t)arg;

  if (gpio_get_level(pin) != 0)
    return;
  int64_t now = esp_timer_get_time();
  if (now - last_fire[pin] < DEBOUNCE_US)
    return;
  last_fire[pin] = now;

  data.player_id = pin;
  data.transmitter_id = team;
  xQueueSendFromISR(q, &data, NULL);
}

gpio_config_t led_conf = {
    .pin_bit_mask = (1ULL << LED),
    .mode = GPIO_MODE_OUTPUT,
};

gpio_config_t io_conf = {
    .pin_bit_mask =
        (1ULL << SW_1) | (1ULL << SW_2) | (1ULL << SW_3) | (1ULL << SW_4),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_NEGEDGE,
};

void flash_led(void *arg) {
  gpio_set_level(LED, 1);
  vTaskDelay(pdMS_TO_TICKS(500)); // on for 500ms
  gpio_set_level(LED, 0);
  vTaskDelete(NULL);
}

void app_main(void) {
  gpio_install_isr_service(0);

  gpio_config(&io_conf);
  gpio_config(&led_conf);

  gpio_isr_handler_add(SW_1, button_isr, (void *)1);
  gpio_isr_handler_add(SW_2, button_isr, (void *)2);
  gpio_isr_handler_add(SW_3, button_isr, (void *)3);
  gpio_isr_handler_add(SW_4, button_isr, (void *)4);

  nvs_flash_init();

  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  esp_now_init();

  esp_now_peer_info_t peer = {
      .channel = 0, // 0 = use current channel
      .encrypt = false,
  };

  memcpy(peer.peer_addr, receiver_mac, 6);
  esp_now_add_peer(&peer);

  q = xQueueCreate(10, sizeof(uint32_t));

  xTaskCreate(flash_led, "flash", 2048, NULL, 10, NULL);

  while (1) {
    if (xQueueReceive(q, &data, portMAX_DELAY)) {
      esp_now_send(receiver_mac, (uint8_t *)&data, sizeof(data));
      //printf("Pin %" PRIu32 " fired\n", data.player_id);
      xTaskCreate(flash_led, "flash", 2048, NULL, 10, NULL);
    }
  }
}