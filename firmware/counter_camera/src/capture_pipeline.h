/*
 * 撮影 -> SD保存 -> (upload_url設定時)アップロード の共通処理。
 * ONVIF snapshotエンドポイントとローカルWeb UIの試し撮影から共用する。
 */
#pragma once
#include <Arduino.h>
#include "esp_camera.h"

struct CaptureResult {
    bool ok = false;
    camera_fb_t* fb = nullptr;   // ok時のみ非null。使用後は releaseCaptureResult() で解放すること
    String savedPath;
    bool uploadAttempted = false;
    bool uploadOk = false;
    int  uploadHttpCode = 0;
};

CaptureResult captureSaveAndUpload();
void releaseCaptureResult(CaptureResult& result);
