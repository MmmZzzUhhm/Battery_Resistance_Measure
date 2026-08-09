/*
 * 本機で唯一のWebServerインスタンス。
 * web_api(REST API)・web_ui(設定画面) がそれぞれ自分のルートをこれに登録する。
 */
#pragma once
#include <WebServer.h>

extern WebServer httpServer;
