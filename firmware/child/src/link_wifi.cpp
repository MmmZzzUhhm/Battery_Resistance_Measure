#include "link_wifi.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "protocol.h"
#include "config.h"
#include "rtc_clock.h"
#include "pending_queue.h"
#include "ota_update.h"

namespace {

bool downloadAndApplyOta(const String& baseUrl, JsonObjectConst ota) {
    size_t size = ota["size"] | (size_t)0;
    const char* md5 = ota["md5"] | "";
    const char* path = ota["url"] | "";
    if (size == 0 || strlen(path) == 0) return false;

    HTTPClient http;
    http.begin(baseUrl + path);
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[OTA] firmware GET failed: %d\n", code);
        http.end();
        return false;
    }
    if (!otaBegin(size, md5)) {
        http.end();
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[512];
    size_t total = 0;
    uint32_t lastByteMs = millis();
    while (total < size && (millis() - lastByteMs) < 10000) {
        size_t avail = stream->available();
        if (avail == 0) { delay(2); continue; }
        size_t n = stream->readBytes(buf, min(avail, sizeof(buf)));
        if (n == 0) continue;
        if (!otaWrite(buf, n)) { http.end(); return false; }
        total += n;
        lastByteMs = millis();
    }
    http.end();

    if (total != size) {
        Serial.printf("[OTA] incomplete download %u/%u\n", (unsigned)total, (unsigned)size);
        otaAbort();
        return false;
    }
    return otaEnd();
}

} // namespace

bool wifiSyncSession(uint32_t timeoutMs) {
    if (strlen(cfg.wifi_ssid) == 0) {
        Serial.println("[WiFi] no SSID configured");
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
        delay(200);
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] connect timeout");
        WiFi.mode(WIFI_OFF);
        return false;
    }

    IPAddress parentIp = WiFi.gatewayIP();
    if (parentIp == IPAddress(0, 0, 0, 0)) parentIp.fromString(PARENT_AP_DEFAULT_IP);
    String baseUrl = "http://" + parentIp.toString();

    JsonDocument doc;
    doc["device_id"]  = cfg.device_id;
    doc["fw_version"] = cfg.fw_version;
    doc["battery_id"] = cfg.battery_id;
    doc["batt_mv"]    = 0; // 供給電圧ADC未実装 (将来拡張用プレースホルダ)
    doc["rtc_epoch"]  = (long)rtcClock.nowEpoch();
    // measArr はserializeJson()実行まで生存させる必要がある (serialized()は文字列を所有せず参照するため)
    String measArr = pendingQueue.toJsonArray(PENDING_QUEUE_MAX_RECORDS);
    doc["measurements"] = serialized(measArr);
    String body;
    serializeJson(doc, body);

    HTTPClient http;
    http.begin(baseUrl + WIFI_SYNC_PATH);
    http.addHeader("Content-Type", "application/json");
    int code = http.POST(body);

    bool ok = false;
    if (code == HTTP_CODE_OK) {
        String respBody = http.getString();
        JsonDocument resp;
        if (deserializeJson(resp, respBody) == DeserializationError::Ok && (resp["ok"] | false)) {
            ok = true;
            if (resp["server_epoch"].is<int64_t>()) rtcClock.adjustEpoch(resp["server_epoch"]);
            if (resp["ack_seq"].is<uint32_t>())      pendingQueue.ackSeq(resp["ack_seq"]);
            if (resp["config"].is<JsonObjectConst>()) {
                String cfgJson;
                serializeJson(resp["config"], cfgJson);
                configApplyJson(cfgJson);
            }
        }
        http.end();

        if (ok && (resp["ota"]["available"] | false)) {
            if (downloadAndApplyOta(baseUrl, resp["ota"].as<JsonObjectConst>())) {
                Serial.println("[OTA] rebooting into new firmware");
                delay(200);
                ESP.restart();
            }
        }
    } else {
        Serial.printf("[WiFi] sync POST failed: %d\n", code);
        http.end();
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return ok;
}
