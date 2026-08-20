/*
 * 子機(BLE Peripheral)との疎通確認 (本番の同期処理とは独立、副作用のない読み取りのみ)。
 * 子機は自身のdevice_idをBLEアドバタイズ名としてそのまま使う(firmware/child/src/link_ble.cpp)ため、
 * スキャン結果の名前=device_idで対象を特定できる。
 */
#pragma once
#include <Arduino.h>
#include <vector>

struct BleChildAdv {
    String name;
    String address;
    int    rssi;
};

// scanSeconds間スキャンし、対象サービスをアドバタイズしている子機の一覧を返す。
std::vector<BleChildAdv> bleScanChildren(uint32_t scanSeconds);

struct BleChildStatus {
    bool   ok;
    String error;
    String device_id;
    String fw_version;
    String battery_id;
    int64_t rtc_epoch;
    uint32_t pending_count;
};

// targetName(device_id)を指定するとその名前の子機のみに接続する。空文字なら最初に見つかった子機。
// STATUS特性を読むだけの軽量な疎通確認 (データ吸い上げ・時刻書込・OTA等の副作用は行わない)。
BleChildStatus bleCheckChildStatus(const String& targetName, uint32_t scanSeconds);
