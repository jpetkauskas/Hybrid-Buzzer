#include "buzz.h"


buzz_profile ba = {.length = 500, .buzzes = 1};
buzz_profile bb = {.length = 200, .buzzes = 2};

buzz_profile bn[] = {{.length = 500, .buzzes = 1},
                     {.length = 200, .buzzes = 2}};

void buzz(void *arg) {
  buzz_profile *b = (buzz_profile *)arg;

  for (int i = 0; i < b->buzzes; i++) {
    gpio_set_level(BUZZ, 1);
    vTaskDelay(pdMS_TO_TICKS(b->length));
    gpio_set_level(BUZZ, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  vTaskDelete(NULL);
}