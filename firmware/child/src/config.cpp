#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include "protocol.h"

ChildConfig cfg = {};
static Preferences prefs;
static const char* NVS_NS = "childcfg";

const char* linkModeToStr(LinkMode m) { return m == LINK_WIFI ? "wifi" : "ble"; }
LinkMode    linkModeFromStr(const char* s) { return (s && strcmp(s, "wifi") == 0) ? LINK_WIFI : LINK_BLE; }

void configLoad() {
    prefs.begin(NVS_NS, true);
    cfg.provisioned = prefs.getBool("provisioned", false);
    strlcpy(cfg.device_id,   prefs.getString("device_id", "").c_str(),   sizeof(cfg.device_id));
    strlcpy(cfg.battery_id,  prefs.getString("battery_id", "").c_str(),  sizeof(cfg.battery_id));
    cfg.link_mode = linkModeFromStr(prefs.getString("link_mode", "ble").c_str());
    cfg.wake_interval_sec = prefs.getUInt("wake_ival", DEFAULT_WAKE_INTERVAL_SEC);
    cfg.i2c_addr  = (uint8_t)prefs.getUInt("i2c_addr", DEFAULT_I2C_ADDR);
    strlcpy(cfg.wifi_ssid, prefs.getString("wifi_ssid", "").c_str(), sizeof(cfg.wifi_ssid));
    strlcpy(cfg.wifi_pass, prefs.getString("wifi_pass", "").c_str(), sizeof(cfg.wifi_pass));
    strlcpy(cfg.fw_version, prefs.getString("fw_version", "1.0.0").c_str(), sizeof(cfg.fw_version));
    prefs.end();
    if (cfg.wake_interval_sec < MIN_WAKE_INTERVAL_SEC) cfg.wake_interval_sec = MIN_WAKE_INTERVAL_SEC;
}

void configSave() {
    prefs.begin(NVS_NS, false);
    prefs.putBool("provisioned", cfg.provisioned);
    prefs.putString("device_id",  cfg.device_id);
    prefs.putString("battery_id", cfg.battery_id);
    prefs.putString("link_mode",  linkModeToStr(cfg.link_mode));
    prefs.putUInt("wake_ival",    cfg.wake_interval_sec);
    prefs.putUInt("i2c_addr",     cfg.i2c_addr);
    prefs.putString("wifi_ssid",  cfg.wifi_ssid);
    prefs.putString("wifi_pass",  cfg.wifi_pass);
    prefs.putString("fw_version", cfg.fw_version);
    prefs.end();
}

bool configApplyJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

    bool changed = false;
    auto applyStr = [&](const char* key, char* dst, size_t n) {
        if (doc[key].is<const char*>()) {
            const char* v = doc[key];
            if (strncmp(dst, v, n) != 0) { strlcpy(dst, v, n); changed = true; }
        }
    };
    applyStr("device_id",  cfg.device_id,  sizeof(cfg.device_id));
    applyStr("battery_id", cfg.battery_id, sizeof(cfg.battery_id));
    applyStr("wifi_ssid",  cfg.wifi_ssid,  sizeof(cfg.wifi_ssid));
    applyStr("wifi_pass",  cfg.wifi_pass,  sizeof(cfg.wifi_pass));

    if (doc["link_mode"].is<const char*>()) {
        LinkMode m = linkModeFromStr(doc["link_mode"]);
        if (m != cfg.link_mode) { cfg.link_mode = m; changed = true; }
    }
    if (doc["wake_interval_sec"].is<uint32_t>()) {
        uint32_t v = doc["wake_interval_sec"];
        if (v < MIN_WAKE_INTERVAL_SEC) v = MIN_WAKE_INTERVAL_SEC;
        if (v != cfg.wake_interval_sec) { cfg.wake_interval_sec = v; changed = true; }
    }
    if (doc["i2c_addr"].is<uint8_t>()) {
        uint8_t v = doc["i2c_addr"];
        if (v != cfg.i2c_addr) { cfg.i2c_addr = v; changed = true; }
    }

    if (changed) {
        cfg.provisioned = true;
        configSave();
    }
    return changed;
}

String configToJson() {
    JsonDocument doc;
    doc["device_id"]         = cfg.device_id;
    doc["battery_id"]        = cfg.battery_id;
    doc["link_mode"]         = linkModeToStr(cfg.link_mode);
    doc["wake_interval_sec"] = cfg.wake_interval_sec;
    doc["i2c_addr"]          = cfg.i2c_addr;
    doc["wifi_ssid"]         = cfg.wifi_ssid;
    doc["fw_version"]        = cfg.fw_version;
    String out;
    serializeJson(doc, out);
    return out;
}
