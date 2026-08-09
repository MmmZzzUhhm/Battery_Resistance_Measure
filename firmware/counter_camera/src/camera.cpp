/*
 * XIAO ESP32S3 Sense 内蔵カメラ
 *
 * Sense拡張基板のカメラFPCコネクタに固定配線されたピンを使用する
 * (parent_hwtestのtest_camera.cppで実機動作確認済み。
 *  参考: https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/ )
 */
#include "camera.h"
#include "config.h"
#include <Arduino.h>

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
    return esp_camera_fb_get();
}
