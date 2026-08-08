/*
 * SD_MMC (1bitモード) を使ったデータ蓄積:
 *  - /data/YYYY-MM-DD.csv    … 恒久履歴 (削除されない)
 *  - /queue/<epoch>_<child>.json … ポータル未送信キュー (アップロード成功後に削除)
 *  - /firmware/<version>.bin … 子機向けOTAファームウェアキャッシュ
 *
 * SD_MMC 1bitのCLK/CMD/D0ピンは実機配線依存のため build_flags で上書き可能。
 */
#pragma once
#include <Arduino.h>
#include <FS.h>

bool sdBegin();
uint64_t sdFreeBytes();

void sdAppendHistory(const char* childId, const char* batteryId, int64_t ts,
                      float rMohm, float v, bool valid);

// measurementsJsonArray は "[{...},{...}]" 形式のJSON配列文字列
void sdEnqueueForCloud(const char* childId, const String& measurementsJsonArray);

// 未送信キューを古い順に1件処理する。無ければfalseを返す。
bool sdQueuePeek(String& outPath, String& outChildId, String& outJsonArray);
void sdQueueRemove(const String& path);

bool   sdFirmwareExists(const char* version);
size_t sdFirmwareSize(const char* version);
File   sdFirmwareOpenWrite(const char* version);
File   sdFirmwareOpenRead(const char* version);
