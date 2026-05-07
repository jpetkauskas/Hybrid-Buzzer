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

static uint32_t led_array[] = {LED_1, LED_2, LED_3, LED_4,
                               LED_5, LED_6, LED_7, LED_8};

packet incoming;

bool latch_state = false;

static QueueHandle_t q;

static int64_t last_fire;

typedef struct buzz_profile {
  uint32_t length;
  uint8_t buzzes;
} buzz_profile;

buzz_profile ba = {.length = 500, .buzzes = 1};
buzz_profile bb = {.length = 200, .buzzes = 2};

buzz_profile bn[] = {{.length = 500, .buzzes = 1}, {.length = 200, .buzzes = 2}};

static void IRAM_ATTR button_isr(void *arg) {
  if (gpio_get_level(CLEAR) != 0)
    return;
  int64_t now = esp_timer_get_time();
  if (now - last_fire < DEBOUNCE_US)
    return;
  last_fire = now;

  for (int i = 0; i < 8; i++) {
    gpio_set_level(led_array[i], 0);
  }

  latch_state = false;
}

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  xQueueSendFromISR(q, data, NULL);
}

gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << CLEAR),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_NEGEDGE,
};
gpio_config_t output_conf = {
    .pin_bit_mask = (1ULL << LED_1) | (1ULL << LED_2) | (1ULL << LED_3) |
                    (1ULL << LED_4) | (1ULL << LED_5) | (1ULL << LED_6) |
                    (1ULL << LED_7) | (1ULL << LED_8) | (1ULL << BUZZ),
    .mode = GPIO_MODE_OUTPUT,
};

void buzz(void *arg) {
  buzz_profile *b = (buzz_profile *)arg;

  for (int i = 0; i < b->buzzes; i++) {
    gpio_set_level(BUZZ, 1);
    vTaskDelay(pdMS_TO_TICKS(b->length));
    gpio_set_level(BUZZ, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  vTaskDelete(NULL);
}

void app_main(void) {
  gpio_install_isr_service(0);

  gpio_config(&output_conf);

  gpio_config(&io_conf);
  gpio_isr_handler_add(CLEAR, button_isr, (void *)NULL);

  nvs_flash_init();

  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  esp_now_init();

  esp_now_register_recv_cb(on_recv);

  q = xQueueCreate(10, sizeof(packet));

  xTaskCreate(buzz, "buzz", 2048, &bn[0], 10, NULL);

  while (1) {
    if (xQueueReceive(q, &incoming, portMAX_DELAY) && !latch_state) {
      latch_state = true;

      int8_t team = incoming.transmitter_id;
      int8_t player = incoming.player_id;

      uint32_t player_led_index = (team * 4) + player;
      gpio_set_level(led_array[player_led_index - 1], 1);

      xTaskCreate(buzz, "buzz", 2048, &bn[team], 10, NULL);

      printf("Team %d, player %d, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
             team, player,
             incoming.transmitter_mac[0], incoming.transmitter_mac[1],
             incoming.transmitter_mac[2], incoming.transmitter_mac[3],
             incoming.transmitter_mac[4], incoming.transmitter_mac[5]);
    }
  }
}
