/*
 * SD_MMC (1bitモード) への撮影画像保存:
 *   /DCIM/img_YYYYMMDD_HHMMSS.jpg … NTP時刻同期済みの場合
 *   /DCIM/img_<連番>.jpg          … 未同期の場合 (連番はNVSに永続化)
 *
 * 時刻はPCF8563T RTCから起動時に取得され、WiFi STA接続後のNTP同期でも補正される
 * (main.cpp, rtc_clock.cpp, ntp_sync.cpp参照)。
 */
#pragma once
#include <Arduino.h>
#include <FS.h>

bool sdBegin();
bool sdIsAvailable(); // sdBegin()がカードを検出できたかどうか (未挿入時は撮影後の保存処理をスキップするため)
uint64_t sdFreeBytes();

// 保存に成功したら "/DCIM/xxx.jpg" 形式のパスを返す。失敗時は空文字列。
String sdSaveJpeg(const uint8_t* buf, size_t len);
