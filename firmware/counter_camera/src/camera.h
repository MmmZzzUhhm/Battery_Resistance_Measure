/*
 * XIAO ESP32S3 Sense 内蔵カメラ
 */
#pragma once
#include "esp_camera.h"

bool cameraBegin();

// cfg.image_rotationに応じた向き補正・回転を適用して返す (0/180度はセンサーのレジスタのみ、
// 90/270度はソフトウェアでのJPEG再エンコードを伴う)。
// 成功時は呼び出し側で cameraReleaseFrame() で解放すること (esp_camera_fb_return()は使わないこと)。
// 失敗時はnullptr。
camera_fb_t* cameraCapture();
void cameraReleaseFrame(camera_fb_t* fb);
