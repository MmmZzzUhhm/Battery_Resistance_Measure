/*
 * ローカル診断/設定用REST API (ONVIF外、本機独自)
 *
 * 上位システムからの撮影要求はONVIF経由 (onvif_routes.cpp の /onvif/snapshot等) で
 * 受け付ける。/api/capture はWeb UIの「試し撮影」ボタンからの動作確認用。
 */
#include "web_api.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <time.h>
#include "http_server.h"
#include "config.h"
#include "storage_sd.h"
#include "capture_pipeline.h"
#include "rtc_clock.h"

namespace {

void handleStatus() {
    JsonDocument doc;
    doc["device_id"]   = cfg.device_id;
    doc["uptime_s"]    = (uint32_t)(millis() / 1000);
    doc["wifi_sta_ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    doc["wifi_ap_ip"]  = WiFi.softAPIP().toString();
    doc["sd_free_kb"]  = (uint32_t)(sdFreeBytes() / 1024);
    doc["sd_present"]  = sdIsAvailable();
    doc["rtc_available"] = rtcClock.isAvailable();
    doc["now_epoch"] = (uint32_t)time(nullptr);
    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

// POST(またはGET) /api/capture: ローカル動作確認用の試し撮影
void handleCapture() {
    CaptureResult r = captureAndSave();
    if (!r.ok) {
        httpServer.send(500, "application/json", "{\"ok\":false,\"error\":\"capture_failed\"}");
        return;
    }

    JsonDocument doc;
    doc["ok"]          = true;
    doc["bytes"]       = (uint32_t)r.fb->len;
    doc["width"]       = r.fb->width;
    doc["height"]      = r.fb->height;
    doc["saved_path"]  = r.savedPath;
    doc["saved_to_sd"] = r.savedPath.length() > 0;

    releaseCaptureResult(r);

    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

void handleGetConfig() {
    httpServer.send(200, "application/json", configToJson());
}

// frame_sizeはカメラ初期化(cameraBegin())時にのみ読み込まれるため、変更を反映するには再起動が必要。
void handlePostConfig() {
    if (!httpServer.hasArg("plain")) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
        return;
    }
    int oldFrameSize = cfg.frame_size;
    bool ok = configApplyJson(httpServer.arg("plain"));
    bool needsRestart = ok && cfg.frame_size != oldFrameSize;

    if (!ok) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }
    httpServer.send(200, "application/json",
        needsRestart ? "{\"ok\":true,\"restarting\":true}" : "{\"ok\":true}");

    if (needsRestart) {
        delay(300); // レスポンス送信完了待ち
        ESP.restart();
    }
}

} // namespace

void webApiRegisterRoutes() {
    httpServer.on("/api/status",  HTTP_GET,  handleStatus);
    httpServer.on("/api/capture", HTTP_POST, handleCapture);
    httpServer.on("/api/capture", HTTP_GET,  handleCapture); // 動作確認用に簡易アクセスも許可
    httpServer.on("/api/config",  HTTP_GET,  handleGetConfig);
    httpServer.on("/api/config",  HTTP_POST, handlePostConfig);
}
