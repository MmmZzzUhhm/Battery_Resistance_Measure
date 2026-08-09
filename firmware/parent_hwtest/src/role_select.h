/*
 * 起動時にシリアルモニタからの入力で基板の実装構成(役割)を選択する。
 * この基板は1枚のPCBで複数用途を満たす設計のため、実際に何を検査すべきかは
 * 部品実装(BOM)によって異なる。オペレータが目視で実装内容を確認し番号を入力する。
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
const char* roleApSsid(BoardRole role);

// シリアルから 1/2/3/4 の入力があるまでブロックし、選択された構成を返す。
BoardRole selectRoleFromSerial();
