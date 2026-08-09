// ポータルサーバー エントリポイント
// 開発 (Windows): npm run dev  → APIのみ起動、Reactは別途 `npm run dev` (Vite) をportal/clientで実行
// 本番 (Raspberry Pi 4): npm start → APIに加えて portal/client のビルド済み静的ファイルも配信する
const path = require('path');
const fs = require('fs');
const express = require('express');
const cors = require('cors');

const deviceApi = require('./routes/deviceApi');
const adminApi = require('./routes/adminApi');
const cameraApi = require('./routes/cameraApi');

const app = express();
const PORT = process.env.PORT || 8080;

app.use(cors());
app.use(express.json({ limit: '2mb' }));

app.use('/api/v1', deviceApi);
app.use('/api/v1', adminApi);
app.use('/api/v1', cameraApi);

app.get('/api/health', (_req, res) => res.json({ ok: true }));

// 本番配信: portal/client/dist が存在すればそこを静的配信する
const clientDist = path.join(__dirname, '..', '..', 'client', 'dist');
if (fs.existsSync(clientDist)) {
  app.use(express.static(clientDist));
  app.get('*', (req, res, next) => {
    if (req.path.startsWith('/api/')) return next();
    res.sendFile(path.join(clientDist, 'index.html'));
  });
}

app.listen(PORT, () => {
  console.log(`[portal] listening on http://0.0.0.0:${PORT}`);
});
