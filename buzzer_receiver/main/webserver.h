#pragma once

/*
 * Self-contained "Hello World" web server.
 *
 * Brings up a SoftAP (alongside the existing STA interface that carries
 * ESPNOW) and serves a single page. Call once after WiFi has been started
 * (esp_wifi_start) and ESPNOW initialized. Non-blocking: the HTTP server
 * runs in its own task.
 *
 * The SoftAP is forced onto the channel ESPNOW is already using, so adding
 * the web server never changes the radio channel and therefore does not
 * interfere with ESPNOW traffic.
 */
void start_webserver(void);

/*
 * Update the buzz winner shown by the web page. Safe to call from the main
 * loop; the served page reflects the most recent winner.
 */
void webserver_set_winner(int team, int player);
