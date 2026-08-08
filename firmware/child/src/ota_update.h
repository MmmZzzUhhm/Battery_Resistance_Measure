/*
 * Update.h (ESP32 OTAパーティション書込) の薄いラッパ。
 * BLE(OTA_CTRL/OTA_DATA)・WiFi(firmware GET)どちらの経路からも同じ関数で書き込む。
 */
#pragma once
#include <Arduino.h>

bool otaBegin(size_t sizeBytes, const char* md5Hex /* nullable */);
bool otaWrite(const uint8_t* data, size_t len);
// 成功したら true を返し、呼び出し側は速やかに ESP.restart() すること。
bool otaEnd();
void otaAbort();
