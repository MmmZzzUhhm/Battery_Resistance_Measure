import { useEffect, useState } from 'react';
import { api } from '../api.js';

export default function Firmware() {
  const [list, setList] = useState([]);
  const [version, setVersion] = useState('');
  const [file, setFile] = useState(null);
  const [status, setStatus] = useState(null);

  const [gateways, setGateways] = useState([]);
  const [selGateway, setSelGateway] = useState('');
  const [children, setChildren] = useState([]);
  const [selChild, setSelChild] = useState('');
  const [selVersion, setSelVersion] = useState('');

  function loadList() { api.firmwareList().then(setList); }
  useEffect(loadList, []);
  useEffect(() => { api.gateways().then(setGateways); }, []);
  useEffect(() => {
    if (selGateway) api.children(selGateway).then(setChildren);
    else setChildren([]);
  }, [selGateway]);

  async function upload() {
    if (!version || !file) { setStatus({ ok: false, msg: 'バージョンとファイルを指定してください' }); return; }
    try {
      const r = await api.firmwareUpload(version, file);
      setStatus({ ok: r.ok, msg: r.ok ? 'アップロード完了' : JSON.stringify(r) });
      loadList();
    } catch (e) { setStatus({ ok: false, msg: e.message }); }
  }

  async function assign() {
    if (!selGateway || !selChild || !selVersion) return;
    await api.setFirmwareTarget(selGateway, selChild, selVersion);
    setStatus({ ok: true, msg: `${selChild} へ ${selVersion} を配信対象に設定しました` });
  }

  return (
    <div>
      <h2>子機ファームウェア管理</h2>

      <div className="card">
        <strong>アップロード</strong>
        <label>バージョン文字列</label>
        <input value={version} onChange={(e) => setVersion(e.target.value)} placeholder="1.1.0" />
        <label>ファイル (.bin)</label>
        <input type="file" accept=".bin" onChange={(e) => setFile(e.target.files[0])} />
        <div style={{ marginTop: 12 }}><button onClick={upload}>アップロード</button></div>
        {status && <p style={{ color: status.ok ? 'var(--status-good)' : 'var(--status-critical)' }}>{status.msg}</p>}
      </div>

      <div className="card">
        <strong>登録済みファームウェア</strong>
        <table>
          <thead><tr><th>バージョン</th><th>サイズ</th><th>MD5</th><th>登録日時</th></tr></thead>
          <tbody>
            {list.map((f) => (
              <tr key={f.version}>
                <td>{f.version}</td>
                <td className="num">{(f.size / 1024).toFixed(1)} KB</td>
                <td style={{ fontFamily: 'monospace', fontSize: '.75rem' }}>{f.md5}</td>
                <td>{new Date(f.uploaded_at).toLocaleString()}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div className="card">
        <strong>子機への配信割り当て</strong>
        <label>ゲートウェイ</label>
        <select value={selGateway} onChange={(e) => setSelGateway(e.target.value)}>
          <option value="">選択してください</option>
          {gateways.map((g) => <option key={g.gateway_id} value={g.gateway_id}>{g.gateway_id}</option>)}
        </select>
        <label>子機</label>
        <select value={selChild} onChange={(e) => setSelChild(e.target.value)}>
          <option value="">選択してください</option>
          {children.map((c) => <option key={c.child_id} value={c.child_id}>{c.child_id}</option>)}
        </select>
        <label>配信するバージョン</label>
        <select value={selVersion} onChange={(e) => setSelVersion(e.target.value)}>
          <option value="">選択してください</option>
          {list.map((f) => <option key={f.version} value={f.version}>{f.version}</option>)}
        </select>
        <div style={{ marginTop: 12 }}><button onClick={assign}>配信対象に設定</button></div>
      </div>
    </div>
  );
}
