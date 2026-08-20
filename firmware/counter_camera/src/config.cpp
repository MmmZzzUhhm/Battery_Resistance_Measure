#include "config.h"
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>
#include "esp_camera.h" // FRAMESIZE_VGA等のシンボル定義 (数値を直接ハードコードしない)
#include "camera.h"
#include "light_control.h"

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
    cfg.led1_level = prefs.getInt("led1_lvl", LIGHT_LEVEL_MIN);
    cfg.led2_level = prefs.getInt("led2_lvl", LIGHT_LEVEL_MIN);
    cfg.jpeg_quality = prefs.getInt("jpeg_q", 12);
    cfg.frame_size   = prefs.getInt("frame_sz", (int)FRAMESIZE_VGA);
    cfg.image_rotation = prefs.getInt("img_rot", 0);
    cfg.img_brightness     = prefs.getInt("i_bri", 0);
    cfg.img_contrast       = prefs.getInt("i_con", 0);
    cfg.img_saturation     = prefs.getInt("i_sat", 0);
    cfg.img_sharpness      = prefs.getInt("i_shp", 0);
    cfg.img_special_effect = prefs.getInt("i_eff", 0);
    cfg.img_wb_mode        = prefs.getInt("i_wbm", 0);
    cfg.img_awb      = prefs.getBool("i_awb", true);
    cfg.img_awb_gain = prefs.getBool("i_awbg", true);
    cfg.img_aec      = prefs.getBool("i_aec", true);
    cfg.img_aec2     = prefs.getBool("i_aec2", false);
    cfg.img_ae_level  = prefs.getInt("i_ael", 0);
    cfg.img_aec_value = prefs.getInt("i_aecv", 300);
    cfg.img_agc      = prefs.getBool("i_agc", true);
    cfg.img_agc_gain    = prefs.getInt("i_agcg", 0);
    cfg.img_gainceiling = prefs.getInt("i_gc", 0);
    cfg.img_bpc     = prefs.getBool("i_bpc", true);
    cfg.img_wpc     = prefs.getBool("i_wpc", true);
    cfg.img_raw_gma = prefs.getBool("i_gma", true);
    cfg.img_lenc    = prefs.getBool("i_lenc", true);
    cfg.img_denoise = prefs.getInt("i_dns", 0);
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
    prefs.putInt("led1_lvl", cfg.led1_level);
    prefs.putInt("led2_lvl", cfg.led2_level);
    prefs.putInt("jpeg_q",        cfg.jpeg_quality);
    prefs.putInt("frame_sz",      cfg.frame_size);
    prefs.putInt("img_rot",       cfg.image_rotation);
    prefs.putInt("i_bri",  cfg.img_brightness);
    prefs.putInt("i_con",  cfg.img_contrast);
    prefs.putInt("i_sat",  cfg.img_saturation);
    prefs.putInt("i_shp",  cfg.img_sharpness);
    prefs.putInt("i_eff",  cfg.img_special_effect);
    prefs.putInt("i_wbm",  cfg.img_wb_mode);
    prefs.putBool("i_awb",  cfg.img_awb);
    prefs.putBool("i_awbg", cfg.img_awb_gain);
    prefs.putBool("i_aec",  cfg.img_aec);
    prefs.putBool("i_aec2", cfg.img_aec2);
    prefs.putInt("i_ael",   cfg.img_ae_level);
    prefs.putInt("i_aecv",  cfg.img_aec_value);
    prefs.putBool("i_agc",  cfg.img_agc);
    prefs.putInt("i_agcg",  cfg.img_agc_gain);
    prefs.putInt("i_gc",    cfg.img_gainceiling);
    prefs.putBool("i_bpc",  cfg.img_bpc);
    prefs.putBool("i_wpc",  cfg.img_wpc);
    prefs.putBool("i_gma",  cfg.img_raw_gma);
    prefs.putBool("i_lenc", cfg.img_lenc);
    prefs.putInt("i_dns",   cfg.img_denoise);
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
    doc["led1_level"]    = cfg.led1_level;
    doc["led2_level"]    = cfg.led2_level;
    doc["jpeg_quality"]  = cfg.jpeg_quality;
    doc["frame_size"]    = cfg.frame_size;
    doc["image_rotation"] = cfg.image_rotation;
    doc["img_brightness"]     = cfg.img_brightness;
    doc["img_contrast"]       = cfg.img_contrast;
    doc["img_saturation"]     = cfg.img_saturation;
    doc["img_sharpness"]      = cfg.img_sharpness;
    doc["img_special_effect"] = cfg.img_special_effect;
    doc["img_wb_mode"]        = cfg.img_wb_mode;
    doc["img_awb"]      = cfg.img_awb;
    doc["img_awb_gain"] = cfg.img_awb_gain;
    doc["img_aec"]      = cfg.img_aec;
    doc["img_aec2"]     = cfg.img_aec2;
    doc["img_ae_level"]  = cfg.img_ae_level;
    doc["img_aec_value"] = cfg.img_aec_value;
    doc["img_agc"]         = cfg.img_agc;
    doc["img_agc_gain"]    = cfg.img_agc_gain;
    doc["img_gainceiling"] = cfg.img_gainceiling;
    doc["img_bpc"]     = cfg.img_bpc;
    doc["img_wpc"]     = cfg.img_wpc;
    doc["img_raw_gma"] = cfg.img_raw_gma;
    doc["img_lenc"]    = cfg.img_lenc;
    doc["img_denoise"] = cfg.img_denoise;
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
    if (doc["led1_level"].is<int>()) {
        int lv = doc["led1_level"];
        if (lv >= LIGHT_LEVEL_OFF && lv <= LIGHT_LEVEL_MAX) cfg.led1_level = lv;
    }
    if (doc["led2_level"].is<int>()) {
        int lv = doc["led2_level"];
        if (lv >= LIGHT_LEVEL_OFF && lv <= LIGHT_LEVEL_MAX) cfg.led2_level = lv;
    }
    if (doc["jpeg_quality"].is<int>())           cfg.jpeg_quality = doc["jpeg_quality"];
    if (doc["frame_size"].is<int>())             cfg.frame_size   = doc["frame_size"];
    if (doc["image_rotation"].is<int>()) {
        int r = doc["image_rotation"];
        if (r == 0 || r == 90 || r == 180 || r == 270) cfg.image_rotation = r;
    }
    if (doc["img_brightness"].is<int>())     cfg.img_brightness = doc["img_brightness"];
    if (doc["img_contrast"].is<int>())       cfg.img_contrast = doc["img_contrast"];
    if (doc["img_saturation"].is<int>())     cfg.img_saturation = doc["img_saturation"];
    if (doc["img_sharpness"].is<int>())      cfg.img_sharpness = doc["img_sharpness"];
    if (doc["img_special_effect"].is<int>()) cfg.img_special_effect = doc["img_special_effect"];
    if (doc["img_wb_mode"].is<int>())        cfg.img_wb_mode = doc["img_wb_mode"];
    if (doc["img_awb"].is<bool>())      cfg.img_awb = doc["img_awb"];
    if (doc["img_awb_gain"].is<bool>()) cfg.img_awb_gain = doc["img_awb_gain"];
    if (doc["img_aec"].is<bool>())      cfg.img_aec = doc["img_aec"];
    if (doc["img_aec2"].is<bool>())     cfg.img_aec2 = doc["img_aec2"];
    if (doc["img_ae_level"].is<int>())  cfg.img_ae_level = doc["img_ae_level"];
    if (doc["img_aec_value"].is<int>()) cfg.img_aec_value = doc["img_aec_value"];
    if (doc["img_agc"].is<bool>())         cfg.img_agc = doc["img_agc"];
    if (doc["img_agc_gain"].is<int>())     cfg.img_agc_gain = doc["img_agc_gain"];
    if (doc["img_gainceiling"].is<int>())  cfg.img_gainceiling = doc["img_gainceiling"];
    if (doc["img_bpc"].is<bool>())     cfg.img_bpc = doc["img_bpc"];
    if (doc["img_wpc"].is<bool>())     cfg.img_wpc = doc["img_wpc"];
    if (doc["img_raw_gma"].is<bool>()) cfg.img_raw_gma = doc["img_raw_gma"];
    if (doc["img_lenc"].is<bool>())    cfg.img_lenc = doc["img_lenc"];
    if (doc["img_denoise"].is<int>())  cfg.img_denoise = doc["img_denoise"];
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
    cameraApplySensorSettings();
    return true;
}
