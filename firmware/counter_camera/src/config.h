/*
 * 薄型カウンタカメラ設定 (NVS/Preferences 永続化)
 */
#pragma once
#include <Arduino.h>

struct CounterCameraConfig {
    char     device_id[32];
    char     wifi_sta_ssid[64];   // 上位システムと同じネットワークへの接続先
    char     wifi_sta_pass[64];
    char     ap_ssid[32];         // STA未接続時/設定用のフォールバックSoftAP
    char     ap_pass[64];
    int      jpeg_quality;        // 0(高品質)〜63(低品質)
    int      frame_size;          // framesize_t 値 (5=QVGA 8=VGA 9=SVGA 10=XGA 11=HD 13=UXGA)

    // 固定IP設定 (use_static_ip=false の場合はDHCPを使用)
    bool     use_static_ip;
    char     static_ip[16];
    char     static_gateway[16];
    char     static_subnet[16];
    char     static_dns[16];

    // NTPサーバー (ONVIF GetNTP/SetNTPで上位システムから変更可能。PCF8563T RTCへ同期時刻を保持する)
    char     ntp_server1[64];
    char     ntp_server2[64];

    // タイムゾーン (POSIX TZ形式。ONVIF SetSystemDateAndTimeのTimeZone要素で設定可能)
    // 例: 日本標準時(UTC+9)は "JST-9" (POSIX形式は符号がUTCからのオフセットと逆になる点に注意)
    char     timezone_tz[32];
};

extern CounterCameraConfig cfg;

void configLoad();
void configSave();
String configToJson();          // Web UI表示/APIレスポンス用 (機密情報はマスク)
bool  configApplyJson(const String& json);

// cfg.timezone_tz (POSIX TZ文字列) をTZ環境変数へ反映する。
// 起動時、およびtimezone_tzを変更した直後に呼ぶこと。
void applyTimezoneEnv();
