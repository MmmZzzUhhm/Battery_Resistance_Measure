import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { api } from '../api.js';

export default function Dashboard() {
  const [gateways, setGateways] = useState([]);
  const [error, setError] = useState(null);

  useEffect(() => {
    api.gateways().then(setGateways).catch((e) => setError(e.message));
  }, []);

  return (
    <div>
      <h2>親機(ゲートウェイ)一覧</h2>
      {error && <p style={{ color: 'var(--status-critical)' }}>{error}</p>}
      <div className="card">
        <table>
          <thead>
            <tr>
              <th>ゲートウェイID</th><th>最終heartbeat</th><th>稼働時間</th><th>SD空き容量</th><th>FWバージョン</th>
            </tr>
          </thead>
          <tbody>
            {gateways.length === 0 && (
              <tr><td colSpan={5}>まだ親機からの通信がありません</td></tr>
            )}
            {gateways.map((g) => (
              <tr key={g.gateway_id}>
                <td><Link to={`/gateways/${g.gateway_id}`}>{g.gateway_id}</Link></td>
                <td>{g.last_heartbeat_at ? new Date(g.last_heartbeat_at).toLocaleString() : '-'}</td>
                <td className="num">{g.uptime_s != null ? `${Math.floor(g.uptime_s / 60)}分` : '-'}</td>
                <td className="num">{g.sd_free_kb != null ? `${(g.sd_free_kb / 1024).toFixed(1)}MB` : '-'}</td>
                <td>{g.fw_version || '-'}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
