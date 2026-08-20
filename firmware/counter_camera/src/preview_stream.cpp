#include "preview_stream.h"
#include "http_server.h"
#include "camera.h"
#include "config.h"
#include "light_control.h"
#include <WiFi.h>

namespace {

// SD保存・アップロードは行わない(プレビュー中は毎フレームcameraCapture()するのみ)。
// 照明は配信中(画角調整中)のみ設定明るさで点灯し、接続が切れたら消灯する。
void handleStream() {
    WiFiClient client = httpServer.client();
    const char* boundary = "countercam_preview";

    // Nagleアルゴリズムが有効なままだと、小さいマルチパートチャンクの送信が
    // 数百ms単位で遅延し、ブラウザ側で映像が全く表示されない/固まって見える
    // 症状になることがあるため無効化する。
    client.setNoDelay(true);

    lightSetLevel(LIGHT_LED1, (uint8_t)cfg.led1_level);
    lightSetLevel(LIGHT_LED2, (uint8_t)cfg.led2_level);

    client.print("HTTP/1.1 200 OK\r\n");
    client.printf("Content-Type: multipart/x-mixed-replace; boundary=%s\r\n\r\n", boundary);

    while (client.connected()) {
        camera_fb_t* fb = cameraCapture();
        if (!fb) {
            delay(100);
            continue;
        }
        client.printf("--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", boundary, (unsigned)fb->len);
        client.write(fb->buf, fb->len);
        client.print("\r\n");
        cameraReleaseFrame(fb);

        if (!client.connected()) break;
        delay(50); // フレームレートを抑制 (設置調整用途のため高フレームレートは不要)
    }

    lightSetLevel(LIGHT_LED1, LIGHT_LEVEL_OFF);
    lightSetLevel(LIGHT_LED2, LIGHT_LEVEL_OFF);
}

} // namespace

void previewStreamRegisterRoute() {
    httpServer.on("/preview/stream", HTTP_GET, handleStream);
}
