// カメラ1台分の「スナップショットURLへGETしにいき、結果を保存する」処理。
// 管理APIの「今すぐ撮影」ボタンと cameraScheduler.js の定期実行の両方から共用する。
const fs = require('fs');
const path = require('path');
const { db, CAMERA_IMAGE_DIR } = require('../db');
const { recognizeCounter } = require('./ocrClient');

// 撮影完了後に非同期でOCR(VCBカウンター認識)を実行し、完了したらDBへ書き戻す。
// OCRは10〜30秒程度かかるため、撮影処理自体(captureCamera)の応答を待たせないよう
// fire-and-forgetで実行する。呼び出し直後のocr_statusは既定の'pending'のまま。
function runOcrForCapture(captureId, imagePath, cameraId, capturedAtMs) {
  const capturedAtIso = new Date(capturedAtMs).toISOString();
  recognizeCounter(imagePath, cameraId, capturedAtIso).then((result) => {
    const ocrStatus = result.status === 'success' ? 'done' : (result.status || 'error');
    db.prepare(
      'UPDATE camera_captures SET counter_value = ?, ocr_status = ?, ocr_raw = ? WHERE id = ?'
    ).run(result.number_text ?? null, ocrStatus, JSON.stringify(result), captureId);
  }).catch((e) => {
    db.prepare(
      'UPDATE camera_captures SET ocr_status = ?, ocr_raw = ? WHERE id = ?'
    ).run('error', JSON.stringify({ error: e.message }), captureId);
  });
}

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
    const filePath = path.join(dir, filename);
    fs.writeFileSync(filePath, buf);

    const info = db.prepare(
      `INSERT INTO camera_captures (camera_id, filename, size, captured_at)
       VALUES (?, ?, ?, ?)`
    ).run(camera.camera_id, filename, buf.length, now);

    db.prepare(
      'UPDATE cameras SET last_capture_at = ?, last_capture_ok = 1 WHERE camera_id = ?'
    ).run(now, camera.camera_id);

    runOcrForCapture(info.lastInsertRowid, filePath, camera.camera_id, now);

    return { ok: true, id: info.lastInsertRowid, filename, size: buf.length };
  } catch (e) {
    db.prepare(
      'UPDATE cameras SET last_capture_at = ?, last_capture_ok = 0 WHERE camera_id = ?'
    ).run(now, camera.camera_id);
    return { ok: false, error: e.message };
  }
}

module.exports = { captureCamera };
