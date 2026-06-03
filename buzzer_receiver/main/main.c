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
#include "wireless.h"

packet incoming; //incoming espnow data buffer

void app_main(void) 
{
  q = xQueueCreate(10, sizeof(packet)); //FreeRTOS packet

  receiver_init_gpio(); //GPIO helper
  
  init_buzz(); //initializes buzzer FreeRTOS overhead

  send_buzz(&bn[0]); //buzz once on powerup

  receiver_init_wireless(); //espnow overhead + autopair, returns once ready 

  while (1) 
  {
    if (xQueueReceive(q, &incoming, portMAX_DELAY) && !latch_state) 
    {
      latch_state = true;

      int8_t team = incoming.transmitter_id;
      int8_t player = incoming.player_id;

      uint32_t player_led_index = (team * 4) + player;
      gpio_set_level(led_array[player_led_index - 1], 1);

      buzz_profile *bp = &bn[team];
      xQueueSend(buzz_queue, &bp, 0);

      printf("Team %d, player %d\n", team, player);
    }
  }
}
