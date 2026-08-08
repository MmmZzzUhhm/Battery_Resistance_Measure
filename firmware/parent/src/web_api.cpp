#include "web_api.h"
#include <WiFi.h>
#include <SD_MMC.h>
#include <ArduinoJson.h>
#include "http_server.h"
#include "config.h"
#include "rtc_clock.h"
#include "storage_sd.h"
#include "child_registry.h"

namespace {

File g_uploadFile;

void handleStatus() {
    JsonDocument doc;
    doc["gateway_id"]   = cfg.gateway_id;
    doc["uptime_s"]     = (uint32_t)(millis() / 1000);
    doc["rtc_epoch"]    = (int64_t)rtcClock.nowEpoch();
    doc["wifi_sta_ip"]  = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
    doc["wifi_ap_ip"]   = WiFi.softAPIP().toString();
    doc["sd_free_kb"]   = (uint32_t)(sdFreeBytes() / 1024);
    doc["children_seen"] = registrySeenCount();
    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

void handleChildren() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    int n = registrySeenCount();
    for (int i = 0; i < n; i++) {
        String cid = registrySeenChildId(i);
        JsonObject o = arr.add<JsonObject>();
        o["child_id"] = cid;
        ChildLastReading last;
        if (registryGetLastReading(cid, last)) {
            JsonObject lo = o["last"].to<JsonObject>();
            lo["ts"]     = last.ts;
            lo["r_mohm"] = last.r_mohm;
            lo["v"]      = last.v;
            lo["valid"]  = last.valid;
        }
        PendingChildUpdate pend;
        if (registryGetPending(cid, pend)) {
            o["pending_config"] = pend.hasConfig;
            o["pending_ota"]    = pend.hasOta;
        }
    }
    String out;
    serializeJson(doc, out);
    httpServer.send(200, "application/json", out);
}

// child_id が指定されていれば該当行のみ抽出する (単純な部分一致フィルタ)
void handleHistory() {
    if (!httpServer.hasArg("date")) {
        httpServer.send(400, "text/plain", "date=YYYY-MM-DD required");
        return;
    }
    String path = "/data/" + httpServer.arg("date") + ".csv";
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) {
        httpServer.send(404, "text/plain", "no data for that date");
        return;
    }
    if (!httpServer.hasArg("child_id")) {
        httpServer.streamFile(f, "text/csv");
        f.close();
        return;
    }
    String childId = httpServer.arg("child_id");
    String out = "ts,child_id,battery_id,r_mohm,v,valid\n";
    while (f.available()) {
        String line = f.readStringUntil('\n');
        if (line.indexOf("," + childId + ",") >= 0) {
            out += line + "\n";
        }
    }
    f.close();
    httpServer.send(200, "text/csv", out);
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

void handleFirmwareUploadDone() {
    httpServer.send(200, "application/json", "{\"ok\":true}");
}

void handleFirmwareUploadChunk() {
    HTTPUpload& upload = httpServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String version = httpServer.hasArg("version") ? httpServer.arg("version") : "unknown";
        g_uploadFile = sdFirmwareOpenWrite(version.c_str());
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (g_uploadFile) g_uploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (g_uploadFile) g_uploadFile.close();
    }
}

} // namespace

void webApiRegisterRoutes() {
    httpServer.on("/api/status",   HTTP_GET,  handleStatus);
    httpServer.on("/api/children", HTTP_GET,  handleChildren);
    httpServer.on("/api/history",  HTTP_GET,  handleHistory);
    httpServer.on("/api/config",   HTTP_GET,  handleGetConfig);
    httpServer.on("/api/config",   HTTP_POST, handlePostConfig);
    httpServer.on("/api/firmware/upload", HTTP_POST, handleFirmwareUploadDone, handleFirmwareUploadChunk);
}
