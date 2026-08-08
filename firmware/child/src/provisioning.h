/*
 * 初回プロビジョニング (device_id 未設定時のみ)。
 * 自身をAPとして立ち上げ、簡易Webフォームで device_id/link_mode/WiFi情報等を設定させる。
 * 通常運用(Deep Sleepサイクル)には入らず、保存後に再起動して通常フローへ移行する。
 */
#pragma once

void provisioningBegin();
void provisioningLoop();
