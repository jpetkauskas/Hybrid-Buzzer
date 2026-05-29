#include "buzz.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"

QueueHandle_t buzz_queue;

buzz_profile ba = {.length = 500, .buzzes = 1};
buzz_profile bb = {.length = 200, .buzzes = 2};

buzz_profile bn[] = {{.length = 500, .buzzes = 1},
                     {.length = 200, .buzzes = 2}};

void buzz(void *arg) {
  buzz_profile *b = (buzz_profile *)arg;
  while (1) {
    if (xQueueReceive(buzz_queue, &b, portMAX_DELAY)) {
      for (int i = 0; i < b->buzzes; i++) {
        gpio_set_level(BUZZ, 1);
        vTaskDelay(pdMS_TO_TICKS(b->length));
        gpio_set_level(BUZZ, 0);
        vTaskDelay(pdMS_TO_TICKS(50));
      }
    }
  }
}