#include "child_wifi.h"
#include <ArduinoJson.h>
#include "protocol.h"
#include "http_server.h"
#include "rtc_clock.h"
#include "child_registry.h"
#include "sync_common.h"
#include "storage_sd.h"

namespace {

void handleSync() {
    if (!httpServer.hasArg("plain")) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, httpServer.arg("plain")) != DeserializationError::Ok) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }

    String childId   = doc["device_id"]  | "";
    String batteryId = doc["battery_id"] | childId;
    if (childId.length() == 0) {
        httpServer.send(400, "application/json", "{\"ok\":false,\"error\":\"device_id required\"}");
        return;
    }
    registryMarkSeen(childId);

    uint32_t ackSeq = 0;
    if (doc["measurements"].is<JsonArrayConst>()) {
        ackSeq = processIncomingMeasurements(childId, batteryId, doc["measurements"].as<JsonArrayConst>());
    }

    JsonDocument resp;
    resp["ok"]           = true;
    resp["server_epoch"] = (int64_t)rtcClock.nowEpoch();
    resp["ack_seq"]      = ackSeq;

    PendingChildUpdate pend;
    if (registryGetPending(childId, pend)) {
        if (pend.hasConfig) {
            JsonDocument cfgDoc;
            deserializeJson(cfgDoc, pend.configJson);
            resp["config"] = cfgDoc;
        }
        if (pend.hasOta && sdFirmwareExists(pend.otaVersion.c_str())) {
            JsonObject ota = resp["ota"].to<JsonObject>();
            ota["available"] = true;
            ota["version"]   = pend.otaVersion;
            ota["size"]      = (uint32_t)sdFirmwareSize(pend.otaVersion.c_str());
            ota["md5"]       = pend.otaMd5;
            ota["url"]       = String(WIFI_FIRMWARE_PATH) + "?version=" + pend.otaVersion;
        }
    }

    String out;
    serializeJson(resp, out);
    httpServer.send(200, "application/json", out);
}

void handleFirmware() {
    if (!httpServer.hasArg("version")) {
        httpServer.send(400, "text/plain", "version required");
        return;
    }
    String version = httpServer.arg("version");
    if (!sdFirmwareExists(version.c_str())) {
        httpServer.send(404, "text/plain", "not found");
        return;
    }
    File f = sdFirmwareOpenRead(version.c_str());
    if (!f) {
        httpServer.send(500, "text/plain", "open failed");
        return;
    }
    httpServer.streamFile(f, "application/octet-stream");
    f.close();
}

} // namespace

void childWifiRegisterRoutes() {
    httpServer.on(WIFI_SYNC_PATH, HTTP_POST, handleSync);
    httpServer.on(WIFI_FIRMWARE_PATH, HTTP_GET, handleFirmware);
}
