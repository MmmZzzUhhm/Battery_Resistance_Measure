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

const char* roleApSsid(BoardRole role) {
    switch (role) {
        case ROLE_PARENT:         return "HWTEST-PARENT";
        case ROLE_ULTRASONIC:     return "HWTEST-ULTRASONIC";
        case ROLE_DOPPLER:        return "HWTEST-DOPPLER";
        case ROLE_COUNTER_CAMERA: return "HWTEST-COUNTERCAM";
    }
    return "HWTEST-UNKNOWN";
}

static void printMenu() {
    Serial.println("\n==================== 構成選択 ====================");
    Serial.println("  実装されている部品に応じて番号を選んでください。");
    Serial.println("  1: 親機                   (SD_MMC + SHT31 + PCF8563T)");
    Serial.println("  2: 超音波センサ            (SPH0641LU4H-1 PDMマイクのみ)");
    Serial.println("  3: マイクロ波ドップラーセンサカメラ (IMD-2000 + SD_MMC + PCF8563T)");
    Serial.println("  4: 薄型カウンタカメラ      (Senseカメラ + SD_MMC)");
    Serial.println("===================================================");
    Serial.print("番号を入力してください (1-4): ");
}

BoardRole selectRoleFromSerial() {
    printMenu();
    unsigned long lastPrompt = millis();

    while (true) {
        if (Serial.available() > 0) {
            char c = (char)Serial.read();
            if (c == '1') { Serial.println(c); return ROLE_PARENT; }
            if (c == '2') { Serial.println(c); return ROLE_ULTRASONIC; }
            if (c == '3') { Serial.println(c); return ROLE_DOPPLER; }
            if (c == '4') { Serial.println(c); return ROLE_COUNTER_CAMERA; }
            if (c != '\r' && c != '\n') {
                Serial.println(c);
                Serial.println("!! 1〜4のいずれかを入力してください !!");
                printMenu();
            }
            lastPrompt = millis();
        }
        if (millis() - lastPrompt > 15000) {
            lastPrompt = millis();
            printMenu();
        }
        delay(10);
    }
}
