// 薄型カウンタカメラ・既存PTZカメラ共通の管理API。
// カメラは自発的にポータルへpushしない (pull型)。ポータル側が snapshot_url へ都度GETしにいく。
const express = require('express');
const fs = require('fs');
const path = require('path');
const { db, CAMERA_IMAGE_DIR } = require('../db');
const { captureCamera } = require('../services/cameraCapture');

const router = express.Router();

router.get('/cameras', (_req, res) => {
  const rows = db.prepare(`
    SELECT c.camera_id, c.label, c.snapshot_url, c.capture_interval_min,
           c.last_capture_at, c.last_capture_ok,
           (SELECT COUNT(*) FROM camera_captures WHERE camera_id = c.camera_id) AS capture_count
    FROM cameras c ORDER BY c.camera_id
  `).all();
  res.json(rows);
});

router.post('/cameras', (req, res) => {
  const { camera_id, label, snapshot_url, cf_access_client_id, cf_access_client_secret, capture_interval_min } = req.body || {};
  if (!camera_id || !snapshot_url) {
    return res.status(400).json({ ok: false, error: 'camera_id and snapshot_url required' });
  }
  db.prepare(
    `INSERT INTO cameras (camera_id, label, snapshot_url, cf_access_client_id, cf_access_client_secret, capture_interval_min, created_at)
     VALUES (?, ?, ?, ?, ?, ?, ?)`
  ).run(
    camera_id, label || camera_id, snapshot_url,
    cf_access_client_id || null, cf_access_client_secret || null,
    capture_interval_min || 720, Date.now()
  );
  res.json({ ok: true });
});

router.put('/cameras/:camera_id', (req, res) => {
  const existing = db.prepare('SELECT * FROM cameras WHERE camera_id = ?').get(req.params.camera_id);
  if (!existing) return res.status(404).json({ ok: false, error: 'not found' });

  const { label, snapshot_url, cf_access_client_id, cf_access_client_secret, capture_interval_min } = req.body || {};
  db.prepare(
    `UPDATE cameras SET label=?, snapshot_url=?, cf_access_client_id=?, cf_access_client_secret=?, capture_interval_min=?
     WHERE camera_id=?`
  ).run(
    label ?? existing.label,
    snapshot_url ?? existing.snapshot_url,
    cf_access_client_id !== undefined ? (cf_access_client_id || null) : existing.cf_access_client_id,
    cf_access_client_secret !== undefined ? (cf_access_client_secret || null) : existing.cf_access_client_secret,
    capture_interval_min ?? existing.capture_interval_min,
    req.params.camera_id
  );
  res.json({ ok: true });
});

router.delete('/cameras/:camera_id', (req, res) => {
  db.prepare('DELETE FROM cameras WHERE camera_id = ?').run(req.params.camera_id);
  db.prepare('DELETE FROM camera_captures WHERE camera_id = ?').run(req.params.camera_id);
  res.json({ ok: true });
});

// 今すぐ撮影 (手動ボタン・スケジューラの両方から使う共通処理)
router.post('/cameras/:camera_id/capture', async (req, res) => {
  const camera = db.prepare('SELECT * FROM cameras WHERE camera_id = ?').get(req.params.camera_id);
  if (!camera) return res.status(404).json({ ok: false, error: 'not found' });

  const result = await captureCamera(camera);
  res.status(result.ok ? 200 : 502).json(result);
});

router.get('/cameras/:camera_id/captures', (req, res) => {
  const limit = Math.min(Number(req.query.limit) || 100, 500);
  const rows = db.prepare(
    'SELECT * FROM camera_captures WHERE camera_id = ? ORDER BY captured_at DESC LIMIT ?'
  ).all(req.params.camera_id, limit);
  res.json(rows);
});

router.get('/camera-captures/:id/image', (req, res) => {
  const capture = db.prepare('SELECT * FROM camera_captures WHERE id = ?').get(req.params.id);
  if (!capture) return res.status(404).json({ ok: false, error: 'not found' });

  const filePath = path.join(CAMERA_IMAGE_DIR, capture.camera_id, capture.filename);
  if (!fs.existsSync(filePath)) return res.status(404).json({ ok: false, error: 'file missing' });

  res.setHeader('Content-Type', 'image/jpeg');
  fs.createReadStream(filePath).pipe(res);
});

// 将来のラズパイ上OCRワーカーが読み取り結果を書き込む用 (今回はエンドポイントのみ用意)
router.put('/camera-captures/:id/ocr', (req, res) => {
  const { counter_value, ocr_status } = req.body || {};
  db.prepare(
    'UPDATE camera_captures SET counter_value = ?, ocr_status = ? WHERE id = ?'
  ).run(counter_value ?? null, ocr_status || 'done', req.params.id);
  res.json({ ok: true });
});

module.exports = router;
