/*
 * WiFi STA → 親機SoftAP への HTTP同期 (docs/protocol.md §3)
 */
#pragma once
#include <Arduino.h>

bool wifiSyncSession(uint32_t timeoutMs);
