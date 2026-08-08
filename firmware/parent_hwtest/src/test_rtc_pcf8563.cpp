#include "test_rtc_pcf8563.h"
#include "hwtest_common.h"
#include <RTClib.h>

void testRtcPcf8563() {
    RTC_PCF8563 rtc;
    if (!rtc.begin()) {
        report("RTC PCF8563T", false, "begin() failed - not found on I2C bus (addr 0x51)");
        return;
    }
    bool hadLostPower = rtc.lostPower();
    DateTime setTime(2026, 1, 1, 0, 0, 0);
    rtc.adjust(setTime);
    delay(1100);
    DateTime now = rtc.now();
    int64_t diff = (int64_t)now.unixtime() - (int64_t)setTime.unixtime();
    bool ok = diff >= 1 && diff <= 5;
    char d[96];
    snprintf(d, sizeof(d), "set->read diff=%llds (lostPower flag was %s before adjust)",
        (long long)diff, hadLostPower ? "true(正常: 初回通電時は真になる)" : "false");
    report("RTC PCF8563T", ok, d);
}
