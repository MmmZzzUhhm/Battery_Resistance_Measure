/*
 * 子機(XIAO ESP32C6)/親機(XIAO ESP32S3) 共有プロトコル定数 (canonical版)
 * 詳細仕様: docs/protocol.md を参照。
 *
 * このファイルを変更したら firmware/child/include/protocol.h と
 * firmware/parent/include/protocol.h の複製にも同じ内容を反映すること。
 * (Windowsの一部ツールチェインが日本語+空白混じりの絶対パスへの -I 参照を
 *  解決できないため、各プロジェクト側に複製を置く運用としている)
 */
#pragma once

// ── プロトコルバージョン ─────────────────────────────────────
#define PROTOCOL_VERSION 1

// ── BLE GATT: サービス/特性 UUID (docs/protocol.md §2.1) ────
#define BLE_SVC_UUID          "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0000"
#define BLE_CHR_STATUS_UUID   "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0001"
#define BLE_CHR_TIME_UUID     "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0002"
#define BLE_CHR_DATA_UUID     "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0003"
#define BLE_CHR_DATA_ACK_UUID "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0004"
#define BLE_CHR_CONFIG_UUID   "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0005"
#define BLE_CHR_OTA_CTRL_UUID "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0006"
#define BLE_CHR_OTA_DATA_UUID "6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0007"

// BLEの1回の同期セッションで許容する最大接続時間 [ms]
#define BLE_SYNC_TIMEOUT_MS   20000
// DATA特性の1回のReadで返す最大測定件数 (ATT値512byte上限に収めるため)
#define BLE_DATA_BATCH_MAX    6

// ── WiFi (子機↔親機ローカルAP) ───────────────────────────────
#define WIFI_SYNC_PATH        "/api/child/sync"
#define WIFI_FIRMWARE_PATH    "/api/child/firmware"
#define PARENT_AP_DEFAULT_IP  "192.168.4.1"
#define PARENT_AP_SSID_PREFIX "BATGW-"
#define CHILD_WIFI_SYNC_TIMEOUT_MS 15000

// ── 親機↔ポータル REST API ───────────────────────────────────
#define CLOUD_API_KEY_HEADER  "X-API-Key"
#define CLOUD_MEASUREMENTS_PATH_FMT   "/api/v1/gateways/%s/measurements"
#define CLOUD_CONFIG_PATH_FMT         "/api/v1/gateways/%s/config"
#define CLOUD_HEARTBEAT_PATH_FMT      "/api/v1/gateways/%s/heartbeat"
#define CLOUD_FW_PENDING_PATH_FMT     "/api/v1/gateways/%s/firmware/pending?child_id=%s"
#define CLOUD_FW_BLOB_PATH_FMT        "/api/v1/firmware/blob/%s"

// ── 既定値 ────────────────────────────────────────────────────
#define DEFAULT_WAKE_INTERVAL_SEC  600     // 子機の既定Wake間隔 (10分)
#define MIN_WAKE_INTERVAL_SEC      60
#define DEFAULT_I2C_ADDR           0x03    // IWS7817 既定I2Cアドレス
#define PENDING_QUEUE_MAX_RECORDS  30       // 子機NVS内 未送信キューの最大件数

// ── 測定データJSON フィールド名 (docs/protocol.md §1) ─────────
#define FLD_SEQ         "seq"
#define FLD_TS          "ts"
#define FLD_R_MOHM      "r_mohm"
#define FLD_V           "v"
#define FLD_VALID       "valid"
#define FLD_CHILD_ID    "child_id"
#define FLD_BATTERY_ID  "battery_id"
#define FLD_BATT_MV     "batt_mv"
#define FLD_RSSI        "rssi"
