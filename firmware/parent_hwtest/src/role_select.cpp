#include "role_select.h"

const char* roleName(BoardRole role) {
    switch (role) {
        case ROLE_PARENT:         return "親機 (SD_MMC + SHT31 + PCF8563T)";
        case ROLE_ULTRASONIC:     return "超音波センサ (SPH0641LU4H-1 PDMマイク)";
        case ROLE_DOPPLER:        return "マイクロ波ドップラーセンサカメラ (IMD-2000 + SD_MMC + PCF8563T)";
        case ROLE_COUNTER_CAMERA: return "薄型カウンタカメラ (Senseカメラ + SD_MMC)";
    }
    return "不明な構成";
}
