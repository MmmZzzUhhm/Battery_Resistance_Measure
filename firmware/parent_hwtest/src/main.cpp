/*
 * 基板 動作確認チェックアプリ (本番ファームウェアとは完全に独立したプロジェクト)
 * 対象: XIAO ESP32S3 をベースに、部品実装違いで複数用途を兼ねる新規基板
 *   1: 親機                     (SD_MMC 1bit + SHT31 + PCF8563T)
 *   2: 超音波センサ              (SPH0641LU4H-1 PDMマイクのみ)
 *   3: マイクロ波ドップラーセンサカメラ (IMD-2000 + SD_MMC 1bit + PCF8563T)
 *   4: 薄型カウンタカメラ        (XIAO ESP32S3 Sense内蔵カメラ + SD_MMC 1bit)
 *
 * 製作した基板の動作チェックをするための常駐Web UIアプリ。
 * WiFi未設定ならAP設定モード(固定IPも設定可)、設定済みならSTA接続してテストモードへ入る。
 * テストモードでは構成別の一括テストに加え、RTC読取・カメラ撮影/SD保存/表示・
 * LED明るさ制御・BLE子機疎通確認・APモードへの再移行をブラウザから行える。
 *
 * 使い方: pio run -t upload && pio device monitor でシリアルログを確認。
 */
#include <Arduino.h>
#include <Wire.h>
#include "hwtest_config.h"
#include "hwtest_webserver.h"

#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 5
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 6
#endif

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("\n### 基板 動作確認チェックアプリ ###\n");

    // I2C(Wire)は一括テスト(testI2CScan())内でのみ初期化されており、それを実行しないまま
    // LED明るさ制御やRTC読取のWeb APIを呼ぶとWireが未初期化のため失敗する。ここで起動時に
    // 一度だけ初期化しておく(counter_cameraのmain.cppと同じ方式)。
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    hwtestConfigLoad();
    hwtestWebServerBegin();
}

void loop() {
    hwtestWebServerLoop();
}
