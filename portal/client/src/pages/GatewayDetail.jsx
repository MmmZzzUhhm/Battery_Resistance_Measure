import { useEffect, useState } from 'react';
import { Link, useParams } from 'react-router-dom';
import { api } from '../api.js';

export default function GatewayDetail() {
  const { gatewayId } = useParams();
  const [children, setChildren] = useState([]);
  const [error, setError] = useState(null);
  const [edits, setEdits] = useState({});

  function load() {
    api.children(gatewayId).then(setChildren).catch((e) => setError(e.message));
  }
  useEffect(load, [gatewayId]);

  function editField(childId, field, value) {
    setEdits((prev) => ({ ...prev, [childId]: { ...prev[childId], [field]: value } }));
  }

  async function save(child) {
    const e = edits[child.child_id] || {};
    await api.updateChild(gatewayId, child.child_id, {
      battery_id: e.battery_id ?? child.battery_id,
      link_mode: e.link_mode ?? child.link_mode,
      wake_interval_sec: e.wake_interval_sec ?? child.wake_interval_sec,
      i2c_addr: e.i2c_addr ?? child.i2c_addr,
    });
    load();
  }

  return (
    <div>
      <h2>ゲートウェイ: {gatewayId}</h2>
      {error && <p style={{ color: 'var(--status-critical)' }}>{error}</p>}
      <div className="card">
        <table>
          <thead>
            <tr>
              <th>子機ID</th><th>電池ID</th><th>通信方式</th><th>Wake間隔(秒)</th><th>I2Cアドレス</th>
              <th>最終確認</th><th></th>
            </tr>
          </thead>
          <tbody>
            {children.length === 0 && (
              <tr><td colSpan={7}>まだこの親機に紐づく子機がありません</td></tr>
            )}
            {children.map((c) => (
              <tr key={c.child_id}>
                <td><Link to={`/gateways/${gatewayId}/children/${c.child_id}`}>{c.child_id}</Link></td>
                <td><input defaultValue={c.battery_id} onChange={(e) => editField(c.child_id, 'battery_id', e.target.value)} /></td>
                <td>
                  <select defaultValue={c.link_mode} onChange={(e) => editField(c.child_id, 'link_mode', e.target.value)}>
                    <option value="ble">BLE</option>
                    <option value="wifi">WiFi</option>
                  </select>
                </td>
                <td><input type="number" defaultValue={c.wake_interval_sec} style={{ width: 90 }}
                  onChange={(e) => editField(c.child_id, 'wake_interval_sec', Number(e.target.value))} /></td>
                <td><input type="number" defaultValue={c.i2c_addr} style={{ width: 60 }}
                  onChange={(e) => editField(c.child_id, 'i2c_addr', Number(e.target.value))} /></td>
                <td>{c.last_seen_at ? new Date(c.last_seen_at).toLocaleString() : '-'}</td>
                <td><button onClick={() => save(c)}>保存</button></td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <p style={{ fontSize: '.8rem', color: 'var(--text-secondary)' }}>
        設定変更は次回の親機↔ポータル同期時に取得され、対象子機の次回Wake時に配信されます。
      </p>
    </div>
  );
}
