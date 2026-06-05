#include "webserver.h"

#include "captive_dns.h"
#include "esp_attr.h"
#include "esp_http_server.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
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
#define NUM_TEAMS        2 /* matches the two transmitters */
#define PLAYERS_PER_TEAM 4 /* player_id is 1-based (1..4); see main.c LED index */

/* Most recent buzz winner, updated via webserver_set_winner(). Only shown
   while latch_state is true. team is 0-based, player is 1-based. */
static volatile int winner_team = -1;
static volatile int winner_player = -1;

/* Per-player match stats, indexed [team][player-1]. Adjusted only from the
   httpd task (score/reset handlers), so they need no lock. A team's total is
   the sum of its players' points. Served raw via /stats.json; the page adds
   names and builds the CSV. */
static int player_scores[NUM_TEAMS][PLAYERS_PER_TEAM] = {{0}};
static int player_powers[NUM_TEAMS][PLAYERS_PER_TEAM] = {{0}}; /* awards >= 15 */
static int player_tens[NUM_TEAMS][PLAYERS_PER_TEAM] = {{0}};   /* awards 1..14 */
static int player_negs[NUM_TEAMS][PLAYERS_PER_TEAM] = {{0}};   /* negative awards */

#define POWER_POINTS 15 /* an award this large or more counts as a power */

/* Index of the current packet question. Advanced on each clear/score cycle and
   broadcast to clients, which hold the parsed packet and render questions[q].
   Written only from the httpd task (see clear_pending_from_isr for the ISR
   path), so it needs no lock. */
static int question_index = 0;

/* Number of questions in the loaded packet, reported by the page so the index
   can be clamped to the last question. 0 = unknown (no packet loaded yet). */
static int question_count = 0;

/* Set by the CLEAR ISR so the httpd-task broadcast advances the question index,
   keeping all index writes on one task. */
static volatile bool clear_pending_from_isr = false;

/* Teams that have negged the current question (shown struck through on the
   page). Marked in the score handler; cleared automatically when the question
   advances — see broadcast_work. */
static bool team_negged[NUM_TEAMS] = {false};

/* The last index broadcast; when it changes the question has advanced, which
   resets the per-question neg flags. */
static int last_broadcast_index = -1;

/* Random per-boot id sent to clients. When the page sees it change, it knows
   the device power-cycled and discards any packet it had cached. */
static uint32_t session_id = 0;

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

/* Build the JSON state pushed to clients: buzz status, whether a buzz is
   active (and who), per-team totals, and per-player scores. The status text is
   fully controlled and contains no JSON-special characters, so it can be
   embedded without escaping. Every append is guarded so a full buffer can only
   truncate, never overflow. */
static void format_state_json(char *buf, size_t len)
{
  char status[64];
  format_status(status, sizeof(status));

  int n = 0;
  if (n < (int)len)
  {
    n += snprintf(buf + n, len - n,
                  "{\"status\":\"%s\",\"active\":%s,\"wteam\":%d,\"wplayer\":%d,\"q\":%d,\"sid\":%u,\"teams\":[",
                  status, latch_state ? "true" : "false", winner_team, winner_player,
                  question_index, (unsigned)session_id);
  }

  for (int t = 0; t < NUM_TEAMS && n < (int)len; t++)
  {
    int total = 0;
    for (int p = 0; p < PLAYERS_PER_TEAM; p++)
    {
      total += player_scores[t][p];
    }
    n += snprintf(buf + n, len - n, "%s%d", t ? "," : "", total);
  }

  if (n < (int)len)
  {
    n += snprintf(buf + n, len - n, "],\"players\":[");
  }

  for (int t = 0; t < NUM_TEAMS && n < (int)len; t++)
  {
    n += snprintf(buf + n, len - n, "%s[", t ? "," : "");
    for (int p = 0; p < PLAYERS_PER_TEAM && n < (int)len; p++)
    {
      n += snprintf(buf + n, len - n, "%s%d", p ? "," : "", player_scores[t][p]);
    }
    if (n < (int)len)
    {
      n += snprintf(buf + n, len - n, "]");
    }
  }

  if (n < (int)len)
  {
    n += snprintf(buf + n, len - n, "],\"negged\":[");
  }
  for (int t = 0; t < NUM_TEAMS && n < (int)len; t++)
  {
    n += snprintf(buf + n, len - n, "%s%s", t ? "," : "", team_negged[t] ? "true" : "false");
  }
  if (n < (int)len)
  {
    snprintf(buf + n, len - n, "]}");
  }
}

/* Push one SSE event (the JSON state payload) to a single client. Returns
   false if the socket is dead. */
static bool sse_send(int fd, const char *payload)
{
  char event[320];
  int n = snprintf(event, sizeof(event), "data: %s\n\n", payload);
  return httpd_socket_send(server, fd, event, n, 0) >= 0;
}

/* Runs in the httpd task (via httpd_queue_work): broadcast state to every
   client, dropping any whose socket has died. */
static void broadcast_work(void *arg)
{
  /* A physical CLEAR press flagged from the ISR advances the question here, in
     the httpd task, so question_index is never written from interrupt context.
     Web clear/score advance it directly in their handlers. */
  if (clear_pending_from_isr)
  {
    clear_pending_from_isr = false;
    question_index++;
  }

  /* Keep the index within the packet: never past the last question, never < 0.
     Done here so every path (clear, score, nav, physical button) is bounded. */
  if (question_count > 0 && question_index > question_count - 1)
  {
    question_index = question_count - 1;
  }
  if (question_index < 0)
  {
    question_index = 0;
  }

  /* A new question clears the per-question neg flags. */
  if (question_index != last_broadcast_index)
  {
    last_broadcast_index = question_index;
    for (int t = 0; t < NUM_TEAMS; t++)
    {
      team_negged[t] = false;
    }
  }

  char payload[256];
  format_state_json(payload, sizeof(payload));

  for (int i = 0; i < client_count;)
  {
    if (sse_send(client_fds[i], payload))
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
  clear_pending_from_isr = true; /* broadcast_work advances the question */
  BaseType_t higher_priority_task_woken = pdFALSE;
  xSemaphoreGiveFromISR(event_sem, &higher_priority_task_woken);
  if (higher_priority_task_woken == pdTRUE)
  {
    portYIELD_FROM_ISR();
  }
}

/* index.html is embedded into the firmware (see EMBED_TXTFILES in CMakeLists);
   EMBED_TXTFILES null-terminates it, so it can be served as a C string. */
extern const char index_html_start[] asm("_binary_index_html_start");

/* Catch-all page handler. Served for any path (see catch_all_uri) so every URL
   a client tries — including OS captive-portal probes — lands on the buzzer
   page. The page opens a long-lived /events stream and updates in place, so it
   never reloads and reflects buzzes, clears, and scores the instant they
   change. */
static esp_err_t page_get_handler(httpd_req_t *req)
{
  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, index_html_start, HTTPD_RESP_USE_STRLEN);
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

  char payload[256];
  format_state_json(payload, sizeof(payload));
  sse_send(fd, payload);

  return ESP_OK;
}

/* Web "Clear" button. Performs the same reset as the physical CLEAR button,
   then pushes the cleared state to every client. Runs in the httpd task (same
   context as broadcast_work), so it can broadcast directly. */
static esp_err_t clear_post_handler(httpd_req_t *req)
{
  clear_buzz();
  question_index++; /* each clear/arm cycle advances to the next question */
  broadcast_work(NULL);
  httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

/* Award points to the player who buzzed: POST /score?delta=<d>. The points go
   to the current buzz winner (team/player), so only a latched-in buzz can
   score — mirroring real play, where you award whoever rang in. Runs in the
   httpd task, so it updates state and broadcasts directly. */
static esp_err_t score_post_handler(httpd_req_t *req)
{
  char query[32];
  char value[12];
  int delta = 0;

  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK &&
      httpd_query_key_value(query, "delta", value, sizeof(value)) == ESP_OK)
  {
    delta = atoi(value);
  }

  if (latch_state && winner_team >= 0 && winner_team < NUM_TEAMS &&
      winner_player >= 1 && winner_player <= PLAYERS_PER_TEAM)
  {
    int wp = winner_player - 1;
    player_scores[winner_team][wp] += delta;
    if (delta < 0)
    {
      player_negs[winner_team][wp]++;
      team_negged[winner_team] = true; /* struck through until the question advances */
    }
    else if (delta >= POWER_POINTS)
    {
      player_powers[winner_team][wp]++;
    }
    else if (delta > 0)
    {
      player_tens[winner_team][wp]++;
    }
  }

  clear_buzz(); /* end this buzz so another team can ring in */
  if (delta > 0)
  {
    question_index++; /* a correct award moves on; a neg stays so others answer */
  }
  broadcast_work(NULL);
  httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

/* Zero every player's score: POST /resetscores. */
static esp_err_t resetscores_post_handler(httpd_req_t *req)
{
  for (int t = 0; t < NUM_TEAMS; t++)
  {
    for (int p = 0; p < PLAYERS_PER_TEAM; p++)
    {
      player_scores[t][p] = 0;
      player_powers[t][p] = 0;
      player_tens[t][p] = 0;
      player_negs[t][p] = 0;
    }
  }
  broadcast_work(NULL);
  httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

/* Manually move through the packet (for skips, corrections, restart):
   POST /nav?to=<n> jumps to an absolute index, POST /nav?d=<±n> moves
   relative. Index is clamped at 0; the page clamps the upper end. */
static esp_err_t nav_post_handler(httpd_req_t *req)
{
  char query[32];
  char value[12];

  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK)
  {
    /* count: how many questions the loaded packet has, for clamping. */
    if (httpd_query_key_value(query, "count", value, sizeof(value)) == ESP_OK)
    {
      question_count = atoi(value);
    }
    if (httpd_query_key_value(query, "to", value, sizeof(value)) == ESP_OK)
    {
      question_index = atoi(value);
    }
    else if (httpd_query_key_value(query, "d", value, sizeof(value)) == ESP_OK)
    {
      question_index += atoi(value);
    }
  }

  broadcast_work(NULL); /* clamps the index to [0, count - 1] */
  httpd_resp_send(req, "ok", HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

/* Append one "name":[[team0 players],[team1 players]] matrix to buf. first=1
   omits the leading comma. Returns the new length. */
static int append_matrix(char *buf, int n, int len, const char *name,
                         int m[NUM_TEAMS][PLAYERS_PER_TEAM], int first)
{
  if (n < len) n += snprintf(buf + n, len - n, "%s\"%s\":[", first ? "" : ",", name);
  for (int t = 0; t < NUM_TEAMS && n < len; t++)
  {
    n += snprintf(buf + n, len - n, "%s[", t ? "," : "");
    for (int p = 0; p < PLAYERS_PER_TEAM && n < len; p++)
    {
      n += snprintf(buf + n, len - n, "%s%d", p ? "," : "", m[t][p]);
    }
    if (n < len) n += snprintf(buf + n, len - n, "]");
  }
  if (n < len) n += snprintf(buf + n, len - n, "]");
  return n;
}

/* Raw per-player match stats as JSON: GET /stats.json. The page owns the
   team/player names and combines them with these counters to build and
   download the CSV, so no string/CSV formatting is needed on the device. */
static esp_err_t stats_json_handler(httpd_req_t *req)
{
  char buf[512];
  int len = sizeof(buf), n = 0;
  if (n < len) n += snprintf(buf + n, len - n, "{");
  n = append_matrix(buf, n, len, "powers", player_powers, 1);
  n = append_matrix(buf, n, len, "tens", player_tens, 0);
  n = append_matrix(buf, n, len, "negs", player_negs, 0);
  n = append_matrix(buf, n, len, "points", player_scores, 0);
  if (n < len) snprintf(buf + n, len - n, "}");

  httpd_resp_set_type(req, "application/json");
  httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
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

static const httpd_uri_t score_uri = {
    .uri = "/score",
    .method = HTTP_POST,
    .handler = score_post_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t resetscores_uri = {
    .uri = "/resetscores",
    .method = HTTP_POST,
    .handler = resetscores_post_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t nav_uri = {
    .uri = "/nav",
    .method = HTTP_POST,
    .handler = nav_post_handler,
    .user_ctx = NULL,
};

static const httpd_uri_t stats_uri = {
    .uri = "/stats.json",
    .method = HTTP_GET,
    .handler = stats_json_handler,
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
  session_id = esp_random(); /* new id each boot; the page clears its packet when it changes */

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
    httpd_register_uri_handler(server, &score_uri);
    httpd_register_uri_handler(server, &resetscores_uri);
    httpd_register_uri_handler(server, &nav_uri);
    httpd_register_uri_handler(server, &stats_uri);
    httpd_register_uri_handler(server, &catch_all_uri);
  }
}
