/*
 * ニッスイ八王子工場キュービクル監視システム(ポータル)への画像アップロード。
 * POST {portal_base_url}/api/v1/cameras/{device_id}/images (生JPEGボディ)
 * ポータル側の認証は portal/server/src/middleware/cameraAuth.js と同じ
 * X-API-Key ヘッダによるTOFU(初回アクセスで自動登録)方式。
 *
 * ポータル手前にCloudflare Access(Zero Trust)のファイアウォールがある場合、
 * Service Tokenを CF-Access-Client-Id / CF-Access-Client-Secret ヘッダで送る。
 * (cfg.cf_access_client_id が空ならヘッダは付与しない)
 */
#include "uploader.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <time.h>
#include "config.h"

bool uploadJpeg(const uint8_t* buf, size_t len, const String& filename, int& outCode, String& outResp) {
    outCode = 0;
    if (strlen(cfg.portal_base_url) == 0) return false;

    String url = String(cfg.portal_base_url) + "/api/v1/cameras/" + cfg.device_id + "/images";
    HTTPClient http;
    NetworkClientSecure secureClient;
    WiFiClient plainClient;
    bool began = url.startsWith("https://")
        ? (secureClient.setInsecure(), http.begin(secureClient, url))
        : http.begin(plainClient, url);
    if (!began) return false;

    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("X-API-Key", cfg.portal_api_key);
    if (strlen(cfg.cf_access_client_id) > 0) {
        http.addHeader("CF-Access-Client-Id", cfg.cf_access_client_id);
        http.addHeader("CF-Access-Client-Secret", cfg.cf_access_client_secret);
    }
    time_t now = time(nullptr);
    if (now > 1672531200) { // NTP同期済み (2023-01-01以降) の場合のみ撮影時刻を送る
        http.addHeader("X-Captured-At", String((uint32_t)now));
    }

    outCode = http.POST((uint8_t*)buf, len);
    if (outCode > 0) outResp = http.getString();
    http.end();
    return outCode > 0;
}
