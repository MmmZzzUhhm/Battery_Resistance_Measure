#include "test_led.h"
#include "hwtest_common.h"
#include "led_control.h"

// LEDをまだ接続していない状態でも、コネクタのGND側ピンとの間の抵抗値をテスタ(Ωレンジ)で
// 実測できるよう、各レベルをこの時間だけ保持する。
#define LED_HOLD_MS 5000UL

namespace {

const char* expectedOhmText(uint8_t level) {
    switch (level) {
        case 1: return "約10Ω";
        case 2: return "約43Ω";
        case 3: return "約150Ω";
        case 4: return "約470Ω";
        default: return "オープン(無限大)";
    }
}

// led: 1または2。levelを設定し、期待抵抗値と測定箇所をシリアルへ表示してLED_HOLD_MSだけ保持する。
bool setAndHold(LightId led, uint8_t level) {
    bool ok = lightSetLevel(led, level);
    Serial.printf(
        "[LED%u] level=%u 設定%s / 期待抵抗 %s / J%u 2番ピン-GND間をテスタ(Ω)で測定してください "
        "(%lu秒間保持します)\n",
        (unsigned)led, level, ok ? "OK" : "失敗(I2C ACKなし)", expectedOhmText(level),
        led == LIGHT_LED1 ? 8 : 10, LED_HOLD_MS / 1000UL
    );
    delay(LED_HOLD_MS);
    return ok;
}

}  // namespace

// LED1(J8, S1-S4/bit0-3)とLED2(J10, S5-S8/bit4-7)を順にレベル1→4→消灯へ切り替え、都度
// 5秒間保持する。その間にテスタ(Ωレンジ)でコネクタの2番ピン-GND間を実測し、期待抵抗値
// (10/43/150/470Ω、消灯時オープン)と一致するか確認すること。ADG728への書込ACKも併せて検査する。
void testLedAdg728() {
    bool allOk = true;
    String detail;

    Serial.println("\n[LED ADG728] LED1(J8)を順に切り替えます...");
    for (uint8_t level = 1; level <= 4; level++) {
        if (!setAndHold(LIGHT_LED1, level)) {
            allOk = false;
            detail += "LED1 level" + String(level) + "書込失敗 ";
        }
    }
    setAndHold(LIGHT_LED1, 0);  // 消灯

    Serial.println("\n[LED ADG728] LED2(J10)を順に切り替えます...");
    for (uint8_t level = 1; level <= 4; level++) {
        if (!setAndHold(LIGHT_LED2, level)) {
            allOk = false;
            detail += "LED2 level" + String(level) + "書込失敗 ";
        }
    }
    setAndHold(LIGHT_LED2, 0);  // 消灯

    report(
        "LED ADG728 (0x4C)", allOk,
        allOk ? "LED1/LED2ともレベル1-4の書込ACK OK (シリアルログの期待抵抗値とテスタ実測値が一致するか確認)"
              : detail
    );
}
