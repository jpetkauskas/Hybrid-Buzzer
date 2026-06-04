#include "webserver.h"

#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <string.h>

#define AP_SSID     "BuzzerReceiver"
#define AP_MAX_CONN 4

static esp_err_t hello_get_handler(httpd_req_t *req)
{
  const char *resp = "Hello World";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static const httpd_uri_t hello_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = hello_get_handler,
    .user_ctx = NULL,
};

void start_webserver(void)
{
  /* Keep the AP on whatever channel ESPNOW is currently using so the single
     radio's channel is never changed out from under ESPNOW. */
  uint8_t primary_channel = 1;
  wifi_second_chan_t second_channel;
  esp_wifi_get_channel(&primary_channel, &second_channel);

  esp_netif_create_default_wifi_ap();

  wifi_config_t ap_config = {
      .ap = {
          .ssid = AP_SSID,
          .ssid_len = strlen(AP_SSID),
          .channel = primary_channel,
          .max_connection = AP_MAX_CONN,
          .authmode = WIFI_AUTH_OPEN,
      },
  };

  /* Enable AP in addition to the existing STA interface (ESPNOW). */
  esp_wifi_set_mode(WIFI_MODE_APSTA);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  httpd_handle_t server = NULL;
  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &hello_uri);
  }
}
