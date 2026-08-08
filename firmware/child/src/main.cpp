/*
 * 鉛蓄電池 内部抵抗測定 子機ファームウェア
 * XIAO ESP32C6 + IWS7817(I2C) + PCF8563T(RTC)
 *
 * 通常はDeep Sleepし、設定間隔でWakeして
 *   1. IWS7817測定 → 未送信キューへ追加
 *   2. link_mode設定に従いBLE または WiFiで親機と同期(データ送信/設定・OTA受信)
 *   3. 再びDeep Sleep
 * を繰り返す。device_id未設定の初回起動時のみプロビジョニングAP+Webへ入る。
 *
 * 詳細仕様: docs/protocol.md
 */
#include <Arduino.h>
#include <Wire.h>
#include <esp_sleep.h>
#include "protocol.h"
#include "config.h"
#include "rtc_clock.h"
#include "iws7817.h"
#include "pending_queue.h"
#include "link_ble.h"
#include "link_wifi.h"
#include "provisioning.h"

#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 22
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 23
#endif

static bool g_provisioning = false;

static void goToSleep(uint32_t seconds) {
    Serial.printf("[SLEEP] deep sleep for %u s\n", (unsigned)seconds);
    Serial.flush();
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
    esp_deep_sleep_start();
    // ここには戻らない
}

void setup() {
    delay(10000);
    Serial.begin(115200);
    Serial.println("HELLO");

    configLoad();

    if (!cfg.provisioned || strlen(cfg.device_id) == 0) {
        Serial.println("[BOOT] not provisioned, entering setup mode");
        provisioningBegin();
        g_provisioning = true;
        return;
    }

    Serial.printf("[BOOT] device_id=%s link_mode=%s wake=%us\n",
        cfg.device_id, linkModeToStr(cfg.link_mode), (unsigned)cfg.wake_interval_sec);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(10000);
    rtcClock.begin();

    IwsMeasurement m = readIWS7817(cfg.i2c_addr);
    int64_t ts = rtcClock.nowEpoch();
    if (m.valid) {
        Serial.printf("[MEAS] r=%.3f mOhm v=%.4f V\n", m.r_mohm, m.v);
    } else {
        Serial.println("[MEAS] invalid reading");
    }

    pendingQueue.load();
    pendingQueue.push(m, ts);
    pendingQueue.save();

    bool synced;
    if (cfg.link_mode == LINK_BLE) {
        synced = bleSyncSession(BLE_SYNC_TIMEOUT_MS);
    } else {
        synced = wifiSyncSession(CHILD_WIFI_SYNC_TIMEOUT_MS);
    }
    Serial.printf("[SYNC] result=%d pending=%u\n", synced, (unsigned)pendingQueue.size());

    goToSleep(cfg.wake_interval_sec);
}

void loop() {
    if (g_provisioning) {
        provisioningLoop();
    }
    // 通常運用時はsetup()内でDeep Sleepに入るため、ここには到達しない。
}
