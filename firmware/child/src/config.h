/*
 * 子機設定 (NVS/Preferences 永続化)
 * スキーマは docs/protocol.md §4 の CONFIG と一致させること。
 */
#pragma once
#include <Arduino.h>

enum LinkMode : uint8_t {
    LINK_BLE  = 0,
    LINK_WIFI = 1,
};

struct ChildConfig {
    bool     provisioned;
    char     device_id[32];
    char     battery_id[32];
    LinkMode link_mode;
    uint32_t wake_interval_sec;
    uint8_t  i2c_addr;
    char     wifi_ssid[64];
    char     wifi_pass[64];
    char     fw_version[16];
};

extern ChildConfig cfg;

void configLoad();
void configSave();

// JSON文字列から設定を更新し保存する (BLE CONFIG書込 / WiFi sync応答 双方から使用)
// 戻り値: 実際に値が変化したか
bool configApplyJson(const String& json);

// 現在の設定をJSON文字列にシリアライズする (BLE CONFIG読出 / provisioning画面表示用)
String configToJson();

const char* linkModeToStr(LinkMode m);
LinkMode    linkModeFromStr(const char* s);
