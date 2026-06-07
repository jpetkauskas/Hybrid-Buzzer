#pragma once

#include "esp_netif_ip_addr.h"

/*
 * Minimal captive-portal DNS server.
 *
 * Spawns a task that answers every A query with `resolve_to` (the SoftAP's
 * own IP). Combined with the AP's DHCP handing out this device as the DNS
 * server, any hostname a connected client looks up resolves to the device,
 * so the buzzer page is reachable by any URL with no per-client setup.
 */
void start_captive_dns(esp_ip4_addr_t resolve_to);
