/*
 * 構成(役割)に依存しない共通検査項目
 */
#pragma once
#include <Arduino.h>

void testChipInfo();
void testI2CScan();
void testWifiApSoftAp(const char* ssid, const char* pass);
void testWifiStaScan();
void testBleAdvertise(const char* deviceName);
