#include "provisioning.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "protocol.h"

namespace {

WebServer server(80);

const char PAGE[] PROGMEM = R"====(
<!DOCTYPE html><html lang="ja"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>子機 初期設定</title>
<style>
body{font-family:sans-serif;max-width:480px;margin:24px auto;padding:0 12px;background:#fafafa}
h2{color:#1976D2}
label{display:block;margin:8px 0 2px;font-size:.9em;color:#555}
input,select{width:100%;box-sizing:border-box;padding:8px;border:1px solid #ccc;border-radius:4px;font-size:1em}
button{margin-top:16px;padding:10px 24px;background:#1976D2;color:#fff;border:none;border-radius:4px;cursor:pointer;font-size:1em}
#status{margin-top:12px;padding:8px;border-radius:4px;display:none}
.ok{background:#E8F5E9;color:#2E7D32}
.err{background:#FFEBEE;color:#C62828}
</style></head><body>
<h2>鉛蓄電池監視子機 初期設定</h2>
<label>デバイスID</label><input id="device_id" placeholder="batt-01">
<label>電池ID</label><input id="battery_id" placeholder="batt-01">
<label>通信方式</label>
<select id="link_mode"><option value="ble">BLE(親機へアドバタイズ)</option><option value="wifi">WiFi(親機APへ接続)</option></select>
<label>親機WiFi SSID (通信方式=WiFiの時のみ)</label><input id="wifi_ssid">
<label>親機WiFi パスワード</label><input id="wifi_pass" type="password">
<label>測定間隔 (秒, 最小60)</label><input id="wake_interval_sec" type="number" min="60" value="600">
<label>IWS7817 I2Cアドレス (10進 or 0x..)</label><input id="i2c_addr" value="0x03">
<button onclick="save()">保存して再起動</button>
<div id="status"></div>
<script>
async function save(){
  const data={
    device_id: document.getElementById('device_id').value.trim(),
    battery_id: document.getElementById('battery_id').value.trim(),
    link_mode: document.getElementById('link_mode').value,
    wifi_ssid: document.getElementById('wifi_ssid').value.trim(),
    wifi_pass: document.getElementById('wifi_pass').value,
    wake_interval_sec: Math.max(60, +document.getElementById('wake_interval_sec').value),
  };
  const ia=document.getElementById('i2c_addr').value.trim();
  data.i2c_addr=parseInt(ia,ia.toLowerCase().startsWith('0x')?16:10);
  const s=document.getElementById('status');
  s.style.display='block';
  if(!data.device_id){ s.className='err'; s.textContent='デバイスIDは必須です'; return; }
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
    const j=await r.json();
    if(j.ok){s.className='ok';s.textContent='保存しました。再起動します...';}
    else{s.className='err';s.textContent='エラー: '+JSON.stringify(j);}
  }catch(e){s.className='err';s.textContent='通信エラー: '+e;}
}
</script></body></html>
)====";

void handleRoot() {
    server.send_P(200, "text/html; charset=utf-8", PAGE);
}

void handlePostConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"no body\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid json\"}");
        return;
    }
    const char* deviceId = doc["device_id"] | "";
    if (strlen(deviceId) == 0) {
        server.send(400, "application/json", "{\"ok\":false,\"error\":\"device_id required\"}");
        return;
    }

    strlcpy(cfg.device_id, deviceId, sizeof(cfg.device_id));
    strlcpy(cfg.battery_id, doc["battery_id"] | deviceId, sizeof(cfg.battery_id));
    cfg.link_mode = linkModeFromStr(doc["link_mode"] | "ble");
    strlcpy(cfg.wifi_ssid, doc["wifi_ssid"] | "", sizeof(cfg.wifi_ssid));
    strlcpy(cfg.wifi_pass, doc["wifi_pass"] | "", sizeof(cfg.wifi_pass));
    cfg.wake_interval_sec = doc["wake_interval_sec"] | DEFAULT_WAKE_INTERVAL_SEC;
    if (cfg.wake_interval_sec < MIN_WAKE_INTERVAL_SEC) cfg.wake_interval_sec = MIN_WAKE_INTERVAL_SEC;
    cfg.i2c_addr = doc["i2c_addr"] | DEFAULT_I2C_ADDR;
    cfg.provisioned = true;
    configSave();

    server.send(200, "application/json", "{\"ok\":true}");
    delay(400);
    ESP.restart();
}

} // namespace

void provisioningBegin() {
    // WiFi.mode()より前にWiFi.macAddress()を呼ぶとMACが0埋めで返るため、
    // モード設定を先に行う。
    WiFi.mode(WIFI_AP);

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char ssid[32];
    snprintf(ssid, sizeof(ssid), "BATT-SETUP-%02X%02X%02X", mac[3], mac[4], mac[5]);

    WiFi.softAP(ssid, "battsetup");
    Serial.printf("[PROV] AP started: SSID=%s PASS=battsetup IP=%s\n",
        ssid, WiFi.softAPIP().toString().c_str());

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/config", HTTP_POST, handlePostConfig);
    server.onNotFound(handleRoot);
    server.begin();
}

void provisioningLoop() {
    server.handleClient();
}
