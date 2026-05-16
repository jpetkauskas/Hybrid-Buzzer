#include "button.h"
#include "config.h"

int64_t last_fire[GPIO_NUM_MAX];

void IRAM_ATTR button_isr(void *arg) {
  uint32_t pin = (uint32_t)arg;
  uint8_t pin_id;

  if (gpio_get_level(pin) != 0)
    return;
  int64_t now = esp_timer_get_time();
  if (now - last_fire[pin] < DEBOUNCE_US)
    return;
  last_fire[pin] = now;

  switch (pin) {
    case SW_1:
      pin_id = 1;
      break;
    case SW_2:
      pin_id = 2;
      break;
    case SW_3:
      pin_id = 3;
      break;
    case SW_4:
      pin_id = 4;
      break;
    default:
      pin_id = 0;
  }

  data.player_id = pin_id;
  data.transmitter_id = team;
  xQueueSendFromISR(q, &data, NULL);
}