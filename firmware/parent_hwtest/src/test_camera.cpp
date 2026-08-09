/*
 * XIAO ESP32S3 Sense 内蔵カメラ 動作確認
 *
 * Sense拡張基板のカメラFPCコネクタに固定配線されたピンを使用する
 * (実機配線に合わせて変更する類のものではないため、build_flagsでの
 *  上書きは想定していない。参考: xiao_camera/src/main.cpp,
 *  https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/ )
 *
 * 判定: カメラ初期化 + JPEGキャプチャに成功し、JPEG SOIマーカー(0xFF 0xD8)を確認できたらPASS。
 */
#include "test_camera.h"
#include "hwtest_common.h"
#include <Arduino.h>
#include "esp_camera.h"

namespace {

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

bool initCamera() {
    camera_config_t cfg = {};
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.pin_d0       = Y2_GPIO_NUM;
    cfg.pin_d1       = Y3_GPIO_NUM;
    cfg.pin_d2       = Y4_GPIO_NUM;
    cfg.pin_d3       = Y5_GPIO_NUM;
    cfg.pin_d4       = Y6_GPIO_NUM;
    cfg.pin_d5       = Y7_GPIO_NUM;
    cfg.pin_d6       = Y8_GPIO_NUM;
    cfg.pin_d7       = Y9_GPIO_NUM;
    cfg.pin_xclk     = XCLK_GPIO_NUM;
    cfg.pin_pclk     = PCLK_GPIO_NUM;
    cfg.pin_vsync    = VSYNC_GPIO_NUM;
    cfg.pin_href     = HREF_GPIO_NUM;
    cfg.pin_sccb_sda = SIOD_GPIO_NUM;
    cfg.pin_sccb_scl = SIOC_GPIO_NUM;
    cfg.pin_pwdn     = PWDN_GPIO_NUM;
    cfg.pin_reset    = RESET_GPIO_NUM;
    cfg.xclk_freq_hz = 10000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.jpeg_quality = 12;

    if (psramFound()) {
        cfg.frame_size  = FRAMESIZE_VGA;
        cfg.fb_count    = 2;
        cfg.fb_location = CAMERA_FB_IN_PSRAM;
        cfg.grab_mode   = CAMERA_GRAB_LATEST;
    } else {
        cfg.frame_size  = FRAMESIZE_QVGA;
        cfg.fb_count    = 1;
        cfg.fb_location = CAMERA_FB_IN_DRAM;
        cfg.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("  [Camera] esp_camera_init失敗 (0x%x)\n", err);
        return false;
    }
    return true;
}

} // namespace

void testCamera() {
    if (!initCamera()) {
        report("Sense カメラ", false, "esp_camera_init失敗 (配線、またはSense基板の実装を確認)");
        return;
    }

    // 起動直後の数フレームは露出/AWB調整中で乱れることがあるため、
    // 複数回キャプチャして最後に成功したフレームで判定する。
    int okCount = 0;
    size_t lastLen = 0;
    bool sawSoi = false;
    for (int i = 0; i < 5; i++) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) {
            okCount++;
            lastLen = fb->len;
            if (fb->len >= 2 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8) {
                sawSoi = true;
            }
            esp_camera_fb_return(fb);
        }
        delay(150);
    }

    esp_camera_deinit();

    char d[96];
    bool pass = (okCount > 0) && sawSoi;
    snprintf(d, sizeof(d), "キャプチャ成功%d/5回, 最終フレーム%uバイト, JPEG SOI %s",
        okCount, (unsigned)lastLen, sawSoi ? "確認" : "未確認");
    report("Sense カメラ", pass, d);
}
