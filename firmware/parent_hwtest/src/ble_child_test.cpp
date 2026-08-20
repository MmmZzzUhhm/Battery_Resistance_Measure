#include "ble_child_test.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>

// firmware/common/include/protocol.h と同じ値 (子機/親機と共通のBLE GATT UUID)。
#define BLE_SVC_UUID        "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0000"
#define BLE_CHR_STATUS_UUID "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0001"

namespace {

class ListScanCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    std::vector<BleChildAdv>* out;
    BLEUUID svcUuid;
    explicit ListScanCallbacks(std::vector<BleChildAdv>* results) : out(results), svcUuid(BLE_SVC_UUID) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (!advertisedDevice.haveServiceUUID() || !advertisedDevice.isAdvertisingService(svcUuid)) return;
        for (auto& existing : *out) {
            if (existing.address == advertisedDevice.getAddress().toString().c_str()) return;  // 重複除外
        }
        BleChildAdv adv;
        adv.name    = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "(名前なし)";
        adv.address = advertisedDevice.getAddress().toString().c_str();
        adv.rssi    = advertisedDevice.haveRSSI() ? advertisedDevice.getRSSI() : 0;
        out->push_back(adv);
    }
};

class TargetScanCallbacks : public BLEAdvertisedDeviceCallbacks {
public:
    String targetName;
    BLEUUID svcUuid;
    volatile bool found = false;
    BLEAdvertisedDevice* result = nullptr;

    explicit TargetScanCallbacks(const String& name) : targetName(name), svcUuid(BLE_SVC_UUID) {}

    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (found) return;
        if (!advertisedDevice.haveServiceUUID() || !advertisedDevice.isAdvertisingService(svcUuid)) return;
        if (targetName.length() > 0) {
            String advName = advertisedDevice.haveName() ? advertisedDevice.getName().c_str() : "";
            if (advName != targetName) return;
        }
        delete result;
        result = new BLEAdvertisedDevice(advertisedDevice);
        found = true;
        BLEDevice::getScan()->stop();
    }
};

}  // namespace

std::vector<BleChildAdv> bleScanChildren(uint32_t scanSeconds) {
    std::vector<BleChildAdv> results;
    ListScanCallbacks cb(&results);

    BLEDevice::init("");
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&cb);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->start(scanSeconds, false);
    scan->clearResults();
    BLEDevice::deinit(false);

    return results;
}

BleChildStatus bleCheckChildStatus(const String& targetName, uint32_t scanSeconds) {
    BleChildStatus st = {};
    st.ok = false;

    BLEDevice::init("");
    TargetScanCallbacks cb(targetName);
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&cb);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->start(scanSeconds, false);
    scan->clearResults();

    if (!cb.found || !cb.result) {
        st.error = targetName.length() > 0 ? ("子機 \"" + targetName + "\" が見つかりません") : "子機が見つかりません";
        BLEDevice::deinit(false);
        return st;
    }

    BLEClient* client = BLEDevice::createClient();
    bool connected = client->connect(cb.result);
    if (!connected) {
        st.error = "接続に失敗しました";
        delete client;
        BLEDevice::deinit(false);
        return st;
    }

    BLEUUID svcUuid(BLE_SVC_UUID);
    BLERemoteService* svc = client->getService(svcUuid);
    if (!svc) {
        st.error = "サービスが見つかりません";
    } else {
        auto chStatus = svc->getCharacteristic(BLE_CHR_STATUS_UUID);
        if (!chStatus) {
            st.error = "STATUS特性が見つかりません";
        } else {
            JsonDocument doc;
            if (deserializeJson(doc, chStatus->readValue()) != DeserializationError::Ok) {
                st.error = "STATUS読み取り値のJSON解析に失敗しました";
            } else {
                st.device_id     = String((const char*)(doc["device_id"] | ""));
                st.fw_version    = String((const char*)(doc["fw_version"] | ""));
                st.battery_id    = String((const char*)(doc["battery_id"] | ""));
                st.rtc_epoch     = doc["rtc_epoch"] | 0;
                st.pending_count = doc["pending_count"] | 0;
                st.ok = st.device_id.length() > 0;
                if (!st.ok) st.error = "device_idが空です";
            }
        }
    }

    client->disconnect();
    delete client;
    BLEDevice::deinit(false);
    return st;
}
