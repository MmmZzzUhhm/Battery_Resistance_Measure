#include "storage_sd.h"
#include <SD_MMC.h>
#include <Preferences.h>
#include <time.h>

#ifndef PIN_SD_CLK
#define PIN_SD_CLK 2
#endif
#ifndef PIN_SD_CMD
#define PIN_SD_CMD 3
#endif
#ifndef PIN_SD_D0
#define PIN_SD_D0  1
#endif

namespace {

const char* DCIM_DIR = "/DCIM";
bool g_sdAvailable = false;

// NTP同期済みかどうかの簡易判定 (2023-01-01以降ならOKとみなす)
bool timeIsSynced() {
    return time(nullptr) > 1672531200;
}

// 未同期時に使う連番。SD不使用中もリブートを跨いで一意になるようNVSに永続化する。
uint32_t nextSequenceNumber() {
    Preferences prefs;
    prefs.begin("camstate", false);
    uint32_t seq = prefs.getUInt("seq", 0) + 1;
    prefs.putUInt("seq", seq);
    prefs.end();
    return seq;
}

String buildFilename() {
    if (timeIsSynced()) {
        time_t now = time(nullptr);
        struct tm tmInfo;
        gmtime_r(&now, &tmInfo);
        char buf[40];
        snprintf(buf, sizeof(buf), "img_%04d%02d%02d_%02d%02d%02d.jpg",
            tmInfo.tm_year + 1900, tmInfo.tm_mon + 1, tmInfo.tm_mday,
            tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec);
        return String(buf);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "img_%08u.jpg", nextSequenceNumber());
    return String(buf);
}

} // namespace

bool sdBegin() {
    SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
    if (!SD_MMC.begin("/sdcard", true)) {
        g_sdAvailable = false;
        return false;
    }
    if (!SD_MMC.exists(DCIM_DIR)) {
        SD_MMC.mkdir(DCIM_DIR);
    }
    g_sdAvailable = true;
    return true;
}

bool sdIsAvailable() {
    return g_sdAvailable;
}

uint64_t sdFreeBytes() {
    return SD_MMC.totalBytes() - SD_MMC.usedBytes();
}

String sdSaveJpeg(const uint8_t* buf, size_t len) {
    String path = String(DCIM_DIR) + "/" + buildFilename();
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) return "";
    size_t written = f.write(buf, len);
    f.close();
    if (written != len) {
        SD_MMC.remove(path);
        return "";
    }
    return path;
}
