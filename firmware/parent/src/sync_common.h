/*
 * BLE/WiFi両経路で共通の「子機から受け取った測定データの取り込み」処理。
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// arr の各要素は {seq, ts, r_mohm, v, valid} (docs/protocol.md §1)。
// SD履歴への追記とポータル向けキューへの投入を行い、受信できた最大seqを返す(0=データ無し)。
uint32_t processIncomingMeasurements(const String& childId, const String& batteryId, JsonArrayConst arr);
