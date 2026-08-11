/*
 * PCF8563T RTC ラッパ (バッテリーバックアップ済み)
 * firmware/parent/src/rtc_clock.h と同一実装。
 */
#pragma once
#include <Arduino.h>

class RtcClock {
public:
    bool begin();                 // I2C初期化後に呼ぶ (Wire.begin済み前提)
    int64_t nowEpoch();           // 現在時刻をunix epoch秒で返す (RTC未検出時は0)
    void    adjustEpoch(int64_t epochSec);
    bool    isAvailable() const { return available_; }

private:
    bool available_ = false;
};

extern RtcClock rtcClock;
