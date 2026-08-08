#include "link_ble.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <ArduinoJson.h>
#include "protocol.h"
#include "config.h"
#include "rtc_clock.h"
#include "pending_queue.h"
#include "ota_update.h"

namespace {

volatile bool s_connected     = false;
volatile bool s_everConnected = false;
volatile bool s_otaRebootFlag = false;

size_t   s_otaExpected = 0;
size_t   s_otaReceived = 0;
bool     s_otaActive   = false;

BLEServer*         s_server = nullptr;
BLECharacteristic* s_chStatus  = nullptr;
BLECharacteristic* s_chTime    = nullptr;
BLECharacteristic* s_chData    = nullptr;
BLECharacteristic* s_chDataAck = nullptr;
BLECharacteristic* s_chConfig  = nullptr;
BLECharacteristic* s_chOtaCtrl = nullptr;
BLECharacteristic* s_chOtaData = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        s_connected = true;
        s_everConnected = true;
        Serial.println("[BLE] parent connected");
    }
    void onDisconnect(BLEServer*) override {
        s_connected = false;
        Serial.println("[BLE] parent disconnected");
    }
};

class StatusCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* c) override {
        JsonDocument doc;
        doc["device_id"]      = cfg.device_id;
        doc["fw_version"]     = cfg.fw_version;
        doc["battery_id"]     = cfg.battery_id;
        doc["batt_mv"]        = 0; // 供給電圧ADC未実装 (将来拡張用プレースホルダ)
        doc["rtc_epoch"]      = (long)rtcClock.nowEpoch();
        doc["pending_count"]  = (uint32_t)pendingQueue.size();
        String out;
        serializeJson(doc, out);
        c->setValue(out.c_str());
    }
};

class TimeCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        String v = c->getValue();
        if (v.length() != 8) return;
        int64_t epoch;
        memcpy(&epoch, v.c_str(), 8);
        rtcClock.adjustEpoch(epoch);
        Serial.printf("[BLE] time synced: %lld\n", (long long)epoch);
    }
};

class DataCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* c) override {
        String json = pendingQueue.toJsonArray(BLE_DATA_BATCH_MAX);
        c->setValue(json.c_str());
    }
};

class DataAckCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        JsonDocument doc;
        if (deserializeJson(doc, c->getValue()) != DeserializationError::Ok) return;
        if (doc["ack_seq"].is<uint32_t>()) {
            pendingQueue.ackSeq(doc["ack_seq"]);
        }
    }
};

class ConfigCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* c) override {
        c->setValue(configToJson().c_str());
    }
    void onWrite(BLECharacteristic* c) override {
        configApplyJson(c->getValue());
    }
};

class OtaCtrlCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        JsonDocument doc;
        if (deserializeJson(doc, c->getValue()) != DeserializationError::Ok) return;
        const char* cmd = doc["cmd"] | "";
        if (strcmp(cmd, "begin") == 0) {
            s_otaExpected = doc["size"] | 0;
            const char* md5 = doc["md5"] | "";
            s_otaActive = otaBegin(s_otaExpected, md5);
            s_otaReceived = 0;
        } else if (strcmp(cmd, "end") == 0) {
            if (s_otaActive && s_otaReceived == s_otaExpected) {
                s_otaRebootFlag = otaEnd();
            } else {
                Serial.println("[OTA] size mismatch at end, aborting");
                otaAbort();
            }
            s_otaActive = false;
        } else if (strcmp(cmd, "abort") == 0) {
            otaAbort();
            s_otaActive = false;
        }
    }
};

class OtaDataCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* c) override {
        if (!s_otaActive) return;
        // OTAのバイナリはNUL(0x00)を含みうる。String::length()は内部バッファ長を保持しているため
        // (strlen()に依存しないため) 埋め込みNULがあっても正しいバイト数として扱える。
        String v = c->getValue();
        if (otaWrite((const uint8_t*)v.c_str(), v.length())) {
            s_otaReceived += v.length();
        } else {
            s_otaActive = false;
        }
    }
};

ServerCallbacks   s_serverCb;
StatusCallbacks   s_statusCb;
TimeCallbacks     s_timeCb;
DataCallbacks     s_dataCb;
DataAckCallbacks  s_dataAckCb;
ConfigCallbacks   s_configCb;
OtaCtrlCallbacks  s_otaCtrlCb;
OtaDataCallbacks  s_otaDataCb;

} // namespace

bool bleSyncSession(uint32_t timeoutMs) {
    s_connected = false;
    s_everConnected = false;
    s_otaRebootFlag = false;

    BLEDevice::init(cfg.device_id);
    s_server = BLEDevice::createServer();
    s_server->setCallbacks(&s_serverCb);

    BLEService* svc = s_server->createService(BLE_SVC_UUID);

    s_chStatus = svc->createCharacteristic(BLE_CHR_STATUS_UUID, BLECharacteristic::PROPERTY_READ);
    s_chStatus->setCallbacks(&s_statusCb);

    s_chTime = svc->createCharacteristic(BLE_CHR_TIME_UUID, BLECharacteristic::PROPERTY_WRITE);
    s_chTime->setCallbacks(&s_timeCb);

    s_chData = svc->createCharacteristic(BLE_CHR_DATA_UUID, BLECharacteristic::PROPERTY_READ);
    s_chData->setCallbacks(&s_dataCb);

    s_chDataAck = svc->createCharacteristic(BLE_CHR_DATA_ACK_UUID, BLECharacteristic::PROPERTY_WRITE);
    s_chDataAck->setCallbacks(&s_dataAckCb);

    s_chConfig = svc->createCharacteristic(BLE_CHR_CONFIG_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    s_chConfig->setCallbacks(&s_configCb);

    s_chOtaCtrl = svc->createCharacteristic(BLE_CHR_OTA_CTRL_UUID, BLECharacteristic::PROPERTY_WRITE);
    s_chOtaCtrl->setCallbacks(&s_otaCtrlCb);

    s_chOtaData = svc->createCharacteristic(BLE_CHR_OTA_DATA_UUID,
        BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_WRITE);
    s_chOtaData->setCallbacks(&s_otaDataCb);

    svc->start();

    BLEAdvertising* adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SVC_UUID);
    adv->setScanResponse(true);
    adv->start();

    Serial.printf("[BLE] advertising as \"%s\" for up to %u ms\n", cfg.device_id, (unsigned)timeoutMs);

    uint32_t t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (s_everConnected && !s_connected) break; // 一度接続後、切断されたらセッション終了
        delay(50);
    }

    adv->stop();
    if (s_connected) s_server->disconnect(0);

    bool result = s_everConnected;

    // BLEスタックを解放してからDeep Sleepへ (消費電力低減)
    BLEDevice::deinit(true);

    if (s_otaRebootFlag) {
        Serial.println("[OTA] rebooting into new firmware");
        delay(200);
        ESP.restart();
    }

    return result;
}
