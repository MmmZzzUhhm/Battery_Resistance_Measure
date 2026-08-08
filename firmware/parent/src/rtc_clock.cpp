#include "rtc_clock.h"
#include <RTClib.h>

RtcClock rtcClock;
static RTC_PCF8563 rtc;

bool RtcClock::begin() {
    available_ = rtc.begin();
    if (!available_) {
        Serial.println("[RTC] PCF8563T not found");
        return false;
    }
    if (rtc.lostPower()) {
        Serial.println("[RTC] power was lost, time is not trustworthy until next sync");
    }
    return true;
}

int64_t RtcClock::nowEpoch() {
    if (!available_) return 0;
    return (int64_t)rtc.now().unixtime();
}

void RtcClock::adjustEpoch(int64_t epochSec) {
    if (!available_ || epochSec <= 0) return;
    rtc.adjust(DateTime((uint32_t)epochSec));
}
