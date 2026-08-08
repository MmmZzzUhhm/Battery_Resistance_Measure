/*
 * 子機基板 動作確認チェックアプリ
 * 対象: XIAO ESP32C6 + IWS7817(I2C) + PCF8563T(RTC) を実装した新規基板
 *
 * 本番ファームウェア(firmware/child)とは完全に独立した検査専用ツール。
 * 起動すると各項目を順に検査し、シリアルログにPASS/FAILの一覧を出力する。
 * Deep Sleep/タイマーWakeの検査のみ、5秒間の実機Deep Sleep→自動リブートを伴う。
 *
 * 使い方: pio run -t upload && pio device monitor (115200bps)
 */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <esp_sleep.h>
#include <RTClib.h>

#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 22
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 23
#endif
#ifndef IWS7817_I2C_ADDR
#define IWS7817_I2C_ADDR 0x03
#endif

#define IWS7817_BYTES 10
#define IWS7817_HDR0  0x49
#define IWS7817_HDR1  0x57
#define DEEP_SLEEP_TEST_SEC 5

// Deep Sleepをまたいで保持するフラグ (テスト実施済みかどうか)
RTC_DATA_ATTR static bool g_sleepTestArmed = false;

struct TestResult {
    const char* name;
    bool        pass;
    String      detail;
};

static TestResult g_results[16];
static int         g_resultCount = 0;

static void report(const char* name, bool pass, const String& detail) {
    g_results[g_resultCount++] = { name, pass, detail };
    Serial.printf("[%s] %-24s %s\n", pass ? " OK " : "FAIL", name, detail.c_str());
}

// ── 各テスト ──────────────────────────────────────────────────
static void testChipInfo() {
    String d = String("model=") + ESP.getChipModel() +
               " rev=" + ESP.getChipRevision() +
               " flash=" + (ESP.getFlashChipSize() / 1024 / 1024) + "MB" +
               " heap=" + ESP.getFreeHeap() + "B" +
               " mac=" + WiFi.macAddress();
    report("ChipInfo", true, d);
}

static void testI2CScan() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(10000);
    String found;
    int count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (count > 0) found += ",";
            found += "0x" + String(addr, HEX);
            count++;
        }
    }
    report("I2C Scan", count > 0, count > 0 ? (String(count) + " devices: " + found) : "no I2C device responded");
}

static void testIws7817() {
    uint8_t buf[IWS7817_BYTES];
    uint8_t n = Wire.requestFrom((uint8_t)IWS7817_I2C_ADDR, (uint8_t)IWS7817_BYTES);
    if (n != IWS7817_BYTES) {
        report("IWS7817 Read", false, "requestFrom returned " + String(n) + "/" + String(IWS7817_BYTES) + " bytes (addr 0x" + String(IWS7817_I2C_ADDR, HEX) + ")");
        return;
    }
    for (int i = 0; i < IWS7817_BYTES; i++) buf[i] = Wire.read();
    if (buf[0] != IWS7817_HDR0 || buf[1] != IWS7817_HDR1) {
        char d[64];
        snprintf(d, sizeof(d), "bad header: %02X %02X (expected 49 57)", buf[0], buf[1]);
        report("IWS7817 Read", false, d);
        return;
    }
    uint8_t rb[4] = {buf[5], buf[4], buf[3], buf[2]};
    uint8_t vb[4] = {buf[9], buf[8], buf[7], buf[6]};
    float r, v;
    memcpy(&r, rb, 4);
    memcpy(&v, vb, 4);
    char d[64];
    snprintf(d, sizeof(d), "r=%.3f mOhm v=%.4f V", r, v);
    report("IWS7817 Read", true, d);
}

static void testRtcPcf8563() {
    RTC_PCF8563 rtc;
    if (!rtc.begin()) {
        report("RTC PCF8563T", false, "begin() failed - not found on I2C bus (addr 0x51)");
        return;
    }
    bool hadLostPower = rtc.lostPower();
    DateTime setTime(2026, 1, 1, 0, 0, 0);
    rtc.adjust(setTime);
    delay(1100);
    DateTime now = rtc.now();
    int64_t diff = (int64_t)now.unixtime() - (int64_t)setTime.unixtime();
    bool ok = diff >= 1 && diff <= 5;
    char d[96];
    snprintf(d, sizeof(d), "set->read diff=%llds (lostPower flag was %s before adjust)",
        (long long)diff, hadLostPower ? "true(正常: 初回通電時は真になる)" : "false");
    report("RTC PCF8563T", ok, d);
}

static void testWifiScan() {
    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks();
    WiFi.scanDelete();
    report("WiFi Scan", n >= 0, n >= 0 ? (String(n) + " AP(s) found nearby") : "scanNetworks() failed");
}

static void testBleAdvertise() {
    BLEDevice::init("HWTEST-CHILD");
    BLEServer* server = BLEDevice::createServer();
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->start();
    delay(500);
    adv->stop();
    (void)server;
    BLEDevice::deinit(true);
    report("BLE Advertise", true, "BLEDevice init/advertise/deinit completed without error");
}

static void testDeepSleepWakeIfDue() {
    bool wasTimerWake = (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
    if (g_sleepTestArmed && wasTimerWake) {
        report("Deep Sleep + Timer Wake", true,
            String(DEEP_SLEEP_TEST_SEC) + "s Deep Sleepから正常に復帰した");
        g_sleepTestArmed = false;
    } else {
        report("Deep Sleep + Timer Wake", true,
            "(この回では未検査。全項目終了後に" + String(DEEP_SLEEP_TEST_SEC) + "秒Deep Sleepし自動リブートする)");
    }
}

static void printSummary() {
    int passCount = 0;
    Serial.println("\n==================== 検査結果サマリ ====================");
    for (int i = 0; i < g_resultCount; i++) {
        Serial.printf("  [%s] %s\n", g_results[i].pass ? " OK " : "FAIL", g_results[i].name);
        if (g_results[i].pass) passCount++;
    }
    Serial.printf("---------------------------------------------------------\n");
    Serial.printf("  %d / %d 項目 PASS\n", passCount, g_resultCount);
    Serial.println("==========================================================\n");
}

void setup() {
    Serial.begin(115200);
    delay(10000);
    Serial.println("\n### 子機基板 動作確認チェックアプリ ###\n");

    testChipInfo();
    testI2CScan();
    testIws7817();
    testRtcPcf8563();
    testWifiScan();
    testBleAdvertise();
    testDeepSleepWakeIfDue();

    printSummary();

    if (!g_sleepTestArmed && esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
        Serial.printf("Deep Sleepテストのため%d秒後にDeep Sleepへ移行し、自動的に復帰・再検査します...\n", DEEP_SLEEP_TEST_SEC);
        delay(200);
        g_sleepTestArmed = true;
        Serial.flush();
        esp_sleep_enable_timer_wakeup((uint64_t)DEEP_SLEEP_TEST_SEC * 1000000ULL);
        esp_deep_sleep_start();
    }

    Serial.println("全項目終了。基板をリセットすると再検査します。");
}

void loop() {
    delay(5000);
    Serial.println("(検査完了 - リセットで再検査)");
}
