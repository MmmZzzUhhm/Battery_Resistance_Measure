import { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';
import { api } from '../api.js';

export default function CameraDetail() {
  const { cameraId } = useParams();
  const [captures, setCaptures] = useState([]);
  const [error, setError] = useState(null);

  useEffect(() => {
    api.cameraCaptures(cameraId, { limit: 200 })
      .then(setCaptures)
      .catch((e) => setError(e.message));
  }, [cameraId]);

  return (
    <div>
      <h2>カメラ: {cameraId}</h2>
      {error && <p style={{ color: 'var(--status-critical)' }}>{error}</p>}
      <div className="card">
        <strong>撮影履歴 (直近{captures.length}件)</strong>
        {captures.length === 0 ? (
          <p>まだ撮影データがありません</p>
        ) : (
          <table>
            <thead>
              <tr><th>画像</th><th>撮影日時</th><th>受信日時</th><th>カウンタ値</th></tr>
            </thead>
            <tbody>
              {captures.map((c) => (
                <tr key={c.id}>
                  <td><img className="thumb" src={api.cameraCaptureImageUrl(c.id)} alt={`capture ${c.id}`} /></td>
                  <td>{c.captured_at ? new Date(c.captured_at).toLocaleString() : '-'}</td>
                  <td>{new Date(c.received_at).toLocaleString()}</td>
                  <td>
                    {c.counter_value != null
                      ? <span className="num">{c.counter_value}</span>
                      : <span className="tag">未読み取り</span>}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>
    </div>
  );
}
