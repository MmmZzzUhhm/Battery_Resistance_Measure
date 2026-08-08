/*
 * WiFi STA接続時にNTPでPCF8563T RTCを較正する。
 */
#pragma once

// 現在WiFi STAが接続済みであることが前提。成功したらtrue。
bool ntpSyncRtc();
