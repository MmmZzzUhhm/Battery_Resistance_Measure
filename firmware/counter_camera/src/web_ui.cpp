#include "web_ui.h"
#include "http_server.h"

namespace {

const char PAGE_HTML[] PROGMEM = R"====(
<!DOCTYPE html><html lang="ja"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>薄型カウンタカメラ設定</title>
<style>
body{font-family:sans-serif;max-width:520px;margin:24px auto;padding:0 12px;background:#fafafa}
h2{color:#1976D2}
label{display:block;margin:8px 0 2px;font-size:.9em;color:#555}
input,select{width:100%;box-sizing:border-box;padding:8px;border:1px solid #ccc;border-radius:4px;font-size:1em}
h3{color:#555;font-size:.95em;margin:20px 0 4px;border-top:1px solid #ddd;padding-top:14px}
button{margin-top:16px;margin-right:8px;padding:10px 24px;background:#1976D2;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:1em}
button.sec{background:#757575}
#meta{color:#777;font-size:.85em;margin:8px 0 16px}
#status{margin-top:12px;padding:8px;border-radius:4px;display:none}
.ok{background:#E8F5E9;color:#2E7D32}.err{background:#FFEBEE;color:#C62828}
</style></head><body>
<h2>薄型カウンタカメラ設定</h2>
<div id="meta">読込中...</div>

<h3>設置時プレビュー(画角調整用)</h3>
<button class="sec" id="previewBtn" onclick="togglePreview()">プレビュー開始</button>
<p style="color:#888;font-size:.8em;margin:4px 0">プレビュー中は他の操作(保存・撮影・照明等)ができません。画角調整が終わったら停止してください。</p>
<div id="previewWrap" style="display:none;margin-top:8px">
  <img id="previewImg" style="width:100%;border-radius:4px;border:1px solid #ccc" alt="ライブプレビュー">
</div>

<label>デバイスID</label><input id="device_id">
<label>WiFi STA SSID</label><input id="wifi_sta_ssid">
<label>WiFi STA パスワード</label><input id="wifi_sta_pass" type="password" placeholder="(変更しない場合は空白)">

<h3>IPアドレス設定</h3>
<label>取得方法</label>
<select id="use_static_ip" onchange="toggleStaticIp()">
  <option value="0">DHCP(自動)</option>
  <option value="1">固定IP</option>
</select>
<div id="staticIpFields">
  <label>IPアドレス</label><input id="static_ip" placeholder="192.168.1.100">
  <label>ゲートウェイ</label><input id="static_gateway" placeholder="192.168.1.1">
  <label>サブネットマスク</label><input id="static_subnet" placeholder="255.255.255.0">
  <label>DNSサーバー (省略可)</label><input id="static_dns" placeholder="192.168.1.1">
</div>

<h3>時刻設定 (PCF8563T RTC)</h3>
<label>NTPサーバー1</label><input id="ntp_server1" placeholder="pool.ntp.org">
<label>NTPサーバー2</label><input id="ntp_server2" placeholder="time.google.com">
<label>タイムゾーン (POSIX TZ形式。例: 日本は "JST-9")</label><input id="timezone_tz" placeholder="UTC0">
<p style="color:#888;font-size:.8em;margin:4px 0">記録される日時(UTCDateTime)自体は常にUTC。ここはONVIF表示用のローカル時刻換算にのみ使用。</p>

<h3>カメラ設定</h3>
<label>JPEG品質 (0=高品質〜63=低品質)</label><input id="jpeg_quality" type="number" min="0" max="63">
<label>解像度コード (5=QVGA 8=VGA 9=SVGA 10=XGA 11=HD 13=UXGA)</label><input id="frame_size" type="number" min="0" max="13">
<label>取付向き補正 (画像回転)</label>
<select id="image_rotation">
  <option value="0">0度 (回転なし)</option>
  <option value="90">90度</option>
  <option value="180">180度 (上下逆さま取付)</option>
  <option value="270">270度</option>
</select>
<button onclick="save()">保存</button>
<button class="sec" onclick="capture()">試し撮影</button>
<button class="sec" onclick="light(true)">照明ON</button>
<button class="sec" onclick="light(false)">照明OFF</button>
<div id="status"></div>
<p style="color:#888;font-size:.8em">ONVIFエンドポイント: /onvif/device_service, /onvif/media_service, /onvif/imaging_service, /onvif/snapshot</p>
<script>
async function load(){
  const st = await fetch('/api/status').then(r=>r.json());
  const now = st.now_epoch ? new Date(st.now_epoch * 1000).toLocaleString() : '不明';
  document.getElementById('meta').textContent =
    `STA=${st.wifi_sta_ip||'(未接続)'}  AP=${st.wifi_ap_ip}  SD空き=${st.sd_free_kb}KB  稼働=${st.uptime_s}s  ` +
    `RTC=${st.rtc_available?'検出':'未検出'}  現在時刻=${now}`;
  const cfg = await fetch('/api/config').then(r=>r.json());
  for (const k of ['device_id','wifi_sta_ssid','jpeg_quality','frame_size','image_rotation',
                    'static_ip','static_gateway','static_subnet','static_dns',
                    'ntp_server1','ntp_server2','timezone_tz']) {
    const el = document.getElementById(k);
    if (el) el.value = cfg[k];
  }
  document.getElementById('use_static_ip').value = cfg.use_static_ip ? '1' : '0';
  toggleStaticIp();
}
function toggleStaticIp(){
  document.getElementById('staticIpFields').style.display =
    document.getElementById('use_static_ip').value === '1' ? '' : 'none';
}
function showStatus(msg, ok){
  const el = document.getElementById('status');
  el.textContent = msg; el.className = ok ? 'ok' : 'err'; el.style.display = 'block';
  setTimeout(()=>el.style.display='none', 4000);
}
async function save(){
  const body = {
    device_id: document.getElementById('device_id').value,
    wifi_sta_ssid: document.getElementById('wifi_sta_ssid').value,
    jpeg_quality: parseInt(document.getElementById('jpeg_quality').value, 10),
    frame_size: parseInt(document.getElementById('frame_size').value, 10),
    image_rotation: parseInt(document.getElementById('image_rotation').value, 10),
    use_static_ip: document.getElementById('use_static_ip').value === '1',
    static_ip: document.getElementById('static_ip').value,
    static_gateway: document.getElementById('static_gateway').value,
    static_subnet: document.getElementById('static_subnet').value,
    static_dns: document.getElementById('static_dns').value,
    ntp_server1: document.getElementById('ntp_server1').value,
    ntp_server2: document.getElementById('ntp_server2').value,
    timezone_tz: document.getElementById('timezone_tz').value,
  };
  const pass = document.getElementById('wifi_sta_pass').value;
  if (pass) body.wifi_sta_pass = pass;
  const r = await fetch('/api/config', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)});
  showStatus(r.ok ? '保存しました (再起動が必要な場合があります)' : '保存に失敗しました', r.ok);
  load();
}
async function capture(){
  showStatus('撮影中...', true);
  const r = await fetch('/api/capture', {method:'POST'});
  const j = await r.json();
  showStatus(j.ok ? `撮影OK: ${j.saved_to_sd ? j.saved_path : '(SD未挿入のため保存なし)'} (${j.bytes}B)` : '撮影に失敗しました', j.ok);
}
async function light(on){
  const r = await fetch(on ? '/onvif/light/on' : '/onvif/light/off');
  const j = await r.json();
  showStatus(`照明: ${j.state}`, j.ok);
}
let previewOn = false;
function togglePreview(){
  previewOn = !previewOn;
  const wrap = document.getElementById('previewWrap');
  const img = document.getElementById('previewImg');
  const btn = document.getElementById('previewBtn');
  if (previewOn) {
    wrap.style.display = '';
    img.src = '/preview/stream?_=' + Date.now();
    btn.textContent = 'プレビュー停止';
  } else {
    img.src = '';
    wrap.style.display = 'none';
    btn.textContent = 'プレビュー開始';
  }
}
load();
</script></body></html>
)====";

void handleRoot() {
    httpServer.send_P(200, "text/html; charset=utf-8", PAGE_HTML);
}

} // namespace

void webUiRegisterRoutes() {
    httpServer.on("/", HTTP_GET, handleRoot);
}
