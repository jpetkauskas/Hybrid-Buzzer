#include "wireless.h"
#include "esp_now.h"
#include "led.h"
#include <stdint.h>
#include <string.h>

uint8_t receiver_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // save from previous default?

esp_now_peer_info_t peer = {.channel = 0, .encrypt = false,};

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) 
{
  
  packet *handshake;
  memcpy(&handshake, &data, sizeof(data));

  if(handshake->player_id == 3 && handshake->transmitter_id == 3 && (memcmp(handshake->transmitter_mac, DEFAULT_MAC_VALUE, 6) == 0))
  {
    uint8_t *sender_mac = info->src_addr;
    memcpy(receiver_mac, sender_mac, 6);
    led_trigger();
    esp_now_unregister_recv_cb();
    xSemaphoreGive(received_sem);
  }
}

//  ESPNOW hardware initialization helper
static void wireless_hardware_init(void) 
{
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_now_init();
}

void init_transmitter_wireless(void) 
{
  wireless_hardware_init();
  received_sem = xSemaphoreCreateBinary(); // semaphore for espnow callback

  esp_now_register_recv_cb(on_recv);
  xSemaphoreTake(received_sem, portMAX_DELAY); // blocks here until receiver pairing beacon is received

  // Defining receiver MAC
  memcpy(peer.peer_addr, receiver_mac, 6);
  esp_now_add_peer(&peer);
}
