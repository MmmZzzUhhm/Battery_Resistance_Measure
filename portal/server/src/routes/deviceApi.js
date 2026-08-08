// 親機(ファームウェア)から呼ばれるAPI。docs/protocol.md §5 のスキーマに準拠する。
const express = require('express');
const fs = require('fs');
const path = require('path');
const { db, FIRMWARE_DIR } = require('../db');
const { gatewayAuth } = require('../middleware/gatewayAuth');

const router = express.Router();

// POST /api/v1/gateways/:gateway_id/measurements
router.post('/gateways/:gateway_id/measurements', gatewayAuth, (req, res) => {
  const gatewayId = req.params.gateway_id;
  const measurements = req.body && req.body.measurements;
  if (!Array.isArray(measurements)) {
    return res.status(400).json({ ok: false, error: 'measurements array required' });
  }

  const insertMeas = db.prepare(`
    INSERT INTO measurements (gateway_id, child_id, battery_id, seq, ts, r_mohm, v, valid, received_at)
    VALUES (@gateway_id, @child_id, @battery_id, @seq, @ts, @r_mohm, @v, @valid, @received_at)
  `);
  const upsertChild = db.prepare(`
    INSERT INTO children (child_id, gateway_id, battery_id, last_seen_at)
    VALUES (@child_id, @gateway_id, @battery_id, @last_seen_at)
    ON CONFLICT(child_id) DO UPDATE SET
      gateway_id = excluded.gateway_id,
      battery_id = COALESCE(excluded.battery_id, children.battery_id),
      last_seen_at = excluded.last_seen_at
  `);

  const now = Date.now();
  const tx = db.transaction((rows) => {
    for (const m of rows) {
      if (!m.child_id) continue;
      insertMeas.run({
        gateway_id: gatewayId,
        child_id: m.child_id,
        battery_id: m.battery_id || m.child_id,
        seq: m.seq || 0,
        ts: m.ts || Math.floor(now / 1000),
        r_mohm: m.r_mohm ?? null,
        v: m.v ?? null,
        valid: m.valid ? 1 : 0,
        received_at: now,
      });
      upsertChild.run({
        child_id: m.child_id,
        gateway_id: gatewayId,
        battery_id: m.battery_id || m.child_id,
        last_seen_at: now,
      });
    }
  });
  tx(measurements);

  res.json({ ok: true, received: measurements.length });
});

// GET /api/v1/gateways/:gateway_id/config
router.get('/gateways/:gateway_id/config', gatewayAuth, (req, res) => {
  const children = db
    .prepare('SELECT * FROM children WHERE gateway_id = ?')
    .all(req.params.gateway_id);

  res.json({
    children: children.map((c) => ({
      child_id: c.child_id,
      battery_id: c.battery_id,
      link_mode: c.link_mode,
      wake_interval_sec: c.wake_interval_sec,
      i2c_addr: c.i2c_addr,
    })),
    updated_at: new Date().toISOString(),
  });
});

// POST /api/v1/gateways/:gateway_id/heartbeat
router.post('/gateways/:gateway_id/heartbeat', gatewayAuth, (req, res) => {
  const { uptime_s, sd_free_kb, fw_version } = req.body || {};
  db.prepare(
    `UPDATE gateways SET last_heartbeat_at=?, uptime_s=?, sd_free_kb=?, fw_version=? WHERE gateway_id=?`
  ).run(Date.now(), uptime_s ?? null, sd_free_kb ?? null, fw_version ?? null, req.params.gateway_id);
  res.json({ ok: true });
});

// GET /api/v1/gateways/:gateway_id/firmware/pending?child_id=
router.get('/gateways/:gateway_id/firmware/pending', gatewayAuth, (req, res) => {
  const childId = req.query.child_id;
  if (!childId) return res.status(400).json({ ok: false, error: 'child_id required' });

  const target = db
    .prepare('SELECT * FROM firmware_targets WHERE gateway_id = ? AND child_id = ?')
    .get(req.params.gateway_id, childId);
  if (!target) return res.json({ available: false });

  const fw = db.prepare('SELECT * FROM firmware_images WHERE version = ?').get(target.version);
  if (!fw) return res.json({ available: false });

  res.json({
    available: true,
    version: fw.version,
    size: fw.size,
    md5: fw.md5,
    download_url: `/api/v1/firmware/blob/${encodeURIComponent(fw.version)}`,
  });
});

// GET /api/v1/firmware/blob/:version (親機がダウンロードしてSDにキャッシュする)
router.get('/firmware/blob/:version', (req, res) => {
  const fw = db.prepare('SELECT * FROM firmware_images WHERE version = ?').get(req.params.version);
  if (!fw) return res.status(404).json({ ok: false, error: 'not found' });

  const filePath = path.join(FIRMWARE_DIR, fw.filename);
  if (!fs.existsSync(filePath)) return res.status(404).json({ ok: false, error: 'file missing' });

  res.setHeader('Content-Type', 'application/octet-stream');
  fs.createReadStream(filePath).pipe(res);
});

module.exports = router;
