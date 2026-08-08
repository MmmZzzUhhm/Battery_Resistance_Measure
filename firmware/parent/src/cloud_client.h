/*
 * 親機 → ポータル(クラウド相当) の同期 (docs/protocol.md §5)
 * WiFi STAでインターネット/社内網に接続済みの時のみ意味を持つ。
 */
#pragma once

// 未送信キューのアップロード・設定取得・heartbeat・OTA配信確認を1サイクル実行する。
// 呼び出し側 (main.cpp) が cfg.cloud_sync_interval_sec 間隔で呼ぶ。
void cloudSyncTick();
