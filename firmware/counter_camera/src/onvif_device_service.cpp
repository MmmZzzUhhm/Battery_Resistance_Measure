#include "onvif_device_service.h"
#include "onvif_common.h"
#include "config.h"
#include <time.h>

namespace {

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
    char buf[256];
    snprintf(buf, sizeof(buf),
        "<tds:GetSystemDateAndTimeResponse><tds:SystemDateAndTime>"
        "<tt:DateTimeType>NTP</tt:DateTimeType><tt:DaylightSavings>false</tt:DaylightSavings>"
        "<tt:UTCDateTime><tt:Time><tt:Hour>%d</tt:Hour><tt:Minute>%d</tt:Minute><tt:Second>%d</tt:Second></tt:Time>"
        "<tt:Date><tt:Year>%d</tt:Year><tt:Month>%d</tt:Month><tt:Day>%d</tt:Day></tt:Date></tt:UTCDateTime>"
        "</tds:SystemDateAndTime></tds:GetSystemDateAndTimeResponse>",
        t.tm_hour, t.tm_min, t.tm_sec, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return String(buf);
}

} // namespace

String onvifHandleDeviceService(const String& requestBody) {
    String inner;
    if (soapBodyContainsAction(requestBody, "GetSystemDateAndTime")) {
        inner = handleGetSystemDateAndTime();
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
