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
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <esp_sleep.h>
#include <RTClib.h>
#include <Preferences.h>

#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 22
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 23
#endif
#ifndef IWS7817_I2C_ADDR
#define IWS7817_I2C_ADDR 0x03
#endif
#ifndef PIN_IWS7817_PWR
#define PIN_IWS7817_PWR 21  // XIAO ESP32C6 D3 = I2C_Power_CTRL (TPS22917 ON制御)
#endif
#define IWS7817_POWER_ON_DELAY_MS 2000  // TPS22917起動+IWS7817内部(絶縁DCDC/マイコン)起動待ち。
                                          // 起動が不十分な状態で通信を試みるとその電源サイクル中
                                          // 応答不能になり続ける可能性があるため十分長く取る。

#define IWS7817_BYTES 10
#define IWS7817_HDR0  0x49
#define IWS7817_HDR1  0x57
#define DEEP_SLEEP_TEST_SEC 5

#define POWER_CHECK_AP_SSID "HWTEST-CHILD-PWR"
#define POWER_CHECK_AP_PASS "hwtest1234"
#define POWER_CHECK_TIMEOUT_MS (5UL * 60UL * 1000UL)  // 無操作でも5分でタイムアウトし自動検査へ進む
#define WIFI_STA_CONNECT_TIMEOUT_MS 15000UL

// 本番ファームウェア(firmware/child, config.cpp)と同じNVS領域を使う。
// 一度ブラウザから自宅/現場WiFiへ設定すれば、hwtestと本番ソフトを行き来しても
// 毎回WiFi設定をやり直さずに済む (どちらのファームでも同じ childcfg/wifi_ssid 等を読む)。
#define CHILD_NVS_NAMESPACE "childcfg"

// ── IWS7817専用 ソフトウェア(bit-bang) I2C読み取り ────────────────────────
// IWS7817はWriteに応答せず「START+addr+R+data+STOP」の読み取り単体プロトコルにのみ応答する。
// ESP-IDFの新I2Cドライバ(esp_driver_i2c, IDF>=5.4でesp32-hal-i2c-ng.cが有効)はこの
// Write無し読み取り単体をESP_ERR_INVALID_STATEで拒否するため、Wireのハードウェアペリフェラルを
// 使わず直接GPIOを叩くソフトウェアI2Cで代替する。RTC(0x51)等の通常のI2C機器はWireのままでよいため、
// 読み取り中だけWire.end()で一時解放し、完了後にWire.begin()し直して復元する。
#define IWS7817_BB_HALF_PERIOD_US 100  // 約5kHz相当。過去にM5AtomS3+IWS7817で実際に通信できた
                                        // 実績のあるクロック(Wire.begin(sda,scl,5000UL))に合わせる。

static inline void iwsSclRelease() { pinMode(PIN_I2C_SCL, INPUT_PULLUP); }
static inline void iwsSclLow()     { pinMode(PIN_I2C_SCL, OUTPUT); digitalWrite(PIN_I2C_SCL, LOW); }
static inline void iwsSdaRelease() { pinMode(PIN_I2C_SDA, INPUT_PULLUP); }
static inline void iwsSdaLow()     { pinMode(PIN_I2C_SDA, OUTPUT); digitalWrite(PIN_I2C_SDA, LOW); }
static inline bool iwsSdaRead()    { pinMode(PIN_I2C_SDA, INPUT_PULLUP); return digitalRead(PIN_I2C_SDA); }

static void iwsClockHigh() {
    iwsSclRelease();
    uint32_t waitStart = micros();  // クロックストレッチング対応
    while (digitalRead(PIN_I2C_SCL) == LOW && (micros() - waitStart) < 10000UL) {}
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
}
static void iwsClockLow() {
    iwsSclLow();
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
}
static void iwsBbStart() {
    iwsSdaRelease(); iwsSclRelease(); delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
    iwsSdaLow();  delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
    iwsSclLow();  delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
}
static void iwsBbStop() {
    iwsSdaLow();     delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
    iwsClockHigh();
    iwsSdaRelease(); delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
}
static void iwsBbWriteBit(bool bit) {
    if (bit) iwsSdaRelease(); else iwsSdaLow();
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);  // SDA遷移が確実に安定するまで待つ (旧: HALF/2)
    iwsClockHigh();
    iwsClockLow();
}
// SDA解放後、十分待ってから2回サンプリングし、両方一致した場合のみその値を採用する。
// 一致しない場合は「不安定=Highに遷移中」とみなしHigh(NACK側)を返す。ゴーストACK対策。
static bool iwsBbReadBit() {
    iwsSdaRelease();
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);  // 旧: HALF/2 → 安定待ちを倍に
    iwsClockHigh();
    bool bit1 = iwsSdaRead();
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US / 4);
    bool bit2 = iwsSdaRead();
    iwsClockLow();
    return bit1 || bit2;
}
#define IWS7817_BB_DEBUG 1  // 診断用: bit-bang I2Cの詳細ログをSerialへ出力 (解決したら0にしてよい)

// 7bitアドレス+Rビットを送信し、ACKが返れば true
static bool iwsBbSendAddrRead(uint8_t addr7) {
    uint8_t b = (uint8_t)((addr7 << 1) | 0x01);
#if IWS7817_BB_DEBUG
    Serial.printf("[IWS7817 bb] addr+R byte = 0x%02X\n", b);
#endif
    for (int i = 7; i >= 0; i--) iwsBbWriteBit((b >> i) & 0x01);
    bool nack = iwsBbReadBit();
#if IWS7817_BB_DEBUG
    Serial.printf("[IWS7817 bb] ACK bit: SDA=%d -> %s\n", nack, nack ? "NACK" : "ACK");
#endif
    return !nack;
}
// 1バイト受信し、最終バイトでなければACK(SDA Low)、最終バイトならNACK(SDA High)を返す
static uint8_t iwsBbReadByte(bool ack) {
    uint8_t v = 0;
    for (int i = 0; i < 8; i++) v = (uint8_t)((v << 1) | (iwsBbReadBit() ? 1 : 0));
    iwsBbWriteBit(!ack);
    return v;
}
// データシート(IW7817-IS/CS Manual 2.3.1)より: 「読出しリクエストを実行してから次の読出しリクエストを
// 発行するまで1秒以上の時間間隔が必要です。1秒以下で取得すると受信データは不定です」。
// この間隔を守らないと応答自体が不安定(ACKの有無すら不定)になり得るため、読み取り関数の入口で
// 必ず前回実行から一定時間空けるようにする。
#define IWS7817_MIN_READ_INTERVAL_MS 1200UL
static uint32_t g_lastIwsReadMs = 0;
static void iwsWaitReadInterval() {
    if (g_lastIwsReadMs == 0) {
        return;
    }
    uint32_t elapsed = millis() - g_lastIwsReadMs;
    if (elapsed < IWS7817_MIN_READ_INTERVAL_MS) {
        delay(IWS7817_MIN_READ_INTERVAL_MS - elapsed);
    }
}

// IWS7817をbit-bangで読み取る。addr7はアドレス、bufにn バイト格納。戻り値はACK有無。
static bool iws7817BbRead(uint8_t addr7, uint8_t* buf, uint8_t n) {
    iwsWaitReadInterval();
    Wire.end();
#if IWS7817_BB_DEBUG
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    pinMode(PIN_I2C_SCL, INPUT_PULLUP);
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
    Serial.printf("[IWS7817 bb] idle bus: SDA=%d SCL=%d (両方1が正常。0があればプルアップ不良/バス固着の疑い)\n",
        digitalRead(PIN_I2C_SDA), digitalRead(PIN_I2C_SCL));
#endif
    iwsBbStart();
    bool ok = iwsBbSendAddrRead(addr7);
    if (ok) {
        for (uint8_t i = 0; i < n; i++) {
            buf[i] = iwsBbReadByte(i < (uint8_t)(n - 1));
#if IWS7817_BB_DEBUG
            Serial.printf("[IWS7817 bb] byte[%u] = 0x%02X\n", (unsigned)i, buf[i]);
#endif
        }
    }
    iwsBbStop();
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(10000);
    g_lastIwsReadMs = millis();
    return ok;
}

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

// I2Cバスリカバリ: SDAが過去の(中断された)通信でLowに詰まったまま止まっている場合に解除する。
// SCLを9回手動クロックしてスタック中のスレーブに残りのビットを吐き出させ、その後手動でSTOP
// コンディションを送って確実に閉じる。M5AtomS3 + IWS7817の過去の開発資産(iws7817_i2CScan)で
// 実際に通信確立に必要だった手順そのもの。Wire.begin()より前、電源投入後に毎回実行する。
static void iws7817BusRecover() {
    Wire.end();
    pinMode(PIN_I2C_SCL, OUTPUT);
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_I2C_SCL, HIGH); delayMicroseconds(50);
        digitalWrite(PIN_I2C_SCL, LOW);  delayMicroseconds(50);
    }
    // STOPコンディション
    pinMode(PIN_I2C_SDA, OUTPUT);
    digitalWrite(PIN_I2C_SDA, LOW);  delayMicroseconds(50);
    digitalWrite(PIN_I2C_SCL, HIGH); delayMicroseconds(50);
    digitalWrite(PIN_I2C_SDA, HIGH); delayMicroseconds(50);
    Serial.println("[IWS7817] I2Cバスリカバリ完了 (SCL9クロック+STOP)");
}

// I2C_Power_CTRL (D3) をONにしてIWS7817へ5Vを供給する (TPS22917負荷スイッチ経由)。
// 電源投入直後は「読出しリクエスト間隔1秒以上」ルールの基準時刻もリセットし、
// 起動直後の最初の読み取りにも十分な間隔が空くようにする。電源投入後にバスリカバリも実行する。
static void iws7817PowerOn() {
    pinMode(PIN_IWS7817_PWR, OUTPUT);
    digitalWrite(PIN_IWS7817_PWR, HIGH);
    g_lastIwsReadMs = millis();
    delay(IWS7817_POWER_ON_DELAY_MS);
    iws7817BusRecover();
}

// ── 電源確認パネル (ブラウザからIWS7817の電源ON/OFFと通電状態を確認する) ──────
static WebServer  g_webServer(80);
static bool       g_iwsPowerOn = false;
static bool       g_proceedRequested = false;
static bool       g_staConnected = false;

static void loadSavedWifi(String& ssid, String& pass) {
    Preferences prefs;
    prefs.begin(CHILD_NVS_NAMESPACE, true);
    ssid = prefs.getString("wifi_ssid", "");
    pass = prefs.getString("wifi_pass", "");
    prefs.end();
}

static void saveWifi(const String& ssid, const String& pass) {
    Preferences prefs;
    prefs.begin(CHILD_NVS_NAMESPACE, false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);
    prefs.end();
}

static String iwsStatusLine() {
    uint8_t buf[IWS7817_BYTES];
    if (!iws7817BbRead(IWS7817_I2C_ADDR, buf, IWS7817_BYTES)) {
        return "応答なし (ACKなし, addr 0x" + String(IWS7817_I2C_ADDR, HEX) + ")";
    }
    if (buf[0] != IWS7817_HDR0 || buf[1] != IWS7817_HDR1) {
        char d[64];
        snprintf(d, sizeof(d), "ヘッダ不正: %02X %02X (期待値 49 57)", buf[0], buf[1]);
        return String(d);
    }
    uint8_t rb[4] = {buf[5], buf[4], buf[3], buf[2]};
    uint8_t vb[4] = {buf[9], buf[8], buf[7], buf[6]};
    float r, v;
    memcpy(&r, rb, 4);
    memcpy(&v, vb, 4);
    char d[64];
    snprintf(d, sizeof(d), "正常応答: r=%.3f mOhm v=%.4f V", r, v);
    return String(d);
}

static void handlePowerCheckRoot() {
    String status = g_iwsPowerOn ? iwsStatusLine() : "(電源OFF中)";
    String netInfo = g_staConnected
        ? ("WiFi接続中: " + WiFi.SSID() + " (" + WiFi.localIP().toString() + ")")
        : ("APモード: " + String(POWER_CHECK_AP_SSID));
    String html =
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>IWS7817 電源確認</title>"
        "<style>body{font-family:sans-serif;margin:2em;} button{font-size:1.2em;padding:0.6em 1.2em;margin:0.3em;} "
        ".on{background:#2E7D32;color:#fff;} .off{background:#C62828;color:#fff;}</style></head><body>"
        "<h2>IWS7817 電源確認</h2>"
        "<p style='color:#555;font-size:.9em'>" + netInfo + " <a href='/wifi'>[WiFi設定]</a></p>"
        "<p>電源状態: <b>" + String(g_iwsPowerOn ? "ON" : "OFF") + "</b></p>"
        "<p>I2C応答: " + status + "</p>"
        "<p><a href='/on'><button class='on'>電源 ON</button></a> "
        "<a href='/off'><button class='off'>電源 OFF</button></a> "
        "<a href='/'><button>再読込</button></a></p>"
        "<hr><p><a href='/continue'><button>この状態で自動検査へ進む &raquo;</button></a></p>"
        "</body></html>";
    g_webServer.send(200, "text/html; charset=utf-8", html);
}

static void handleWifiSettingsGet() {
    String savedSsid, savedPass;
    loadSavedWifi(savedSsid, savedPass);
    String html =
        "<!DOCTYPE html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>WiFi設定</title>"
        "<style>body{font-family:sans-serif;margin:2em;max-width:420px} "
        "input{width:100%;box-sizing:border-box;padding:8px;margin:4px 0;border:1px solid #ccc;border-radius:4px} "
        "button{font-size:1.1em;padding:0.6em 1.2em;margin-top:12px;background:#1976D2;color:#fff;border:none;border-radius:4px}"
        "</style></head><body>"
        "<h2>WiFi設定</h2>"
        "<p style='color:#555;font-size:.9em'>本番ファームウェア(firmware/child)と同じ設定を共有します。"
        "保存すると再起動し、指定したWiFiへの接続を試みます(失敗時はAPモードへ自動的に戻ります)。</p>"
        "<form method='POST' action='/wifi'>"
        "<label>SSID</label><input name='ssid' value='" + savedSsid + "'>"
        "<label>パスワード</label><input name='pass' type='password' value='" + savedPass + "'>"
        "<button type='submit'>保存して再起動</button>"
        "</form>"
        "<p><a href='/'>&laquo; 戻る</a></p>"
        "</body></html>";
    g_webServer.send(200, "text/html; charset=utf-8", html);
}

static void handleWifiSettingsPost() {
    String ssid = g_webServer.arg("ssid");
    String pass = g_webServer.arg("pass");
    saveWifi(ssid, pass);
    g_webServer.send(200, "text/html; charset=utf-8",
        "<meta charset='utf-8'><p>保存しました。再起動します...</p>");
    delay(400);
    ESP.restart();
}

static void handlePowerOn() {
    digitalWrite(PIN_IWS7817_PWR, HIGH);
    g_lastIwsReadMs = millis();
    delay(IWS7817_POWER_ON_DELAY_MS);
    iws7817BusRecover();
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(10000);
    g_iwsPowerOn = true;
    g_webServer.sendHeader("Location", "/");
    g_webServer.send(303);
}

static void handlePowerOff() {
    digitalWrite(PIN_IWS7817_PWR, LOW);
    g_iwsPowerOn = false;
    g_webServer.sendHeader("Location", "/");
    g_webServer.send(303);
}

static void handleContinue() {
    g_proceedRequested = true;
    g_webServer.send(200, "text/html; charset=utf-8",
        "<meta charset='utf-8'><p>自動検査を開始します。このページは閉じてください。</p>");
}

// 保存済みのWiFi設定(本番ファームウェアと共有)があれば接続を試みる。成功すればtrueを返す。
static bool tryConnectSavedWifi() {
    String ssid, pass;
    loadSavedWifi(ssid, pass);
    if (ssid.length() == 0) {
        return false;
    }

    Serial.printf("[電源確認パネル] 保存済みWiFiへ接続試行: SSID=%s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_STA_CONNECT_TIMEOUT_MS) {
        delay(200);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[電源確認パネル] 接続タイムアウト。APモードにフォールバックします。");
        WiFi.disconnect(true);
        return false;
    }
    Serial.printf("[電源確認パネル] WiFi接続成功: IP=%s\n", WiFi.localIP().toString().c_str());
    return true;
}

// 自動検査群の前に、ブラウザからIWS7817の電源ON/OFFと通電状態(I2C応答)を手動確認できる
// Webパネルを起動する。「自動検査へ進む」を押すか、無操作タイムアウトで復帰する。
// 保存済みWiFi(本番ファームと共有するNVS)があれば先に接続を試み、毎回のAP切替を不要にする。
static void runPowerCheckPanel() {
    pinMode(PIN_IWS7817_PWR, OUTPUT);
    digitalWrite(PIN_IWS7817_PWR, LOW);
    g_iwsPowerOn = false;
    g_proceedRequested = false;

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(10000);

    g_staConnected = tryConnectSavedWifi();
    if (g_staConnected) {
        Serial.printf("[電源確認パネル] ブラウザで http://%s/ を開いてください (最大%lu分待機)\n",
            WiFi.localIP().toString().c_str(), POWER_CHECK_TIMEOUT_MS / 60000UL);
    } else {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(POWER_CHECK_AP_SSID, POWER_CHECK_AP_PASS);
        IPAddress ip = WiFi.softAPIP();
        Serial.printf("\n[電源確認パネル] WiFi AP起動: SSID=%s PASS=%s\n", POWER_CHECK_AP_SSID, POWER_CHECK_AP_PASS);
        Serial.printf("[電源確認パネル] ブラウザで http://%s/ を開いてください (最大%lu分待機)\n",
            ip.toString().c_str(), POWER_CHECK_TIMEOUT_MS / 60000UL);
    }

    g_webServer.on("/", handlePowerCheckRoot);
    g_webServer.on("/on", handlePowerOn);
    g_webServer.on("/off", handlePowerOff);
    g_webServer.on("/continue", handleContinue);
    g_webServer.on("/wifi", HTTP_GET, handleWifiSettingsGet);
    g_webServer.on("/wifi", HTTP_POST, handleWifiSettingsPost);
    g_webServer.begin();

    uint32_t start = millis();
    while (!g_proceedRequested && (millis() - start) < POWER_CHECK_TIMEOUT_MS) {
        g_webServer.handleClient();
        delay(2);
    }

    g_webServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println(g_proceedRequested
        ? "[電源確認パネル] 「自動検査へ進む」が押されました。続行します。"
        : "[電源確認パネル] タイムアウトしました。自動検査を続行します。");
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

// 指定アドレスにbit-bang読み取りを試し、ヘッダ・値まで解釈してreport()する共通処理
static void tryIwsAddrAndReport(const char* testName, uint8_t addr) {
    uint8_t buf[IWS7817_BYTES];
    if (!iws7817BbRead(addr, buf, IWS7817_BYTES)) {
        report(testName, false, "no ACK (addr 0x" + String(addr, HEX) + ")");
        return;
    }
    if (buf[0] != IWS7817_HDR0 || buf[1] != IWS7817_HDR1) {
        char d[80];
        snprintf(
            d, sizeof(d), "addr 0x%02X: ACKしたがヘッダ不正 %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X (期待値先頭 49 57)", addr,
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9]
        );
        report(testName, false, d);
        return;
    }
    uint8_t rb[4] = {buf[5], buf[4], buf[3], buf[2]};
    uint8_t vb[4] = {buf[9], buf[8], buf[7], buf[6]};
    float r, v;
    memcpy(&r, rb, 4);
    memcpy(&v, vb, 4);
    char d[64];
    snprintf(d, sizeof(d), "addr 0x%02X: r=%.3f mOhm v=%.4f V", addr, r, v);
    report(testName, true, d);
}

static void testIws7817() {
    tryIwsAddrAndReport("IWS7817 Read", IWS7817_I2C_ADDR);
}

// データシート(IW7817-IS/CS Manual 2.3.1)より、I2Cアドレス設定範囲は0-15固定
// (基板上のジャンパ/DIPスイッチ下位4bitのみで決まる)。0x52等は物理的にあり得ないため、
// これまでの全アドレス(1-126)スキャンで見えていた0x52-0x55等は真の応答ではなくノイズだったと判断。
// 0-15の範囲だけを対象に、データシート規定の「読出しリクエスト間隔1秒以上」を守ってスキャンする。
static void testIwsAddrScan0to15() {
    int found = -1;
    for (uint8_t a = 0; a <= 15; a++) {
        uint8_t buf[IWS7817_BYTES];
        if (iws7817BbRead(a, buf, IWS7817_BYTES) && buf[0] == IWS7817_HDR0 && buf[1] == IWS7817_HDR1) {
            found = a;
            break;
        }
    }
    if (found >= 0) {
        report("IWS7817 Addr Scan (0-15)", true, "addr 0x" + String(found, HEX) + " で正常応答を確認");
    } else {
        report("IWS7817 Addr Scan (0-15)", false, "0-15の全アドレスで応答なし/ヘッダ不一致 (1秒以上間隔を空けて検査済み)");
    }
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

    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
        runPowerCheckPanel(); // 手動電源確認パネル (Deep Sleep再検査時はスキップ)
    }

    iws7817PowerOn(); // IWS7817の電源をON (I2Cスキャン/IWS7817読取テストで検出できるようにする)

    testChipInfo();
    testI2CScan();
    testIws7817();
    testIwsAddrScan0to15();
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
