/*
 * LED照明 明るさ調整制御 (ADG728経由、counter_cameraのlight_control.hと同一仕様)
 * 詳細はfirmware/counter_camera/src/light_control.hのコメントを参照。
 */
#pragma once
#include <Arduino.h>

#define LIGHT_LEVEL_OFF 0
#define LIGHT_LEVEL_MIN 1  // 最も明るい (10Ω)
#define LIGHT_LEVEL_MAX 4  // 最も暗い   (470Ω)

enum LightId {
    LIGHT_LED1 = 1,
    LIGHT_LED2 = 2,
};

// 戻り値はADG728への書き込み(I2C)が成功したかどうか。
bool lightSetLevel(LightId led, uint8_t level);
uint8_t lightGetLevel(LightId led);
