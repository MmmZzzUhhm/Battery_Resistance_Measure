#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

CounterCameraConfig cfg = {};
static Preferences prefs;
static const char* NVS_NS = "camcfg";

static String defaultDeviceId() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[16];
    snprintf(buf, sizeof(buf), "cam-%02x%02x%02x", mac[3], mac[4], mac[5]);
    return String(buf);
}

void configLoad() {
    prefs.begin(NVS_NS, true);
    strlcpy(cfg.device_id, prefs.getString("device_id", defaultDeviceId()).c_str(), sizeof(cfg.device_id));
    strlcpy(cfg.wifi_sta_ssid, prefs.getString("sta_ssid", "").c_str(), sizeof(cfg.wifi_sta_ssid));
    strlcpy(cfg.wifi_sta_pass, prefs.getString("sta_pass", "").c_str(), sizeof(cfg.wifi_sta_pass));
    String defAp = String("COUNTERCAM-") + cfg.device_id;
    strlcpy(cfg.ap_ssid, prefs.getString("ap_ssid", defAp).c_str(), sizeof(cfg.ap_ssid));
    strlcpy(cfg.ap_pass, prefs.getString("ap_pass", "countercam").c_str(), sizeof(cfg.ap_pass));
    cfg.jpeg_quality = prefs.getInt("jpeg_q", 12);
    cfg.frame_size   = prefs.getInt("frame_sz", 8); // 既定 VGA
    cfg.use_static_ip = prefs.getBool("use_static", false);
    strlcpy(cfg.static_ip,      prefs.getString("static_ip", "").c_str(),      sizeof(cfg.static_ip));
    strlcpy(cfg.static_gateway, prefs.getString("static_gw", "").c_str(),      sizeof(cfg.static_gateway));
    strlcpy(cfg.static_subnet,  prefs.getString("static_sn", "255.255.255.0").c_str(), sizeof(cfg.static_subnet));
    strlcpy(cfg.static_dns,     prefs.getString("static_dns", "").c_str(),     sizeof(cfg.static_dns));
    strlcpy(cfg.ntp_server1, prefs.getString("ntp1", "pool.ntp.org").c_str(),   sizeof(cfg.ntp_server1));
    strlcpy(cfg.ntp_server2, prefs.getString("ntp2", "time.google.com").c_str(), sizeof(cfg.ntp_server2));
    strlcpy(cfg.timezone_tz, prefs.getString("tz", "UTC0").c_str(), sizeof(cfg.timezone_tz));
    prefs.end();
    applyTimezoneEnv();
}

void applyTimezoneEnv() {
    setenv("TZ", strlen(cfg.timezone_tz) > 0 ? cfg.timezone_tz : "UTC0", 1);
    tzset();
}

void configSave() {
    prefs.begin(NVS_NS, false);
    prefs.putString("device_id",  cfg.device_id);
    prefs.putString("sta_ssid",   cfg.wifi_sta_ssid);
    prefs.putString("sta_pass",   cfg.wifi_sta_pass);
    prefs.putString("ap_ssid",    cfg.ap_ssid);
    prefs.putString("ap_pass",    cfg.ap_pass);
    prefs.putInt("jpeg_q",        cfg.jpeg_quality);
    prefs.putInt("frame_sz",      cfg.frame_size);
    prefs.putBool("use_static",   cfg.use_static_ip);
    prefs.putString("static_ip",  cfg.static_ip);
    prefs.putString("static_gw",  cfg.static_gateway);
    prefs.putString("static_sn",  cfg.static_subnet);
    prefs.putString("static_dns", cfg.static_dns);
    prefs.putString("ntp1",       cfg.ntp_server1);
    prefs.putString("ntp2",       cfg.ntp_server2);
    prefs.putString("tz",         cfg.timezone_tz);
    prefs.end();
}

String configToJson() {
    JsonDocument doc;
    doc["device_id"]     = cfg.device_id;
    doc["wifi_sta_ssid"] = cfg.wifi_sta_ssid;
    doc["ap_ssid"]       = cfg.ap_ssid;
    doc["jpeg_quality"]  = cfg.jpeg_quality;
    doc["frame_size"]    = cfg.frame_size;
    doc["use_static_ip"]   = cfg.use_static_ip;
    doc["static_ip"]       = cfg.static_ip;
    doc["static_gateway"]  = cfg.static_gateway;
    doc["static_subnet"]   = cfg.static_subnet;
    doc["static_dns"]      = cfg.static_dns;
    doc["ntp_server1"] = cfg.ntp_server1;
    doc["ntp_server2"] = cfg.ntp_server2;
    doc["timezone_tz"] = cfg.timezone_tz;
    String out;
    serializeJson(doc, out);
    return out;
}

bool configApplyJson(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

    if (doc["device_id"].is<const char*>())     strlcpy(cfg.device_id, doc["device_id"], sizeof(cfg.device_id));
    if (doc["wifi_sta_ssid"].is<const char*>())  strlcpy(cfg.wifi_sta_ssid, doc["wifi_sta_ssid"], sizeof(cfg.wifi_sta_ssid));
    if (doc["wifi_sta_pass"].is<const char*>())  strlcpy(cfg.wifi_sta_pass, doc["wifi_sta_pass"], sizeof(cfg.wifi_sta_pass));
    if (doc["ap_ssid"].is<const char*>())        strlcpy(cfg.ap_ssid, doc["ap_ssid"], sizeof(cfg.ap_ssid));
    if (doc["ap_pass"].is<const char*>())        strlcpy(cfg.ap_pass, doc["ap_pass"], sizeof(cfg.ap_pass));
    if (doc["jpeg_quality"].is<int>())           cfg.jpeg_quality = doc["jpeg_quality"];
    if (doc["frame_size"].is<int>())             cfg.frame_size   = doc["frame_size"];
    if (doc["use_static_ip"].is<bool>())         cfg.use_static_ip = doc["use_static_ip"];
    if (doc["static_ip"].is<const char*>())      strlcpy(cfg.static_ip, doc["static_ip"], sizeof(cfg.static_ip));
    if (doc["static_gateway"].is<const char*>()) strlcpy(cfg.static_gateway, doc["static_gateway"], sizeof(cfg.static_gateway));
    if (doc["static_subnet"].is<const char*>())  strlcpy(cfg.static_subnet, doc["static_subnet"], sizeof(cfg.static_subnet));
    if (doc["static_dns"].is<const char*>())     strlcpy(cfg.static_dns, doc["static_dns"], sizeof(cfg.static_dns));
    if (doc["ntp_server1"].is<const char*>()) strlcpy(cfg.ntp_server1, doc["ntp_server1"], sizeof(cfg.ntp_server1));
    if (doc["ntp_server2"].is<const char*>()) strlcpy(cfg.ntp_server2, doc["ntp_server2"], sizeof(cfg.ntp_server2));
    if (doc["timezone_tz"].is<const char*>()) strlcpy(cfg.timezone_tz, doc["timezone_tz"], sizeof(cfg.timezone_tz));

    configSave();
    applyTimezoneEnv();
    return true;
}
