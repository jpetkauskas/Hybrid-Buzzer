#include "webserver.h"

#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* Buzz latch owned by main/config: true while a winner is latched in, cleared
   to false by the CLEAR button ISR. Gating the page on it means the display
   resets to "No buzz yet" the instant clear is pressed, with no need to poke
   the web server from interrupt context. */
extern bool latch_state;

#define AP_SSID      "BuzzerReceiver"
#define AP_MAX_CONN  4
#define POLL_MS      400 /* how often the page re-fetches the status text */

/* Stringify so POLL_MS can be embedded directly in the page's script. */
#define STR_(x) #x
#define STR(x)  STR_(x)

/* Most recent buzz winner, updated via webserver_set_winner(). Only shown
   while latch_state is true. 32-bit aligned ints are read/written atomically
   on the ESP32, so no lock is needed for this simple display. */
static volatile int winner_team = -1;
static volatile int winner_player = -1;

void webserver_set_winner(int team, int player)
{
  winner_team = team;
  winner_player = player;
}

/* Served once. The script polls /status and swaps the text in place, so the
   page never reloads — only the ~20-byte status line travels on each tick. */
static esp_err_t root_get_handler(httpd_req_t *req)
{
  static const char page[] =
      "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
      "<title>Buzzer</title></head><body><h1 id=\"s\">...</h1>"
      "<script>"
      "async function p(){try{let r=await fetch('status');"
      "document.getElementById('s').textContent=await r.text();}catch(e){}}"
      "setInterval(p," STR(POLL_MS) ");p();"
      "</script></body></html>";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

/* Minimal payload polled by the page: the current winner, or "No buzz yet". */
static esp_err_t status_get_handler(httpd_req_t *req)
{
  char status[64];
  if (latch_state)
  {
    snprintf(status, sizeof(status), "Team %d, player %d", winner_team, winner_player);
  }
  else
  {
    snprintf(status, sizeof(status), "No buzz yet");
  }

  httpd_resp_set_type(req, "text/plain");
  httpd_resp_send(req, status, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static const httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t status_uri = {
    .uri = "/status",
    .method = HTTP_GET,
    .handler = status_get_handler,
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
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &status_uri);
  }
}
