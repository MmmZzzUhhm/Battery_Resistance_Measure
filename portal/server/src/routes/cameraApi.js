// 薄型カウンタカメラ関連API。
// デバイス向け(cameraAuth必須)とReact管理画面向け(認証なし、adminApi.jsと同様の扱い)を
// 1ファイルにまとめている(gateways系とは全く別のデバイス種別のため、既存ファイルは肥大化させない)。
const express = require('express');
const fs = require('fs');
const path = require('path');
const { db, CAMERA_IMAGE_DIR } = require('../db');
const { cameraAuth } = require('../middleware/cameraAuth');

const router = express.Router();

// ── デバイス向け ────────────────────────────────────────────
// POST /api/v1/cameras/:camera_id/images (生JPEGボディ)
router.post(
  '/cameras/:camera_id/images',
  cameraAuth,
  express.raw({ type: 'image/jpeg', limit: '5mb' }),
  (req, res) => {
    const cameraId = req.params.camera_id;
    if (!Buffer.isBuffer(req.body) || req.body.length === 0) {
      return res.status(400).json({ ok: false, error: 'image/jpeg body required' });
    }

    const now = Date.now();
    const capturedAtHeader = req.header('X-Captured-At');
    const capturedAt = capturedAtHeader ? Number(capturedAtHeader) * 1000 : null;

    const dir = path.join(CAMERA_IMAGE_DIR, cameraId);
    fs.mkdirSync(dir, { recursive: true });
    const filename = `${now}.jpg`;
    fs.writeFileSync(path.join(dir, filename), req.body);

    const info = db.prepare(
      `INSERT INTO camera_captures (camera_id, filename, size, captured_at, received_at)
       VALUES (?, ?, ?, ?, ?)`
    ).run(cameraId, filename, req.body.length, capturedAt, now);

    db.prepare('UPDATE cameras SET last_seen_at = ? WHERE camera_id = ?').run(now, cameraId);

    res.json({ ok: true, id: info.lastInsertRowid, filename });
  }
);

// ── React管理画面向け ────────────────────────────────────────
router.get('/cameras', (_req, res) => {
  const rows = db.prepare(`
    SELECT c.camera_id, c.last_seen_at,
           (SELECT COUNT(*) FROM camera_captures WHERE camera_id = c.camera_id) AS capture_count
    FROM cameras c ORDER BY c.last_seen_at DESC
  `).all();
  res.json(rows);
});

router.get('/cameras/:camera_id/captures', (req, res) => {
  const limit = Math.min(Number(req.query.limit) || 100, 500);
  const rows = db.prepare(
    'SELECT * FROM camera_captures WHERE camera_id = ? ORDER BY received_at DESC LIMIT ?'
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
