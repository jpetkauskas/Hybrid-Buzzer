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

#include "button.h"
#include "buzz.h"
#include "config.h"
#include "webserver.h"
#include "wireless.h"

void app_main(void)
{
  q = xQueueCreate(10, sizeof(packet)); //FreeRTOS packet

  receiver_init_gpio(); //GPIO helper

  init_buzz(); //initializes buzzer FreeRTOS overhead

  send_buzz(&bn[0]); //buzz once on powerup

  init_sync_task(); //must come before receiver_init_wireless

  receiver_init_wireless(); //espnow overhead + autopair, returns once ready

  while (1)
  {
    packet first;
    if (xQueueReceive(q, &first, portMAX_DELAY) != pdTRUE) continue;

    if (latch_state) continue; /* discard packets that arrived while latched */

    /* Collect a competing packet within the arbitration window. */
    packet winner = first;
    packet competitor;
    bool contested = false;
    if (xQueueReceive(q, &competitor, pdMS_TO_TICKS(20)) == pdTRUE)
    {
      contested = true;
      if (competitor.timestamp_us < winner.timestamp_us)
      {
        winner = competitor;
      }
    }

    latch_state = true;

    int8_t team   = winner.transmitter_id;
    int8_t player = winner.player_id;

    gpio_set_level(led_array[(team * 4) + player - 1], 1);

    buzz_profile *bp = &bn[team];
    xQueueSend(buzz_queue, &bp, 0);

    webserver_set_winner(team, player);
  }
}
