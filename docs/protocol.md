# 通信プロトコル仕様 (単一情報源)

子機ファームウェア(`firmware/child`)・親機ファームウェア(`firmware/parent`)・ポータルサーバー(`portal/`)は
すべてこのドキュメントに定義されたスキーマ/UUID/エンドポイントに従う。定数値は
`firmware/common/include/protocol.h` にも同じ値で定義されており、ズレが生じた場合は
このファイルを正とする。

## 1. 共通測定データスキーマ

子機での1回の測定結果。BLE/WiFiどちらの経路でも同じフィールド名を使う。

| フィールド | 型 | 説明 |
|---|---|---|
| `seq` | uint32 | 子機ローカルの単調増加シーケンス番号(再送・ACK・重複排除に使用) |
| `ts` | int64 (unix epoch秒) | 子機PCF8563Tから取得した測定時刻 |
| `r_mohm` | float | 内部抵抗 [mΩ]。IWS7817が測定不可の場合 `-1.0` |
| `v` | float | 電池電圧 [V] |
| `valid` | bool | IWS7817からの読み取りが成功したか |

親機がポータルへ送る際は、上記に子機/電池を特定する情報を付加する:

| フィールド | 型 | 説明 |
|---|---|---|
| `child_id` | string | 子機のdevice_id |
| `battery_id` | string | 電池識別子(通常device_idと同じだが独立設定可) |
| `batt_mv` | int | 子機自身の電源電圧 [mV](子機ノードの健全性用、任意) |
| `rssi` | int | 受信時のBLE/WiFi RSSI(任意) |

## 2. 子機↔親機: BLEプロトコル

子機 = GATT **Server(Peripheral)**、親機 = GATT **Client(Central)**。
常時稼働する親機がスキャンし、子機のアドバタイズ(Wake時のみ)を検出して接続する。

### 2.1 サービス/特性 UUID

Service UUID: `6f1e2a00-6d5c-4a3b-9d1e-2a6f1e2a0000`

| 特性名 | UUID | プロパティ | 方向 | 内容 |
|---|---|---|---|---|
| STATUS | `...0001` | Read | 子機→親機 | JSON: `{device_id, fw_version, battery_id, batt_mv, rtc_epoch, pending_count}` |
| TIME | `...0002` | Write | 親機→子機 | 8byte little-endian int64 = 親機の現在epoch秒。子機はこれでRTCを補正 |
| DATA | `...0003` | Read | 子機→親機 | 未送信測定データのJSON配列(1回の読み取りで最大6件、ATT値512byte上限に収める。残りは`DATA_ACK`後に再読み取りして分割取得) |
| DATA_ACK | `...0004` | Write | 親機→子機 | JSON: `{"ack_seq": N}` 子機は `seq <= ack_seq` のキュー項目を破棄 |
| CONFIG | `...0005` | Read/Write | 双方向 | JSON設定オブジェクト(§4)。Read=子機の現在設定、Write=親機からの新設定(子機はNVS保存し次回Readで反映確認) |
| OTA_CTRL | `...0006` | Write | 親機→子機 | JSON制御フレーム: `{"cmd":"begin","size":N,"md5":"hex"}` / `{"cmd":"end"}` / `{"cmd":"abort"}` |
| OTA_DATA | `...0007` | Write (No Response) | 親機→子機 | ファームウェア生バイト列。1回の書込= 1チャンク(MTU-3バイト程度)。子機は受信の都度 `Update.write()` に流し込む |

ATT値は仕様上512byteまで自動的にフラグメント/再結合される(Read Blob / Prepare-Execute Write)ため、
STATUS/DATA/CONFIGはアプリ側でのチャンク分割実装は不要。OTA_DATAのみファーム全体が512byteを大きく
超えるため、`OTA_CTRL`の`begin`で宣言したサイズに達するまで複数回の書込をストリームとして扱う。

### 2.2 BLE同期シーケンス(親機視点)

1. スキャンで既知の子機(サービスUUID+advデータ内device_id)を発見 → 接続
2. `TIME` へ現在epoch秒を書込
3. `CONFIG` を Read → 保留中の設定変更があれば `CONFIG` へ Write
4. `DATA` を Read → 空になるまで繰り返し取得し、都度 `DATA_ACK` を Write
5. 該当子機宛のOTAが保留されていれば `OTA_CTRL{begin}` → `OTA_DATA` 連続書込 → `OTA_CTRL{end}`
6. 切断(子機は切断をトリガに測定間隔分のDeep Sleepへ)

接続から切断まで最大20秒のタイムアウトを設ける。

## 3. 子機↔親機: WiFiプロトコル

子機 = HTTP **クライアント**、親機 = 親機自身のSoftAP上で HTTP **サーバー**。

Base URL: `http://<parent_ap_ip>` (既定 `192.168.4.1`)

### `POST /api/child/sync`

Request:
```json
{
  "device_id": "batt-01",
  "fw_version": "1.0.0",
  "battery_id": "batt-01",
  "batt_mv": 3710,
  "rtc_epoch": 1785000000,
  "measurements": [
    {"seq": 101, "ts": 1785000000, "r_mohm": 4.523, "v": 12.68, "valid": true}
  ]
}
```

Response:
```json
{
  "ok": true,
  "server_epoch": 1785000042,
  "ack_seq": 101,
  "config": { "...CONFIGスキーマ(§4)、変更がある時のみ含む..." : null },
  "ota": {"available": true, "version": "1.1.0", "size": 734112, "md5": "…", "url": "/api/child/firmware?version=1.1.0"}
}
```

### `GET /api/child/firmware?version=1.1.0`

`application/octet-stream` でファームウェアバイナリ全体を返す。子機はストリームしながら `Update.write()` する。

## 4. 子機設定(CONFIG)スキーマ

BLEの`CONFIG`特性・WiFiの`sync`レスポンス`config`フィールド、共通:

```json
{
  "device_id": "batt-01",
  "battery_id": "batt-01",
  "link_mode": "ble",
  "wake_interval_sec": 600,
  "i2c_addr": 3,
  "wifi_ssid": "BATGW-01",
  "wifi_pass": "xxxxxxxx",
  "fw_version": "1.0.0"
}
```

- `link_mode`: `"ble"` または `"wifi"`。デバイスごとに固定選択(自動フォールバックなし)
- `wifi_ssid`/`wifi_pass`: `link_mode=wifi` の時のみ有効。既定で親機自身のSoftAP情報
- 変更は親機経由でのみ配信される(子機は直接クラウドと通信しない)

## 5. 親機↔ポータル REST API

親機 = HTTPSクライアント、ポータル = サーバー。認証は全リクエストにヘッダ `X-API-Key: <gateway_api_key>` を付与。
失敗時は共通で `{"ok": false, "error": "message"}` + 4xx/5xx を返す。

### `POST /api/v1/gateways/:gateway_id/measurements`

Request:
```json
{
  "measurements": [
    {"child_id":"batt-01","battery_id":"batt-01","seq":101,
     "ts":1785000000,"r_mohm":4.523,"v":12.68,
     "valid":true,"batt_mv":3710,"rssi":-55}
  ]
}
```
`ts` は unix epoch秒(数値)。子機↔親機と同じ表現を使うことで、親機ファームウェアでの
ISO8601変換処理を不要にしている。ポータル側でDB保存/表示時にDateへ変換する。

Response: `{"ok": true, "received": 1}`

### `GET /api/v1/gateways/:gateway_id/config`

Response:
```json
{
  "children": [
    {"child_id":"batt-01","link_mode":"ble","wake_interval_sec":600,"i2c_addr":3}
  ],
  "updated_at": "2026-07-26T00:00:00+09:00"
}
```
親機は前回取得内容とdiffを取り、変更があった子機のみ次回同期時にCONFIGを配信する。

### `POST /api/v1/gateways/:gateway_id/heartbeat`

Request: `{"uptime_s":12345,"sd_free_kb":102400,"children_seen":["batt-01","batt-02"],"fw_version":"1.0.0"}`
Response: `{"ok": true}`

### `GET /api/v1/gateways/:gateway_id/firmware/pending?child_id=batt-01`

Response: `{"available":true,"version":"1.1.0","size":734112,"md5":"…","download_url":"/api/v1/firmware/blob/1.1.0"}`
(利用可能なOTAが無い場合は `{"available":false}`)

### `GET /api/v1/firmware/blob/:version`

`application/octet-stream`。親機は一度SDへ保存しキャッシュしてから子機へ配信する。

### ポータル管理用API(親機からは呼ばれない、React管理画面専用)

- `POST /api/v1/gateways/:gateway_id/firmware` … .binアップロード(multipart)
- `PUT /api/v1/gateways/:gateway_id/config` … 子機設定の変更登録
- `GET /api/v1/gateways/:gateway_id/measurements?child_id=&from=&to=` … 履歴取得(グラフ表示用)

## 6. 時刻同期の方針

- ポータルサーバー: OS標準のNTPで正確な時刻を保持(信頼できる時刻源)
- 親機: WiFi STA接続時に `configTime()`(NTP)でPCF8563Tを較正。未接続時は電池バックアップされたRTCの時刻をそのまま使用
- 子機: 直接NTPと通信しない。BLEの`TIME`特性、またはWiFi `sync`レスポンスの`server_epoch`で親機から都度時刻を受け取りPCF8563Tを補正

## 7. バージョニング

このプロトコルは `PROTOCOL_VERSION = 1` とする(`protocol.h` 参照)。将来の非互換変更time は
値をインクリメントし、親機側で子機/ポータル双方のバージョン不一致を検知できるようにする。
