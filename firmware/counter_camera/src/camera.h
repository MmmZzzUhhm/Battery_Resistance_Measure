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

// cfgのimg_*設定(明るさ・コントラスト・ホワイトバランス・露出・ゲイン等)をすべてセンサーへ
// 反映する。cameraBegin()内で初回適用される他、Web UI/ONVIFで変更した直後にも呼ぶこと。
// カメラ未初期化(センサー取得前)なら何もしない。
void cameraApplySensorSettings();
