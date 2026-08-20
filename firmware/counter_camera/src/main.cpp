/*
 * 薄型カウンタカメラ ファームウェア
 * XIAO ESP32S3 Sense(内蔵カメラ) + microSD(SD_MMC 1bit) + PCF8563T(RTC)
 *
 * 機能:
 *   - WiFi STAで上位ネットワークに接続 (未設定/未接続時はSoftAPで設定画面を提供)
 *     固定IP設定可 (cfg.use_static_ip)。DHCP環境に依存したくない場合に使用する
 *   - ONVIF(簡易実装)で上位システムからの撮影要求・設定を受け付ける (onvif_*.cpp参照)
 *     PTZ/動画配信/WS-Discoveryは実装しない (H/W非対応・要件外のため)。
 *     時刻管理(GetSystemDateAndTime/SetSystemDateAndTime/GetNTP/SetNTP)は実装済み。
 *   - 照明制御は独自の簡易HTTPエンドポイント (/onvif/light/on|off, ONVIF標準外)
 *   - 撮影画像はSDカード挿入時のみ/DCIM/へ保存 (未挿入なら保存をスキップ)。
 *     上位システム(ポータル)への引き渡しは、ポータル側からの撮影要求(ONVIF snapshot等)に
 *     対して撮影結果をその場でHTTPレスポンスとして返すpull型のため、本機からの能動的な
 *     アップロードは行わない。
 *   - ローカルWeb UI/REST APIで状態確認・設定変更
 *
 * NOTE: 時刻はPCF8563T RTC(バッテリーバックアップ)を主とし、
 *       WiFi STA接続後にNTP同期できた場合はRTCも補正する (rtc_clock.cpp, ntp_sync.cpp参照)。
 *       起動直後、NTP同期が完了する前でもRTCの時刻をシステムクロックに反映するため、
 *       ネットワーク未接続時でも撮影ファイル名の時刻はおおむね正しい。
 *
 * NOTE: 照明制御はADG728(I2Cアナログデータセレクタ)経由でLED1/LED2それぞれの
 *       電流制限抵抗(10/43/150/470Ω)を切り替える方式(light_control.cpp参照)。
 */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "camera.h"
#include "storage_sd.h"
#include "light_control.h"
#include "rtc_clock.h"
#include "ntp_sync.h"
#include "http_server.h"
#include "web_api.h"
#include "web_ui.h"
#include "onvif_routes.h"
#include "preview_stream.h"

#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 5
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 6
#endif

// ESP32はAP+STA同時動作時、APも必ずSTAと同じチャンネルで動作する制約があるため、
// 設定済みのSTA先(自宅ルーター等)が今いる場所で圏外だと、延々と接続/スキャンを
// 繰り返す間APが不安定になり見えなくなることがある。一定時間で接続を諦めてAP専用へ
// 切り替えることで、設定用SoftAPを確実に使えるようにするフェイルセーフ。
#define WIFI_STA_CONNECT_TIMEOUT_MS 20000UL

namespace {
bool g_staEverConnected = false;
bool g_staGaveUp = false;
unsigned long g_staConnectStartMs = 0;
}

void setup() {
    Serial.begin(115200);
    delay(50);

    // configLoad()がMACアドレスからデフォルトdevice_idを生成するため、
    // WiFiドライバをここで先に立ち上げておく (これより前だとMACが0埋めで返る)。
    WiFi.mode(WIFI_AP_STA);

    configLoad();
    Serial.printf("[BOOT] device_id=%s\n", cfg.device_id);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    if (rtcClock.begin()) {
        int64_t rtcEpoch = rtcClock.nowEpoch();
        if (rtcEpoch > 0) {
            struct timeval tv = { (time_t)rtcEpoch, 0 };
            settimeofday(&tv, nullptr);
            Serial.printf("[RTC] system clock seeded from RTC: epoch=%lld\n", (long long)rtcEpoch);
        }
    }

    if (!cameraBegin()) {
        Serial.println("[BOOT] WARNING: Camera init failed, /api/capture will fail");
    }
    if (!sdBegin()) {
        Serial.println("[BOOT] WARNING: SD card unavailable, capture will not be saved locally");
    }
    lightControlBegin();

    WiFi.softAP(cfg.ap_ssid, cfg.ap_pass);
    Serial.printf("[WiFi] AP started: SSID=%s IP=%s\n", cfg.ap_ssid, WiFi.softAPIP().toString().c_str());
    if (strlen(cfg.wifi_sta_ssid) > 0) {
        if (cfg.use_static_ip && strlen(cfg.static_ip) > 0) {
            IPAddress ip, gw, sn, dns;
            ip.fromString(cfg.static_ip);
            gw.fromString(cfg.static_gateway);
            sn.fromString(cfg.static_subnet);
            if (strlen(cfg.static_dns) > 0) dns.fromString(cfg.static_dns);
            if (!WiFi.config(ip, gw, sn, dns)) {
                Serial.println("[WiFi] WARNING: static IP config failed, falling back to DHCP");
            } else {
                Serial.printf("[WiFi] static IP configured: %s\n", cfg.static_ip);
            }
        }
        WiFi.begin(cfg.wifi_sta_ssid, cfg.wifi_sta_pass);
        g_staConnectStartMs = millis();
        Serial.printf("[WiFi] connecting STA to %s...\n", cfg.wifi_sta_ssid);
    }

    webApiRegisterRoutes();
    webUiRegisterRoutes();
    onvifRegisterRoutes();
    previewStreamRegisterRoute();
    httpServer.begin();

    Serial.println("[BOOT] ready");
}

void loop() {
    httpServer.handleClient();

    if (strlen(cfg.wifi_sta_ssid) > 0 && !g_staEverConnected && !g_staGaveUp) {
        if (WiFi.status() == WL_CONNECTED) {
            g_staEverConnected = true;
            Serial.printf("[WiFi] STA connected: %s\n", WiFi.localIP().toString().c_str());
            ntpSyncNow();
        } else if (millis() - g_staConnectStartMs > WIFI_STA_CONNECT_TIMEOUT_MS) {
            g_staGaveUp = true;
            WiFi.setAutoReconnect(false);
            WiFi.disconnect();
            WiFi.mode(WIFI_AP);
            Serial.println("[WiFi] STA connect timed out, giving up and staying AP-only so SoftAP stays visible");
        }
    }
}
