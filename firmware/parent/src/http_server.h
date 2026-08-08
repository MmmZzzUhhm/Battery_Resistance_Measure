/*
 * 親機で唯一のWebServerインスタンス。
 * child_wifi(子機同期)・web_api(ローカルREST API)・web_ui(ダッシュボード) が
 * それぞれ自分のルートをこれに登録する。
 */
#pragma once
#include <WebServer.h>

extern WebServer httpServer;
