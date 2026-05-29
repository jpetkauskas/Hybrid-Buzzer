#include "wireless.h"

void on_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  xQueueSendFromISR(q, data, NULL);
}