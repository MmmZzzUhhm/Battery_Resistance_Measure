#include "onvif_device_service.h"
#include "onvif_common.h"
#include "config.h"
#include "rtc_clock.h"
#include "ntp_sync.h"
#include <time.h>

namespace {

// bodyの中からtagNameで指定した要素の値(整数)を取り出す。名前空間prefixの違いは無視する。
bool extractTagInt(const String& body, const char* tagName, int& out) {
    int idx = body.indexOf(tagName);
    if (idx < 0) return false;
    int gt = body.indexOf('>', idx);
    if (gt < 0) return false;
    int lt = body.indexOf('<', gt);
    if (lt < 0) return false;
    out = body.substring(gt + 1, lt).toInt();
    return true;
}

// fromIndex以降で最初に見つかったtagNameの値(文字列)を取り出す。
// 見つかった場合、その要素の直後の位置を返す(次のタグを続けて検索するため)。見つからない場合は-1。
int extractTagStringFrom(const String& body, const char* tagName, int fromIndex, String& out) {
    int idx = body.indexOf(tagName, fromIndex);
    if (idx < 0) return -1;
    int gt = body.indexOf('>', idx);
    if (gt < 0) return -1;
    int lt = body.indexOf('<', gt);
    if (lt < 0) return -1;
    out = body.substring(gt + 1, lt);
    return lt;
}

String handleGetDeviceInformation() {
    String inner = "<tds:GetDeviceInformationResponse>";
    inner += "<tds:Manufacturer>" + String(ONVIF_MANUFACTURER) + "</tds:Manufacturer>";
    inner += "<tds:Model>" + String(ONVIF_MODEL) + "</tds:Model>";
    inner += "<tds:FirmwareVersion>" + String(ONVIF_FIRMWARE_VERSION) + "</tds:FirmwareVersion>";
    inner += "<tds:SerialNumber>" + String(cfg.device_id) + "</tds:SerialNumber>";
    inner += "<tds:HardwareId>XIAO-ESP32S3-Sense</tds:HardwareId>";
    inner += "</tds:GetDeviceInformationResponse>";
    return inner;
}

String handleGetCapabilities() {
    String base = onvifDeviceBaseUrl();
    String inner = "<tds:GetCapabilitiesResponse><tds:Capabilities>";
    inner += "<tt:Device><tt:XAddr>" + base + "/onvif/device_service</tt:XAddr></tt:Device>";
    inner += "<tt:Media><tt:XAddr>" + base + "/onvif/media_service</tt:XAddr></tt:Media>";
    inner += "<tt:Imaging><tt:XAddr>" + base + "/onvif/imaging_service</tt:XAddr></tt:Imaging>";
    inner += "</tds:Capabilities></tds:GetCapabilitiesResponse>";
    return inner;
}

String handleGetServices() {
    String base = onvifDeviceBaseUrl();
    String inner = "<tds:GetServicesResponse>";
    inner += "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/device/wsdl</tds:Namespace>"
             "<tds:XAddr>" + base + "/onvif/device_service</tds:XAddr>"
             "<tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tds:Version></tds:Service>";
    inner += "<tds:Service><tds:Namespace>http://www.onvif.org/ver10/media/wsdl</tds:Namespace>"
             "<tds:XAddr>" + base + "/onvif/media_service</tds:XAddr>"
             "<tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tds:Version></tds:Service>";
    inner += "<tds:Service><tds:Namespace>http://www.onvif.org/ver20/imaging/wsdl</tds:Namespace>"
             "<tds:XAddr>" + base + "/onvif/imaging_service</tds:XAddr>"
             "<tds:Version><tt:Major>2</tt:Major><tt:Minor>5</tt:Minor></tds:Version></tds:Service>";
    inner += "</tds:GetServicesResponse>";
    return inner;
}

String handleGetSystemDateAndTime() {
    time_t now = time(nullptr);
    struct tm t;
    gmtime_r(&now, &t);
    char buf[420];
    snprintf(buf, sizeof(buf),
        "<tds:GetSystemDateAndTimeResponse><tds:SystemDateAndTime>"
        "<tt:DateTimeType>NTP</tt:DateTimeType><tt:DaylightSavings>false</tt:DaylightSavings>"
        "<tt:UTCDateTime><tt:Time><tt:Hour>%d</tt:Hour><tt:Minute>%d</tt:Minute><tt:Second>%d</tt:Second></tt:Time>"
        "<tt:Date><tt:Year>%d</tt:Year><tt:Month>%d</tt:Month><tt:Day>%d</tt:Day></tt:Date></tt:UTCDateTime>"
        "</tds:SystemDateAndTime></tds:GetSystemDateAndTimeResponse>",
        t.tm_hour, t.tm_min, t.tm_sec, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return String(buf);
}

// DateTimeType=Manualなら指定されたUTCDateTimeをシステム時刻とRTC(PCF8563T)に設定する。
// DateTimeType=NTPなら現在設定済みのNTPサーバーへ即時再同期する。
String handleSetSystemDateAndTime(const String& body) {
    String dtType;
    extractTagStringFrom(body, "DateTimeType", 0, dtType);

    if (dtType.indexOf("Manual") >= 0) {
        struct tm t = {};
        int v;
        if (extractTagInt(body, "Year", v))   t.tm_year = v - 1900;
        if (extractTagInt(body, "Month", v))  t.tm_mon  = v - 1;
        if (extractTagInt(body, "Day", v))    t.tm_mday = v;
        if (extractTagInt(body, "Hour", v))   t.tm_hour = v;
        if (extractTagInt(body, "Minute", v)) t.tm_min  = v;
        if (extractTagInt(body, "Second", v)) t.tm_sec  = v;
        // configTime(0, 0, ...) で常にgmtOffset=0運用のため、mktime()の結果はUTCとして扱える
        // (ntp_sync.cpp / firmware/parent/src/ntp_sync.cpp と同じ方式)。
        time_t epoch = mktime(&t);
        struct timeval tv = { epoch, 0 };
        settimeofday(&tv, nullptr);
        rtcClock.adjustEpoch((int64_t)epoch);
        Serial.printf("[ONVIF] SetSystemDateAndTime(Manual) -> epoch=%ld\n", (long)epoch);
    } else {
        ntpSyncNow();
    }
    return "<tds:SetSystemDateAndTimeResponse/>";
}

String handleGetNTP() {
    char buf[400];
    snprintf(buf, sizeof(buf),
        "<tds:GetNTPResponse><tds:NTPInformation>"
        "<tt:FromDHCP>false</tt:FromDHCP>"
        "<tt:NTPManual><tt:Type>DNS</tt:Type><tt:DNSname>%s</tt:DNSname></tt:NTPManual>"
        "<tt:NTPManual><tt:Type>DNS</tt:Type><tt:DNSname>%s</tt:DNSname></tt:NTPManual>"
        "</tds:NTPInformation></tds:GetNTPResponse>",
        cfg.ntp_server1, cfg.ntp_server2);
    return String(buf);
}

// SetNTPRequest中のNTPManual/DNSnameを先頭から最大2件読み取り、cfg.ntp_server1/2に反映して即時再同期する。
String handleSetNTP(const String& body) {
    String s1, s2;
    int next = extractTagStringFrom(body, "DNSname", 0, s1);
    if (next >= 0) extractTagStringFrom(body, "DNSname", next, s2);

    if (s1.length() > 0) strlcpy(cfg.ntp_server1, s1.c_str(), sizeof(cfg.ntp_server1));
    if (s2.length() > 0) strlcpy(cfg.ntp_server2, s2.c_str(), sizeof(cfg.ntp_server2));
    configSave();
    ntpSyncNow();
    return "<tds:SetNTPResponse/>";
}

} // namespace

String onvifHandleDeviceService(const String& requestBody) {
    String inner;
    if (soapBodyContainsAction(requestBody, "SetSystemDateAndTime")) {
        inner = handleSetSystemDateAndTime(requestBody);
    } else if (soapBodyContainsAction(requestBody, "GetSystemDateAndTime")) {
        inner = handleGetSystemDateAndTime();
    } else if (soapBodyContainsAction(requestBody, "SetNTP")) {
        inner = handleSetNTP(requestBody);
    } else if (soapBodyContainsAction(requestBody, "GetNTP")) {
        inner = handleGetNTP();
    } else if (soapBodyContainsAction(requestBody, "GetDeviceInformation")) {
        inner = handleGetDeviceInformation();
    } else if (soapBodyContainsAction(requestBody, "GetCapabilities")) {
        inner = handleGetCapabilities();
    } else if (soapBodyContainsAction(requestBody, "GetServices")) {
        inner = handleGetServices();
    } else {
        return soapFault("Unsupported device service action");
    }
    return soapEnvelopeWrap(inner);
}
