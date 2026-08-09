/*
 * 薄型カウンタカメラ ファームウェア
 * XIAO ESP32S3 Sense(内蔵カメラ) + microSD(SD_MMC 1bit)
 *
 * 機能:
 *   - WiFi STAで上位ネットワークに接続 (未設定/未接続時はSoftAPで設定画面を提供)
 *     固定IP設定可 (cfg.use_static_ip)。DHCP環境に依存したくない場合に使用する
 *   - ONVIF(簡易実装)で上位システムからの撮影要求・設定を受け付ける (onvif_*.cpp参照)
 *     PTZ/動画配信/WS-Discovery/認証は実装しない (H/W非対応・要件外のため)
 *   - 照明制御は独自の簡易HTTPエンドポイント (/onvif/light/on|off, ONVIF標準外)
 *   - 撮影画像はSDカード(/DCIM/)に保存し、cfg.portal_base_url へ撮影の都度アップロード
 *   - ローカルWeb UI/REST APIで状態確認・設定変更
 *
 * NOTE: この基板にはRTCが実装されていない。撮影ファイル名の時刻は
 *       WiFi STA接続後のNTP同期に依存する (storage_sd.cpp参照)。
 *
 * NOTE: 照明制御用GPIO(PIN_LIGHT_CTRL)は未確定のため、platformio.iniで
 *       未定義(-1)としている。実機ピン確定後にbuild_flagsへ追加すること。
 */
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "camera.h"
#include "storage_sd.h"
#include "light_control.h"
#include "http_server.h"
#include "web_api.h"
#include "web_ui.h"
#include "onvif_routes.h"
#include "preview_stream.h"

namespace {
bool g_staEverConnected = false;
}

void setup() {
    Serial.begin(115200);
    delay(50);

    // configLoad()がMACアドレスからデフォルトdevice_idを生成するため、
    // WiFiドライバをここで先に立ち上げておく (これより前だとMACが0埋めで返る)。
    WiFi.mode(WIFI_AP_STA);

    configLoad();
    Serial.printf("[BOOT] device_id=%s\n", cfg.device_id);

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

    if (strlen(cfg.wifi_sta_ssid) > 0 && WiFi.status() == WL_CONNECTED && !g_staEverConnected) {
        g_staEverConnected = true;
        Serial.printf("[WiFi] STA connected: %s\n", WiFi.localIP().toString().c_str());
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }
}
