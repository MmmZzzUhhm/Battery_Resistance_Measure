/*
 * XIAO ESP32S3 Sense 内蔵カメラ
 */
#pragma once
#include "esp_camera.h"

bool cameraBegin();

// 成功時は呼び出し側で esp_camera_fb_return() で解放すること。失敗時はnullptr。
camera_fb_t* cameraCapture();
