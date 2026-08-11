#include "capture_pipeline.h"
#include "camera.h"
#include "storage_sd.h"

CaptureResult captureAndSave() {
    CaptureResult r;
    camera_fb_t* fb = cameraCapture();
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
