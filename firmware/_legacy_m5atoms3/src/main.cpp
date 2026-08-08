/*
 * IWS7817 内部抵抗測定モジュール ファームウェア
 * M5Stack AtomS3 (ESP32-S3) + IWS7817 (I2C)
 *
 * 機能:
 *  - WiFi STA 接続 (未設定時は AP モードで設定受付)
 *  - HTTP REST API で設定変更 (GET/POST /api/config)
 *  - UDP で測定データを PC へ送信
 *  - UDP でコマンド受信 (start / stop / measure / ping / reboot)
 *  - ボタン押下で測定開始/停止トグル
 *  - mDNS: {device_id}.local
 *
 * Grove (SDA=GPIO1, SCL=GPIO2), I2C Clock=10kHz
 */

#include <M5Unified.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiUDP.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>

// ── IWS7817 プロトコル ────────────────────────────────────────
#define IWS7817_BYTES  10
#define IWS7817_HDR0   0x49
#define IWS7817_HDR1   0x57

// ── デフォルト設定値 ──────────────────────────────────────────
#define DEF_DEVICE_ID    "iws7817_01"
#define DEF_I2C_ADDR     0x03
#define DEF_UDP_PORT     7817    // PC側受信ポート (データ送信先)
#define DEF_CMD_PORT     7818    // このモジュールのコマンド受信ポート
#define DEF_INTERVAL_MS  2000
#define DEF_TARGET_IP    "255.255.255.255"  // ブロードキャスト

#define AP_SSID_PREFIX   "IWS7817-"
#define AP_PASSWORD      "iws78170"
#define AP_LOCAL_IP      "192.168.4.1"
#define NVS_NAMESPACE    "iws7817"
#define WIFI_TIMEOUT_MS  20000
#define DISP_UPDATE_MS   500
#define CMD_BUF_SIZE     300

// ── 設定構造体 ────────────────────────────────────────────────
struct DeviceConfig {
    char     device_id[32];
    char     wifi_ssid[64];
    char     wifi_pass[64];
    uint8_t  i2c_addr;
    char     target_ip[16];
    uint16_t udp_port;
    uint16_t cmd_port;
    uint32_t interval_ms;
    bool     auto_measure;
};

// ── 測定データ ────────────────────────────────────────────────
struct MeasData {
    bool     valid;
    float    r_mohm;
    float    v;
    uint32_t ts_ms;
};

// ── グローバル変数 ────────────────────────────────────────────
static DeviceConfig cfg;
static Preferences  prefs;
static WebServer    httpServer(80);
static WiFiUDP      dataUdp;
static WiFiUDP      cmdUdp;

static bool      measuring    = false;
static bool      wifiOk       = false;
static bool      apMode       = false;
static MeasData  lastData     = {};
static uint32_t  lastMeasMs   = 0;
static uint32_t  lastDispMs   = 0;

// ── NVS 設定読み書き ──────────────────────────────────────────
static void loadConfig() {
    prefs.begin(NVS_NAMESPACE, true);
    strlcpy(cfg.device_id, prefs.getString("device_id", DEF_DEVICE_ID).c_str(), sizeof(cfg.device_id));
    strlcpy(cfg.wifi_ssid, prefs.getString("wifi_ssid", "").c_str(),            sizeof(cfg.wifi_ssid));
    strlcpy(cfg.wifi_pass, prefs.getString("wifi_pass", "").c_str(),            sizeof(cfg.wifi_pass));
    strlcpy(cfg.target_ip, prefs.getString("target_ip", DEF_TARGET_IP).c_str(), sizeof(cfg.target_ip));
    cfg.i2c_addr    = (uint8_t) prefs.getUInt("i2c_addr",    DEF_I2C_ADDR);
    cfg.udp_port    = (uint16_t)prefs.getUInt("udp_port",    DEF_UDP_PORT);
    cfg.cmd_port    = (uint16_t)prefs.getUInt("cmd_port",    DEF_CMD_PORT);
    cfg.interval_ms =           prefs.getUInt("interval_ms", DEF_INTERVAL_MS);
    cfg.auto_measure =          prefs.getBool("auto_measure", true);
    prefs.end();
    if (cfg.interval_ms < 1000) cfg.interval_ms = 1000;
}

static void saveConfig() {
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("device_id",   cfg.device_id);
    prefs.putString("wifi_ssid",   cfg.wifi_ssid);
    prefs.putString("wifi_pass",   cfg.wifi_pass);
    prefs.putString("target_ip",   cfg.target_ip);
    prefs.putUInt("i2c_addr",      cfg.i2c_addr);
    prefs.putUInt("udp_port",      cfg.udp_port);
    prefs.putUInt("cmd_port",      cfg.cmd_port);
    prefs.putUInt("interval_ms",   cfg.interval_ms);
    prefs.putBool("auto_measure",  cfg.auto_measure);
    prefs.end();
}

// ── IWS7817 読み取り ──────────────────────────────────────────
static MeasData readIWS7817() {
    MeasData d = {};
    uint8_t buf[IWS7817_BYTES];
    uint8_t n = Wire.requestFrom(cfg.i2c_addr, (uint8_t)IWS7817_BYTES);
    if (n != IWS7817_BYTES) {
        Serial.printf("[IWS7817] Read failed: %d/%d bytes\n", n, IWS7817_BYTES);
        return d;
    }
    for (int i = 0; i < IWS7817_BYTES; i++) buf[i] = Wire.read();
    if (buf[0] != IWS7817_HDR0 || buf[1] != IWS7817_HDR1) {
        Serial.printf("[IWS7817] Bad header: %02X %02X\n", buf[0], buf[1]);
        return d;
    }
    // ビッグエンディアン float → リトルエンディアン
    uint8_t rb[4] = {buf[5], buf[4], buf[3], buf[2]};
    uint8_t vb[4] = {buf[9], buf[8], buf[7], buf[6]};
    memcpy(&d.r_mohm, rb, 4);
    memcpy(&d.v,      vb, 4);
    d.valid  = true;
    d.ts_ms  = millis();
    return d;
}

// ── UDP データ送信 ────────────────────────────────────────────
static void sendDataUDP(const MeasData& d) {
    JsonDocument doc;
    doc["id"]    = cfg.device_id;
    doc["addr"]  = cfg.i2c_addr;
    doc["r"]     = d.valid ? d.r_mohm : -999.0f;
    doc["v"]     = d.valid ? d.v      : -999.0f;
    doc["valid"] = d.valid;
    doc["t"]     = d.ts_ms;
    char buf[256];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    dataUdp.beginPacket(cfg.target_ip, cfg.udp_port);
    dataUdp.write((uint8_t*)buf, len);
    dataUdp.endPacket();
}

// ── ディスプレイ更新 ──────────────────────────────────────────
static void updateDisplay() {
    auto& d = M5.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextSize(1);

    // デバイスID
    d.setTextColor(TFT_CYAN);
    d.setCursor(0, 0);
    d.print(cfg.device_id);

    // WiFi状態
    d.setCursor(0, 10);
    if (apMode) {
        d.setTextColor(TFT_ORANGE);
        d.printf("AP %s", AP_LOCAL_IP);
    } else if (wifiOk) {
        d.setTextColor(TFT_GREEN);
        d.print(WiFi.localIP().toString().c_str());
    } else {
        d.setTextColor(TFT_RED);
        d.print("No WiFi...");
    }

    // 測定状態
    d.setCursor(0, 20);
    d.setTextColor(measuring ? TFT_YELLOW : TFT_DARKGREY);
    d.print(measuring ? "[MEASURING]" : "[ STOPPED ]");

    d.drawLine(0, 30, 127, 30, TFT_DARKGREY);

    // 内部抵抗
    d.setTextColor(TFT_YELLOW);
    d.setTextSize(1);
    d.setCursor(0, 33);
    d.print("R [mOhm]");

    d.setTextSize(3);
    d.setCursor(0, 44);
    if (!lastData.valid) {
        d.setTextColor(TFT_DARKGREY);
        d.print("  ---");
    } else if (lastData.r_mohm == -1.0f) {
        d.setTextColor(TFT_RED);
        d.print(" O/R");
    } else {
        d.setTextColor(TFT_WHITE);
        char s[12];
        snprintf(s, sizeof(s), "%6.2f", lastData.r_mohm);
        d.print(s);
    }

    d.drawLine(0, 80, 127, 80, TFT_DARKGREY);

    // 電圧
    d.setTextColor(TFT_CYAN);
    d.setTextSize(1);
    d.setCursor(0, 83);
    d.print("V [Volt]");

    d.setTextSize(2);
    d.setCursor(0, 94);
    if (!lastData.valid) {
        d.setTextColor(TFT_DARKGREY);
        d.print("  ---");
    } else if (lastData.v == -1.0f) {
        d.setTextColor(TFT_RED);
        d.print("<lim");
    } else if (lastData.v == 99.0f) {
        d.setTextColor(TFT_RED);
        d.print(">lim");
    } else {
        d.setTextColor(TFT_WHITE);
        char s[12];
        snprintf(s, sizeof(s), "%6.3f", lastData.v);
        d.print(s);
    }

    // I2Cアドレス表示
    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY);
    d.setCursor(0, 120);
    d.printf("I2C:0x%02X", cfg.i2c_addr);
}

// ── HTTP ハンドラ: 設定画面 (ブラウザ用) ────────────────────
static const char CONFIG_HTML[] PROGMEM = R"====(
<!DOCTYPE html><html lang="ja"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>IWS7817 設定</title>
<style>
body{font-family:sans-serif;max-width:520px;margin:24px auto;padding:0 12px;background:#fafafa}
h2{color:#1976D2}
label{display:block;margin:8px 0 2px;font-size:.9em;color:#555}
input[type=text],input[type=password],input[type=number]{width:100%;box-sizing:border-box;
  padding:8px;border:1px solid #ccc;border-radius:4px;font-size:1em}
.row{display:flex;gap:8px}
.row input{flex:1}
button{margin-top:16px;padding:10px 24px;background:#1976D2;color:#fff;
  border:none;border-radius:4px;cursor:pointer;font-size:1em}
button.red{background:#E53935}
#status{margin-top:12px;padding:8px;border-radius:4px;display:none}
.ok{background:#E8F5E9;color:#2E7D32}
.err{background:#FFEBEE;color:#C62828}
</style></head><body>
<h2>IWS7817 設定</h2>
<label>デバイスID</label>
<input id="device_id" type="text">
<label>WiFi SSID</label>
<input id="wifi_ssid" type="text">
<label>WiFi パスワード</label>
<input id="wifi_pass" type="password" placeholder="(変更しない場合は空白)">
<label>送信先 IP アドレス</label>
<input id="target_ip" type="text">
<div class="row">
  <div style="flex:1"><label>UDP データポート</label><input id="udp_port" type="number" min="1024" max="65535"></div>
  <div style="flex:1"><label>UDP コマンドポート</label><input id="cmd_port" type="number" min="1024" max="65535"></div>
</div>
<div class="row">
  <div style="flex:1"><label>I2C アドレス (hex)</label><input id="i2c_addr" type="text"></div>
  <div style="flex:1"><label>測定間隔 (ms, min:1000)</label><input id="interval_ms" type="number" min="1000"></div>
</div>
<label><input type="checkbox" id="auto_measure"> 起動時に測定を自動開始</label>
<br>
<button onclick="save()">保存して再起動</button>
<button class="red" onclick="if(confirm('再起動しますか?'))fetch('/api/reboot',{method:'POST'})">再起動のみ</button>
<div id="status"></div>
<script>
async function load(){
  const c=await fetch('/api/config').then(r=>r.json());
  ['device_id','wifi_ssid','target_ip','udp_port','cmd_port','interval_ms'].forEach(k=>{
    document.getElementById(k).value=c[k]??'';
  });
  document.getElementById('i2c_addr').value='0x'+c.i2c_addr.toString(16).padStart(2,'0');
  document.getElementById('auto_measure').checked=c.auto_measure;
}
async function save(){
  const data={};
  ['device_id','wifi_ssid','target_ip'].forEach(k=>data[k]=document.getElementById(k).value.trim());
  const wp=document.getElementById('wifi_pass').value;
  if(wp) data.wifi_pass=wp;
  const ia=document.getElementById('i2c_addr').value.trim();
  data.i2c_addr=parseInt(ia,ia.startsWith('0x')?16:10);
  data.udp_port=+document.getElementById('udp_port').value;
  data.cmd_port=+document.getElementById('cmd_port').value;
  data.interval_ms=Math.max(1000,+document.getElementById('interval_ms').value);
  data.auto_measure=document.getElementById('auto_measure').checked;
  const s=document.getElementById('status');
  s.style.display='block';
  try{
    const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)});
    const j=await r.json();
    if(j.ok){s.className='ok';s.textContent='保存しました。デバイスを再起動します...';}
    else{s.className='err';s.textContent='エラー: '+JSON.stringify(j);}
  }catch(e){s.className='err';s.textContent='通信エラー: '+e;}
}
load();
</script></body></html>
)====";

// ── HTTP ハンドラ ─────────────────────────────────────────────
static void handleRoot() {
    httpServer.send_P(200, "text/html; charset=utf-8", CONFIG_HTML);
}

static void handleGetConfig() {
    JsonDocument doc;
    doc["device_id"]    = cfg.device_id;
    doc["wifi_ssid"]    = cfg.wifi_ssid;
    doc["target_ip"]    = cfg.target_ip;
    doc["i2c_addr"]     = cfg.i2c_addr;
    doc["udp_port"]     = cfg.udp_port;
    doc["cmd_port"]     = cfg.cmd_port;
    doc["interval_ms"]  = cfg.interval_ms;
    doc["auto_measure"] = cfg.auto_measure;
    char buf[512];
    serializeJson(doc, buf, sizeof(buf));
    httpServer.send(200, "application/json", buf);
}

static void handlePostConfig() {
    if (!httpServer.hasArg("plain")) {
        httpServer.send(400, "application/json", "{\"error\":\"no body\"}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, httpServer.arg("plain")) != DeserializationError::Ok) {
        httpServer.send(400, "application/json", "{\"error\":\"invalid json\"}");
        return;
    }
    if (doc["device_id"].is<const char*>())  strlcpy(cfg.device_id, doc["device_id"], sizeof(cfg.device_id));
    if (doc["wifi_ssid"].is<const char*>())  strlcpy(cfg.wifi_ssid, doc["wifi_ssid"], sizeof(cfg.wifi_ssid));
    if (doc["wifi_pass"].is<const char*>())  strlcpy(cfg.wifi_pass, doc["wifi_pass"], sizeof(cfg.wifi_pass));
    if (doc["target_ip"].is<const char*>())  strlcpy(cfg.target_ip, doc["target_ip"], sizeof(cfg.target_ip));
    if (doc["i2c_addr"].is<uint8_t>())       cfg.i2c_addr    = doc["i2c_addr"];
    if (doc["udp_port"].is<uint16_t>())      cfg.udp_port    = doc["udp_port"];
    if (doc["cmd_port"].is<uint16_t>())      cfg.cmd_port    = doc["cmd_port"];
    if (doc["interval_ms"].is<uint32_t>()) {
        cfg.interval_ms = doc["interval_ms"];
        if (cfg.interval_ms < 1000) cfg.interval_ms = 1000;
    }
    if (doc["auto_measure"].is<bool>())      cfg.auto_measure = doc["auto_measure"];
    saveConfig();
    httpServer.send(200, "application/json", "{\"ok\":true}");
    delay(300);
    ESP.restart();
}

static void handleGetStatus() {
    JsonDocument doc;
    doc["device_id"]  = cfg.device_id;
    doc["measuring"]  = measuring;
    doc["wifi_ok"]    = wifiOk;
    doc["ap_mode"]    = apMode;
    doc["ip"]         = apMode ? AP_LOCAL_IP : WiFi.localIP().toString().c_str();
    doc["rssi"]       = wifiOk ? WiFi.RSSI() : 0;
    doc["i2c_addr"]   = cfg.i2c_addr;
    doc["interval_ms"] = cfg.interval_ms;
    JsonObject m = doc["last"].to<JsonObject>();
    m["valid"]    = lastData.valid;
    m["r_mohm"]   = lastData.r_mohm;
    m["v"]        = lastData.v;
    m["ts_ms"]    = lastData.ts_ms;
    char buf[512];
    serializeJson(doc, buf, sizeof(buf));
    httpServer.send(200, "application/json", buf);
}

static void handlePostMeasure() {
    lastData = readIWS7817();
    if (wifiOk || apMode) sendDataUDP(lastData);
    JsonDocument doc;
    doc["valid"]  = lastData.valid;
    doc["r_mohm"] = lastData.r_mohm;
    doc["v"]      = lastData.v;
    char buf[128];
    serializeJson(doc, buf, sizeof(buf));
    httpServer.send(200, "application/json", buf);
}

static void handlePostStart() {
    measuring = true;
    httpServer.send(200, "application/json", "{\"ok\":true,\"measuring\":true}");
}
static void handlePostStop() {
    measuring = false;
    httpServer.send(200, "application/json", "{\"ok\":true,\"measuring\":false}");
}
static void handlePostReboot() {
    httpServer.send(200, "application/json", "{\"ok\":true}");
    delay(300);
    ESP.restart();
}

// ── UDP コマンド処理 ──────────────────────────────────────────
static void handleUDPCommands() {
    int n = cmdUdp.parsePacket();
    if (n <= 0) return;
    char buf[CMD_BUF_SIZE];
    n = cmdUdp.read(buf, CMD_BUF_SIZE - 1);
    if (n <= 0) return;
    buf[n] = '\0';

    JsonDocument doc;
    if (deserializeJson(doc, buf) != DeserializationError::Ok) return;
    const char* cmd = doc["cmd"] | "";

    if (strcmp(cmd, "start") == 0) {
        measuring = true;
        Serial.println("[CMD] start");

    } else if (strcmp(cmd, "stop") == 0) {
        measuring = false;
        Serial.println("[CMD] stop");

    } else if (strcmp(cmd, "measure") == 0) {
        lastData = readIWS7817();
        sendDataUDP(lastData);
        Serial.println("[CMD] measure");

    } else if (strcmp(cmd, "ping") == 0) {
        JsonDocument resp;
        resp["pong"]      = true;
        resp["id"]        = cfg.device_id;
        resp["ip"]        = apMode ? AP_LOCAL_IP : WiFi.localIP().toString().c_str();
        resp["measuring"] = measuring;
        resp["i2c_addr"]  = cfg.i2c_addr;
        resp["udp_port"]  = cfg.udp_port;
        resp["cmd_port"]  = cfg.cmd_port;
        char rbuf[256];
        size_t rlen = serializeJson(resp, rbuf, sizeof(rbuf));
        cmdUdp.beginPacket(cmdUdp.remoteIP(), cmdUdp.remotePort());
        cmdUdp.write((uint8_t*)rbuf, rlen);
        cmdUdp.endPacket();
        Serial.printf("[CMD] ping from %s\n", cmdUdp.remoteIP().toString().c_str());

    } else if (strcmp(cmd, "reboot") == 0) {
        Serial.println("[CMD] reboot");
        delay(200);
        ESP.restart();
    }
}

// ── WiFi 接続 ─────────────────────────────────────────────────
static void startWiFi() {
    if (strlen(cfg.wifi_ssid) == 0) {
        // SSID 未設定: AP モード
        apMode = true;
        String apSSID = String(AP_SSID_PREFIX) + cfg.device_id;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
        Serial.printf("AP mode: SSID=%s PW=%s IP=%s\n", apSSID.c_str(), AP_PASSWORD, AP_LOCAL_IP);
        return;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass);
    Serial.printf("Connecting to %s", cfg.wifi_ssid);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
        delay(300);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiOk = true;
        Serial.printf("\nWiFi OK: %s  RSSI=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
        MDNS.begin(cfg.device_id);
        MDNS.addService("http", "tcp", 80);
    } else {
        // 接続失敗: AP モードへフォールバック
        Serial.println("\nWiFi timeout, starting AP fallback");
        apMode = true;
        String apSSID = String(AP_SSID_PREFIX) + cfg.device_id;
        WiFi.mode(WIFI_AP);
        WiFi.softAP(apSSID.c_str(), AP_PASSWORD);
    }
}

// ── setup ─────────────────────────────────────────────────────
void setup() {
    delay(10000);
    Serial.begin(115200);

    auto mcfg = M5.config();
    M5.begin(mcfg);
    M5.Power.begin();

    loadConfig();

    // 起動画面
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 0);
    M5.Display.printf("ID: %s\nWiFi...", cfg.device_id);

    // I2C (Grove SDA=1, SCL=2)
    Wire.begin(2, 1);
    Wire.setClock(10000);

    // // I2C スキャン (デバッグ)
    // Serial.println("--- I2C Scan ---");
    // for (uint8_t addr = 1; addr < 127; addr++) {
    //     Wire.beginTransmission(addr);
    //     if (Wire.endTransmission() == 0)
    //         Serial.printf("  Found: 0x%02X\n", addr);
    // }
    // Serial.println("----------------");

    startWiFi();

    // HTTP サーバ
    httpServer.on("/",            HTTP_GET,  handleRoot);
    httpServer.on("/api/config",  HTTP_GET,  handleGetConfig);
    httpServer.on("/api/config",  HTTP_POST, handlePostConfig);
    httpServer.on("/api/status",  HTTP_GET,  handleGetStatus);
    httpServer.on("/api/measure", HTTP_POST, handlePostMeasure);
    httpServer.on("/api/start",   HTTP_POST, handlePostStart);
    httpServer.on("/api/stop",    HTTP_POST, handlePostStop);
    httpServer.on("/api/reboot",  HTTP_POST, handlePostReboot);
    httpServer.onNotFound(handleRoot);
    httpServer.begin();

    // UDP
    dataUdp.begin(0);             // 送信専用 (任意ポート)
    cmdUdp.begin(cfg.cmd_port);   // コマンド受信

    measuring = cfg.auto_measure;

    Serial.printf("Ready: id=%s i2c=0x%02X target=%s:%u cmd_port=%u interval=%ums auto=%d\n",
        cfg.device_id, cfg.i2c_addr, cfg.target_ip,
        cfg.udp_port, cfg.cmd_port, cfg.interval_ms, cfg.auto_measure);

    updateDisplay();
}

// ── loop ──────────────────────────────────────────────────────
void loop() {
    M5.update();
    httpServer.handleClient();
    handleUDPCommands();

    // 定期測定
    if (measuring && millis() - lastMeasMs >= cfg.interval_ms) {
        lastData = readIWS7817();
        if (wifiOk || apMode) sendDataUDP(lastData);
        lastMeasMs = millis();
        if (lastData.valid)
            Serial.printf("[MEAS] r=%.3f mOhm  v=%.4f V\n", lastData.r_mohm, lastData.v);
        else
            Serial.println("[MEAS] invalid");
    }

    // ボタン: 測定開始/停止トグル
    if (M5.BtnA.wasClicked()) {
        measuring = !measuring;
        Serial.printf("Button: measuring=%s\n", measuring ? "ON" : "OFF");
    }

    // ディスプレイ定期更新
    if (millis() - lastDispMs >= DISP_UPDATE_MS) {
        updateDisplay();
        lastDispMs = millis();
    }

    // WiFi 再接続監視 (STA モード)
    if (!apMode) {
        if (WiFi.status() == WL_CONNECTED) {
            wifiOk = true;
        } else {
            wifiOk = false;
        }
    }

    delay(5);
}
