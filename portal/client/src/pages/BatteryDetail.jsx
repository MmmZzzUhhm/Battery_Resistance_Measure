import { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';
import { api } from '../api.js';
import LineChart from '../components/LineChart.jsx';

export default function BatteryDetail() {
  const { gatewayId, childId } = useParams();
  const [rows, setRows] = useState([]);
  const [error, setError] = useState(null);

  useEffect(() => {
    api.measurements(gatewayId, { child_id: childId, limit: 1000 })
      .then((data) => setRows(data.slice().reverse()))
      .catch((e) => setError(e.message));
  }, [gatewayId, childId]);

  const valid = rows.filter((r) => r.valid && r.r_mohm != null && r.v != null);
  const rSeries = [{ id: 'r', label: '内部抵抗 [mΩ]', color: 'var(--series-1)', points: valid.map((r) => ({ x: r.ts, y: r.r_mohm })) }];
  const vSeries = [{ id: 'v', label: '電圧 [V]', color: 'var(--series-2)', points: valid.map((r) => ({ x: r.ts, y: r.v })) }];

  return (
    <div>
      <h2>電池: {childId} <span style={{ fontSize: '.8rem', color: 'var(--text-secondary)' }}>({gatewayId})</span></h2>
      {error && <p style={{ color: 'var(--status-critical)' }}>{error}</p>}
      {valid.length === 0 ? (
        <div className="card">まだデータがありません</div>
      ) : (
        <>
          <div className="card">
            <strong>内部抵抗の推移</strong>
            <LineChart series={rSeries} yLabel="mΩ" />
          </div>
          <div className="card">
            <strong>電圧の推移</strong>
            <LineChart series={vSeries} yLabel="V" formatY={(v) => v.toFixed(3)} />
          </div>
        </>
      )}
      <div className="card">
        <strong>生データ (直近{Math.min(rows.length, 50)}件)</strong>
        <table>
          <thead><tr><th>日時</th><th>内部抵抗[mΩ]</th><th>電圧[V]</th><th>状態</th></tr></thead>
          <tbody>
            {rows.slice(-50).reverse().map((r) => (
              <tr key={r.id}>
                <td>{new Date(r.ts * 1000).toLocaleString()}</td>
                <td className="num">{r.r_mohm?.toFixed(3) ?? '-'}</td>
                <td className="num">{r.v?.toFixed(4) ?? '-'}</td>
                <td>{r.valid ? <span className="tag good">OK</span> : <span className="tag critical">異常</span>}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
