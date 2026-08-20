/*
 * 基板動作チェックアプリ 常駐Web UI。
 * WiFi未設定ならAP設定モード、設定済みでSTA接続に成功すればテストモードとして動作する。
 */
#pragma once
#include <Arduino.h>

// 起動時に1回呼ぶ。WiFi設定の有無・STA接続成否に応じてAP設定モード/テストモードを開始する。
void hwtestWebServerBegin();

// loop()から毎回呼ぶ (WebServer::handleClient()相当)。
void hwtestWebServerLoop();
