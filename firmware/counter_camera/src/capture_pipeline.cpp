#include "capture_pipeline.h"
#include "camera.h"
#include "storage_sd.h"
#include "config.h"
#include "light_control.h"

namespace {
// LED点灯直後の数フレームは、点灯前の暗い状態で決まった露出(AEC)を引き継ぐため暗く写る。
// 安定するまで数フレーム捨ててから本番の1枚を撮影する。
void settleLightBeforeCapture() {
    for (int i = 0; i < 2; i++) {
        camera_fb_t* fb = cameraCapture();
        if (fb) cameraReleaseFrame(fb);
        delay(100);
    }
}
}  // namespace

CaptureResult captureAndSave() {
    CaptureResult r;

    bool lightNeeded = cfg.led1_level > 0 || cfg.led2_level > 0;
    if (lightNeeded) {
        lightSetLevel(LIGHT_LED1, (uint8_t)cfg.led1_level);
        lightSetLevel(LIGHT_LED2, (uint8_t)cfg.led2_level);
        settleLightBeforeCapture();
    }

    camera_fb_t* fb = cameraCapture();

    if (lightNeeded) {
        lightSetLevel(LIGHT_LED1, LIGHT_LEVEL_OFF);
        lightSetLevel(LIGHT_LED2, LIGHT_LEVEL_OFF);
    }

    if (!fb) {
        return r;
    }
    r.ok = true;
    r.fb = fb;

    if (sdIsAvailable()) {
        r.savedPath = sdSaveJpeg(fb->buf, fb->len);
    }

    return r;
}

void releaseCaptureResult(CaptureResult& result) {
    if (result.fb) {
        cameraReleaseFrame(result.fb);
        result.fb = nullptr;
    }
}
