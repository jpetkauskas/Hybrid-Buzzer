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

  // initialization steps
  init_transmitter_gpio(q);

  led_init();

  init_transmitter_wireless();

  // flash LED once on boot
  led_trigger();

  // pair
  /*
  while(!system_pair())
  {
  }
  */

  //
  while (1) {
    if (xQueueReceive(q, &data, portMAX_DELAY)) {
      esp_now_send(receiver_mac, (uint8_t *)&data, sizeof(data));
      printf("Pin %d fired\n", data.player_id);
      led_trigger();
    }
  }
}