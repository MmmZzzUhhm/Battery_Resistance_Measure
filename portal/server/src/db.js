// SQLite (better-sqlite3) データベース初期化。
// data/portal.db にファイルを作成する。Windows開発機/Raspberry Pi 4本番どちらでも
// 同じファイルベースDBで動作する (外部DBサーバー不要)。
const path = require('path');
const fs = require('fs');
const Database = require('better-sqlite3');

const DATA_DIR = process.env.PORTAL_DATA_DIR || path.join(__dirname, '..', 'data');
const FIRMWARE_DIR = path.join(DATA_DIR, 'firmware');
const CAMERA_IMAGE_DIR = path.join(DATA_DIR, 'camera_images');

fs.mkdirSync(DATA_DIR, { recursive: true });
fs.mkdirSync(FIRMWARE_DIR, { recursive: true });
fs.mkdirSync(CAMERA_IMAGE_DIR, { recursive: true });

const db = new Database(path.join(DATA_DIR, 'portal.db'));
db.pragma('journal_mode = WAL');

db.exec(`
CREATE TABLE IF NOT EXISTS gateways (
  gateway_id TEXT PRIMARY KEY,
  api_key TEXT NOT NULL,
  created_at INTEGER NOT NULL,
  last_heartbeat_at INTEGER,
  uptime_s INTEGER,
  sd_free_kb INTEGER,
  fw_version TEXT
);

CREATE TABLE IF NOT EXISTS children (
  child_id TEXT PRIMARY KEY,
  gateway_id TEXT NOT NULL,
  battery_id TEXT,
  link_mode TEXT NOT NULL DEFAULT 'ble',
  wake_interval_sec INTEGER NOT NULL DEFAULT 600,
  i2c_addr INTEGER NOT NULL DEFAULT 3,
  last_seen_at INTEGER,
  config_updated_at INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS measurements (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  gateway_id TEXT NOT NULL,
  child_id TEXT NOT NULL,
  battery_id TEXT,
  seq INTEGER,
  ts INTEGER NOT NULL,
  r_mohm REAL,
  v REAL,
  valid INTEGER NOT NULL DEFAULT 1,
  received_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_measurements_child_ts ON measurements(child_id, ts);

CREATE TABLE IF NOT EXISTS firmware_images (
  version TEXT PRIMARY KEY,
  filename TEXT NOT NULL,
  size INTEGER NOT NULL,
  md5 TEXT NOT NULL,
  uploaded_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS firmware_targets (
  gateway_id TEXT NOT NULL,
  child_id TEXT NOT NULL,
  version TEXT NOT NULL,
  PRIMARY KEY (gateway_id, child_id)
);

-- カメラはポータル側から都度スナップショットURLへGETしにいくpull型 (push受信は行わない)。
-- snapshot_url はONVIF(薄型カメラ)/独自CGI(既存PTZカメラ)いずれも「GETすればJPEGが返る」
-- 前提の共通インターフェースとして扱う。Cloudflare Access配下にある場合はcf_access_*を設定する。
CREATE TABLE IF NOT EXISTS cameras (
  camera_id TEXT PRIMARY KEY,
  label TEXT,
  snapshot_url TEXT NOT NULL,
  cf_access_client_id TEXT,
  cf_access_client_secret TEXT,
  capture_interval_min INTEGER NOT NULL DEFAULT 720,
  created_at INTEGER NOT NULL,
  last_capture_at INTEGER,
  last_capture_ok INTEGER
);

CREATE TABLE IF NOT EXISTS camera_captures (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  camera_id TEXT NOT NULL,
  filename TEXT NOT NULL,
  size INTEGER NOT NULL,
  captured_at INTEGER NOT NULL,
  counter_value REAL,
  ocr_status TEXT NOT NULL DEFAULT 'pending'
);
CREATE INDEX IF NOT EXISTS idx_camera_captures_camera_ts ON camera_captures(camera_id, captured_at);
`);

module.exports = { db, FIRMWARE_DIR, CAMERA_IMAGE_DIR };
