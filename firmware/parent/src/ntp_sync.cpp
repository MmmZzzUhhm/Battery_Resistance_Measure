#include "ntp_sync.h"
#include <Arduino.h>
#include <time.h>
#include "rtc_clock.h"

bool ntpSyncRtc() {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    struct tm ti;
    if (!getLocalTime(&ti, 5000)) {
        Serial.println("[NTP] sync failed");
        return false;
    }
    time_t epoch = mktime(&ti);
    rtcClock.adjustEpoch((int64_t)epoch);
    Serial.printf("[NTP] RTC synced to epoch %ld\n", (long)epoch);
    return true;
}
