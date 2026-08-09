/*
 * IMD-2000 (InnoSenT 24GHz FMCWドップラーセンサ, UART接続) 動作確認
 *
 * プロトコルは M5ATOMS3_TrailCamera3_Ver1.0/lib/IMD2000 (実機で動作確認済み) の
 * サブセットを再実装。HWテストではStartAcquisition送信→GetTargetList応答の
 * 受信確認のみ行う(ターゲット0件でも通信自体が成立していればOK)。
 *
 * 送信フレーム: [0x68][LEN][LEN][0x68][ADDR][0x01][CMD][payload...][CHK][0x16]
 * 受信フレーム: [0x68]=ACK または [0xA2]=ターゲットリスト
 * 電源投入後 5〜7秒 でデータ送信を開始する仕様のため、起動待機を挟む。
 */
#include "test_imd2000.h"
#include "hwtest_common.h"
#include <Arduino.h>

#ifndef PIN_IMD2000_TX
#define PIN_IMD2000_TX 43
#endif
#ifndef PIN_IMD2000_RX
#define PIN_IMD2000_RX 44
#endif

#define IMD2000_BAUDRATE    256000
#define IMD2000_ADDRESS     0x64
#define IMD2000_CTRL        0x01
#define IMD2000_CMD_START   0xD1
#define IMD2000_CMD_GETLIST 0xDA
#define IMD2000_ETX         0x16
#define IMD2000_MAX_TARGETS 20

namespace {

HardwareSerial ImdSerial(1);

void sendFrame(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen) {
    uint8_t buf[16];
    int idx = 0;
    uint8_t len = 3 + payloadLen;
    buf[idx++] = 0x68;
    buf[idx++] = len;
    buf[idx++] = len;
    buf[idx++] = 0x68;
    buf[idx++] = IMD2000_ADDRESS;
    buf[idx++] = IMD2000_CTRL;
    buf[idx++] = cmd;
    uint8_t cs = IMD2000_ADDRESS + IMD2000_CTRL + cmd;
    for (uint8_t i = 0; i < payloadLen; i++) {
        buf[idx++] = payload[i];
        cs += payload[i];
    }
    buf[idx++] = cs;
    buf[idx++] = IMD2000_ETX;
    ImdSerial.write(buf, idx);
    ImdSerial.flush();
}

bool readByte(uint8_t& b, unsigned long timeoutMs) {
    unsigned long t0 = millis();
    while (millis() - t0 < timeoutMs) {
        if (ImdSerial.available() > 0) {
            b = (uint8_t)ImdSerial.read();
            return true;
        }
    }
    return false;
}

// 0x68(ACK)フレーム または 0xA2(ターゲットリスト)フレームを1つ受信する
bool receiveFrame(uint8_t* outBuf, uint16_t& outLen, uint16_t maxLen, unsigned long timeoutMs) {
    outLen = 0;
    uint8_t stx = 0;
    unsigned long t0 = millis();
    bool found = false;
    while (millis() - t0 < timeoutMs) {
        if (ImdSerial.available() > 0) {
            stx = (uint8_t)ImdSerial.read();
            if (stx == 0x68 || stx == 0xA2) {
                found = true;
                break;
            }
        }
    }
    if (!found) return false;

    outBuf[0] = stx;
    uint16_t idx = 1;

    if (stx == 0x68) {
        // [0x68][LEN][LEN][0x68][body(LEN)][CHK][0x16]
        if (!readByte(outBuf[idx++], 100)) return false;
        uint8_t frameLen = outBuf[1];
        uint16_t remaining = (uint16_t)frameLen + 4;
        for (uint16_t i = 0; i < remaining && idx < maxLen; i++) {
            if (!readByte(outBuf[idx++], 100)) return false;
        }
    } else {
        // [0xA2][ADDR][CTRL][FC][nrOfTargets(2)][listId(2)][blockage(2)][reserved(2)][targets...]
        for (int i = 0; i < 11 && idx < maxLen; i++) {
            if (!readByte(outBuf[idx++], 100)) return false;
        }
        uint16_t nrOfTargets = ((uint16_t)outBuf[4] << 8) | outBuf[5];
        if (nrOfTargets > IMD2000_MAX_TARGETS) return false;
        uint16_t remaining = (uint16_t)nrOfTargets * 16 + 2;
        for (uint16_t i = 0; i < remaining && idx < maxLen; i++) {
            if (!readByte(outBuf[idx++], 100)) return false;
        }
    }
    outLen = idx;
    return true;
}

} // namespace

namespace {

// 受信した生バイトを内容に関わらず16進ダンプする(配線/電源側の切り分け用診断)。
// windowMs の間、受信があるたびにタイムアウトをリセットしながら読み続ける。
// 戻り値は受信できた総バイト数。
uint32_t dumpRawFor(unsigned long windowMs) {
    uint32_t count = 0;
    unsigned long lastRx = millis();
    Serial.print("  [IMD-2000 RAW] ");
    while (millis() - lastRx < windowMs) {
        if (ImdSerial.available() > 0) {
            uint8_t b = (uint8_t)ImdSerial.read();
            if (count < 64) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02X ", b);
                Serial.print(hex);
            }
            count++;
            lastRx = millis();
        }
    }
    if (count == 0) {
        Serial.println("(受信バイトなし)");
    } else if (count > 64) {
        Serial.printf("... (計%u バイト、先頭64バイトのみ表示)\n", (unsigned)count);
    } else {
        Serial.println();
    }
    return count;
}

} // namespace

void testImd2000() {
    ImdSerial.begin(IMD2000_BAUDRATE, SERIAL_8N1, PIN_IMD2000_RX, PIN_IMD2000_TX);
    delay(100);

    uint8_t startPayload[2] = {0x00, 0x00};
    sendFrame(IMD2000_CMD_START, startPayload, 2);

    Serial.println("  [IMD-2000] StartAcquisition送信。ウォームアップ待機中(約7秒、受信バイトは下に生表示)...");
    uint32_t warmupRxCount = dumpRawFor(7000);

    sendFrame(IMD2000_CMD_GETLIST, nullptr, 0);

    static uint8_t rxBuf[400];
    uint16_t rxLen = 0;
    bool gotTargetList = false;
    uint16_t nrOfTargets = 0;
    for (int attempt = 0; attempt < 2; attempt++) {
        if (!receiveFrame(rxBuf, rxLen, sizeof(rxBuf), 300)) break;
        if (rxBuf[0] == 0x68) continue; // ACKはスキップして次フレームを待つ
        gotTargetList = true;
        nrOfTargets = ((uint16_t)rxBuf[4] << 8) | rxBuf[5];
        break;
    }

    // GetTargetListの応答が正しく解釈できなかった場合、残っている生バイトも見ておく
    // (STXが0x68/0xA2以外の値になっている等、想定外の応答が来ていないかの確認)
    uint32_t getlistRxCount = 0;
    if (!gotTargetList) {
        Serial.print("  [IMD-2000] GetTargetList応答を確認できず。追加で500ms生受信を確認: ");
        getlistRxCount = dumpRawFor(500);
    }

    char d[160];
    if (gotTargetList) {
        snprintf(d, sizeof(d), "GetTargetList応答受信 (nrOfTargets=%u) TX=%d RX=%d Baud=%d",
            nrOfTargets, PIN_IMD2000_TX, PIN_IMD2000_RX, IMD2000_BAUDRATE);
        report("IMD-2000 ドップラーセンサ", true, d);
    } else {
        snprintf(d, sizeof(d), "応答なし (TX=%d RX=%d Baud=%d) 受信バイト数: warmup=%u getlist=%u - 0なら配線/電源、非0ならプロトコル不一致の疑い",
            PIN_IMD2000_TX, PIN_IMD2000_RX, IMD2000_BAUDRATE, (unsigned)warmupRxCount, (unsigned)getlistRxCount);
        report("IMD-2000 ドップラーセンサ", false, d);
    }
}
