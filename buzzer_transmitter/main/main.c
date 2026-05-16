#include "driver/gpio.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "packet.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "config.h"

void flash_led(void *arg) {
  gpio_set_level(LED, 1);
  vTaskDelay(pdMS_TO_TICKS(500)); // on for 500ms
  gpio_set_level(LED, 0);
  vTaskDelete(NULL);
}

void app_main(void) {
  q = xQueueCreate(10, sizeof(uint32_t));

  init_transmitter_gpio();
  init_transmitter_wireless();

  xTaskCreate(flash_led, "flash", 2048, NULL, 10, NULL);

  while (1) {
    if (xQueueReceive(q, &data, portMAX_DELAY)) {
      esp_now_send(receiver_mac, (uint8_t *)&data, sizeof(data));
      printf("Pin %d fired\n", data.player_id);
      xTaskCreate(flash_led, "flash", 2048, NULL, 10, NULL);
    }
  }
}