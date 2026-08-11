/*
 * XIAO ESP32S3 Sense 内蔵カメラ
 *
 * Sense拡張基板のカメラFPCコネクタに固定配線されたピンを使用する
 * (parent_hwtestのtest_camera.cppで実機動作確認済み。
 *  参考: https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/ )
 *
 * 向き補正(上下反転・回転)について:
 * このセンサーはpin_reset/pin_pwdn未接続のため、ファームウェア再書き込み後もセンサー内部の
 * レジスタ状態が保持される。さらにset_hmirror()/set_vflip()を明示的に書き込むと、書き込むたびに
 * 結果が変わる(再現性がない)ことを実機で確認した。そのためセンサーのレジスタには一切触れず、
 * 常にソフトウェア側でJPEGをデコード→上下反転(BASE_VFLIP)+取付向き補正(cfg.image_rotation)を
 * 適用→再エンコードする。BASE_VFLIP=trueであることは実機確認済み (mirrorは不要)。
 */
#include "camera.h"
#include "config.h"
#include <Arduino.h>
#include "img_converters.h"

namespace {

// 実機確認済みの向き補正基準値 (cfg.image_rotation=0の基準)。センサーレジスタではなく
// ソフトウェア側で毎回適用する (camera.cpp先頭のコメント参照)。
constexpr bool BASE_MIRROR = false;
constexpr bool BASE_VFLIP  = true;

constexpr int PWDN_GPIO_NUM  = -1;
constexpr int RESET_GPIO_NUM = -1;
constexpr int XCLK_GPIO_NUM  = 10;
constexpr int SIOD_GPIO_NUM  = 40;
constexpr int SIOC_GPIO_NUM  = 39;
constexpr int Y9_GPIO_NUM    = 48;
constexpr int Y8_GPIO_NUM    = 11;
constexpr int Y7_GPIO_NUM    = 12;
constexpr int Y6_GPIO_NUM    = 14;
constexpr int Y5_GPIO_NUM    = 16;
constexpr int Y4_GPIO_NUM    = 18;
constexpr int Y3_GPIO_NUM    = 17;
constexpr int Y2_GPIO_NUM    = 15;
constexpr int VSYNC_GPIO_NUM = 38;
constexpr int HREF_GPIO_NUM  = 47;
constexpr int PCLK_GPIO_NUM  = 13;

// RGB888バッファを回転する (mirror/vflipは向き調整デバッグ用に残す)。
// 戻り値はmalloc()確保 (呼び出し側でfree()必須)。失敗時nullptr。
uint8_t* transformRgb888(const uint8_t* src, int w, int h, int rotation, bool mirror, bool vflip,
                          int& outW, int& outH) {
    if (rotation == 90 || rotation == 270) {
        outW = h;
        outH = w;
    } else {
        outW = w;
        outH = h;
    }

    uint8_t* dst = (uint8_t*)malloc((size_t)outW * outH * 3);
    if (!dst) return nullptr;

    for (int ny = 0; ny < outH; ny++) {
        for (int nx = 0; nx < outW; nx++) {
            int sx, sy; // 回転のみを適用した場合の元画像上の座標
            switch (rotation) {
                case 90:  sx = ny;         sy = h - 1 - nx; break;
                case 180: sx = w - 1 - nx; sy = h - 1 - ny; break;
                case 270: sx = w - 1 - ny; sy = nx;         break;
                default:  sx = nx;         sy = ny;         break;
            }
            if (mirror) sx = w - 1 - sx;
            if (vflip)  sy = h - 1 - sy;

            const uint8_t* s = src + (size_t)(sy * w + sx) * 3;
            uint8_t* d = dst + (size_t)(ny * outW + nx) * 3;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
        }
    }
    return dst;
}

// JPEGフレームに回転を適用した新規JPEGフレームを返す (呼び出し側でcameraReleaseFrame()必須)。
// 失敗時はnullptr (元のfbは呼び出し側で解放すること)。
camera_fb_t* transformJpegFrame(camera_fb_t* fb, int rotation, bool mirror, bool vflip) {
    size_t rgbLen = (size_t)fb->width * fb->height * 3;
    uint8_t* rgb = (uint8_t*)malloc(rgbLen);
    if (!rgb) return nullptr;
    if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb)) {
        free(rgb);
        return nullptr;
    }

    int outW, outH;
    uint8_t* transformed = transformRgb888(rgb, fb->width, fb->height, rotation, mirror, vflip, outW, outH);
    free(rgb);
    if (!transformed) return nullptr;

    uint8_t* jpgBuf = nullptr;
    size_t jpgLen = 0;
    bool ok = fmt2jpg(transformed, (size_t)outW * outH * 3, outW, outH, PIXFORMAT_RGB888,
                       cfg.jpeg_quality, &jpgBuf, &jpgLen);
    free(transformed);
    if (!ok) return nullptr;

    camera_fb_t* out = (camera_fb_t*)calloc(1, sizeof(camera_fb_t));
    if (!out) {
        free(jpgBuf);
        return nullptr;
    }
    out->buf       = jpgBuf;
    out->len       = jpgLen;
    out->width     = outW;
    out->height    = outH;
    out->format    = PIXFORMAT_JPEG;
    out->timestamp = fb->timestamp;
    return out;
}

} // namespace

bool cameraBegin() {
    camera_config_t c = {};
    c.ledc_channel = LEDC_CHANNEL_0;
    c.ledc_timer   = LEDC_TIMER_0;
    c.pin_d0       = Y2_GPIO_NUM;
    c.pin_d1       = Y3_GPIO_NUM;
    c.pin_d2       = Y4_GPIO_NUM;
    c.pin_d3       = Y5_GPIO_NUM;
    c.pin_d4       = Y6_GPIO_NUM;
    c.pin_d5       = Y7_GPIO_NUM;
    c.pin_d6       = Y8_GPIO_NUM;
    c.pin_d7       = Y9_GPIO_NUM;
    c.pin_xclk     = XCLK_GPIO_NUM;
    c.pin_pclk     = PCLK_GPIO_NUM;
    c.pin_vsync    = VSYNC_GPIO_NUM;
    c.pin_href     = HREF_GPIO_NUM;
    c.pin_sccb_sda = SIOD_GPIO_NUM;
    c.pin_sccb_scl = SIOC_GPIO_NUM;
    c.pin_pwdn     = PWDN_GPIO_NUM;
    c.pin_reset    = RESET_GPIO_NUM;
    c.xclk_freq_hz = 10000000;
    c.pixel_format = PIXFORMAT_JPEG;
    c.jpeg_quality = cfg.jpeg_quality;

    if (psramFound()) {
        c.frame_size  = (framesize_t)cfg.frame_size;
        c.fb_count    = 2;
        c.fb_location = CAMERA_FB_IN_PSRAM;
        c.grab_mode   = CAMERA_GRAB_LATEST;
    } else {
        Serial.println("[Camera] WARNING: No PSRAM detected!");
        c.frame_size  = (framesize_t)min(cfg.frame_size, (int)FRAMESIZE_SVGA);
        c.fb_count    = 1;
        c.fb_location = CAMERA_FB_IN_DRAM;
        c.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&c);
    if (err != ESP_OK) {
        Serial.printf("[Camera] esp_camera_init失敗 (0x%x)\n", err);
        return false;
    }

    // NOTE: このセンサーはset_hmirror()/set_vflip()の実効性が再現しない(実機確認: 同じ値を
    // 書き込んでも結果が変わることがある)。そのためセンサーのレジスタには一切触れず、
    // 現在保持されている状態のまま使う (実機確認済み: 無補正で正しい向きになる状態)。

    // ウォームアップ (露出/AWB安定待ち)。最初の数フレームは捨てる。
    for (int i = 0; i < 3; i++) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
        delay(150);
    }

    Serial.println("[Camera] Initialized");
    return true;
}

camera_fb_t* cameraCapture() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return nullptr;

    camera_fb_t* transformed = transformJpegFrame(fb, cfg.image_rotation, BASE_MIRROR, BASE_VFLIP);
    esp_camera_fb_return(fb);
    if (!transformed) {
        Serial.println("[Camera] WARNING: image transform failed");
    }
    return transformed;
}

// cameraCapture()が返すフレームは常にtransformJpegFrame()のmalloc()確保。
void cameraReleaseFrame(camera_fb_t* fb) {
    if (!fb) return;
    free(fb->buf);
    free(fb);
}
