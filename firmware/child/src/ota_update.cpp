#include "ota_update.h"
#include <Update.h>

bool otaBegin(size_t sizeBytes, const char* md5Hex) {
    if (!Update.begin(sizeBytes, U_FLASH)) {
        Serial.printf("[OTA] begin failed: %s\n", Update.errorString());
        return false;
    }
    if (md5Hex && strlen(md5Hex) == 32) {
        if (!Update.setMD5(md5Hex)) {
            Serial.println("[OTA] setMD5 failed");
        }
    }
    Serial.printf("[OTA] begin size=%u\n", (unsigned)sizeBytes);
    return true;
}

bool otaWrite(const uint8_t* data, size_t len) {
    size_t written = Update.write(const_cast<uint8_t*>(data), len);
    if (written != len) {
        Serial.printf("[OTA] write short: %u/%u (%s)\n", (unsigned)written, (unsigned)len, Update.errorString());
        return false;
    }
    return true;
}

bool otaEnd() {
    bool ok = Update.end(true);
    if (!ok) {
        Serial.printf("[OTA] end failed: %s\n", Update.errorString());
    } else {
        Serial.println("[OTA] success, rebooting after sync completes");
    }
    return ok;
}

void otaAbort() {
    Update.abort();
}
