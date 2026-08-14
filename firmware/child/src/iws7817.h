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

// I2C_Power_CTRL (XIAO D3) 制御。HIGHでTPS22917負荷スイッチがONし、
// I2C_Power5V経由でIWS7817に5Vが供給される。バッテリー駆動時の待機消費を
// 抑えるため、測定の直前にiws7817PowerOn()、読み取り後にiws7817PowerOff()すること。
void iws7817PowerBegin();
void iws7817PowerOn();
void iws7817PowerOff();

IwsMeasurement readIWS7817(uint8_t i2cAddr);
