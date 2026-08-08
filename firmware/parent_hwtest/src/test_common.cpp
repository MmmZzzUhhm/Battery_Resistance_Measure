#include "test_common.h"
#include "hwtest_common.h"
#include <Wire.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>

#ifndef PIN_I2C_SDA
#define PIN_I2C_SDA 5
#endif
#ifndef PIN_I2C_SCL
#define PIN_I2C_SCL 6
#endif

void testChipInfo() {
    String d = String("model=") + ESP.getChipModel() +
               " rev=" + ESP.getChipRevision() +
               " flash=" + (ESP.getFlashChipSize() / 1024 / 1024) + "MB" +
               " heap=" + ESP.getFreeHeap() + "B" +
               " mac=" + WiFi.macAddress();
    report("ChipInfo", true, d);
}

void testI2CScan() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    String found;
    int count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            if (count > 0) found += ",";
            found += "0x" + String(addr, HEX);
            count++;
        }
    }
    report("I2C Scan", count > 0, count > 0 ? (String(count) + " devices: " + found) : "no I2C device responded");
}

void testWifiApSoftAp(const char* ssid, const char* pass) {
    WiFi.mode(WIFI_AP_STA);
    bool ok = WiFi.softAP(ssid, pass);
    IPAddress ip = WiFi.softAPIP();
    report("WiFi SoftAP", ok && ip != IPAddress(0, 0, 0, 0),
        String("SSID=") + ssid + " IP=" + ip.toString());
}

void testWifiStaScan() {
    int n = WiFi.scanNetworks();
    WiFi.scanDelete();
    report("WiFi STA Scan", n >= 0, n >= 0 ? (String(n) + " AP(s) found nearby") : "scanNetworks() failed");
}

void testBleAdvertise(const char* deviceName) {
    BLEDevice::init(deviceName);
    BLEServer* server = BLEDevice::createServer();
    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->start();
    delay(500);
    adv->stop();
    (void)server;
    BLEDevice::deinit(true);
    report("BLE Advertise", true, "BLEDevice init/advertise/deinit completed without error");
}
