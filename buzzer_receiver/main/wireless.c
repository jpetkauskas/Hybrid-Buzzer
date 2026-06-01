#include "wireless.h"
#include "buzz.h"
#include "config.h"
#include "driver/gpio.h"
#include "esp_now.h"
#include "packet.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t transmitter_mac_addresses[2][6];

esp_now_peer_info_t peer = {.channel = 0, .encrypt = false,};

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) 
{
  packet received;
  memcpy(&received, data, sizeof(packet));

  uint8_t incoming_mac[6];
  memcpy(incoming_mac, info->src_addr, 6);

  received.transmitter_id = 4;

  if(memcmp(&incoming_mac, transmitter_mac_addresses[0], 6) == 0)
  {
    received.transmitter_id = 0;
    printf("MODIFIED team, 0\n");

  } 
  else if(memcmp(&incoming_mac, transmitter_mac_addresses[1], 6) == 0)
  {
    received.transmitter_id = 1;
    printf("MODIFIED team, 1\n");
  }
  xQueueSendFromISR(q, &received, NULL);
}

void pairing_recv_callback(const esp_now_recv_info_t *info, const uint8_t *data, int len) 
{
  static uint8_t caller_index = 0;

  uint8_t incoming_mac[6];
  memcpy(incoming_mac, info->src_addr, 6);
  
  if ((caller_index != 0) && (memcmp(incoming_mac, transmitter_mac_addresses[caller_index - 1], 6) !=0)) 
  {
    memcpy(transmitter_mac_addresses[caller_index], incoming_mac, 6);
    gpio_set_level(LED_5, 1);
    caller_index++;
    transmitter_b_paired = true;
  } else if (caller_index == 0) 
  {
    memcpy(transmitter_mac_addresses[0], incoming_mac, 6);
    gpio_set_level(LED_1, 1);
    caller_index++;
    transmitter_a_paired = true;
  }

  if (transmitter_a_paired && transmitter_b_paired) {
    printf("pairing complete");
    xSemaphoreGive(pairing_complete);
  }
}

void receiver_hardware_init(void) {
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_now_init();
}

void receiver_init_wireless(void) {

  receiver_hardware_init();

  /*
  register pairing callback
  broadcast pairing beacon every 1500 ms until both transmitters respond
  register regular callback
  */

  pairing_complete = xSemaphoreCreateBinary(); // semaphore for espnow callback

  esp_now_register_recv_cb(pairing_recv_callback);

  memcpy(peer.peer_addr, DEFAULT_MAC_VALUE, 6);
  esp_now_add_peer(&peer);
  esp_now_send(DEFAULT_MAC_VALUE, (uint8_t *)&handshake, sizeof(handshake));

  xSemaphoreTake(
      pairing_complete,
      portMAX_DELAY); // blocks here until receiver pairing beacon is received

  // gpio_set_level(LED_1, 0);
  // gpio_set_level(LED_5, 0);

  esp_now_unregister_recv_cb();
  esp_now_register_recv_cb(on_recv);
}