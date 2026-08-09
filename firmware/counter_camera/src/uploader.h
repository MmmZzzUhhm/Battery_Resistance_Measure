/*
 * 撮影画像を cfg.portal_base_url へアップロードする。
 */
#pragma once
#include <Arduino.h>

// アップロード先未設定(空文字列)ならfalseを返す。
// outCode: HTTPステータスコード (通信自体に失敗した場合は<=0)
bool uploadJpeg(const uint8_t* buf, size_t len, const String& filename, int& outCode, String& outResp);
