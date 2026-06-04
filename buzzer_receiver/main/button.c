#include "button.h"
#include "webserver.h"

int64_t last_fire;

void IRAM_ATTR button_isr(void *arg) {
  if (gpio_get_level(CLEAR) != 0)
    return;
  int64_t now = esp_timer_get_time();
  if (now - last_fire < DEBOUNCE_US)
    return;
  last_fire = now;

  for (int i = 0; i < 8; i++) {
    gpio_set_level(led_array[i], 0);
  }

  latch_state = false;

  webserver_notify_clear_from_isr(); //push the cleared state to web clients
}
