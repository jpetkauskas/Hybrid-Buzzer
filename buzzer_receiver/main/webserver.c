#include "webserver.h"

#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <string.h>

#define AP_SSID     "BuzzerReceiver"
#define AP_MAX_CONN 4

/* Most recent buzz winner, updated via webserver_set_winner(). Negative team
   means nobody has buzzed yet. 32-bit aligned ints are read/written
   atomically on the ESP32, so no lock is needed for this simple display. */
static volatile int winner_team = -1;
static volatile int winner_player = -1;

void webserver_set_winner(int team, int player)
{
  winner_team = team;
  winner_player = player;
}

static esp_err_t winner_get_handler(httpd_req_t *req)
{
  int team = winner_team;
  int player = winner_player;

  char resp[64];
  if (team < 0)
  {
    snprintf(resp, sizeof(resp), "No buzz yet");
  }
  else
  {
    snprintf(resp, sizeof(resp), "Team %d, player %d", team, player);
  }

  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static const httpd_uri_t winner_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = winner_get_handler,
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
    httpd_register_uri_handler(server, &winner_uri);
  }
}
