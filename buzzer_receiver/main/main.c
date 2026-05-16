#include "driver/gpio.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_now.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "packet.h"
#include "portmacro.h"
#include "sdkconfig.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"
#include "button.h"
#include "wireless.h"
#include "buzz.h"

packet incoming;

void app_main(void) {
  q = xQueueCreate(10, sizeof(packet));

  receiver_init_gpio();
  receiver_init_wireless();


  xTaskCreate(buzz, "buzz", 2048, &bn[0], 10, NULL);

  while (1) {
    if (xQueueReceive(q, &incoming, portMAX_DELAY) && !latch_state) {
      latch_state = true;

      int8_t team = incoming.transmitter_id;
      int8_t player = incoming.player_id;

      uint32_t player_led_index = (team * 4) + player;
      gpio_set_level(led_array[player_led_index - 1], 1);

      xTaskCreate(buzz, "buzz", 2048, &bn[team], 10, NULL);

      printf("Team %d, player %d\n", team, player);
    }
  }
}
