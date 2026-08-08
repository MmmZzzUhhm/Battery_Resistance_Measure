// React管理画面から呼ばれるAPI。親機からは呼ばれない。
const express = require('express');
const crypto = require('crypto');
const multer = require('multer');
const fs = require('fs');
const path = require('path');
const { db, FIRMWARE_DIR } = require('../db');

const router = express.Router();
const upload = multer({ dest: path.join(FIRMWARE_DIR, '.tmp') });
fs.mkdirSync(path.join(FIRMWARE_DIR, '.tmp'), { recursive: true });

// ── ゲートウェイ ────────────────────────────────────────────
router.get('/gateways', (_req, res) => {
  const rows = db.prepare('SELECT * FROM gateways ORDER BY created_at DESC').all();
  res.json(rows.map(({ api_key, ...rest }) => rest)); // api_keyは一覧では隠す
});

router.get('/gateways/:gateway_id/children', (req, res) => {
  const rows = db
    .prepare('SELECT * FROM children WHERE gateway_id = ? ORDER BY child_id')
    .all(req.params.gateway_id);
  res.json(rows);
});

router.put('/gateways/:gateway_id/children/:child_id', (req, res) => {
  const { gateway_id, child_id } = req.params;
  const { battery_id, link_mode, wake_interval_sec, i2c_addr } = req.body || {};

  const existing = db.prepare('SELECT * FROM children WHERE child_id = ?').get(child_id);
  if (!existing) {
    db.prepare(
      `INSERT INTO children (child_id, gateway_id, battery_id, link_mode, wake_interval_sec, i2c_addr, config_updated_at)
       VALUES (?, ?, ?, ?, ?, ?, ?)`
    ).run(
      child_id, gateway_id, battery_id || child_id,
      link_mode || 'ble', wake_interval_sec || 600, i2c_addr ?? 3, Date.now()
    );
  } else {
    db.prepare(
      `UPDATE children SET battery_id=?, link_mode=?, wake_interval_sec=?, i2c_addr=?, config_updated_at=?
       WHERE child_id=?`
    ).run(
      battery_id || existing.battery_id,
      link_mode || existing.link_mode,
      wake_interval_sec || existing.wake_interval_sec,
      i2c_addr ?? existing.i2c_addr,
      Date.now(),
      child_id
    );
  }
  res.json({ ok: true });
});

// ── 測定履歴 ────────────────────────────────────────────────
router.get('/gateways/:gateway_id/measurements', (req, res) => {
  const { child_id, from, to, limit } = req.query;
  let sql = 'SELECT * FROM measurements WHERE gateway_id = ?';
  const args = [req.params.gateway_id];
  if (child_id) { sql += ' AND child_id = ?'; args.push(child_id); }
  if (from)     { sql += ' AND ts >= ?'; args.push(Number(from)); }
  if (to)       { sql += ' AND ts <= ?'; args.push(Number(to)); }
  sql += ' ORDER BY ts DESC LIMIT ?';
  args.push(Math.min(Number(limit) || 500, 5000));

  res.json(db.prepare(sql).all(...args));
});

// ── ファームウェア ──────────────────────────────────────────
router.get('/firmware', (_req, res) => {
  res.json(db.prepare('SELECT * FROM firmware_images ORDER BY uploaded_at DESC').all());
});

router.post('/firmware', upload.single('file'), (req, res) => {
  const version = req.body && req.body.version;
  if (!version || !req.file) {
    return res.status(400).json({ ok: false, error: 'version and file required' });
  }

  const finalName = `${version}.bin`;
  const finalPath = path.join(FIRMWARE_DIR, finalName);
  fs.renameSync(req.file.path, finalPath);

  const buf = fs.readFileSync(finalPath);
  const md5 = crypto.createHash('md5').update(buf).digest('hex');

  db.prepare(
    `INSERT INTO firmware_images (version, filename, size, md5, uploaded_at)
     VALUES (?, ?, ?, ?, ?)
     ON CONFLICT(version) DO UPDATE SET filename=excluded.filename, size=excluded.size,
       md5=excluded.md5, uploaded_at=excluded.uploaded_at`
  ).run(version, finalName, buf.length, md5, Date.now());

  res.json({ ok: true, version, size: buf.length, md5 });
});

// 特定の gateway/child にバージョンを配信対象として割り当てる
router.put('/gateways/:gateway_id/children/:child_id/firmware-target', (req, res) => {
  const { gateway_id, child_id } = req.params;
  const { version } = req.body || {};
  if (!version) return res.status(400).json({ ok: false, error: 'version required' });

  const fw = db.prepare('SELECT * FROM firmware_images WHERE version = ?').get(version);
  if (!fw) return res.status(404).json({ ok: false, error: 'unknown firmware version' });

  db.prepare(
    `INSERT INTO firmware_targets (gateway_id, child_id, version) VALUES (?, ?, ?)
     ON CONFLICT(gateway_id, child_id) DO UPDATE SET version=excluded.version`
  ).run(gateway_id, child_id, version);

  res.json({ ok: true });
});

router.delete('/gateways/:gateway_id/children/:child_id/firmware-target', (req, res) => {
  db.prepare('DELETE FROM firmware_targets WHERE gateway_id = ? AND child_id = ?').run(
    req.params.gateway_id, req.params.child_id
  );
  res.json({ ok: true });
});

module.exports = router;
