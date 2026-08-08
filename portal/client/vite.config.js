import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// 開発時 (Windows PC): Vite dev serverが3000番、APIは別プロセス(8080番)で動くのでproxyする。
// 本番 (Raspberry Pi 4): `vite build` の成果物を portal/server が静的配信するのでproxy設定は不要。
export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,
    proxy: {
      '/api': 'http://localhost:8080',
    },
  },
});
