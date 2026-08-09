import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { api } from '../api.js';

export default function CameraList() {
  const [cameras, setCameras] = useState([]);
  const [error, setError] = useState(null);

  useEffect(() => {
    api.cameras().then(setCameras).catch((e) => setError(e.message));
  }, []);

  return (
    <div>
      <h2>薄型カウンタカメラ 一覧</h2>
      {error && <p style={{ color: 'var(--status-critical)' }}>{error}</p>}
      <div className="card">
        <table>
          <thead>
            <tr>
              <th>カメラID</th><th>最終通信</th><th>撮影件数</th>
            </tr>
          </thead>
          <tbody>
            {cameras.length === 0 && (
              <tr><td colSpan={3}>まだカメラからの通信がありません</td></tr>
            )}
            {cameras.map((c) => (
              <tr key={c.camera_id}>
                <td><Link to={`/cameras/${c.camera_id}`}>{c.camera_id}</Link></td>
                <td>{c.last_seen_at ? new Date(c.last_seen_at).toLocaleString() : '-'}</td>
                <td className="num">{c.capture_count}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
