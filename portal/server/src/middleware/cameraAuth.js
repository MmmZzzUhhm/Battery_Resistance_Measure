// 薄型カウンタカメラ向けエンドポイントの認証。
// 初回アクセス時はそのAPIキーで自動登録する(TOFU: Trust On First Use)。
// 2回目以降は登録済みのAPIキーと一致しない場合401を返す。
// gatewayAuth.js と同じ方式を cameras テーブル向けに複製したもの。
const { db } = require('../db');

function cameraAuth(req, res, next) {
  const cameraId = req.params.camera_id;
  const apiKey = req.header('X-API-Key');

  if (!apiKey) {
    return res.status(401).json({ ok: false, error: 'X-API-Key header required' });
  }

  const existing = db.prepare('SELECT * FROM cameras WHERE camera_id = ?').get(cameraId);
  if (!existing) {
    db.prepare(
      'INSERT INTO cameras (camera_id, api_key, created_at) VALUES (?, ?, ?)'
    ).run(cameraId, apiKey, Date.now());
    req.camera = { camera_id: cameraId, api_key: apiKey };
    return next();
  }

  if (existing.api_key !== apiKey) {
    return res.status(401).json({ ok: false, error: 'invalid api key' });
  }
  req.camera = existing;
  next();
}

module.exports = { cameraAuth };
