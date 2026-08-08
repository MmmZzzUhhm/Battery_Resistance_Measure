/*
 * 親機設定 (NVS/Preferences 永続化)
 */
#pragma once
#include <Arduino.h>

struct ParentConfig {
    char     gateway_id[32];
    char     wifi_sta_ssid[64];   // クラウド(ポータル)/社内網への接続先
    char     wifi_sta_pass[64];
    char     ap_ssid[32];         // 子機(WiFiモード)向けSoftAP
    char     ap_pass[64];
    char     portal_base_url[96]; // 例: http://raspberrypi.local:8080
    char     portal_api_key[64];
    uint32_t cloud_sync_interval_sec;
    uint32_t ble_scan_interval_sec;
};

extern ParentConfig cfg;

void configLoad();
void configSave();
String configToJson();          // Web UI表示/APIレスポンス用 (機密情報はマスク)
bool  configApplyJson(const String& json);
