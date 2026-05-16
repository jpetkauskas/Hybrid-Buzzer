#include "config.h"

packet data;
QueueHandle_t q;

uint8_t team = 1;

uint8_t receiver_mac[6] = {0x20, 0x6E, 0xF1, 0xE1, 0xA0, 0xFC};

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

void init_transmitter_gpio(void){
  gpio_install_isr_service(0);

  gpio_config(&io_conf);
  gpio_config(&led_conf);

  gpio_isr_handler_add(SW_1, button_isr, (void *)SW_1);
  gpio_isr_handler_add(SW_2, button_isr, (void *)SW_2);
  gpio_isr_handler_add(SW_3, button_isr, (void *)SW_3);
  gpio_isr_handler_add(SW_4, button_isr, (void *)SW_4);
}

void init_transmitter_wireless(void){
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
}