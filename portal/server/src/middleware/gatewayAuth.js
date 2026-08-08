// 親機(gateway)向けエンドポイントの認証。
// 初回アクセス時はそのAPIキーで自動登録する(TOFU: Trust On First Use)。
// 2回目以降は登録済みのAPIキーと一致しない場合401を返す。
const { db } = require('../db');

function gatewayAuth(req, res, next) {
  const gatewayId = req.params.gateway_id;
  const apiKey = req.header('X-API-Key');

  if (!apiKey) {
    return res.status(401).json({ ok: false, error: 'X-API-Key header required' });
  }

  const existing = db.prepare('SELECT * FROM gateways WHERE gateway_id = ?').get(gatewayId);
  if (!existing) {
    db.prepare(
      'INSERT INTO gateways (gateway_id, api_key, created_at) VALUES (?, ?, ?)'
    ).run(gatewayId, apiKey, Date.now());
    req.gateway = { gateway_id: gatewayId, api_key: apiKey };
    return next();
  }

  if (existing.api_key !== apiKey) {
    return res.status(401).json({ ok: false, error: 'invalid api key' });
  }
  req.gateway = existing;
  next();
}

module.exports = { gatewayAuth };
