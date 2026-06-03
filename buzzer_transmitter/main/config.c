#include "config.h"

packet data;
QueueHandle_t q;

uint8_t team = 0;

static button_ctx_t btn_ctx[4] = {
    {SW_1, 1, 0, NULL},
    {SW_2, 2, 0, NULL},
    {SW_3, 3, 0, NULL},
    {SW_4, 4, 0, NULL},
};

void init_transmitter_gpio(QueueHandle_t queue) {

  gpio_config_t led_conf = {
      .pin_bit_mask = (1ULL << LED),
      .mode = GPIO_MODE_OUTPUT,
  };

  gpio_config_t io_conf = {
      .pin_bit_mask =
          (1ULL << SW_1) | (1ULL << SW_2) | (1ULL << SW_3) | (1ULL << SW_4),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_NEGEDGE,
  };

  for (int i = 0; i < 4; i++) {
    btn_ctx[i].q = queue;
    btn_ctx[i].transmitter_id = team;
  }

  gpio_install_isr_service(0);

  gpio_config(&io_conf);
  gpio_config(&led_conf);

  gpio_isr_handler_add(SW_1, button_isr, &btn_ctx[0]);
  gpio_isr_handler_add(SW_2, button_isr, &btn_ctx[1]);
  gpio_isr_handler_add(SW_3, button_isr, &btn_ctx[2]);
  gpio_isr_handler_add(SW_4, button_isr, &btn_ctx[3]);
}

