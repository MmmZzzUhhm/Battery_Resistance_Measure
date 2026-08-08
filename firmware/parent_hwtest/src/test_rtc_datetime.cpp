#include "test_rtc_datetime.h"
#include "hwtest_common.h"
#include <RTClib.h>

void testRtcReadTime() {
    RTC_PCF8563 rtc;
    if (!rtc.begin()) {
        report("RTC 日時読み取り", false, "begin() failed - I2Cバス上に見つからない (addr 0x51)");
        return;
    }

    DateTime now = rtc.now();
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
        now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

    // 未設定/バックアップ電池切れ等で明らかにおかしい値を返していないかの簡易チェック
    bool plausible = now.year() >= 2000 && now.year() <= 2099;

    String detail = String("現在時刻=") + buf;
    if (!plausible) detail += " (日時が不自然: 未設定またはバックアップ電池切れの可能性)";
    report("RTC 日時読み取り", plausible, detail);
}
