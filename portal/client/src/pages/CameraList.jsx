import { useEffect, useState } from 'react';
import { Link } from 'react-router-dom';
import { api } from '../api.js';

export default function CameraList() {
  const [cameras, setCameras] = useState([]);
  const [error, setError] = useState(null);
  const [status, setStatus] = useState(null);

  const [cameraId, setCameraId] = useState('');
  const [label, setLabel] = useState('');
  const [snapshotUrl, setSnapshotUrl] = useState('');
  const [cfId, setCfId] = useState('');
  const [cfSecret, setCfSecret] = useState('');
  const [intervalMin, setIntervalMin] = useState(720);

  function load() { api.cameras().then(setCameras).catch((e) => setError(e.message)); }
  useEffect(load, []);

  async function addCamera() {
    if (!cameraId || !snapshotUrl) {
      setStatus({ ok: false, msg: 'カメラIDとスナップショットURLを指定してください' });
      return;
    }
    try {
      await api.createCamera({
        camera_id: cameraId, label, snapshot_url: snapshotUrl,
        cf_access_client_id: cfId, cf_access_client_secret: cfSecret,
        capture_interval_min: Number(intervalMin) || 720,
      });
      setStatus({ ok: true, msg: '登録しました' });
      setCameraId(''); setLabel(''); setSnapshotUrl(''); setCfId(''); setCfSecret('');
      load();
    } catch (e) { setStatus({ ok: false, msg: e.message }); }
  }

  async function captureNow(id) {
    setStatus({ ok: true, msg: `${id}: 撮影中...` });
    try {
      const r = await api.captureCameraNow(id);
      setStatus({ ok: r.ok, msg: `${id}: 撮影${r.ok ? '成功' : '失敗'} (${r.filename || r.error})` });
      load();
    } catch (e) { setStatus({ ok: false, msg: `${id}: ${e.message}` }); }
  }

  return (
    <div>
      <h2>カメラ 一覧</h2>
      {error && <p style={{ color: 'var(--status-critical)' }}>{error}</p>}

      <div className="card">
        <strong>カメラ追加</strong>
        <label>カメラID</label>
        <input value={cameraId} onChange={(e) => setCameraId(e.target.value)} placeholder="cam1-2l" />
        <label>ラベル (表示名)</label>
        <input value={label} onChange={(e) => setLabel(e.target.value)} placeholder="GIS盤(2L) 薄型カメラ1" />
        <label>スナップショットURL</label>
        <input value={snapshotUrl} onChange={(e) => setSnapshotUrl(e.target.value)}
          placeholder="https://cam1-2l.example.com/onvif/snapshot" />
        <label>CF-Access-Client-Id (不要なら空白)</label>
        <input value={cfId} onChange={(e) => setCfId(e.target.value)} placeholder="xxxxxxxx.access" />
        <label>CF-Access-Client-Secret</label>
        <input type="password" value={cfSecret} onChange={(e) => setCfSecret(e.target.value)} />
        <label>撮影間隔 (分)</label>
        <input type="number" value={intervalMin} onChange={(e) => setIntervalMin(e.target.value)} style={{ width: 120 }} />
        <div style={{ marginTop: 12 }}><button onClick={addCamera}>追加</button></div>
      </div>

      {status && <p style={{ color: status.ok ? 'var(--status-good)' : 'var(--status-critical)' }}>{status.msg}</p>}

      <div className="card">
        <table>
          <thead>
            <tr>
              <th>カメラID</th><th>ラベル</th><th>撮影間隔</th><th>最終撮影</th><th>結果</th><th>撮影件数</th><th></th>
            </tr>
          </thead>
          <tbody>
            {cameras.length === 0 && (
              <tr><td colSpan={7}>まだカメラが登録されていません</td></tr>
            )}
            {cameras.map((c) => (
              <tr key={c.camera_id}>
                <td><Link to={`/cameras/${c.camera_id}`}>{c.camera_id}</Link></td>
                <td>{c.label}</td>
                <td className="num">{c.capture_interval_min}分</td>
                <td>{c.last_capture_at ? new Date(c.last_capture_at).toLocaleString() : '-'}</td>
                <td>{c.last_capture_at == null ? '-' : (c.last_capture_ok ? 'OK' : <span className="tag">NG</span>)}</td>
                <td className="num">{c.capture_count}</td>
                <td><button onClick={() => captureNow(c.camera_id)}>今すぐ撮影</button></td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
}
