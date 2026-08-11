#include "ntp_sync.h"
#include <Arduino.h>
#include <time.h>
#include "config.h"
#include "rtc_clock.h"

bool ntpSyncNow() {
    configTime(0, 0, cfg.ntp_server1, cfg.ntp_server2);
    // configTime()はgmtOffset=0を元に内部でTZ環境変数を上書きしてしまうため、
    // cfg.timezone_tz(ONVIFで設定されたローカル時刻表示用TZ)を都度再適用する。
    applyTimezoneEnv();
    struct tm ti;
    if (!getLocalTime(&ti, 5000)) {
        Serial.println("[NTP] sync failed");
        return false;
    }
    time_t epoch = mktime(&ti);
    rtcClock.adjustEpoch((int64_t)epoch);
    Serial.printf("[NTP] synced to epoch %ld (servers: %s, %s)\n",
        (long)epoch, cfg.ntp_server1, cfg.ntp_server2);
    return true;
}
