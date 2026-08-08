/*
 * BLE Central: 子機(Peripheral)を発見して同期する (docs/protocol.md §2)
 */
#pragma once
#include <Arduino.h>

// scanSecondsの間スキャンし、対象子機が見つかれば接続・同期して true を返す。
// 見つからなければ false (呼び出し側で一定間隔ごとに再試行する)。
bool bleScanAndSyncOnce(uint32_t scanSeconds);
