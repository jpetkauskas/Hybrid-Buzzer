#include "buttons.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define SW_1 3
#define SW_2 46
#define SW_3 9
#define SW_4 10

#define DEBOUNCE_US 50000

static QueueHandle_t btn_q;
static int64_t last_fire[GPIO_NUM_MAX];

static gpio_config_t io_conf = {
    .pin_bit_mask =
        (1ULL << SW_1) | (1ULL << SW_2) | (1ULL << SW_3) | (1ULL << SW_4),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_NEGEDGE,
};

static void IRAM_ATTR button_isr(void *arg) {
    uint32_t pin = (uint32_t)arg;

    if (gpio_get_level(pin) != 0)
        return;
    int64_t now = esp_timer_get_time();
    if (now - last_fire[pin] < DEBOUNCE_US)
        return;
    last_fire[pin] = now;

    uint8_t pin_id;
    switch (pin) {
        case SW_1: pin_id = 1; break;
        case SW_2: pin_id = 2; break;
        case SW_3: pin_id = 3; break;
        case SW_4: pin_id = 4; break;
        default:   pin_id = 0;
    }

    xQueueSendFromISR(btn_q, &pin_id, NULL);
}

void buttons_init(QueueHandle_t q) {
    btn_q = q;
    gpio_config(&io_conf);
    gpio_isr_handler_add(SW_1, button_isr, (void *)SW_1);
    gpio_isr_handler_add(SW_2, button_isr, (void *)SW_2);
    gpio_isr_handler_add(SW_3, button_isr, (void *)SW_3);
    gpio_isr_handler_add(SW_4, button_isr, (void *)SW_4);
}
