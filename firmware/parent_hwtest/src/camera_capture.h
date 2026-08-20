/*
 * 薄型カウンタカメラ構成向け: 撮影してSDカードへ保存するテストモード用インタラクティブ機能。
 * test_camera.cpp (自動判定テスト) とは別に、Web UIからのオンデマンド撮影用に用意する。
 */
#pragma once
#include <Arduino.h>
#include <vector>

#define CAMERA_SAVE_DIR "/hwtest_camera"

// カメラ・SDを初期化する (複数回呼んでも安全、初回のみ実処理)。失敗時false。
bool cameraCaptureBegin(String& errorMsg);

// 撮影してSDへ保存する。成功時savedPathに保存先パス(SD上の絶対パス)を格納しtrueを返す。
bool cameraCaptureAndSave(String& savedPath, String& errorMsg);

// 保存済み画像のファイル名一覧 (新しい順、最大maxCount件)。
std::vector<String> cameraListSavedImages(int maxCount);
