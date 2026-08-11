// カメラ1台分の「スナップショットURLへGETしにいき、結果を保存する」処理。
// 管理APIの「今すぐ撮影」ボタンと cameraScheduler.js の定期実行の両方から共用する。
const fs = require('fs');
const path = require('path');
const { db, CAMERA_IMAGE_DIR } = require('../db');

async function captureCamera(camera) {
  const now = Date.now();
  try {
    const headers = {};
    if (camera.cf_access_client_id) {
      headers['CF-Access-Client-Id'] = camera.cf_access_client_id;
      headers['CF-Access-Client-Secret'] = camera.cf_access_client_secret;
    }
    const res = await fetch(camera.snapshot_url, { headers });
    if (!res.ok) {
      throw new Error(`HTTP ${res.status}`);
    }
    const buf = Buffer.from(await res.arrayBuffer());
    if (buf.length === 0) throw new Error('empty response body');

    const dir = path.join(CAMERA_IMAGE_DIR, camera.camera_id);
    fs.mkdirSync(dir, { recursive: true });
    const filename = `${now}.jpg`;
    fs.writeFileSync(path.join(dir, filename), buf);

    const info = db.prepare(
      `INSERT INTO camera_captures (camera_id, filename, size, captured_at)
       VALUES (?, ?, ?, ?)`
    ).run(camera.camera_id, filename, buf.length, now);

    db.prepare(
      'UPDATE cameras SET last_capture_at = ?, last_capture_ok = 1 WHERE camera_id = ?'
    ).run(now, camera.camera_id);

    return { ok: true, id: info.lastInsertRowid, filename, size: buf.length };
  } catch (e) {
    db.prepare(
      'UPDATE cameras SET last_capture_at = ?, last_capture_ok = 0 WHERE camera_id = ?'
    ).run(now, camera.camera_id);
    return { ok: false, error: e.message };
  }
}

module.exports = { captureCamera };
