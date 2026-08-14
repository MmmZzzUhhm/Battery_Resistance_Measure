#include "light_control.h"
#include <Wire.h>

#ifndef ADG728_I2C_ADDR
#define ADG728_I2C_ADDR 0x4C
#endif

namespace {
uint8_t g_switchByte = 0x00;  // ADG728への書き込み値のキャッシュ (bit0=S1 ... bit7=S8)
uint8_t g_led1Level  = 0;
uint8_t g_led2Level  = 0;

bool writeSwitchByte(uint8_t value) {
    Wire.beginTransmission(ADG728_I2C_ADDR);
    Wire.write(value);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("[Light] ADG728 write failed: err=%u\n", err);
        return false;
    }
    g_switchByte = value;
    return true;
}
}  // namespace

void lightControlBegin() {
    if (writeSwitchByte(0x00)) {
        g_led1Level = 0;
        g_led2Level = 0;
    } else {
        Serial.println("[Light] WARNING: ADG728が見つかりません (addr 0x4C) - 照明制御は機能しません");
    }
}

bool lightSetLevel(LightId led, uint8_t level) {
    if (level > LIGHT_LEVEL_MAX) level = LIGHT_LEVEL_MAX;

    uint8_t mask = (led == LIGHT_LED1) ? 0x0F : 0xF0;
    uint8_t shift = (led == LIGHT_LED1) ? 0 : 4;
    uint8_t bits = (level == 0) ? 0 : (uint8_t)(1 << (shift + level - 1));
    uint8_t newValue = (uint8_t)((g_switchByte & ~mask) | bits);

    if (!writeSwitchByte(newValue)) return false;
    if (led == LIGHT_LED1) g_led1Level = level; else g_led2Level = level;
    return true;
}

uint8_t lightGetLevel(LightId led) {
    return (led == LIGHT_LED1) ? g_led1Level : g_led2Level;
}

void lightOn() {
    lightSetLevel(LIGHT_LED1, LIGHT_LEVEL_MIN);
    lightSetLevel(LIGHT_LED2, LIGHT_LEVEL_MIN);
}

void lightOff() {
    lightSetLevel(LIGHT_LED1, LIGHT_LEVEL_OFF);
    lightSetLevel(LIGHT_LED2, LIGHT_LEVEL_OFF);
}

bool lightIsOn() {
    return g_led1Level > 0 || g_led2Level > 0;
}
