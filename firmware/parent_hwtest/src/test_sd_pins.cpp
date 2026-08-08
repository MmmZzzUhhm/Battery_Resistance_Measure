#include "test_sd_pins.h"
#include "hwtest_common.h"
#include <Arduino.h>
#include "driver/gpio.h"

#ifndef PIN_SD_CLK
#define PIN_SD_CLK 7
#endif
#ifndef PIN_SD_CMD
#define PIN_SD_CMD 9
#endif
#ifndef PIN_SD_D0
#define PIN_SD_D0 8
#endif

namespace {

// 何も駆動せず読むだけ。フローティングピンは複数回読むと値が不安定になりやすい。
String sampleIdle(gpio_num_t pin, int samples) {
    pinMode((uint8_t)pin, INPUT);
    int highCount = 0;
    for (int i = 0; i < samples; i++) {
        if (digitalRead((uint8_t)pin)) highCount++;
        delayMicroseconds(200);
    }
    if (highCount == samples) return "常時HIGH";
    if (highCount == 0)       return "常時LOW";
    return String(highCount) + "/" + String(samples) + "回HIGH(不安定=フローティングの可能性)";
}

// ESP32側パッドをHIGH/LOWに駆動し、その場で読み返して一致するかを見る
// (GPIO_MODE_INPUT_OUTPUT で出力しながら同時に入力読みできるようにする)
void driveAndVerify(gpio_num_t pin, bool& highOk, bool& lowOk, int& mismatchCount, int cycles) {
    gpio_config_t cfg = {};
    cfg.pin_bit_mask = 1ULL << pin;
    cfg.mode = GPIO_MODE_INPUT_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&cfg);

    gpio_set_level(pin, 1);
    delayMicroseconds(50);
    highOk = gpio_get_level(pin) == 1;

    gpio_set_level(pin, 0);
    delayMicroseconds(50);
    lowOk = gpio_get_level(pin) == 0;

    mismatchCount = 0;
    for (int i = 0; i < cycles; i++) {
        int want = i % 2;
        gpio_set_level(pin, want);
        delayMicroseconds(50);
        if (gpio_get_level(pin) != want) mismatchCount++;
    }

    // SD_MMCライブラリが後で問題なくピンを制御できるよう、フローティング入力に戻す
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}

} // namespace

void testSdPinIntegrity() {
    Serial.println("[INFO] SD Pin診断: 断線検出はできません(MCU側パッドの短絡/故障のみ検出)");

    // name はプログラム全体の生存期間を持つ文字列リテラルにすること。
    // (report()はconst char*をポインタのまま保持するため、スタック上の一時バッファを
    //  渡すとサマリ表示時にダングリングポインタになる → GPIO番号はdetail側に含める)
    struct PinDef { const char* name; gpio_num_t pin; };
    PinDef pins[] = {
        { "SD Pin CLK", (gpio_num_t)PIN_SD_CLK },
        { "SD Pin CMD", (gpio_num_t)PIN_SD_CMD },
        { "SD Pin D0",  (gpio_num_t)PIN_SD_D0  },
    };

    for (auto& p : pins) {
        String idle = sampleIdle(p.pin, 20);

        bool highOk, lowOk;
        int mismatch;
        driveAndVerify(p.pin, highOk, lowOk, mismatch, 10);

        bool pass = highOk && lowOk && mismatch == 0;
        String detail = "GPIO" + String((int)p.pin) +
            " / idle=" + idle +
            " / HIGH駆動読返=" + (highOk ? "OK" : "NG(GND短絡疑い)") +
            " / LOW駆動読返="  + (lowOk  ? "OK" : "NG(3.3V短絡疑い)") +
            " / トグル不一致=" + String(mismatch) + "/10";
        report(p.name, pass, detail);
    }
}
