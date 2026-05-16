#include "config.h"

QueueHandle_t q;

bool latch_state = false;


gpio_config_t io_conf = {
    .pin_bit_mask = (1ULL << CLEAR),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_NEGEDGE,
};

gpio_config_t output_conf = {
    .pin_bit_mask = (1ULL << LED_1) | (1ULL << LED_2) | (1ULL << LED_3) |
                    (1ULL << LED_4) | (1ULL << LED_5) | (1ULL << LED_6) |
                    (1ULL << LED_7) | (1ULL << LED_8) | (1ULL << BUZZ),
    .mode = GPIO_MODE_OUTPUT,
};

void receiver_init_gpio(void) {
  gpio_install_isr_service(0);

  gpio_config(&output_conf);

  gpio_config(&io_conf);
  gpio_isr_handler_add(CLEAR, button_isr, (void *)NULL);
}

void receiver_init_wireless(void) {
  nvs_flash_init();

  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();

  esp_now_init();

  esp_now_register_recv_cb(on_recv);
}
