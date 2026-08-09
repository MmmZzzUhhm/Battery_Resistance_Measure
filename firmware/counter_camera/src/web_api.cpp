/*
 * ローカル診断/設定用REST API (ONVIF外、本機独自)
 *
 * 上位システムからの撮影要求はONVIF経由 (onvif_routes.cpp の /onvif/snapshot等) で
 * 受け付ける。/api/capture はWeb UIの「試し撮影」ボタンからの動作確認用。
 */
#include "web_api.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include "http_server.h"
#include "config.h"
#include "storage_sd.h"
#include "capture_pipeline.h"

namespace {

void handleStatus() {
    JsonDocument doc;
    doc["device_id"]   = cfg.device_id;
    doc["uptime_s"]    = (uint32_t)(millis() / 1000);
    doc["wifi_sta_ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    doc["wifi_ap_ip"]  = WiFi.softAPIP().toString();
    doc["sd_free_kb"]  = (uint32_t)(sdFreeBytes() / 1024);
    doc["portal_base_url_set"] = strlen(cfg.portal_base_url) > 0;
    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

// POST(またはGET) /api/capture: ローカル動作確認用の試し撮影
void handleCapture() {
    CaptureResult r = captureSaveAndUpload();
    if (!r.ok) {
        httpServer.send(500, "application/json", "{\"ok\":false,\"error\":\"capture_failed\"}");
        return;
    }

    JsonDocument doc;
    doc["ok"]          = true;
    doc["bytes"]       = (uint32_t)r.fb->len;
    doc["saved_path"]  = r.savedPath;
    doc["saved_to_sd"] = r.savedPath.length() > 0;
    doc["upload_attempted"] = r.uploadAttempted;
    doc["upload_ok"]        = r.uploadOk;
    doc["upload_http_code"] = r.uploadHttpCode;

    releaseCaptureResult(r);

    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

void handleGetConfig() {
    httpServer.send(200, "application/json", configToJson());
}

void handlePostConfig() {
    if (!httpServer.hasArg("plain")) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
        return;
    }
    bool ok = configApplyJson(httpServer.arg("plain"));
    httpServer.send(ok ? 200 : 400, "application/json",
        ok ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"invalid json\"}");
}

} // namespace

void webApiRegisterRoutes() {
    httpServer.on("/api/status",  HTTP_GET,  handleStatus);
    httpServer.on("/api/capture", HTTP_POST, handleCapture);
    httpServer.on("/api/capture", HTTP_GET,  handleCapture); // 動作確認用に簡易アクセスも許可
    httpServer.on("/api/config",  HTTP_GET,  handleGetConfig);
    httpServer.on("/api/config",  HTTP_POST, handlePostConfig);
}
