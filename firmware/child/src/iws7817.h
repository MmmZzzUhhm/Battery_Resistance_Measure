/*
 * IWS7817 (東京デバイセズ, I2C 交流インピーダンス方式 内部抵抗測定モジュール) 読取
 * 旧M5AtomS3版 (firmware/_legacy_m5atoms3/src/main.cpp) の readIWS7817() を移植。
 */
#pragma once
#include <Arduino.h>

struct IwsMeasurement {
    bool  valid;
    float r_mohm;
    float v;
};

IwsMeasurement readIWS7817(uint8_t i2cAddr);
