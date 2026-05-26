#include "led.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static TaskHandle_t led_handle;

static void flash_led(void *arg) {
  while (1) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    gpio_set_level(LED, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    gpio_set_level(LED, 0);
  }
}

void led_init(void) {
  xTaskCreate(flash_led, "flash", 2048, NULL, 10, &led_handle);
}

void led_trigger(void) {
  xTaskNotifyGive(led_handle);
}
