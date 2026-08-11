#include "onvif_media_service.h"
#include "onvif_common.h"
#include "config.h"
#include "capture_pipeline.h"
#include "http_server.h"
#include "esp_camera.h"

namespace {

const char* PROFILE_TOKEN = "Profile_1";
const char* VSC_TOKEN     = "VideoSource_1";
const char* VEC_TOKEN     = "VideoEncoder_1";

void frameSizeToWH(int frameSize, int& w, int& h) {
    switch (frameSize) {
        case FRAMESIZE_QVGA: w = 320;  h = 240;  break;
        case FRAMESIZE_SVGA: w = 800;  h = 600;  break;
        case FRAMESIZE_XGA:  w = 1024; h = 768;  break;
        case FRAMESIZE_HD:   w = 1280; h = 720;  break;
        case FRAMESIZE_UXGA: w = 1600; h = 1200; break;
        case FRAMESIZE_VGA:
        default:             w = 640;  h = 480;  break;
    }
}

String handleGetProfiles() {
    int w, h;
    frameSizeToWH(cfg.frame_size, w, h);
    char buf[900];
    snprintf(buf, sizeof(buf),
        "<trt:GetProfilesResponse><trt:Profiles token=\"%s\" fixed=\"true\">"
        "<tt:Name>MainProfile</tt:Name>"
        "<tt:VideoSourceConfiguration token=\"%s\"><tt:Name>VideoSourceConfig</tt:Name>"
        "<tt:SourceToken>%s</tt:SourceToken>"
        "<tt:Bounds x=\"0\" y=\"0\" width=\"%d\" height=\"%d\"/></tt:VideoSourceConfiguration>"
        "<tt:VideoEncoderConfiguration token=\"%s\"><tt:Name>VideoEncoderConfig</tt:Name>"
        "<tt:Encoding>JPEG</tt:Encoding>"
        "<tt:Resolution><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height></tt:Resolution>"
        "<tt:Quality>%d</tt:Quality></tt:VideoEncoderConfiguration>"
        "</trt:Profiles></trt:GetProfilesResponse>",
        PROFILE_TOKEN, VSC_TOKEN, VSC_TOKEN, w, h, VEC_TOKEN, w, h, 63 - cfg.jpeg_quality);
    return String(buf);
}

String handleGetVideoSources() {
    int w, h;
    frameSizeToWH(cfg.frame_size, w, h);
    char buf[300];
    snprintf(buf, sizeof(buf),
        "<trt:GetVideoSourcesResponse><trt:VideoSources token=\"%s\">"
        "<tt:Resolution><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height></tt:Resolution>"
        "</trt:VideoSources></trt:GetVideoSourcesResponse>",
        VSC_TOKEN, w, h);
    return String(buf);
}

String handleGetVideoSourceConfigurations() {
    int w, h;
    frameSizeToWH(cfg.frame_size, w, h);
    char buf[400];
    snprintf(buf, sizeof(buf),
        "<trt:GetVideoSourceConfigurationsResponse><trt:Configurations token=\"%s\">"
        "<tt:Name>VideoSourceConfig</tt:Name><tt:SourceToken>%s</tt:SourceToken>"
        "<tt:Bounds x=\"0\" y=\"0\" width=\"%d\" height=\"%d\"/>"
        "</trt:Configurations></trt:GetVideoSourceConfigurationsResponse>",
        VSC_TOKEN, VSC_TOKEN, w, h);
    return String(buf);
}

String handleGetSnapshotUri() {
    String uri = onvifDeviceBaseUrl() + "/onvif/snapshot";
    String inner = "<trt:GetSnapshotUriResponse><trt:MediaUri><tt:Uri>" + uri +
                   "</tt:Uri><tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
                   "<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
                   "<tt:Timeout>PT30S</tt:Timeout></trt:MediaUri></trt:GetSnapshotUriResponse>";
    return inner;
}

} // namespace

String onvifHandleMediaService(const String& requestBody) {
    String inner;
    if (soapBodyContainsAction(requestBody, "GetSnapshotUri")) {
        inner = handleGetSnapshotUri();
    } else if (soapBodyContainsAction(requestBody, "GetProfiles")) {
        inner = handleGetProfiles();
    } else if (soapBodyContainsAction(requestBody, "GetVideoSourceConfigurations")) {
        inner = handleGetVideoSourceConfigurations();
    } else if (soapBodyContainsAction(requestBody, "GetVideoSources")) {
        inner = handleGetVideoSources();
    } else {
        return soapFault("Unsupported media service action");
    }
    return soapEnvelopeWrap(inner);
}

void onvifHandleSnapshotRoute() {
    CaptureResult r = captureAndSave();
    if (!r.ok) {
        httpServer.send(500, "text/plain", "capture failed");
        return;
    }
    httpServer.setContentLength(r.fb->len);
    httpServer.send(200, "image/jpeg", "");
    httpServer.sendContent((const char*)r.fb->buf, r.fb->len);
    releaseCaptureResult(r);
}
