#include "iws7817.h"
#include <Wire.h>

#ifndef PIN_IWS7817_PWR
#define PIN_IWS7817_PWR 21  // XIAO ESP32C6 D3 = I2C_Power_CTRL (TPS22917 ON制御)
#endif
#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 22
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 23
#endif
#define IWS7817_POWER_ON_DELAY_MS 2000  // TPS22917起動+IWS7817内部(絶縁DCDC/マイコン)起動待ち

#define IWS7817_BYTES 10
#define IWS7817_HDR0  0x49
#define IWS7817_HDR1  0x57

// ── ソフトウェア(bit-bang) I2C読み取り ──────────────────────────────────
// IWS7817はWriteに応答せず「START+addr+R+data+STOP」の読み取り単体プロトコルにのみ応答する。
// ESP-IDFの新I2Cドライバ(esp_driver_i2c, IDF>=5.4でesp32-hal-i2c-ng.cが有効)はこの
// Write無し読み取り単体をESP_ERR_INVALID_STATEで拒否するため、Wireのハードウェアペリフェラルを
// 使わず直接GPIOを叩くソフトウェアI2Cで代替する。RTC等の通常のI2C機器はWireのままでよいため、
// 読み取り中だけWire.end()で一時解放し、完了後にWire.begin()し直して復元する。
// クロックは実機で通信確立に成功した5kHz相当 (データシート上限12kHz)。
#define IWS7817_BB_HALF_PERIOD_US 100

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
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
    iwsClockHigh();
    iwsClockLow();
}
// SDA解放後、十分待ってから2回サンプリングし、両方一致した場合のみその値を採用する。
// 一致しない場合は「不安定=Highに遷移中」とみなしHigh(NACK側)を返す。ゴーストACK対策。
static bool iwsBbReadBit() {
    iwsSdaRelease();
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US);
    iwsClockHigh();
    bool bit1 = iwsSdaRead();
    delayMicroseconds(IWS7817_BB_HALF_PERIOD_US / 4);
    bool bit2 = iwsSdaRead();
    iwsClockLow();
    return bit1 || bit2;
}
// 7bitアドレス+Rビットを送信し、ACKが返れば true
static bool iwsBbSendAddrRead(uint8_t addr7) {
    uint8_t b = (uint8_t)((addr7 << 1) | 0x01);
    for (int i = 7; i >= 0; i--) iwsBbWriteBit((b >> i) & 0x01);
    bool nack = iwsBbReadBit();
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

// I2Cバスリカバリ: SDAが過去の(中断された)通信でLowに詰まったまま止まっている場合に解除する。
// SCLを9回手動クロックしてスタック中のスレーブに残りのビットを吐き出させ、その後手動でSTOP
// コンディションを送って確実に閉じる。M5AtomS3 + IWS7817の過去の開発資産で実際に
// 通信確立に必要だった手順そのもの。電源投入直後、Wire再開前に実行する。
static void iwsBusRecover() {
    Wire.end();
    pinMode(PIN_I2C_SCL, OUTPUT);
    pinMode(PIN_I2C_SDA, INPUT_PULLUP);
    for (int i = 0; i < 9; i++) {
        digitalWrite(PIN_I2C_SCL, HIGH); delayMicroseconds(50);
        digitalWrite(PIN_I2C_SCL, LOW);  delayMicroseconds(50);
    }
    pinMode(PIN_I2C_SDA, OUTPUT);
    digitalWrite(PIN_I2C_SDA, LOW);  delayMicroseconds(50);
    digitalWrite(PIN_I2C_SCL, HIGH); delayMicroseconds(50);
    digitalWrite(PIN_I2C_SDA, HIGH); delayMicroseconds(50);
}

void iws7817PowerBegin() {
    pinMode(PIN_IWS7817_PWR, OUTPUT);
    digitalWrite(PIN_IWS7817_PWR, LOW);
}

void iws7817PowerOn() {
    digitalWrite(PIN_IWS7817_PWR, HIGH);
    g_lastIwsReadMs = millis();  // 起動直後の最初の読み取りにも間隔ルールを適用する
    delay(IWS7817_POWER_ON_DELAY_MS);
    iwsBusRecover();
}

void iws7817PowerOff() {
    digitalWrite(PIN_IWS7817_PWR, LOW);
}

IwsMeasurement readIWS7817(uint8_t i2cAddr) {
    IwsMeasurement d = {};
    iwsWaitReadInterval();

    Wire.end();
    uint8_t buf[IWS7817_BYTES];
    iwsBbStart();
    bool ok = iwsBbSendAddrRead(i2cAddr);
    if (ok) {
        for (uint8_t i = 0; i < IWS7817_BYTES; i++) {
            buf[i] = iwsBbReadByte(i < (uint8_t)(IWS7817_BYTES - 1));
        }
    }
    iwsBbStop();
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(10000);
    g_lastIwsReadMs = millis();

    if (!ok) {
        Serial.printf("[IWS7817] Read failed: no ACK (addr 0x%02X)\n", i2cAddr);
        return d;
    }
    if (buf[0] != IWS7817_HDR0 || buf[1] != IWS7817_HDR1) {
        Serial.printf("[IWS7817] Bad header: %02X %02X\n", buf[0], buf[1]);
        return d;
    }
    // ビッグエンディアン float → リトルエンディアン
    uint8_t rb[4] = {buf[5], buf[4], buf[3], buf[2]};
    uint8_t vb[4] = {buf[9], buf[8], buf[7], buf[6]};
    memcpy(&d.r_mohm, rb, 4);
    memcpy(&d.v,      vb, 4);
    d.valid = true;
    return d;
}
