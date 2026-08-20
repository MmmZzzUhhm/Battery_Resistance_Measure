#include "hwtest_config.h"
#include <Preferences.h>

HwtestConfig hwCfg = {};
static Preferences prefs;
static const char* NVS_NS = "hwtestcfg";

void hwtestConfigLoad() {
    prefs.begin(NVS_NS, true);
    strlcpy(hwCfg.device_name, prefs.getString("device_name", "HWTEST-BOARD").c_str(), sizeof(hwCfg.device_name));
    strlcpy(hwCfg.wifi_ssid, prefs.getString("wifi_ssid", "").c_str(), sizeof(hwCfg.wifi_ssid));
    strlcpy(hwCfg.wifi_pass, prefs.getString("wifi_pass", "").c_str(), sizeof(hwCfg.wifi_pass));
    hwCfg.use_static_ip = prefs.getBool("use_static", false);
    strlcpy(hwCfg.static_ip, prefs.getString("static_ip", "").c_str(), sizeof(hwCfg.static_ip));
    strlcpy(hwCfg.static_gateway, prefs.getString("static_gw", "").c_str(), sizeof(hwCfg.static_gateway));
    strlcpy(hwCfg.static_subnet, prefs.getString("static_sn", "255.255.255.0").c_str(), sizeof(hwCfg.static_subnet));
    strlcpy(hwCfg.static_dns, prefs.getString("static_dns", "").c_str(), sizeof(hwCfg.static_dns));
    hwCfg.last_role = (uint8_t)prefs.getUInt("last_role", 0);
    prefs.end();
}

void hwtestConfigSave() {
    prefs.begin(NVS_NS, false);
    prefs.putString("device_name", hwCfg.device_name);
    prefs.putString("wifi_ssid", hwCfg.wifi_ssid);
    prefs.putString("wifi_pass", hwCfg.wifi_pass);
    prefs.putBool("use_static", hwCfg.use_static_ip);
    prefs.putString("static_ip", hwCfg.static_ip);
    prefs.putString("static_gw", hwCfg.static_gateway);
    prefs.putString("static_sn", hwCfg.static_subnet);
    prefs.putString("static_dns", hwCfg.static_dns);
    prefs.putUInt("last_role", hwCfg.last_role);
    prefs.end();
}
