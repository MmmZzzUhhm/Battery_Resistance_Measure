#include "preview_stream.h"
#include "http_server.h"
#include "camera.h"
#include <WiFi.h>

namespace {

// SD保存・アップロードは行わない(プレビュー中は毎フレームcameraCapture()するのみ)。
void handleStream() {
    WiFiClient client = httpServer.client();
    const char* boundary = "countercam_preview";

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
        esp_camera_fb_return(fb);

        if (!client.connected()) break;
        delay(50); // フレームレートを抑制 (設置調整用途のため高フレームレートは不要)
    }
}

} // namespace

void previewStreamRegisterRoute() {
    httpServer.on("/preview/stream", HTTP_GET, handleStream);
}
