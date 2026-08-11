/*
 * 撮影 -> (SDカード挿入時のみ)SD保存 の共通処理。
 * ONVIF snapshotエンドポイントとローカルWeb UIの試し撮影から共用する。
 * 上位システム(ポータル)への画像受け渡しは、この結果をそのままHTTPレスポンスとして
 * 返すpull型のため、本処理自体にアップロードは含まない。
 */
#pragma once
#include <Arduino.h>
#include "esp_camera.h"

struct CaptureResult {
    bool ok = false;
    camera_fb_t* fb = nullptr;   // ok時のみ非null。使用後は releaseCaptureResult() で解放すること
    String savedPath;
};

CaptureResult captureAndSave();
void releaseCaptureResult(CaptureResult& result);
