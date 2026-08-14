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
    // framesize_t 値。ライブラリのenum定義(esp32-camera/driver/include/sensor.h)実測値:
    // 6=QVGA(320x240) 10=VGA(640x480) 11=SVGA(800x600) 12=XGA(1024x768)
    // 13=HD(1280x720) 14=SXGA(1280x1024) 15=UXGA(1600x1200)
    // (旧バージョンのライブラリを前提とした値と2〜3個ズレるため、ハードコードせず必ずヘッダで確認すること)
    int      frame_size;
    int      image_rotation;      // 取付向き補正: 0/90/180/270 (度、時計回り)

    // カメラ画像設定 (esp32-cameraのsensor_t全項目)。
    // ONVIF Imaging Serviceで公開しているのはbrightness/contrast/saturation/sharpnessのみ
    // (onvif_imaging_service.cpp)。それ以外はローカルWeb UI専用。
    // いずれもここに永続化し、起動時・変更時にcameraApplySensorSettings()でセンサーへ反映する。
    int  img_brightness;     // -2..2
    int  img_contrast;       // -2..2
    int  img_saturation;     // -2..2
    int  img_sharpness;      // -2..2
    int  img_special_effect; // 0=無効 1=ネガ 2=グレースケール 3=赤 4=緑 5=青 6=セピア
    int  img_wb_mode;        // 0=自動 1=晴天 2=曇天 3=オフィス 4=家庭 (AWB有効時のみ意味を持つ)
    bool img_awb;            // オートホワイトバランス
    bool img_awb_gain;
    bool img_aec;            // 自動露出制御(AEC)
    bool img_aec2;
    int  img_ae_level;       // -2..2
    int  img_aec_value;      // 0..1200 (AEC無効時の手動露出値)
    bool img_agc;            // 自動ゲイン制御(AGC)
    int  img_agc_gain;       // 0..30 (AGC無効時の手動ゲイン)
    int  img_gainceiling;    // 0..6 (2x/4x/8x/16x/32x/64x/128x/2x, AGC有効時の上限)
    bool img_bpc;            // 黒点補正
    bool img_wpc;            // 白点補正
    bool img_raw_gma;
    bool img_lenc;           // レンズ補正
    int  img_denoise;

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
