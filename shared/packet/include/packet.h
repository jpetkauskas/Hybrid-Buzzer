#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t transmitter_id;
    uint8_t player_id;
    uint8_t transmitter_mac[6];
    uint32_t timestamp_us;  /* µs since last sync pulse; 0xFFFFFFFF in sync packets */
    uint8_t epoch;          /* incremented each CLEAR; receiver discards mismatched epochs */
} packet;

/* Sentinel values that identify a sync packet (receiver → transmitters). */
#define PACKET_SYNC_ID 0xFF
