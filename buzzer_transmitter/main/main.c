#include "driver/gpio.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led.h"
#include "packet.h"
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "wireless.h"

void app_main(void) {
  // FreeRTOS queue
  q = xQueueCreate(10, sizeof(packet));

  // initializes
  init_transmitter_gpio(q);

  //initializes LED freeRTOS task
  led_init();

  //initializes ESPNOW and returns when paired to receiver
  init_transmitter_wireless();

  while (1) {
    if (xQueueReceive(q, &data, portMAX_DELAY)) {
      esp_now_send(receiver_mac, (uint8_t *)&data, sizeof(data));
      printf("Pin %d fired\n", data.player_id);
      led_trigger();
    }
  }
}