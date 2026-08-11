import { useEffect, useState } from 'react';
import { useParams } from 'react-router-dom';
import { api } from '../api.js';

export default function CameraDetail() {
  const { cameraId } = useParams();
  const [camera, setCamera] = useState(null);
  const [captures, setCaptures] = useState([]);
  const [error, setError] = useState(null);
  const [status, setStatus] = useState(null);
  const [edits, setEdits] = useState({});

  function loadCamera() {
    api.cameras().then((list) => setCamera(list.find((c) => c.camera_id === cameraId))).catch((e) => setError(e.message));
  }
  function loadCaptures() {
    api.cameraCaptures(cameraId, { limit: 200 }).then(setCaptures).catch((e) => setError(e.message));
  }
  useEffect(() => { loadCamera(); loadCaptures(); }, [cameraId]);

  async function captureNow() {
    setStatus({ ok: true, msg: '撮影中...' });
    try {
      const r = await api.captureCameraNow(cameraId);
      setStatus({ ok: r.ok, msg: r.ok ? '撮影成功' : `撮影失敗: ${r.error}` });
      loadCamera(); loadCaptures();
    } catch (e) { setStatus({ ok: false, msg: e.message }); }
  }

  async function saveSettings() {
    await api.updateCamera(cameraId, {
      label: edits.label ?? camera.label,
      snapshot_url: edits.snapshot_url ?? camera.snapshot_url,
      capture_interval_min: edits.capture_interval_min ?? camera.capture_interval_min,
      ...(edits.cf_access_client_id !== undefined ? { cf_access_client_id: edits.cf_access_client_id } : {}),
      ...(edits.cf_access_client_secret !== undefined ? { cf_access_client_secret: edits.cf_access_client_secret } : {}),
    });
    setStatus({ ok: true, msg: '設定を保存しました' });
    loadCamera();
  }

  if (!camera) {
    return (
      <div>
        <h2>カメラ: {cameraId}</h2>
        {error && <p style={{ color: 'var(--status-critical)' }}>{error}</p>}
        <p>読込中...</p>
      </div>
    );
  }

  return (
    <div>
      <h2>カメラ: {camera.label} ({cameraId})</h2>
      {error && <p style={{ color: 'var(--status-critical)' }}>{error}</p>}

      <div className="card">
        <strong>設定</strong>
        <label>ラベル</label>
        <input defaultValue={camera.label} onChange={(e) => setEdits((p) => ({ ...p, label: e.target.value }))} />
        <label>スナップショットURL</label>
        <input defaultValue={camera.snapshot_url} onChange={(e) => setEdits((p) => ({ ...p, snapshot_url: e.target.value }))} />
        <label>CF-Access-Client-Id (変更する場合のみ入力)</label>
        <input placeholder="(変更しない場合は空白)" onChange={(e) => setEdits((p) => ({ ...p, cf_access_client_id: e.target.value }))} />
        <label>CF-Access-Client-Secret (変更する場合のみ入力)</label>
        <input type="password" placeholder="(変更しない場合は空白)" onChange={(e) => setEdits((p) => ({ ...p, cf_access_client_secret: e.target.value }))} />
        <label>撮影間隔 (分)</label>
        <input type="number" defaultValue={camera.capture_interval_min} style={{ width: 120 }}
          onChange={(e) => setEdits((p) => ({ ...p, capture_interval_min: Number(e.target.value) }))} />
        <div style={{ marginTop: 12 }}>
          <button onClick={saveSettings}>保存</button>
          <button className="sec" onClick={captureNow} style={{ marginLeft: 8 }}>今すぐ撮影</button>
        </div>
        {status && <p style={{ color: status.ok ? 'var(--status-good)' : 'var(--status-critical)' }}>{status.msg}</p>}
      </div>

      <div className="card">
        <strong>撮影履歴 (直近{captures.length}件)</strong>
        {captures.length === 0 ? (
          <p>まだ撮影データがありません</p>
        ) : (
          <table>
            <thead>
              <tr><th>画像</th><th>撮影日時</th><th>カウンタ値</th></tr>
            </thead>
            <tbody>
              {captures.map((c) => (
                <tr key={c.id}>
                  <td><img className="thumb" src={api.cameraCaptureImageUrl(c.id)} alt={`capture ${c.id}`} /></td>
                  <td>{new Date(c.captured_at).toLocaleString()}</td>
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
