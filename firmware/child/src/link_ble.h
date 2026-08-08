/*
 * BLE GATT サーバー (Peripheral) — 親機(Central)との同期セッション
 * GATTプロファイルは docs/protocol.md §2 を参照。
 */
#pragma once
#include <Arduino.h>

// アドバタイズ開始→(接続して同期)→切断 or タイムアウト、までを1回のセッションとして実行する。
// 戻り値: 親機との接続が成立したか (true でも同期内容が完全とは限らないが、次回リトライに委ねる)
bool bleSyncSession(uint32_t timeoutMs);
