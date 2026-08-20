/*
 * 基板動作チェックアプリ用の永続設定 (NVS/Preferences)。
 * WiFi(STA)接続先と固定IP設定、直近選択した構成(役割)を保持する。
 * WiFi未設定(wifi_ssidが空)ならAP設定モードへ、設定済みならSTA接続を試みてテストモードへ入る。
 */
#pragma once
#include <Arduino.h>

struct HwtestConfig {
    char device_name[32];   // AP SSID表示名 / mDNS等に使う任意の識別名
    char wifi_ssid[64];
    char wifi_pass[64];
    bool use_static_ip;
    char static_ip[16];
    char static_gateway[16];
    char static_subnet[16];
    char static_dns[16];
    uint8_t last_role;      // 直近選択した構成番号 (1-4、0=未選択)
};

extern HwtestConfig hwCfg;

void hwtestConfigLoad();
void hwtestConfigSave();
