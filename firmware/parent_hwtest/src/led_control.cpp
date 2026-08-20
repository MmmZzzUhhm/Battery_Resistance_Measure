#include "led_control.h"
#include <Wire.h>

#ifndef ADG728_I2C_ADDR
#define ADG728_I2C_ADDR 0x4C
#endif

namespace {
uint8_t g_switchByte = 0x00;
uint8_t g_led1Level  = 0;
uint8_t g_led2Level  = 0;
}  // namespace

bool lightSetLevel(LightId led, uint8_t level) {
    if (level > LIGHT_LEVEL_MAX) level = LIGHT_LEVEL_MAX;

    uint8_t mask  = (led == LIGHT_LED1) ? 0x0F : 0xF0;
    uint8_t shift = (led == LIGHT_LED1) ? 0 : 4;
    uint8_t bits  = (level == 0) ? 0 : (uint8_t)(1 << (shift + level - 1));
    uint8_t newValue = (uint8_t)((g_switchByte & ~mask) | bits);

    Wire.beginTransmission(ADG728_I2C_ADDR);
    Wire.write(newValue);
    if (Wire.endTransmission() != 0) return false;

    g_switchByte = newValue;
    if (led == LIGHT_LED1) g_led1Level = level; else g_led2Level = level;
    return true;
}

uint8_t lightGetLevel(LightId led) {
    return (led == LIGHT_LED1) ? g_led1Level : g_led2Level;
}
