# ポータルサーバーのRaspberry Pi 4セットアップ手順

`portal/`(Node.js/Express + React)を本番運用するRaspberry Pi 4向けの、Node.js/npm環境構築手順。
開発PC(Windows)からWiFi経由でSSH接続し、ラズパイ側で作業することを前提とする。

前提: Raspberry Pi OSがインストール済みで、SSH接続ができる状態であること。

## 1. 現在の状態を確認する

SSHでログイン後、以下を実行する。

```bash
cat /etc/os-release        # OSのバージョン確認
uname -m                   # CPUアーキテクチャ確認
```

`uname -m` の結果が重要:

- `aarch64` → 64bit版 Raspberry Pi OS (推奨。現在の標準)
- `armv7l` → 32bit版 (古い。可能なら64bit版に入れ直すことを検討する。
  `better-sqlite3`(後述)のビルドで詰まりやすい)

## 2. パッケージを最新化

```bash
sudo apt update
sudo apt upgrade -y
```

## 3. Node.jsをインストール

Raspberry Pi OS標準の `apt install nodejs` は古いバージョンが入ることが多いため、
**NodeSource公式リポジトリ** 経由でNode.js 20 (LTS) を入れる。

```bash
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs
```

このスクリプトは「NodeSourceのapt用リポジトリ情報をシステムに追加する」もので、実行にはsudo権限が必要。
中身に不安があれば、実行前に `curl -fsSL https://deb.nodesource.com/setup_20.x` だけで内容を表示して
確認してから進めてもよい。

## 4. インストール確認

```bash
node -v     # v20.x.x のように出ればOK (portal/server/package.json の engines は >=18 なので満たす)
npm -v
```

## 5. ビルドツールを用意 (念のため)

`portal/server` は DB に `better-sqlite3` (C++のネイティブモジュール) を使っている。
プリビルド済みバイナリが無い環境の場合ソースからビルドされるため、失敗しないよう先に入れておく。

```bash
sudo apt install -y build-essential python3 git
```

## 6. コードをラズパイに転送する

`git` が使えるなら、GitHubにpushしてあるリポジトリをそのまま clone するのが簡単。

```bash
git clone https://github.com/<あなたのアカウント>/<リポジトリ名>.git
cd <リポジトリ名>/portal
```

(gitを使わない場合は、PC側から `scp -r` で `portal/` フォルダごと転送する方法もあるが、
`node_modules` はOS/CPUアーキテクチャ依存のビルド済みバイナリを含むため、
**Windows上の`node_modules`はコピーしない**こと。転送前に削除してから送るか、
転送後にラズパイ側で削除して7.のインストールをやり直す。)

## 7. インストール・ビルド・起動

```bash
cd server && npm install       # ここでbetter-sqlite3がラズパイ用にビルド/取得される
cd ../client && npm install && npm run build
cd ../server && npm start
```

`npm start` 後、ブラウザで `http://<ラズパイのIPアドレス>:8080/` にアクセスできれば成功。
ラズパイのIPアドレスは `hostname -I` で確認できる。

## トラブルシューティング

- `server` の `npm install` でエラーが出る場合、特に `better-sqlite3` 関連のビルドエラーが起きやすい。
  5. のビルドツールが入っているか、OSが64bit (`aarch64`) かを再確認する。
- PC(Windows)で作成した `node_modules` をそのまま持ち込むと、アーキテクチャ不一致で
  ネイティブモジュールがロードできずに起動時エラーになる。必ずラズパイ上で `npm install` をやり直すこと。
  一方、ソースコード自体(`.js`/`.jsx`/`.css`など)や `portal/client` のビルド出力(`dist/`)は
  純粋な静的ファイルのためOS/CPUアーキテクチャに依存せず、PCで作ったものをコピーしても問題ない。

## 今後: 自動起動化 (systemd)

再起動時に自動でポータルサーバーを起動したい場合は、`npm start` を `systemd` サービスの `ExecStart`
に指定する方法がある。必要になったら別途手順を追記する。
