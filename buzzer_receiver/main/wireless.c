#include "wireless.h"
#include <stdint.h>
#include <string.h>

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  xQueueSendFromISR(q, data, NULL);
}

void pairing_recv_callback(const esp_now_recv_info_t *info, const uint8_t *data,
                           int len) {
  static uint8_t caller_index = 0;

  uint8_t incoming_mac[6];
  memcpy(incoming_mac, info->src_addr, 6);
  if ((caller_index != 0) && (memcmp(incoming_mac, transmitter_mac_addresss[caller_index], 6) != 0)) 
  {
    memcpy(transmitter_mac_addresss[caller_index], incoming_mac, 6);
    caller_index++;
  } 
  else if (caller_index == 0) {
    memcpy(transmitter_mac_addresss[0], incoming_mac, 6);
    caller_index++;
  }

  if (transmitter_a_paired && transmitter_b_paired) {
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

  pairing_complete = xSemaphoreCreateBinary(); // semaphore for espnow callback

  esp_now_register_recv_cb(pairing_recv_callback);

  received_sem = xSemaphoreCreateBinary(); // semaphore for espnow callback
  */
  esp_now_register_recv_cb(on_recv);
}