/*
 * NTP同期 (cfg.ntp_server1/2 を使用。ONVIF GetNTP/SetNTPで変更可能)
 * 成功時はシステム時刻に加えてPCF8563T RTCも補正する。
 */
#pragma once

bool ntpSyncNow();
