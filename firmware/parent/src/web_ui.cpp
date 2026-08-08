#include "web_ui.h"
#include "http_server.h"

namespace {

const char DASHBOARD_HTML[] PROGMEM = R"====(
<!DOCTYPE html><html lang="ja"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>親機ダッシュボード</title>
<style>
body{font-family:sans-serif;max-width:760px;margin:24px auto;padding:0 12px;background:#fafafa}
h2{color:#1976D2}
table{width:100%;border-collapse:collapse;margin-top:8px}
th,td{padding:6px 8px;border-bottom:1px solid #ddd;text-align:left;font-size:.9em}
th{color:#555;background:#f0f0f0}
.tag{display:inline-block;padding:2px 6px;border-radius:3px;font-size:.75em;color:#fff}
.ok{background:#2E7D32}.warn{background:#E65100}.bad{background:#C62828}
a.navbtn{display:inline-block;margin:8px 8px 8px 0;padding:8px 16px;background:#1976D2;color:#fff;
  text-decoration:none;border-radius:4px;font-size:.9em}
#meta{color:#777;font-size:.85em;margin-bottom:12px}
</style></head><body>
<h2>鉛蓄電池監視 親機ダッシュボード</h2>
<div id="meta">読込中...</div>
<a class="navbtn" href="/config">設定</a>
<table>
<thead><tr><th>子機ID</th><th>内部抵抗 [mΩ]</th><th>電圧 [V]</th><th>最終受信</th><th>状態</th></tr></thead>
<tbody id="rows"><tr><td colspan="5">読込中...</td></tr></tbody>
</table>
<script>
async function load(){
  const st = await fetch('/api/status').then(r=>r.json());
  document.getElementById('meta').textContent =
    `gateway=${st.gateway_id}  STA=${st.wifi_sta_ip||'(未接続)'}  AP=${st.wifi_ap_ip}  SD空き=${st.sd_free_kb}KB  稼働=${st.uptime_s}s`;
  const children = await fetch('/api/children').then(r=>r.json());
  const rows = document.getElementById('rows');
  rows.innerHTML = '';
  if (children.length === 0) { rows.innerHTML = '<tr><td colspan="5">まだ子機からのデータがありません</td></tr>'; return; }
  for (const c of children) {
    const last = c.last || {};
    const tr = document.createElement('tr');
    const ts = last.ts ? new Date(last.ts*1000).toLocaleString() : '-';
    const tag = last.valid ? '<span class="tag ok">OK</span>' : '<span class="tag bad">異常</span>';
    tr.innerHTML = `<td>${c.child_id}</td><td>${last.r_mohm!=null?last.r_mohm.toFixed(2):'-'}</td>
      <td>${last.v!=null?last.v.toFixed(3):'-'}</td><td>${ts}</td><td>${tag}</td>`;
    rows.appendChild(tr);
  }
}
load();
setInterval(load, 10000);
</script></body></html>
)====";

const char CONFIG_HTML[] PROGMEM = R"====(
<!DOCTYPE html><html lang="ja"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>親機設定</title>
<style>
body{font-family:sans-serif;max-width:520px;margin:24px auto;padding:0 12px;background:#fafafa}
h2{color:#1976D2}
label{display:block;margin:8px 0 2px;font-size:.9em;color:#555}
input{width:100%;box-sizing:border-box;padding:8px;border:1px solid #ccc;border-radius:4px;font-size:1em}
button{margin-top:16px;padding:10px 24px;background:#1976D2;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:1em}
a.navbtn{display:inline-block;margin:8px 0;color:#1976D2}
#status{margin-top:12px;padding:8px;border-radius:4px;display:none}
.ok{background:#E8F5E9;color:#2E7D32}.err{background:#FFEBEE;color:#C62828}
</style></head><body>
<a class="navbtn" href="/">← ダッシュボードへ戻る</a>
<h2>親機設定</h2>
<label>ゲートウェイID</label><input id="gateway_id">
<label>WiFi STA SSID (クラウド/社内網)</label><input id="wifi_sta_ssid">
<label>WiFi STA パスワード</label><input id="wifi_sta_pass" type="password" placeholder="(変更しない場合は空白)">
<label>子機向けSoftAP SSID</label><input id="ap_ssid">
<label>子機向けSoftAP パスワード</label><input id="ap_pass" type="password" placeholder="(変更しない場合は空白)">
<label>ポータルURL (例 http://raspberrypi.local:8080)</label><input id="portal_base_url">
<label>ポータルAPIキー</label><input id="portal_api_key" type="password" placeholder="(変更しない場合は空白)">
<label>クラウド同期間隔 (秒)</label><input id="cloud_sync_interval_sec" type="number" min="30">
<label>BLEスキャン間隔 (秒)</label><input id="ble_scan_interval_sec" type="number" min="5">
<button onclick="save()">保存</button>
<div id="status"></div>

<h2>子機ファームウェアアップロード</h2>
<label>バージョン文字列</label><input id="fw_version" placeholder="1.1.0">
<label>ファイル (.bin)</label><input id="fw_file" type="file" accept=".bin">
<button onclick="uploadFw()">アップロード</button>
<div id="fwstatus"></div>

<script>
async function load(){
  const c = await fetch('/api/config').then(r=>r.json());
  for (const k of ['gateway_id','wifi_sta_ssid','ap_ssid','portal_base_url',
                   'cloud_sync_interval_sec','ble_scan_interval_sec']) {
    document.getElementById(k).value = c[k] ?? '';
  }
}
async function save(){
  const data = {};
  for (const k of ['gateway_id','wifi_sta_ssid','ap_ssid','portal_base_url']) {
    data[k] = document.getElementById(k).value.trim();
  }
  const p1 = document.getElementById('wifi_sta_pass').value;
  if (p1) data.wifi_sta_pass = p1;
  const p2 = document.getElementById('ap_pass').value;
  if (p2) data.ap_pass = p2;
  const p3 = document.getElementById('portal_api_key').value;
  if (p3) data.portal_api_key = p3;
  data.cloud_sync_interval_sec = +document.getElementById('cloud_sync_interval_sec').value;
  data.ble_scan_interval_sec   = +document.getElementById('ble_scan_interval_sec').value;
  const s = document.getElementById('status');
  s.style.display = 'block';
  try {
    const r = await fetch('/api/config', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(data)});
    const j = await r.json();
    s.className = j.ok ? 'ok' : 'err';
    s.textContent = j.ok ? '保存しました' : ('エラー: ' + JSON.stringify(j));
  } catch(e) { s.className='err'; s.textContent='通信エラー: '+e; }
}
async function uploadFw(){
  const ver = document.getElementById('fw_version').value.trim();
  const fileInput = document.getElementById('fw_file');
  const s = document.getElementById('fwstatus');
  s.style.display = 'block';
  if (!ver || !fileInput.files.length) { s.className='err'; s.textContent='バージョンとファイルを指定してください'; return; }
  try {
    const r = await fetch(`/api/firmware/upload?version=${encodeURIComponent(ver)}`, {
      method: 'POST', body: fileInput.files[0]
    });
    const j = await r.json();
    s.className = j.ok ? 'ok' : 'err';
    s.textContent = j.ok ? 'アップロード完了' : ('エラー: ' + JSON.stringify(j));
  } catch(e) { s.className='err'; s.textContent='通信エラー: '+e; }
}
load();
</script></body></html>
)====";

void handleRoot()   { httpServer.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML); }
void handleConfig() { httpServer.send_P(200, "text/html; charset=utf-8", CONFIG_HTML); }

} // namespace

void webUiRegisterRoutes() {
    httpServer.on("/", HTTP_GET, handleRoot);
    httpServer.on("/config", HTTP_GET, handleConfig);
}
