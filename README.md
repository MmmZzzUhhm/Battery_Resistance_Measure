# 鉛蓄電池内部抵抗監視システム

複数の鉛蓄電池に取り付けた IWS7817(東京デバイセズ, I2C 交流インピーダンス方式)モジュールで
内部抵抗・電圧を測定し、子機→親機→ポータルサーバーの3階層でデータを収集・可視化するシステム。

設計の背景と決定事項は `docs/protocol.md` を参照。

## 構成

```
firmware/
  common/include/protocol.h   共有プロトコル定数のcanonical版 (docs/protocol.md 参照)
  child/                      子機ファームウェア (XIAO ESP32C6 + IWS7817 + PCF8563T)
  parent/                     親機ファームウェア (XIAO ESP32S3 + PCF8563T + microSD)
  child_hwtest/               子機基板 動作確認チェックアプリ (本番ファームとは独立)
  parent_hwtest/              親機基板 動作確認チェックアプリ (本番ファームとは独立)
  _legacy_m5atoms3/           旧: M5Stack AtomS3単体版 (参考保存, 新方式とは非互換)
portal/
  server/                     Node.js/Express + SQLite (親機からのデータ集約・OTA配信管理)
  client/                     React(Vite) ダッシュボード
docs/
  protocol.md                 通信プロトコル/データスキーマの単一情報源
pc_software/                  旧: 単体デバイス向けPythonデバッグツール (新方式とは非互換)
```

## 子機 (firmware/child)

- 通常はDeep Sleepし、`wake_interval_sec` 間隔でWake→IWS7817測定→親機と同期→再びDeep Sleep。
- 通信方式(BLE/WiFi)はデバイスごとに固定設定 (`link_mode`)。
- 初回起動時(device_id未設定)は自身をAPにして簡易Web画面で初期設定を行う
  (SSID `BATT-SETUP-xxxxxx` / パスワード `battsetup`)。

```
cd firmware/child
pio run            # ビルド
pio run -t upload   # 書き込み
pio device monitor  # シリアルログ
```

## 親機 (firmware/parent)

- 常時稼働。BLE Central(子機スキャン) + WiFi SoftAP(WiFi子機受付) + WiFi STA(ポータル/インターネット) を並行動作。
- microSD(SD_MMC 1bitモード)に全データを恒久保存し、ポータル未送信分をキューとして保持。
- ローカルWeb UI: `http://<親機IP>/` (ダッシュボード) / `http://<親機IP>/config` (設定・OTAアップロード)。
- 初期設定はデフォルトのSoftAP (`BATGW-xxxxxx` / パスワード `battgateway`) に接続し `/config` から行う。

```
cd firmware/parent
pio run
pio run -t upload
```

## 基板動作確認チェックアプリ (firmware/child_hwtest, firmware/parent_hwtest)

新規に起版した子機/親機基板の実装検査専用ツール。本番ファームウェアとは完全に独立した
PlatformIOプロジェクトで、量産検査工程で本番ファームと混在しないようにしている。

起動すると各ハードウェア項目を順に検査し、シリアルログにPASS/FAILを出力する。
配線ピンや検査対象デバイスの既定値は各 `platformio.ini` の `build_flags` (本番ファームと同じ既定値)
から実機配線に合わせて変更できる。

**子機基板** (`firmware/child_hwtest`): チップ情報 / I2Cバススキャン / IWS7817読取 /
PCF8563T RTC(設定→読出検証) / WiFiスキャン / BLEアドバタイズ / Deep Sleep+タイマーWake
(検査完了後に5秒間実際にDeep Sleepし自動復帰することを確認、その回のログで結果表示)。

```
cd firmware/child_hwtest
pio run -t upload
pio device monitor
```

**親機基板** (`firmware/parent_hwtest`): チップ情報 / I2Cバススキャン / PCF8563T RTC /
SD_MMC(1bitマウント+書込/読出/内容検証+空き容量) / WiFi SoftAP起動 / WiFi STAスキャン /
BLEアドバタイズ。検査後は自身が立てるAP `HWTEST-PARENT` (パスワード `hwtest1234`) 経由で
`http://192.168.4.1/` に結果ページを表示し続ける(「再検査」リンクでリブート→再検査)。

```
cd firmware/parent_hwtest
pio run -t upload
pio device monitor
```

## ポータルサーバー (portal/)

開発 (Windows PC):
```
cd portal/server && npm install && npm run dev   # http://localhost:8080
cd portal/client && npm install && npm run dev   # http://localhost:3000 (APIへ自動proxy)
```

本番 (Raspberry Pi 4 / Linux):
```
cd portal/client && npm install && npm run build   # dist/ を生成
cd portal/server  && npm install && npm start       # dist/ を静的配信 + API (http://<Pi>:8080)
```
`systemd` サービス化する場合は `npm start` を `ExecStart` に指定し、
`portal/server/data/` (SQLite DB・ファームウェア保管) を永続ボリュームにする。

## 既知の環境上の注意点

- **プロジェクトパスに日本語/空白を含む場合のビルド失敗**: 一部のWindows向けGCCツールチェイン
  (リンカ ld.exe) は日本語+空白混じりの絶対パスで `.map` ファイルを生成できず失敗する。
  `firmware/child/platformio.ini` / `firmware/parent/platformio.ini` では
  `[platformio] build_dir = C:/pio_build/...` でビルド出力をASCIIパスへ逃がして回避している。
  同様の環境で使う場合、他のツールチェインでも同じ回避策が必要になることがある。
- **protocol.h の複製**: 上記と同じ理由で、`-I` によるプロジェクト外(`firmware/common/`)への
  相対/絶対パス参照をリンカ/コンパイラが解決できないことがあったため、
  `firmware/child/include/protocol.h` と `firmware/parent/include/protocol.h` に
  `firmware/common/include/protocol.h` の複製を配置している。**値を変更する場合は3箇所とも修正すること。**
- **Google Drive の仮想ドライブ上での `npm install` は動作しない**: 本プロジェクトが
  Google Drive for Desktop の仮想ドライブ(例: `J:\マイドライブ\...`、ボリューム種別は
  実体としては `FAT32` 相当としてマウントされる仮想FS)配下にある場合、`npm install` は
  `EBADF`(bad file descriptor) や `ENOENT` を伴う展開エラーで失敗するか、極端に遅くなる。
  これはGoogle Driveの同期を一時停止しても解消しない(仮想ドライブのファイルシステム自体が
  npmの大量の小ファイル書き込みに対応していないため)。またこの仮想ドライブ上では
  NTFSジャンクション/シンボリックリンクの作成もできない。
  **`portal/server` と `portal/client` の `npm install` / `npm run dev` / `npm run build` は、
  必ずローカルディスク(例: `C:\dev\...`)にコピーした作業コピー、またはGoogle Drive
  以外の場所にcloneしたリポジトリで実行すること。** ソースコード(`src/`, `package.json`等)
  自体はDrive上に置いても問題ないが、`node_modules` の生成だけは必ずローカルディスクで行う。
