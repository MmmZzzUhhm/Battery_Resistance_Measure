/*
 * 鉛蓄電池 内部抵抗測定 親機ファームウェア
 * XIAO ESP32S3 + PCF8563T(RTC) + microSD(SD_MMC 1bit)
 *
 * 常時稼働し、以下を並行して行う:
 *   - BLE Central: 起きた子機(BLE設定)をスキャン・接続して同期
 *   - WiFi AP: WiFi設定の子機がHTTPで同期しに来るのを受け付け
 *   - WiFi STA: ポータル(クラウド相当)への定期アップロード/設定取得/heartbeat
 *   - ローカルWeb UI/REST API: 単独運用時の閲覧・設定変更
 *   - SDカード: 全データの恒久保存 + クラウド未送信キュー + OTAファーム保管
 *
 * 詳細仕様: docs/protocol.md
 */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include "protocol.h"
#include "config.h"
#include "rtc_clock.h"
#include "ntp_sync.h"
#include "storage_sd.h"
#include "child_ble.h"
#include "child_wifi.h"
#include "cloud_client.h"
#include "http_server.h"
#include "web_api.h"
#include "web_ui.h"

#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 5
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 6
#endif

namespace {
uint32_t g_lastBleScanMs   = 0;
uint32_t g_lastCloudSyncMs = 0;
bool     g_staEverConnected = false;
} // namespace

void setup() {
    Serial.begin(115200);
    delay(50);

    configLoad();
    Serial.printf("[BOOT] gateway_id=%s\n", cfg.gateway_id);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    rtcClock.begin();

    if (!sdBegin()) {
        Serial.println("[BOOT] WARNING: SD card unavailable, history/queue/OTA disabled");
    }

    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(cfg.ap_ssid, cfg.ap_pass);
    Serial.printf("[WiFi] AP started: SSID=%s IP=%s\n", cfg.ap_ssid, WiFi.softAPIP().toString().c_str());
    if (strlen(cfg.wifi_sta_ssid) > 0) {
        WiFi.begin(cfg.wifi_sta_ssid, cfg.wifi_sta_pass);
        Serial.printf("[WiFi] connecting STA to %s...\n", cfg.wifi_sta_ssid);
    }

    BLEDevice::init(cfg.gateway_id);

    childWifiRegisterRoutes();
    webApiRegisterRoutes();
    webUiRegisterRoutes();
    httpServer.begin();

    Serial.println("[BOOT] ready");
}

void loop() {
    httpServer.handleClient();

    if (strlen(cfg.wifi_sta_ssid) > 0 && WiFi.status() == WL_CONNECTED && !g_staEverConnected) {
        g_staEverConnected = true;
        Serial.printf("[WiFi] STA connected: %s\n", WiFi.localIP().toString().c_str());
        ntpSyncRtc();
    }

    uint32_t now = millis();

    if (now - g_lastBleScanMs >= cfg.ble_scan_interval_sec * 1000UL) {
        g_lastBleScanMs = now;
        bleScanAndSyncOnce(3);
    }

    if (now - g_lastCloudSyncMs >= cfg.cloud_sync_interval_sec * 1000UL) {
        g_lastCloudSyncMs = now;
        cloudSyncTick();
    }
}
