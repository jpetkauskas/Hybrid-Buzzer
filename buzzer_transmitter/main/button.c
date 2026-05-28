#include "button.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "packet.h"

int64_t last_fire[GPIO_NUM_MAX];

void IRAM_ATTR button_isr(void *arg) {
  button_ctx_t *ctx = (button_ctx_t *)arg;

  uint8_t pin_id = ctx->pin_id;
  uint8_t pin = ctx->pin;


  if (gpio_get_level(ctx->pin) != 0)
  {
    return;
  }
  int64_t now = esp_timer_get_time();

  if (now - last_fire[pin] < DEBOUNCE_US)
    return;
  last_fire[pin] = now;

  packet to_send;

  to_send.player_id = pin_id;
  to_send.transmitter_id = ctx->transmitter_id;
  xQueueSendFromISR(ctx->q, &to_send, NULL);