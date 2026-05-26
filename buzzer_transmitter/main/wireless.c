#include "wireless.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include <string.h>

uint8_t receiver_mac[6] = {0x20, 0x6E, 0xF1, 0xE1, 0xA0, 0xFC};

void init_transmitter_wireless(void) {
  nvs_flash_init();

  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  esp_now_init();

  esp_now_peer_info_t peer = {
      .channel = 0,
      .encrypt = false,
  };

  memcpy(peer.peer_addr, receiver_mac, 6);
  esp_now_add_peer(&peer);
}
