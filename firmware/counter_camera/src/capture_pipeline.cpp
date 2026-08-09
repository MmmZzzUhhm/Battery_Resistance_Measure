#include "capture_pipeline.h"
#include "camera.h"
#include "storage_sd.h"
#include "uploader.h"
#include "config.h"

CaptureResult captureSaveAndUpload() {
    CaptureResult r;
    camera_fb_t* fb = cameraCapture();
    if (!fb) {
        return r;
    }
    r.ok = true;
    r.fb = fb;

    r.savedPath = sdSaveJpeg(fb->buf, fb->len);

    r.uploadAttempted = strlen(cfg.portal_base_url) > 0;
    if (r.uploadAttempted) {
        String resp;
        r.uploadOk = uploadJpeg(fb->buf, fb->len, r.savedPath, r.uploadHttpCode, resp);
    }

    return r;
}

void releaseCaptureResult(CaptureResult& result) {
    if (result.fb) {
        esp_camera_fb_return(result.fb);
        result.fb = nullptr;
    }
}
