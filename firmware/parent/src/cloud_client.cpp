#include "cloud_client.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <ArduinoJson.h>
#include "protocol.h"
#include "config.h"
#include "storage_sd.h"
#include "child_registry.h"

namespace {

bool doJsonRequest(const String& path, bool isPost, const String& body, String& outResp, int& outCode) {
    String url = String(cfg.portal_base_url) + path;
    HTTPClient http;
    NetworkClientSecure secureClient;
    WiFiClient plainClient;
    bool began = url.startsWith("https://")
        ? (secureClient.setInsecure(), http.begin(secureClient, url))
        : http.begin(plainClient, url);
    if (!began) return false;

    http.addHeader("Content-Type", "application/json");
    http.addHeader(CLOUD_API_KEY_HEADER, cfg.portal_api_key);
    outCode = isPost ? http.POST(body) : http.GET();
    if (outCode > 0) outResp = http.getString();
    http.end();
    return outCode > 0;
}

bool downloadFirmwareBlob(const String& version, size_t expectedSize) {
    char path[80];
    snprintf(path, sizeof(path), CLOUD_FW_BLOB_PATH_FMT, version.c_str());
    String url = String(cfg.portal_base_url) + path;

    HTTPClient http;
    NetworkClientSecure secureClient;
    WiFiClient plainClient;
    bool began = url.startsWith("https://")
        ? (secureClient.setInsecure(), http.begin(secureClient, url))
        : http.begin(plainClient, url);
    if (!began) return false;
    http.addHeader(CLOUD_API_KEY_HEADER, cfg.portal_api_key);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[Cloud] firmware blob GET failed: %d\n", code);
        http.end();
        return false;
    }

    File f = sdFirmwareOpenWrite(version.c_str());
    if (!f) {
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[512];
    size_t total = 0;
    uint32_t lastByteMs = millis();
    while (total < expectedSize && (millis() - lastByteMs) < 15000) {
        size_t avail = stream->available();
        if (avail == 0) { delay(2); continue; }
        size_t n = stream->readBytes(buf, min(avail, sizeof(buf)));
        if (n == 0) continue;
        f.write(buf, n);
        total += n;
        lastByteMs = millis();
    }
    f.close();
    http.end();

    bool ok = (total == expectedSize);
    Serial.printf("[Cloud] firmware %s downloaded %u/%u bytes (%s)\n",
        version.c_str(), (unsigned)total, (unsigned)expectedSize, ok ? "ok" : "incomplete");
    return ok;
}

void drainQueue() {
    for (int i = 0; i < 5; i++) {
        String path, childId, arr;
        if (!sdQueuePeek(path, childId, arr)) break;

        String body = "{\"measurements\":" + arr + "}";
        char epPath[80];
        snprintf(epPath, sizeof(epPath), CLOUD_MEASUREMENTS_PATH_FMT, cfg.gateway_id);

        String resp; int code;
        if (doJsonRequest(epPath, true, body, resp, code) && code == 200) {
            sdQueueRemove(path);
        } else {
            Serial.printf("[Cloud] measurements upload failed (code=%d), will retry later\n", code);
            break;
        }
    }
}

void pullConfig() {
    char epPath[80];
    snprintf(epPath, sizeof(epPath), CLOUD_CONFIG_PATH_FMT, cfg.gateway_id);
    String resp; int code;
    if (!doJsonRequest(epPath, false, "", resp, code) || code != 200) return;

    JsonDocument doc;
    if (deserializeJson(doc, resp) != DeserializationError::Ok) return;
    if (!doc["children"].is<JsonArrayConst>()) return;

    for (JsonObjectConst c : doc["children"].as<JsonArrayConst>()) {
        String cid = c["child_id"] | "";
        if (cid.length() == 0) continue;
        String cfgJson;
        serializeJson(c, cfgJson);
        registrySetPendingConfig(cid, cfgJson);
    }
}

void sendHeartbeat() {
    JsonDocument hb;
    hb["uptime_s"]   = (uint32_t)(millis() / 1000);
    hb["sd_free_kb"] = (uint32_t)(sdFreeBytes() / 1024);
    hb["fw_version"] = "1.0.0";
    JsonArray seen = hb["children_seen"].to<JsonArray>();
    for (int i = 0; i < registrySeenCount(); i++) seen.add(registrySeenChildId(i));

    String body;
    serializeJson(hb, body);
    char epPath[80];
    snprintf(epPath, sizeof(epPath), CLOUD_HEARTBEAT_PATH_FMT, cfg.gateway_id);
    String resp; int code;
    doJsonRequest(epPath, true, body, resp, code);
}

void checkPendingOta() {
    for (int i = 0; i < registrySeenCount(); i++) {
        String cid = registrySeenChildId(i);
        char epPath[128];
        snprintf(epPath, sizeof(epPath), CLOUD_FW_PENDING_PATH_FMT, cfg.gateway_id, cid.c_str());

        String resp; int code;
        if (!doJsonRequest(epPath, false, "", resp, code) || code != 200) continue;

        JsonDocument doc;
        if (deserializeJson(doc, resp) != DeserializationError::Ok) continue;
        if (!(doc["available"] | false)) continue;

        String version = doc["version"] | "";
        size_t size     = doc["size"] | (size_t)0;
        String md5      = doc["md5"] | "";
        if (version.length() == 0 || size == 0) continue;

        if (!sdFirmwareExists(version.c_str())) {
            if (!downloadFirmwareBlob(version, size)) continue;
        }
        registrySetPendingOta(cid, version, size, md5);
    }
}

} // namespace

void cloudSyncTick() {
    if (strlen(cfg.portal_base_url) == 0) return;
    if (WiFi.status() != WL_CONNECTED) return;

    drainQueue();
    pullConfig();
    sendHeartbeat();
    checkPendingOta();
}
