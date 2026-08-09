/*
 * ONVIF SOAP共通ヘルパ
 *
 * XMLパーサは実装せず、リクエストボディ中にアクション名の要素が
 * 含まれるかを単純な部分文字列検索で判定する簡易実装。
 * 名前空間prefixの揺れ(tds: / SOAP-ENV: 等)に影響されないための実用的な妥協。
 * 実際のONVIFクライアントとの相互接続では細部の調整が必要になる可能性がある。
 */
#pragma once
#include <Arduino.h>

bool soapBodyContainsAction(const String& body, const char* actionName);
String soapEnvelopeWrap(const String& innerXml);
String soapFault(const char* reason);

extern const char* ONVIF_MANUFACTURER;
extern const char* ONVIF_MODEL;
extern const char* ONVIF_FIRMWARE_VERSION;

// 自機のベースURL (例: http://192.168.4.1) を返す。STA接続済みならそちらのIPを優先する。
String onvifDeviceBaseUrl();
