#include "buttons.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "packet.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define LED 8

static uint8_t team = 1;

packet data;

static uint8_t receiver_mac[6] = {0x20, 0x6E, 0xF1, 0xE1, 0xA0, 0xFC};

static QueueHandle_t q;

typedef enum {
    MSG_BEACON = 0,
    MSG_HELLO,
    MSG_DATA,
    MSG_ACK,
} msg_type_t;

typedef struct {
    msg_type_t type;
    uint8_t    data[32];  // extend as needed
} payload_t;

gpio_config_t led_conf = {
    .pin_bit_mask = (1ULL << LED),
    .mode = GPIO_MODE_OUTPUT,
};

void flash_led(void *arg) {
  gpio_set_level(LED, 1);
  vTaskDelay(pdMS_TO_TICKS(200)); // on for 500ms
  gpio_set_level(LED, 0);
  vTaskDelete(NULL);
}

static void on_recv(const esp_now_recv_info_t *info,
                    const uint8_t *data, int len)
{
    payload_t *pkt = (payload_t *)data;

    if (pkt->type == MSG_BEACON) {
        // Got beacon from C — add it as peer if not already registered
        if (!esp_now_is_peer_exist(info->src_addr)) {
            esp_now_peer_info_t peer = { .ifidx = WIFI_IF_STA, .encrypt = false };
            memcpy(peer.peer_addr, info->src_addr, 6);
            esp_now_add_peer(&peer);
            ESP_LOGI("ESPNOW_TX", "Registered receiver: " MACSTR, MAC2STR(info->src_addr));
        }
    }

    if (pkt->type == MSG_ACK) {
        // Normal ACK handling
    }
}

void app_main(void) {
  gpio_install_isr_service(0);

  gpio_config(&led_conf);

  nvs_flash_init();

  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  esp_now_init();

  esp_wifi_get_mac(WIFI_IF_STA, data.transmitter_mac);

  esp_now_peer_info_t peer = {
      .channel = 0, // 0 = use current channel
      .encrypt = false,
  };

  memcpy(peer.peer_addr, receiver_mac, 6);
  esp_now_add_peer(&peer);

  esp_now_register_recv_cb(on_recv);

  q = xQueueCreate(10, sizeof(uint8_t));
  buttons_init(q);

  xTaskCreate(flash_led, "flash", 2048, NULL, 10, NULL);

  uint8_t pin_id;
  while (1) {
    if (xQueueReceive(q, &pin_id, portMAX_DELAY)) {
      data.player_id = pin_id;
      data.transmitter_id = team;
      esp_now_send(receiver_mac, (uint8_t *)&data, sizeof(data));
      printf("Pin %d fired\n", pin_id);
      xTaskCreate(flash_led, "flash", 2048, NULL, 10, NULL);
    }
  }
}