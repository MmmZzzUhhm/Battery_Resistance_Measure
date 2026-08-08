#include "child_ble.h"
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include "protocol.h"
#include "rtc_clock.h"
#include "child_registry.h"
#include "sync_common.h"
#include "storage_sd.h"

namespace {

BLEUUID g_svcUuid(BLE_SVC_UUID);
volatile bool g_foundDevice = false;
BLEAdvertisedDevice* g_foundAdv = nullptr;

class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (g_foundDevice) return;
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(g_svcUuid)) {
            delete g_foundAdv;
            g_foundAdv = new BLEAdvertisedDevice(advertisedDevice);
            g_foundDevice = true;
            BLEDevice::getScan()->stop();
        }
    }
};

ScanCallbacks g_scanCb;

void writeBytes(BLERemoteCharacteristic* ch, const uint8_t* data, size_t len, bool withResponse) {
    if (!ch) return;
    ch->writeValue(const_cast<uint8_t*>(data), len, withResponse);
}

void writeJson(BLERemoteCharacteristic* ch, const String& json) {
    writeBytes(ch, (const uint8_t*)json.c_str(), json.length(), true);
}

} // namespace

bool bleScanAndSyncOnce(uint32_t scanSeconds) {
    g_foundDevice = false;

    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(&g_scanCb);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(99);
    scan->start(scanSeconds, false);
    scan->clearResults();

    if (!g_foundDevice || !g_foundAdv) return false;

    BLEClient* client = BLEDevice::createClient();
    bool connected = client->connect(g_foundAdv);
    if (!connected) {
        Serial.println("[BLE] connect failed");
        delete client;
        return false;
    }

    bool result = false;
    BLERemoteService* svc = client->getService(g_svcUuid);
    if (svc) {
        auto chStatus  = svc->getCharacteristic(BLE_CHR_STATUS_UUID);
        auto chTime    = svc->getCharacteristic(BLE_CHR_TIME_UUID);
        auto chData    = svc->getCharacteristic(BLE_CHR_DATA_UUID);
        auto chDataAck = svc->getCharacteristic(BLE_CHR_DATA_ACK_UUID);
        auto chConfig  = svc->getCharacteristic(BLE_CHR_CONFIG_UUID);
        auto chOtaCtrl = svc->getCharacteristic(BLE_CHR_OTA_CTRL_UUID);
        auto chOtaData = svc->getCharacteristic(BLE_CHR_OTA_DATA_UUID);

        if (chStatus && chTime && chData && chDataAck) {
            JsonDocument statusDoc;
            deserializeJson(statusDoc, chStatus->readValue());
            String childId   = statusDoc["device_id"]  | "";
            String batteryId = statusDoc["battery_id"] | childId;

            if (childId.length() > 0) {
                registryMarkSeen(childId);

                int64_t nowEpoch = rtcClock.nowEpoch();
                writeBytes(chTime, (const uint8_t*)&nowEpoch, sizeof(nowEpoch), true);

                PendingChildUpdate pend;
                bool hasPending = registryGetPending(childId, pend);
                if (hasPending && pend.hasConfig && chConfig) {
                    writeJson(chConfig, pend.configJson);
                }

                uint32_t totalRecords = 0;
                for (int i = 0; i < 8; i++) {
                    JsonDocument dataDoc;
                    if (deserializeJson(dataDoc, chData->readValue()) != DeserializationError::Ok) break;
                    JsonArrayConst arr = dataDoc.as<JsonArrayConst>();
                    size_t n = arr.size();
                    if (n == 0) break;

                    uint32_t maxSeq = processIncomingMeasurements(childId, batteryId, arr);
                    JsonDocument ackDoc;
                    ackDoc["ack_seq"] = maxSeq;
                    String ackJson;
                    serializeJson(ackDoc, ackJson);
                    writeJson(chDataAck, ackJson);

                    totalRecords += n;
                    if (n < BLE_DATA_BATCH_MAX) break;
                }

                if (hasPending && pend.hasOta && chOtaCtrl && chOtaData &&
                    sdFirmwareExists(pend.otaVersion.c_str())) {
                    File f = sdFirmwareOpenRead(pend.otaVersion.c_str());
                    if (f) {
                        JsonDocument beginDoc;
                        beginDoc["cmd"]  = "begin";
                        beginDoc["size"] = (uint32_t)f.size();
                        beginDoc["md5"]  = pend.otaMd5;
                        String beginJson;
                        serializeJson(beginDoc, beginJson);
                        writeJson(chOtaCtrl, beginJson);

                        uint8_t buf[200];
                        size_t total = 0;
                        while (f.available()) {
                            size_t n = f.read(buf, sizeof(buf));
                            writeBytes(chOtaData, buf, n, false);
                            total += n;
                            delay(5);
                        }
                        f.close();
                        writeJson(chOtaCtrl, "{\"cmd\":\"end\"}");
                        registryClearPendingOta(childId);
                        Serial.printf("[BLE] OTA pushed to %s (%u bytes)\n", childId.c_str(), (unsigned)total);
                    }
                }

                Serial.printf("[BLE] synced %s: records=%u\n", childId.c_str(), (unsigned)totalRecords);
                result = true;
            }
        }
    }

    client->disconnect();
    delete client;
    return result;
}
