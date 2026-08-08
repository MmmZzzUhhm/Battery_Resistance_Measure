#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "protocol.h"

ParentConfig cfg = {};
static Preferences prefs;
static const char* NVS_NS = "parentcfg";

static String defaultGatewayId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[16];
    snprintf(buf, sizeof(buf), "gw-%02x%02x%02x", mac[3], mac[4], mac[5]);
    return String(buf);
}

void configLoad() {
    prefs.begin(NVS_NS, true);
    strlcpy(cfg.gateway_id, prefs.getString("gateway_id", defaultGatewayId()).c_str(), sizeof(cfg.gateway_id));
    strlcpy(cfg.wifi_sta_ssid, prefs.getString("sta_ssid", "").c_str(), sizeof(cfg.wifi_sta_ssid));
    strlcpy(cfg.wifi_sta_pass, prefs.getString("sta_pass", "").c_str(), sizeof(cfg.wifi_sta_pass));
    String defAp = String(PARENT_AP_SSID_PREFIX) + cfg.gateway_id;
    strlcpy(cfg.ap_ssid, prefs.getString("ap_ssid", defAp).c_str(), sizeof(cfg.ap_ssid));
    strlcpy(cfg.ap_pass, prefs.getString("ap_pass", "battgateway").c_str(), sizeof(cfg.ap_pass));
    strlcpy(cfg.portal_base_url, prefs.getString("portal_url", "").c_str(), sizeof(cfg.portal_base_url));
    strlcpy(cfg.portal_api_key, prefs.getString("portal_key", "").c_str(), sizeof(cfg.portal_api_key));
    cfg.cloud_sync_interval_sec = prefs.getUInt("cloud_ival", 300);
    cfg.ble_scan_interval_sec   = prefs.getUInt("ble_ival", 15);
    prefs.end();
}

void configSave() {
    prefs.begin(NVS_NS, false);
    prefs.putString("gateway_id", cfg.gateway_id);
    prefs.putString("sta_ssid",   cfg.wifi_sta_ssid);
    prefs.putString("sta_pass",   cfg.wifi_sta_pass);
    prefs.putString("ap_ssid",    cfg.ap_ssid);
    prefs.putString("ap_pass",    cfg.ap_pass);
    prefs.putString("portal_url", cfg.portal_base_url);
    prefs.putString("portal_key", cfg.portal_api_key);
    prefs.putUInt("cloud_ival",   cfg.cloud_sync_interval_sec);
    prefs.putUInt("ble_ival",     cfg.ble_scan_interval_sec);
    prefs.end();
}

String configToJson() {
    JsonDocument doc;
    doc["gateway_id"]     = cfg.gateway_id;
    doc["wifi_sta_ssid"]  = cfg.wifi_sta_ssid;
    doc["ap_ssid"]        = cfg.ap_ssid;
    doc["portal_base_url"]= cfg.portal_base_url;
    doc["cloud_sync_interval_sec"] = cfg.cloud_sync_interval_sec;
    doc["ble_scan_interval_sec"]   = cfg.ble_scan_interval_sec;
    doc["portal_api_key_set"] = strlen(cfg.portal_api_key) > 0;
    String out;
    serializeJson(doc, out);
    return out;
}

bool configApplyJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

    if (doc["gateway_id"].is<const char*>())      strlcpy(cfg.gateway_id, doc["gateway_id"], sizeof(cfg.gateway_id));
    if (doc["wifi_sta_ssid"].is<const char*>())    strlcpy(cfg.wifi_sta_ssid, doc["wifi_sta_ssid"], sizeof(cfg.wifi_sta_ssid));
    if (doc["wifi_sta_pass"].is<const char*>())    strlcpy(cfg.wifi_sta_pass, doc["wifi_sta_pass"], sizeof(cfg.wifi_sta_pass));
    if (doc["ap_ssid"].is<const char*>())          strlcpy(cfg.ap_ssid, doc["ap_ssid"], sizeof(cfg.ap_ssid));
    if (doc["ap_pass"].is<const char*>())          strlcpy(cfg.ap_pass, doc["ap_pass"], sizeof(cfg.ap_pass));
    if (doc["portal_base_url"].is<const char*>())  strlcpy(cfg.portal_base_url, doc["portal_base_url"], sizeof(cfg.portal_base_url));
    if (doc["portal_api_key"].is<const char*>())   strlcpy(cfg.portal_api_key, doc["portal_api_key"], sizeof(cfg.portal_api_key));
    if (doc["cloud_sync_interval_sec"].is<uint32_t>()) cfg.cloud_sync_interval_sec = doc["cloud_sync_interval_sec"];
    if (doc["ble_scan_interval_sec"].is<uint32_t>())   cfg.ble_scan_interval_sec   = doc["ble_scan_interval_sec"];

    configSave();
    return true;
}
