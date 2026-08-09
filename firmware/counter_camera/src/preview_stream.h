/*
 * 設置時の画角調整用 簡易MJPEGプレビュー (ONVIF標準外、本機独自のローカル機能)
 *
 * NOTE: ESP32のWebServerはシングルスレッドのため、プレビュー中は他のHTTP
 *       リクエスト(ONVIF/設定API等)を処理できない。設置時の一時的な使用を
 *       想定しており、常時接続したままにしないこと。
 */
#pragma once

void previewStreamRegisterRoute();
