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
 * Update the buzz winner shown by the web page and push it to connected
 * clients. Call from task context (e.g. the main loop).
 */
void webserver_set_winner(int team, int player);

/*
 * Notify the web server that the buzz was cleared, so connected clients are
 * updated immediately. ISR-safe — intended to be called from the CLEAR button
 * interrupt handler.
 */
void webserver_notify_clear_from_isr(void);
