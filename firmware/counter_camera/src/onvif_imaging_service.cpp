/*
 * ONVIF Imaging Service (縮小版)
 * Brightness/ColorSaturation/Contrast/Sharpness のみ対応。
 * ONVIFのfloatスケール(0〜100)とesp32-cameraのint(-2〜2)を線形変換する。
 */
#include "onvif_imaging_service.h"
#include "onvif_common.h"
#include "config.h"
#include "esp_camera.h"

namespace {

float cameraToOnvif(int v) { return ((float)(v + 2) / 4.0f) * 100.0f; }
int onvifToCamera(float v) {
    int c = (int)((v / 100.0f) * 4.0f - 2.0f + 0.5f);
    if (c < -2) c = -2;
    if (c > 2)  c = 2;
    return c;
}

bool extractTagFloat(const String& body, const char* tagLocalName, float& out) {
    int idx = body.indexOf(tagLocalName);
    if (idx < 0) return false;
    int gt = body.indexOf('>', idx);
    if (gt < 0) return false;
    int lt = body.indexOf('<', gt);
    if (lt < 0) return false;
    out = body.substring(gt + 1, lt).toFloat();
    return true;
}

String settingsXml(const char* rootTag) {
    sensor_t* s = esp_camera_sensor_get();
    int brightness = s ? s->status.brightness : 0;
    int contrast   = s ? s->status.contrast   : 0;
    int saturation = s ? s->status.saturation : 0;
    int sharpness  = s ? s->status.sharpness  : 0;

    char buf[500];
    snprintf(buf, sizeof(buf),
        "<timg:%s><timg:ImagingSettings>"
        "<tt:Brightness>%.1f</tt:Brightness>"
        "<tt:ColorSaturation>%.1f</tt:ColorSaturation>"
        "<tt:Contrast>%.1f</tt:Contrast>"
        "<tt:Sharpness>%.1f</tt:Sharpness>"
        "</timg:ImagingSettings></timg:%s>",
        rootTag, cameraToOnvif(brightness), cameraToOnvif(saturation),
        cameraToOnvif(contrast), cameraToOnvif(sharpness), rootTag);
    return String(buf);
}

String handleGetImagingSettings() {
    return settingsXml("GetImagingSettingsResponse");
}

String handleSetImagingSettings(const String& body) {
    sensor_t* s = esp_camera_sensor_get();
    float v;
    if (s && extractTagFloat(body, "Brightness", v))     s->set_brightness(s, onvifToCamera(v));
    if (s && extractTagFloat(body, "ColorSaturation", v)) s->set_saturation(s, onvifToCamera(v));
    if (s && extractTagFloat(body, "Contrast", v))       s->set_contrast(s, onvifToCamera(v));
    if (s && extractTagFloat(body, "Sharpness", v))      s->set_sharpness(s, onvifToCamera(v));
    return "<timg:SetImagingSettingsResponse/>";
}

String handleGetOptions() {
    return "<timg:GetOptionsResponse><timg:ImagingOptions>"
           "<tt:Brightness><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:Brightness>"
           "<tt:ColorSaturation><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:ColorSaturation>"
           "<tt:Contrast><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:Contrast>"
           "<tt:Sharpness><tt:Min>0</tt:Min><tt:Max>100</tt:Max></tt:Sharpness>"
           "</timg:ImagingOptions></timg:GetOptionsResponse>";
}

} // namespace

String onvifHandleImagingService(const String& requestBody) {
    String inner;
    if (soapBodyContainsAction(requestBody, "SetImagingSettings")) {
        inner = handleSetImagingSettings(requestBody);
    } else if (soapBodyContainsAction(requestBody, "GetImagingSettings")) {
        inner = handleGetImagingSettings();
    } else if (soapBodyContainsAction(requestBody, "GetOptions")) {
        inner = handleGetOptions();
    } else {
        return soapFault("Unsupported imaging service action");
    }
    return soapEnvelopeWrap(inner);
}
