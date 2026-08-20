#include "camera_capture.h"
#include "esp_camera.h"
#include <SD_MMC.h>
#include <algorithm>

#ifndef PIN_SD_CLK
#define PIN_SD_CLK 2
#endif
#ifndef PIN_SD_CMD
#define PIN_SD_CMD 3
#endif
#ifndef PIN_SD_D0
#define PIN_SD_D0 1
#endif

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

bool g_cameraReady = false;
bool g_sdReady     = false;

bool initCameraOnce(String& errorMsg) {
    if (g_cameraReady) return true;

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
    c.jpeg_quality = 12;

    if (psramFound()) {
        c.frame_size  = FRAMESIZE_VGA;
        c.fb_count    = 2;
        c.fb_location = CAMERA_FB_IN_PSRAM;
        c.grab_mode   = CAMERA_GRAB_LATEST;
    } else {
        c.frame_size  = FRAMESIZE_QVGA;
        c.fb_count    = 1;
        c.fb_location = CAMERA_FB_IN_DRAM;
        c.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&c);
    if (err != ESP_OK) {
        char buf[48];
        snprintf(buf, sizeof(buf), "esp_camera_init失敗 (0x%x)", err);
        errorMsg = buf;
        return false;
    }
    // 露出/AWB安定待ち
    for (int i = 0; i < 3; i++) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
        delay(150);
    }
    g_cameraReady = true;
    return true;
}

bool initSdOnce(String& errorMsg) {
    if (g_sdReady) return true;
    SD_MMC.setPins(PIN_SD_CLK, PIN_SD_CMD, PIN_SD_D0);
    if (!SD_MMC.begin("/sdcard", true)) {
        errorMsg = "SD_MMC.begin(1bit)失敗 (配線またはカード未挿入を確認)";
        return false;
    }
    if (!SD_MMC.exists(CAMERA_SAVE_DIR)) {
        SD_MMC.mkdir(CAMERA_SAVE_DIR);
    }
    g_sdReady = true;
    return true;
}

}  // namespace

bool cameraCaptureBegin(String& errorMsg) {
    if (!initCameraOnce(errorMsg)) return false;
    if (!initSdOnce(errorMsg)) return false;
    return true;
}

bool cameraCaptureAndSave(String& savedPath, String& errorMsg) {
    if (!cameraCaptureBegin(errorMsg)) return false;

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        errorMsg = "撮影に失敗しました (esp_camera_fb_get)";
        return false;
    }

    // ファイル名の数値部は固定10桁ゼロ埋め (millis()は最大10桁) にして、
    // 文字列としての辞書順ソートがそのまま時系列順になるようにする。
    char filename[64];
    snprintf(filename, sizeof(filename), "%s/cap_%010lu.jpg", CAMERA_SAVE_DIR, (unsigned long)millis());

    File f = SD_MMC.open(filename, FILE_WRITE);
    if (!f) {
        esp_camera_fb_return(fb);
        errorMsg = "SDへの保存用オープンに失敗しました";
        return false;
    }
    size_t written = f.write(fb->buf, fb->len);
    f.close();
    size_t totalLen = fb->len;
    esp_camera_fb_return(fb);

    if (written != totalLen) {
        errorMsg = "SD書込が不完全でした";
        return false;
    }

    savedPath = filename;
    return true;
}

std::vector<String> cameraListSavedImages(int maxCount) {
    std::vector<String> names;
    String dummy;
    if (!initSdOnce(dummy)) return names;

    File dir = SD_MMC.open(CAMERA_SAVE_DIR);
    if (!dir || !dir.isDirectory()) return names;

    File entry = dir.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) names.push_back(String(entry.name()));
        entry = dir.openNextFile();
    }
    dir.close();

    std::sort(names.begin(), names.end(), std::greater<String>());  // 新しい(値が大きい)ファイル名を先頭に
    if ((int)names.size() > maxCount) names.resize(maxCount);
    return names;
}
