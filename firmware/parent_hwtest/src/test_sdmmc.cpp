#include "test_sdmmc.h"
#include "hwtest_common.h"
#include <SD_MMC.h>

#ifndef PIN_SD_CLK
#define PIN_SD_CLK 7
#endif
#ifndef PIN_SD_CMD
#define PIN_SD_CMD 9
#endif
#ifndef PIN_SD_D0
#define PIN_SD_D0 8
#endif

void testSdMmc() {
    SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
    if (!SD_MMC.begin("/sdcard", true)) {
        char d[96];
        snprintf(d, sizeof(d), "begin(1bit)失敗 (CLK=%d CMD=%d D0=%d の配線を確認)", PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
        report("SD_MMC Mount", false, d);
        return;
    }

    const char* path = "/hwtest_tmp.txt";
    const char* content = "battery-hwtest-check";
    File wf = SD_MMC.open(path, FILE_WRITE);
    if (!wf) {
        report("SD_MMC Mount", false, "マウントは成功したが書込用オープンに失敗");
        return;
    }
    wf.print(content);
    wf.close();

    File rf = SD_MMC.open(path, FILE_READ);
    bool ok = false;
    if (rf) {
        String readBack = rf.readString();
        rf.close();
        ok = (readBack == content);
    }
    SD_MMC.remove(path);

    uint64_t totalMB = SD_MMC.totalBytes() / 1024 / 1024;
    uint64_t freeMB  = (SD_MMC.totalBytes() - SD_MMC.usedBytes()) / 1024 / 1024;
    char d[96];
    snprintf(d, sizeof(d), "write/read/verify %s, 容量%lluMB 空き%lluMB",
        ok ? "OK" : "NG(内容不一致)", (unsigned long long)totalMB, (unsigned long long)freeMB);
    report("SD_MMC Mount", ok, d);
}
