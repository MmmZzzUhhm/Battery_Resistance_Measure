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
    char     portal_base_url[96]; // 例: http://raspberrypi.local:8080 (ニッスイ八王子工場キュービクル監視システム)
    char     portal_api_key[64];
    int      jpeg_quality;        // 0(高品質)〜63(低品質)
    int      frame_size;          // framesize_t 値 (5=QVGA 8=VGA 9=SVGA 10=XGA 11=HD 13=UXGA)

    // 固定IP設定 (use_static_ip=false の場合はDHCPを使用)
    bool     use_static_ip;
    char     static_ip[16];
    char     static_gateway[16];
    char     static_subnet[16];
    char     static_dns[16];

    // Cloudflare Access Service Token (ポータル手前のCloudflareファイアウォールを通過するため)
    char     cf_access_client_id[80];
    char     cf_access_client_secret[80];
};

extern CounterCameraConfig cfg;

void configLoad();
void configSave();
String configToJson();          // Web UI表示/APIレスポンス用 (機密情報はマスク)
bool  configApplyJson(const String& json);
