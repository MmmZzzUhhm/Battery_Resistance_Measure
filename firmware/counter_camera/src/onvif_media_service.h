#pragma once
#include <Arduino.h>

String onvifHandleMediaService(const String& requestBody);

// GET /onvif/snapshot: 撮影しJPEGバイナリをそのまま返す (httpServerへ直接送信)
void onvifHandleSnapshotRoute();
