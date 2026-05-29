#include "wireless.h"
#include "led.h"

/*
actual receiver MAC: {0x20, 0x6E, 0xF1, 0xE1, 0xA0, 0xFC}
*/

uint8_t receiver_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; //save from previous default?

esp_now_peer_info_t peer = {
    .channel = 0,
    .encrypt = false,
};

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  // handshake before blindly registering****
  led_trigger();
  // uint8_t *sender_mac = info->src_addr;
  // memcpy(receiver_mac, sender_mac, 6);
  xSemaphoreGive(received_sem);
}

// abstract ESPNOW hardware initialization

static void wireless_hardware_init(void) {

  nvs_flash_init();

  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  esp_now_init();
}

void init_transmitter_wireless(void) {

  wireless_hardware_init();

  received_sem = xSemaphoreCreateBinary();

  esp_now_register_recv_cb(on_recv);

  xSemaphoreTake(received_sem, portMAX_DELAY);

  led_trigger();

  /*
  listen for receiver beacon
  assign receiver MAC

  perhaps while(MAC==default value){}

  while((memcmp(receiver_mac, DEFAULT_MAC_VALUE, 6) == 0))
  {
  led_trigger();
  }
  */

  // Defining receiver MAC

  memcpy(peer.peer_addr, receiver_mac, 6);

  esp_now_add_peer(&peer);
}
