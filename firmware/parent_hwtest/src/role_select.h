/*
 * 基板の実装構成(役割)。この基板は1枚のPCBで複数用途を満たす設計のため、
 * 実際に何を検査すべきかは部品実装(BOM)によって異なる。テストモードのWeb UIで選択する。
 */
#pragma once
#include <Arduino.h>

enum BoardRole : uint8_t {
    ROLE_PARENT          = 1,  // 親機: SD_MMC(1bit) + SHT31 + PCF8563T(RTC)
    ROLE_ULTRASONIC      = 2,  // 超音波センサ: SPH0641LU4H-1 (PDMマイク)
    ROLE_DOPPLER         = 3,  // マイクロ波ドップラーセンサカメラ: IMD-2000 + SD_MMC(1bit) + PCF8563T(RTC)
    ROLE_COUNTER_CAMERA  = 4,  // 薄型カウンタカメラ: XIAO ESP32S3 Sense内蔵カメラ + SD_MMC(1bit)
};

const char* roleName(BoardRole role);
