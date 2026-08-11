// カメラの定期撮影スケジューラ。
// 1分ごとに全カメラをチェックし、capture_interval_min を超えて未撮影のカメラのみ撮影する。
// (カウンタは異常時のみ動くため、標準では半日〜1日に1回程度の頻度で十分)
const { db } = require('./db');
const { captureCamera } = require('./services/cameraCapture');

const TICK_MS = 60 * 1000;

async function tick() {
  const now = Date.now();
  const cameras = db.prepare('SELECT * FROM cameras').all();
  for (const camera of cameras) {
    const dueAt = (camera.last_capture_at || 0) + camera.capture_interval_min * 60 * 1000;
    if (now < dueAt) continue;
    const result = await captureCamera(camera);
    if (!result.ok) {
      console.warn(`[cameraScheduler] capture failed for ${camera.camera_id}: ${result.error}`);
    }
  }
}

function start() {
  setInterval(() => { tick().catch((e) => console.error('[cameraScheduler] tick error', e)); }, TICK_MS);
}

module.exports = { start };
