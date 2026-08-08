/*
 * 基板 動作確認チェックアプリ (本番ファームウェアとは完全に独立したプロジェクト)
 * 対象: XIAO ESP32S3 をベースに、部品実装違いで複数用途を兼ねる新規基板
 *   1: 親機                     (SD_MMC 1bit + SHT31 + PCF8563T)
 *   2: 超音波センサ              (SPH0641LU4H-1 PDMマイクのみ)
 *   3: マイクロ波ドップラーセンサカメラ (IMD-2000 + SD_MMC 1bit + PCF8563T)
 *
 * 起動時にシリアルモニタで構成番号(1-3)を入力すると、その構成に対応する
 * 検査のみを実行し、シリアルログにPASS/FAILの一覧を出力する。
 * 検査後は自身のAPを起動し、http://192.168.4.1/ に結果ページを表示し続ける
 * (リロードで再検査、再検査時も構成の再入力が必要)。
 *
 * 使い方: pio run -t upload && pio device monitor (115200bps)
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "hwtest_common.h"
#include "role_select.h"
#include "test_common.h"
#include "test_rtc_datetime.h"
#include "test_rtc_pcf8563.h"
#include "test_sht31.h"
#include "test_sd_pins.h"
#include "test_sdmmc.h"
#include "test_sd_dir.h"
#include "test_pdm_mic.h"
#include "test_imd2000.h"

#define AP_PASS "hwtest1234"

static WebServer g_server(80);
static BoardRole g_role;

static void runTestsForRole(BoardRole role) {
    testResultsReset();
    testChipInfo();

    switch (role) {
        case ROLE_PARENT:
            testI2CScan();
            testRtcReadTime();
            testRtcPcf8563();
            testSht31();
            testSdPinIntegrity();
            testSdMmc();
            testSdDirList();
            break;
        case ROLE_ULTRASONIC:
            testPdmMic();
            break;
        case ROLE_DOPPLER:
            testI2CScan();
            testRtcReadTime();
            testRtcPcf8563();
            testSdPinIntegrity();
            testSdMmc();
            testSdDirList();
            testImd2000();
            break;
    }

    testWifiStaScan();
    testWifiApSoftAp(roleApSsid(role), AP_PASS);
    testBleAdvertise(roleApSsid(role));

    printSummary();
}

// ── Web結果ページ ────────────────────────────────────────────
static void handleRoot() {
    String html = "<!DOCTYPE html><html lang=\"ja\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>基板 動作確認</title><style>"
        "body{font-family:sans-serif;max-width:520px;margin:24px auto;padding:0 12px;background:#fafafa}"
        "h2{color:#1976D2}p.role{color:#555}table{width:100%;border-collapse:collapse}"
        "td,th{padding:8px;border-bottom:1px solid #ddd;text-align:left;font-size:.9em}"
        ".ok{color:#2E7D32;font-weight:bold}.fail{color:#C62828;font-weight:bold}"
        "a.btn{display:inline-block;margin-top:16px;padding:10px 20px;background:#1976D2;color:#fff;"
        "text-decoration:none;border-radius:4px}</style></head><body>"
        "<h2>基板 動作確認結果</h2><p class=\"role\">構成: " + String(roleName(g_role)) + "</p>"
        "<table><thead><tr><th>項目</th><th>結果</th><th>詳細</th></tr></thead><tbody>";

    for (int i = 0; i < testResultCount(); i++) {
        const TestResult& r = testResultAt(i);
        html += "<tr><td>" + String(r.name) + "</td><td class=\"" +
                (r.pass ? "ok\">OK" : "fail\">FAIL") + "</td><td>" +
                r.detail + "</td></tr>";
    }
    html += "</tbody></table><a class=\"btn\" href=\"/rerun\">再検査(リブート・構成再選択)</a></body></html>";
    g_server.send(200, "text/html; charset=utf-8", html);
}

static void handleRerun() {
    g_server.send(200, "text/html; charset=utf-8", "再起動します...");
    delay(300);
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n### 基板 動作確認チェックアプリ ###\n");

    g_role = selectRoleFromSerial();
    Serial.printf("\n選択された構成: %s\n\n", roleName(g_role));

    runTestsForRole(g_role);

    g_server.on("/", HTTP_GET, handleRoot);
    g_server.on("/rerun", HTTP_GET, handleRerun);
    g_server.begin();
    Serial.printf("結果ページ: http://%s/ (AP: %s / %s)\n",
        WiFi.softAPIP().toString().c_str(), roleApSsid(g_role), AP_PASS);
}

void loop() {
    g_server.handleClient();
}
