/*
 * 親機が把握している子機ごとの状態:
 *  - ポータルから取得した「配信待ち設定/OTA」
 *  - heartbeat用の「直近見えた子機一覧」
 * BLE経路・WiFi経路どちらの同期処理からも共通で参照する。
 */
#pragma once
#include <Arduino.h>

#define MAX_TRACKED_CHILDREN 32

struct PendingChildUpdate {
    bool   hasConfig = false;
    String configJson;
    bool   hasOta = false;
    String otaVersion;
    size_t otaSize = 0;
    String otaMd5;
};

struct ChildLastReading {
    bool    valid  = false;
    int64_t ts     = 0;
    float   r_mohm = 0;
    float   v      = 0;
};

// ポータルから取得した内容で置き換える (cloud_clientが定期的に呼ぶ)
void registrySetPendingConfig(const String& childId, const String& configJson);
void registrySetPendingOta(const String& childId, const String& version, size_t size, const String& md5);
void registryClearPendingOta(const String& childId);

// 同期処理側 (child_ble/child_wifi) が呼ぶ
bool registryGetPending(const String& childId, PendingChildUpdate& out);
void registryMarkSeen(const String& childId);

// sync_common が受信データ処理のたびに最新値を更新する (Web UI/API表示用)
void registryUpdateLastReading(const String& childId, int64_t ts, float rMohm, float v, bool valid);
bool registryGetLastReading(const String& childId, ChildLastReading& out);

// heartbeat送信用
int    registrySeenCount();
String registrySeenChildId(int index);
