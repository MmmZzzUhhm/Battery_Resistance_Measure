#include "storage_sd.h"
#include <SD_MMC.h>
#include <time.h>
#include "rtc_clock.h"

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

bool ensureDir(const char* path) {
    if (SD_MMC.exists(path)) return true;
    return SD_MMC.mkdir(path);
}

String dateStringFromEpoch(int64_t epoch) {
    time_t t = (time_t)epoch;
    struct tm ti;
    gmtime_r(&t, &ti);
    char buf[16];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &ti);
    return String(buf);
}

} // namespace

bool sdBegin() {
    SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
    if (!SD_MMC.begin("/sdcard", true)) {
        Serial.println("[SD] mount failed (1bit mode) - check wiring/PIN_SD_* build_flags");
        return false;
    }
    ensureDir("/data");
    ensureDir("/queue");
    ensureDir("/firmware");
    Serial.printf("[SD] mounted, %llu MB free\n", (unsigned long long)(sdFreeBytes() / (1024 * 1024)));
    return true;
}

uint64_t sdFreeBytes() {
    return (uint64_t)(SD_MMC.totalBytes() - SD_MMC.usedBytes());
}

void sdAppendHistory(const char* childId, const char* batteryId, int64_t ts,
                      float rMohm, float v, bool valid) {
    String path = "/data/" + dateStringFromEpoch(ts) + ".csv";
    bool isNew = !SD_MMC.exists(path);
    File f = SD_MMC.open(path, FILE_APPEND);
    if (!f) {
        Serial.printf("[SD] failed to open %s for append\n", path.c_str());
        return;
    }
    if (isNew) {
        f.println("ts,child_id,battery_id,r_mohm,v,valid");
    }
    f.printf("%lld,%s,%s,%.4f,%.4f,%d\n", (long long)ts, childId, batteryId, rMohm, v, valid ? 1 : 0);
    f.close();
}

void sdEnqueueForCloud(const char* childId, const String& measurementsJsonArray) {
    char path[80];
    snprintf(path, sizeof(path), "/queue/%lld_%s_%lu.json",
        (long long)rtcClock.nowEpoch(), childId, (unsigned long)millis());
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) {
        Serial.printf("[SD] failed to create queue file %s\n", path);
        return;
    }
    f.print(measurementsJsonArray);
    f.close();
}

bool sdQueuePeek(String& outPath, String& outChildId, String& outJsonArray) {
    File dir = SD_MMC.open("/queue");
    if (!dir || !dir.isDirectory()) return false;

    String bestName;
    File entry = dir.openNextFile();
    while (entry) {
        String name = entry.name();
        entry.close();
        if (bestName.length() == 0 || name < bestName) bestName = name;
        entry = dir.openNextFile();
    }
    dir.close();
    if (bestName.length() == 0) return false;

    outPath = String("/queue/") + bestName;
    // ファイル名: <epoch>_<childId>_<millis>.json から childId を抽出
    int firstUnderscore = bestName.indexOf('_');
    int lastUnderscore   = bestName.lastIndexOf('_');
    outChildId = (firstUnderscore >= 0 && lastUnderscore > firstUnderscore)
        ? bestName.substring(firstUnderscore + 1, lastUnderscore)
        : "unknown";

    File f = SD_MMC.open(outPath, FILE_READ);
    if (!f) return false;
    outJsonArray = f.readString();
    f.close();
    return true;
}

void sdQueueRemove(const String& path) {
    SD_MMC.remove(path);
}

static String firmwarePath(const char* version) {
    return String("/firmware/") + version + ".bin";
}

bool sdFirmwareExists(const char* version) {
    return SD_MMC.exists(firmwarePath(version));
}

size_t sdFirmwareSize(const char* version) {
    File f = SD_MMC.open(firmwarePath(version), FILE_READ);
    if (!f) return 0;
    size_t sz = f.size();
    f.close();
    return sz;
}

File sdFirmwareOpenWrite(const char* version) {
    return SD_MMC.open(firmwarePath(version), FILE_WRITE);
}

File sdFirmwareOpenRead(const char* version) {
    return SD_MMC.open(firmwarePath(version), FILE_READ);
}
