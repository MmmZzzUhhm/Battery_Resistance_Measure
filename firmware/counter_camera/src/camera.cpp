/*
 * XIAO ESP32S3 Sense 内蔵カメラ
 *
 * Sense拡張基板のカメラFPCコネクタに固定配線されたピンを使用する
 * (parent_hwtestのtest_camera.cppで実機動作確認済み。
 *  参考: https://wiki.seeedstudio.com/xiao_esp32s3_pin_multiplexing/ )
 *
 * 向き補正(上下反転・回転)について:
 * 実機検証(hmirror=0/vflip=1 を固定し、hmirrorだけを切り替えて比較)により、
 * センサーのhmirror/vflipレジスタは正しく機能することを確認済み。
 * (以前「レジスタが信頼できない」としていたのは、複数の変更を同時に行っていたことによる
 *  検証不備が原因だった。)
 *
 * 基準(cfg.image_rotation=0)は hmirror=false, vflip=true で正しい向きになる。
 * 0度・180度は寸法を変えない単純な反転(hmirror/vflipの組み合わせ)だけで実現できるため、
 * センサーのレジスタのみで対応し、ソフトウェア処理(デコード→再エンコード)を行わない
 * (設置時プレビューの応答性のため、これが特に重要)。
 * 90度・270度は転置(寸法変更)を伴うためレジスタだけでは実現できず、ソフトウェア側で
 * JPEGデコード→回転→再エンコードする。
 */
#include "camera.h"
#include "config.h"
#include <Arduino.h>
#include "img_converters.h"
#include <string.h>

namespace {

// 実機確認済みの向き補正基準値 (cfg.image_rotation=0の基準、hmirror=false/vflip=trueで正しい向き)。
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

int g_hwAppliedRotation = -1;    // 直前にセンサーレジスタへ反映した回転モード (-1=未反映)
bool g_lastFrameIsMalloced = false; // 直前にcameraCapture()が返したフレームの解放方法の判定用

// cfg.image_rotationに応じてセンサーのhmirror/vflipを設定する。
// 0/180度はこれだけで正しい向きになる。90/270度は転置が必要なためレジスタは基準状態に戻し、
// ソフトウェア側(transformJpegFrame)で全補正する。
void applyHwOrientation(int rotation) {
    if (rotation == g_hwAppliedRotation) return;

    sensor_t* sensor = esp_camera_sensor_get();
    if (!sensor) return;

    if (rotation == 180) {
        // 180度 = 基準(BASE_MIRROR,BASE_VFLIP)の水平・垂直両方反転
        sensor->set_hmirror(sensor, !BASE_MIRROR);
        sensor->set_vflip(sensor, !BASE_VFLIP);
    } else if (rotation == 90 || rotation == 270) {
        // ソフトウェア側で全補正するため、レジスタは無補正に戻す
        sensor->set_hmirror(sensor, false);
        sensor->set_vflip(sensor, false);
    } else {
        sensor->set_hmirror(sensor, BASE_MIRROR);
        sensor->set_vflip(sensor, BASE_VFLIP);
    }

    // レジスタ変更がセンサー出力に反映されるまで数フレーム捨てて待つ。
    for (int i = 0; i < 2; i++) {
        camera_fb_t* warm = esp_camera_fb_get();
        if (warm) esp_camera_fb_return(warm);
    }
    g_hwAppliedRotation = rotation;
}

// 回転0/180度の場合、mirror・vflipの組み合わせは寸法を変えない単純な水平/垂直反転に
// 帰着できる (90/270度のような転置を伴わない)。この場合は追加バッファを確保せず
// その場で反転することで、UXGA等の大きな解像度でもメモリ不足を避ける。
void flipRowsInPlace(uint8_t* buf, int w, int h) {
    size_t rowBytes = (size_t)w * 3;
    uint8_t* tmp = (uint8_t*)malloc(rowBytes);
    if (!tmp) return;
    for (int y = 0; y < h / 2; y++) {
        uint8_t* rowA = buf + (size_t)y * rowBytes;
        uint8_t* rowB = buf + (size_t)(h - 1 - y) * rowBytes;
        memcpy(tmp, rowA, rowBytes);
        memcpy(rowA, rowB, rowBytes);
        memcpy(rowB, tmp, rowBytes);
    }
    free(tmp);
}

void flipColsInPlace(uint8_t* buf, int w, int h) {
    for (int y = 0; y < h; y++) {
        uint8_t* row = buf + (size_t)y * w * 3;
        for (int x = 0; x < w / 2; x++) {
            uint8_t* a = row + (size_t)x * 3;
            uint8_t* b = row + (size_t)(w - 1 - x) * 3;
            uint8_t t0 = a[0], t1 = a[1], t2 = a[2];
            a[0] = b[0]; a[1] = b[1]; a[2] = b[2];
            b[0] = t0;   b[1] = t1;   b[2] = t2;
        }
    }
}

// RGB888バッファを回転する (90/270度、転置を伴うケース専用)。
// 戻り値はmalloc()確保 (呼び出し側でfree()必須)。失敗時nullptr。
uint8_t* transformRgb888(const uint8_t* src, int w, int h, int rotation, bool mirror, bool vflip,
                          int& outW, int& outH) {
    outW = h;
    outH = w;

    uint8_t* dst = (uint8_t*)malloc((size_t)outW * outH * 3);
    if (!dst) return nullptr;

    for (int ny = 0; ny < outH; ny++) {
        for (int nx = 0; nx < outW; nx++) {
            int sx, sy; // 回転のみを適用した場合の元画像上の座標
            if (rotation == 90) { sx = ny;         sy = h - 1 - nx; }
            else                { sx = w - 1 - ny; sy = nx;         } // 270
            if (mirror) sx = w - 1 - sx;
            if (vflip)  sy = h - 1 - sy;

            const uint8_t* s = src + (size_t)(sy * w + sx) * 3;
            uint8_t* d = dst + (size_t)(ny * outW + nx) * 3;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
        }
    }
    return dst;
}

// JPEGフレームに90/270度回転を適用した新規JPEGフレームを返す (呼び出し側でcameraReleaseFrame()必須)。
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

    applyHwOrientation(cfg.image_rotation);

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
    int rotation = cfg.image_rotation;
    applyHwOrientation(rotation);

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return nullptr;

    if (rotation == 90 || rotation == 270) {
        camera_fb_t* transformed = transformJpegFrame(fb, rotation, BASE_MIRROR, BASE_VFLIP);
        esp_camera_fb_return(fb);
        if (!transformed) {
            Serial.println("[Camera] WARNING: image transform failed");
            return nullptr;
        }
        g_lastFrameIsMalloced = true;
        return transformed;
    }

    // 0/180度はセンサーのレジスタだけで正しい向きになっているので、そのまま返す。
    g_lastFrameIsMalloced = false;
    return fb;
}

// cameraCapture()が返したフレームは、内部で記録した解放方法(malloc()確保 or カメラドライバ管理)
// に従って解放する。呼び出しはcameraCapture()の直後、他の撮影を挟まずに行うこと。
void cameraReleaseFrame(camera_fb_t* fb) {
    if (!fb) return;
    if (g_lastFrameIsMalloced) {
        free(fb->buf);
        free(fb);
    } else {
        esp_camera_fb_return(fb);
    }
}
