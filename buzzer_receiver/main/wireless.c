#include "wireless.h"
#include "buzz.h"
#include "config.h"
#include "webserver.h"
#include "driver/gpio.h"
#include "esp_now.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "packet.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

uint8_t transmitter_mac_addresses[2][6];

static TaskHandle_t sync_task_handle = NULL;
static volatile uint8_t current_epoch = 0;

esp_now_peer_info_t peer = {.channel = 0, .encrypt = false,};

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
  if (len < (int)sizeof(packet)) return;

  packet received;
  memcpy(&received, data, sizeof(packet));

  /* Drop sync packets and stale packets from a previous epoch. */
  if (received.transmitter_id == PACKET_SYNC_ID) return;
  if (received.epoch != current_epoch) return;

  uint8_t incoming_mac[6];
  memcpy(incoming_mac, info->src_addr, 6);

  if (memcmp(incoming_mac, transmitter_mac_addresses[0], 6) == 0)
  {
    received.transmitter_id = 0;
  }
  else if (memcmp(incoming_mac, transmitter_mac_addresses[1], 6) == 0)
  {
    received.transmitter_id = 1;
  }
  else
  {
    return; /* unknown source */
  }

  xQueueSendFromISR(q, &received, NULL);
}

/* ── Sync task ──────────────────────────────────────────────────────────── */

static void sync_sender_task(void *arg)
{
  while (1)
  {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    current_epoch++;
    packet sync_pkt = {
        .transmitter_id = PACKET_SYNC_ID,
        .player_id      = PACKET_SYNC_ID,
        .transmitter_mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
        .timestamp_us   = 0xFFFFFFFF,
        .epoch          = current_epoch,
    };
    esp_now_send(DEFAULT_MAC_VALUE, (uint8_t *)&sync_pkt, sizeof(sync_pkt));
  }
}

void init_sync_task(void)
{
  xTaskCreate(sync_sender_task, "sync", 2048, NULL, 10, &sync_task_handle);
}

void request_sync(void)
{
  if (sync_task_handle) xTaskNotifyGive(sync_task_handle);
}

void IRAM_ATTR request_sync_from_isr(void)
{
  if (!sync_task_handle) return;
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(sync_task_handle, &woken);
  portYIELD_FROM_ISR(woken);
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
    gpio_set_level(LED_6, 1);
    gpio_set_level(LED_7, 1);
    gpio_set_level(LED_8, 1);
    send_buzz(&bb);
    caller_index++;
    transmitter_b_paired = true;
  } 
  else if (caller_index == 0) 
  {
    memcpy(transmitter_mac_addresses[0], incoming_mac, 6);
    gpio_set_level(LED_1, 1);
    gpio_set_level(LED_2, 1);
    gpio_set_level(LED_3, 1);
    gpio_set_level(LED_4, 1);
    send_buzz(&ba);
    caller_index++;
    transmitter_a_paired = true;
  }

  if (transmitter_a_paired && transmitter_b_paired) 
  {
    // pairing complete sequence
    xSemaphoreGive(pairing_complete);
  }
}

void receiver_hardware_init(void) 
{
  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  esp_now_init();

  start_webserver(); //boots Hello World web server immediately, independent of ESPNOW pairing
}

void receiver_init_wireless(void) 
{

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

  while(xSemaphoreTake(pairing_complete, pdMS_TO_TICKS(2500)) != pdTRUE) // blocks here until receiver pairing beacon is received
  {
    esp_now_send(DEFAULT_MAC_VALUE, (uint8_t *)&handshake, sizeof(handshake));
  }

  gpio_set_level(LED_1, 0);
  gpio_set_level(LED_2, 0);
  gpio_set_level(LED_3, 0);
  gpio_set_level(LED_4, 0);
  gpio_set_level(LED_5, 0);
  gpio_set_level(LED_6, 0);
  gpio_set_level(LED_7, 0);
  gpio_set_level(LED_8, 0);
  
  send_buzz(&bb);
  send_buzz(&bb);

  esp_now_unregister_recv_cb();
  esp_now_register_recv_cb(on_recv);

  request_sync(); /* establish epoch 1 before the first question */
}