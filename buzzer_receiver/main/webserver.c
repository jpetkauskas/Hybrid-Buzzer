#include "webserver.h"

#include "captive_dns.h"
#include "esp_attr.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Buzz latch owned by main/config: true while a winner is latched in, cleared
   to false by the CLEAR button ISR. The served status is gated on it, so a
   clear shows up as "No buzz yet". */
extern bool latch_state;

/* Defined in button.c: the same reset the physical CLEAR button performs. */
void IRAM_ATTR clear_buzz(void);

#define AP_SSID         "BuzzerReceiver"
#define AP_MAX_CONN     4
#define MAX_CLIENTS     4    /* concurrent live (SSE) viewers */
#define DHCP_OFFER_DNS  0x02 /* OFFER_DNS bit: hand out a DNS server in DHCP */

/* Most recent buzz winner, updated via webserver_set_winner(). Only shown
   while latch_state is true. */
static volatile int winner_team = -1;
static volatile int winner_player = -1;

static httpd_handle_t server = NULL;

/* Registry of connected SSE client sockets. Touched only from the httpd task
   (the /events handler, the close callback, and the queued broadcast all run
   there), so it needs no lock. */
static int client_fds[MAX_CLIENTS];
static int client_count = 0;

/* Given whenever the status changes (buzz or clear) to wake the notifier. */
static SemaphoreHandle_t event_sem = NULL;

static void format_status(char *buf, size_t len)
{
  if (latch_state)
  {
    snprintf(buf, len, "Team %d, player %d", winner_team, winner_player);
  }
  else
  {
    snprintf(buf, len, "No buzz yet");
  }
}

/* Push one SSE event to a single client. Returns false if the socket is dead. */
static bool sse_send(int fd, const char *status)
{
  char event[80];
  int n = snprintf(event, sizeof(event), "data: %s\n\n", status);
  return httpd_socket_send(server, fd, event, n, 0) >= 0;
}

/* Runs in the httpd task (via httpd_queue_work): broadcast status to every
   client, dropping any whose socket has died. */
static void broadcast_work(void *arg)
{
  char status[64];
  format_status(status, sizeof(status));

  for (int i = 0; i < client_count;)
  {
    if (sse_send(client_fds[i], status))
    {
      i++;
    }
    else
    {
      client_fds[i] = client_fds[--client_count]; /* swap-remove dead client */
    }
  }
}

/* httpd_queue_work() is neither ISR-safe nor meant to run inline from the buzz
   path, so a dedicated task bridges the change signal to a broadcast that runs
   in the httpd task. */
static void notifier_task(void *arg)
{
  for (;;)
  {
    xSemaphoreTake(event_sem, portMAX_DELAY);
    if (server)
    {
      httpd_queue_work(server, broadcast_work, NULL);
    }
  }
}

void webserver_set_winner(int team, int player)
{
  winner_team = team;
  winner_player = player;
  if (event_sem)
  {
    xSemaphoreGive(event_sem);
  }
}

void IRAM_ATTR webserver_notify_clear_from_isr(void)
{
  if (!event_sem)
  {
    return;
  }
  BaseType_t higher_priority_task_woken = pdFALSE;
  xSemaphoreGiveFromISR(event_sem, &higher_priority_task_woken);
  if (higher_priority_task_woken == pdTRUE)
  {
    portYIELD_FROM_ISR();
  }
}

/* Catch-all page handler. Served for any path (see catch_all_uri) so every URL
   a client tries — including OS captive-portal probes — lands on the buzzer
   page. EventSource opens a long-lived /events stream and the handler swaps the
   text in place, so the page never reloads and updates the instant a buzz or
   clear happens. */
static esp_err_t page_get_handler(httpd_req_t *req)
{
  static const char page[] =
      "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
      "<title>Buzzer</title></head><body><h1 id=\"s\">...</h1>"
      "<button onclick=\"fetch('clear',{method:'POST'})\">Clear</button>"
      "<script>"
      "var e=new EventSource('events');"
      "e.onmessage=function(ev){document.getElementById('s').textContent=ev.data;};"
      "</script></body></html>";

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

/* SSE stream. Sends raw headers (so we can keep writing via httpd_socket_send
   from broadcast_work), registers the socket, and pushes the current state
   once so a fresh page isn't blank. Returns immediately — never blocks. */
static esp_err_t events_get_handler(httpd_req_t *req)
{
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/event-stream\r\n"
      "Cache-Control: no-cache\r\n"
      "Connection: keep-alive\r\n\r\n";

  int fd = httpd_req_to_sockfd(req);
  if (fd < 0)
  {
    return ESP_FAIL;
  }

  if (httpd_socket_send(server, fd, headers, sizeof(headers) - 1, 0) < 0)
  {
    return ESP_FAIL;
  }

  if (client_count < MAX_CLIENTS)
  {
    client_fds[client_count++] = fd;
  }

  char status[64];
  format_status(status, sizeof(status));
  sse_send(fd, status);

  return ESP_OK;
}

/* Web "Clear" button. Performs the same reset as the physical CLEAR button,
   then pushes the cleared state to every client. Runs in the httpd task (same
   context as broadcast_work), so it can broadcast directly. */
static esp_err_t clear_post_handler(httpd_req_t *req)
{
  clear_buzz();
  broadcast_work(NULL);
  httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

/* Called by httpd when any socket closes; deregister SSE clients so a reused
   fd is never mistaken for a live stream. We override the default close, so we
   must close the socket ourselves. */
static void on_socket_close(httpd_handle_t hd, int fd)
{
  for (int i = 0; i < client_count; i++)
  {
    if (client_fds[i] == fd)
    {
      client_fds[i] = client_fds[--client_count];
      break;
    }
  }
  close(fd);
}

static const httpd_uri_t events_uri = {
    .uri = "/events",
    .method = HTTP_GET,
    .handler = events_get_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t clear_uri = {
    .uri = "/clear",
    .method = HTTP_POST,
    .handler = clear_post_handler,
    .user_ctx = NULL,
};

/* Registered last so the more specific /events wins; matches everything else. */
static const httpd_uri_t catch_all_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = page_get_handler,
    .user_ctx = NULL,
};

void start_webserver(void)
{
  event_sem = xSemaphoreCreateBinary();
  xTaskCreate(notifier_task, "sse_notifier", 3072, NULL, 5, NULL);

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

  /* Captive portal: make the AP's DHCP hand out this device as the DNS server,
     then run a DNS server that resolves every name to the AP's IP. Any URL a
     connected client opens then lands here — no per-client setup, no IP to
     type. */
  esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
  esp_netif_ip_info_t ip_info;
  esp_netif_get_ip_info(ap_netif, &ip_info);

  esp_netif_dns_info_t dns_info = {0};
  dns_info.ip.type = ESP_IPADDR_TYPE_V4;
  dns_info.ip.u_addr.ip4 = ip_info.ip;

  uint8_t offer_dns = DHCP_OFFER_DNS;
  esp_netif_dhcps_stop(ap_netif);
  esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns_info);
  esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET, ESP_NETIF_DOMAIN_NAME_SERVER,
                         &offer_dns, sizeof(offer_dns));
  esp_netif_dhcps_start(ap_netif);

  start_captive_dns(ip_info.ip);

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.close_fn = on_socket_close;
  config.uri_match_fn = httpd_uri_match_wildcard; /* enables the catch-all route */
  if (httpd_start(&server, &config) == ESP_OK)
  {
    httpd_register_uri_handler(server, &events_uri);
    httpd_register_uri_handler(server, &clear_uri);
    httpd_register_uri_handler(server, &catch_all_uri);
  }
}
