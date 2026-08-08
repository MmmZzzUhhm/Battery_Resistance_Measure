#include "test_sd_dir.h"
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

void testSdDirList() {
    // testSdMmc()で既にマウント済みなら再度setPins/beginを呼ばない
    // (マウント後にsetPinsを呼ぶとSD_MMCライブラリが警告を出すため)
    if (SD_MMC.cardType() == CARD_NONE) {
        SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
        if (!SD_MMC.begin("/sdcard", true)) {
            report("SD ディレクトリ読取", false, "マウント失敗のため実施不可");
            return;
        }
    }

    File dir = SD_MMC.open("/");
    if (!dir || !dir.isDirectory()) {
        report("SD ディレクトリ読取", false, "ルートディレクトリのオープンに失敗");
        return;
    }

    int count = 0;
    String sample;
    File entry = dir.openNextFile();
    while (entry) {
        if (count < 8) {
            if (sample.length() > 0) sample += ", ";
            sample += String(entry.name()) + (entry.isDirectory() ? "/" : "");
        }
        count++;
        entry.close();
        entry = dir.openNextFile();
    }
    dir.close();

    String detail = String(count) + "件";
    if (count > 0) detail += " (" + sample + (count > 8 ? ", ..." : "") + ")";
    report("SD ディレクトリ読取", true, detail);
}
