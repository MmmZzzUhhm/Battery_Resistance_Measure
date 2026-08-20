#include "hwtest_webserver.h"
#include <WiFi.h>
#include <WebServer.h>
#include <SD_MMC.h>
#include <RTClib.h>
#include <ArduinoJson.h>
#include "hwtest_config.h"
#include "hwtest_common.h"
#include "role_select.h"
#include "led_control.h"
#include "ble_child_test.h"
#include "camera_capture.h"
#include "test_common.h"
#include "test_rtc_datetime.h"
#include "test_rtc_pcf8563.h"
#include "test_sht31.h"
#include "test_sd_pins.h"
#include "test_sdmmc.h"
#include "test_sd_dir.h"
#include "test_pdm_mic.h"
#include "test_imd2000.h"
#include "test_camera.h"
#include "test_led.h"

#define AP_PASS            "hwtest1234"
#define STA_CONNECT_TIMEOUT_MS 15000UL
#define BLE_SCAN_SECONDS   4

namespace {

WebServer g_server(80);
bool      g_testMode = false;  // false=AP設定モード, true=テストモード(STA)

// ── 共通HTMLヘルパー ──────────────────────────────────────────
String htmlHeader(const char* title) {
    return String(
        "<!DOCTYPE html><html lang='ja'><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>") + title + "</title><style>"
        "body{font-family:sans-serif;max-width:640px;margin:16px auto;padding:0 12px;background:#fafafa}"
        "h2{color:#1976D2;margin-top:1.6em} h2:first-of-type{margin-top:0}"
        "fieldset{border:1px solid #ddd;border-radius:6px;margin:12px 0;padding:8px 12px}"
        "legend{color:#555;padding:0 6px}"
        "label{display:block;margin:8px 0 2px;font-size:.9em;color:#555}"
        "input,select{width:100%;box-sizing:border-box;padding:8px;border:1px solid #ccc;border-radius:4px;font-size:1em}"
        "button{padding:8px 16px;margin:4px 4px 4px 0;background:#1976D2;color:#fff;border:none;border-radius:4px;"
        "cursor:pointer;font-size:.95em}"
        "button.sec{background:#666}button.danger{background:#C62828}"
        "table{width:100%;border-collapse:collapse;margin-top:8px}"
        "td,th{padding:6px;border-bottom:1px solid #ddd;text-align:left;font-size:.85em}"
        ".ok{color:#2E7D32;font-weight:bold}.fail{color:#C62828;font-weight:bold}"
        "#status{margin-top:10px;padding:8px;border-radius:4px;display:none}"
        ".stok{background:#E8F5E9;color:#2E7D32}.sterr{background:#FFEBEE;color:#C62828}"
        "img.thumb{max-width:100%;border:1px solid #ccc;border-radius:4px;margin-top:6px}"
        "ul{padding-left:1.2em}"
        "</style></head><body>";
}
const char* HTML_FOOTER = "</body></html>";

void sendHtml(const String& body) {
    g_server.send(200, "text/html; charset=utf-8", body);
}

// ── AP設定モード ─────────────────────────────────────────────
void handleApRoot() {
    String html = htmlHeader("基板チェック WiFi設定");
    html += "<h2>WiFi設定</h2>"
        "<p style='color:#555;font-size:.9em'>設定を保存すると再起動し、指定したWiFiへの接続を試みます。"
        "接続できればテストモードへ、失敗すれば再度この設定画面に戻ります。</p>"
        "<form id='f'>"
        "<label>デバイス名 (AP SSID表示にも使用)</label><input id='device_name' value='" + String(hwCfg.device_name) + "'>"
        "<label>WiFi SSID</label><input id='wifi_ssid' value='" + String(hwCfg.wifi_ssid) + "'>"
        "<label>WiFi パスワード</label><input id='wifi_pass' type='password' value='" + String(hwCfg.wifi_pass) + "'>"
        "<label><input type='checkbox' id='use_static' style='width:auto' " +
        String(hwCfg.use_static_ip ? "checked" : "") + " onchange='toggleStatic()'> 固定IPを使用する</label>"
        "<div id='staticFields' style='display:" + String(hwCfg.use_static_ip ? "block" : "none") + "'>"
        "<label>IPアドレス</label><input id='static_ip' value='" + String(hwCfg.static_ip) + "'>"
        "<label>ゲートウェイ</label><input id='static_gw' value='" + String(hwCfg.static_gateway) + "'>"
        "<label>サブネットマスク</label><input id='static_sn' value='" + String(hwCfg.static_subnet) + "'>"
        "<label>DNS</label><input id='static_dns' value='" + String(hwCfg.static_dns) + "'>"
        "</div>"
        "<button type='button' onclick='save()'>保存して再起動</button>"
        "</form><div id='status'></div>"
        "<script>"
        "function toggleStatic(){document.getElementById('staticFields').style.display="
        "document.getElementById('use_static').checked?'block':'none';}"
        "function showStatus(msg,ok){const e=document.getElementById('status');e.textContent=msg;"
        "e.className=ok?'stok':'sterr';e.style.display='block';}"
        "async function save(){"
        "const body={device_name:document.getElementById('device_name').value,"
        "wifi_ssid:document.getElementById('wifi_ssid').value,"
        "wifi_pass:document.getElementById('wifi_pass').value,"
        "use_static_ip:document.getElementById('use_static').checked,"
        "static_ip:document.getElementById('static_ip').value,"
        "static_gateway:document.getElementById('static_gw').value,"
        "static_subnet:document.getElementById('static_sn').value,"
        "static_dns:document.getElementById('static_dns').value};"
        "if(!body.wifi_ssid){showStatus('WiFi SSIDは必須です',false);return;}"
        "const r=await fetch('/save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});"
        "const j=await r.json();"
        "if(j.ok){showStatus('保存しました。再起動します...',true);}else{showStatus('保存に失敗しました',false);}"
        "}</script>";
    html += HTML_FOOTER;
    sendHtml(html);
}

void handleApSave() {
    if (!g_server.hasArg("plain")) {
        g_server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, g_server.arg("plain")) != DeserializationError::Ok) {
        g_server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    strlcpy(hwCfg.device_name, doc["device_name"] | "HWTEST-BOARD", sizeof(hwCfg.device_name));
    strlcpy(hwCfg.wifi_ssid, doc["wifi_ssid"] | "", sizeof(hwCfg.wifi_ssid));
    const char* pass = doc["wifi_pass"] | "";
    if (strlen(pass) > 0) strlcpy(hwCfg.wifi_pass, pass, sizeof(hwCfg.wifi_pass));
    hwCfg.use_static_ip = doc["use_static_ip"] | false;
    strlcpy(hwCfg.static_ip, doc["static_ip"] | "", sizeof(hwCfg.static_ip));
    strlcpy(hwCfg.static_gateway, doc["static_gateway"] | "", sizeof(hwCfg.static_gateway));
    strlcpy(hwCfg.static_subnet, doc["static_subnet"] | "255.255.255.0", sizeof(hwCfg.static_subnet));
    strlcpy(hwCfg.static_dns, doc["static_dns"] | "", sizeof(hwCfg.static_dns));
    hwtestConfigSave();

    g_server.send(200, "application/json", "{\"ok\":true}");
    delay(400);
    ESP.restart();
}

void startApConfigMode() {
    g_testMode = false;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(hwCfg.device_name, AP_PASS);
    Serial.printf("[HWTEST] AP設定モード: SSID=%s PASS=%s IP=%s\n",
        hwCfg.device_name, AP_PASS, WiFi.softAPIP().toString().c_str());

    g_server.on("/", HTTP_GET, handleApRoot);
    g_server.on("/save", HTTP_POST, handleApSave);
    g_server.begin();
}

// ── テストモード: 役割別一括テスト ────────────────────────────
void runTestsForRole(BoardRole role) {
    testResultsReset();
    testChipInfo();

    switch (role) {
        case ROLE_PARENT:
            testI2CScan();
            testRtcReadTime();
            testRtcPcf8563();
            testSht31();
            testSdPinIntegrity();
            testSdMmc();
            testSdDirList();
            break;
        case ROLE_ULTRASONIC:
            testPdmMic();
            break;
        case ROLE_DOPPLER:
            testI2CScan();
            testRtcReadTime();
            testRtcPcf8563();
            testSdPinIntegrity();
            testSdMmc();
            testSdDirList();
            testImd2000();
            break;
        case ROLE_COUNTER_CAMERA:
            testI2CScan();
            testSdPinIntegrity();
            testSdMmc();
            testSdDirList();
            testCamera();
            testLedAdg728();
            break;
    }

    testWifiStaScan();
    testWifiApSoftAp(hwCfg.device_name, AP_PASS);
    WiFi.mode(WIFI_STA);  // テストモードのWeb UI継続のためSTA単独に戻す
    testBleAdvertise(hwCfg.device_name);
    printSummary();
}

String renderResultsTable() {
    if (testResultCount() == 0) return "<p style='color:#888'>(未実行)</p>";
    String html = "<table><thead><tr><th>項目</th><th>結果</th><th>詳細</th></tr></thead><tbody>";
    int passCount = 0;
    for (int i = 0; i < testResultCount(); i++) {
        const TestResult& r = testResultAt(i);
        if (r.pass) passCount++;
        html += "<tr><td>" + String(r.name) + "</td><td class='" + (r.pass ? "ok'>OK" : "fail'>FAIL") +
                "</td><td>" + r.detail + "</td></tr>";
    }
    html += "</tbody></table><p>" + String(passCount) + " / " + String(testResultCount()) + " 項目 PASS</p>";
    return html;
}

// ── テストモード: メインページ ────────────────────────────────
void handleTestRoot() {
    String html = htmlHeader("基板動作チェック");
    html += "<p style='color:#555;font-size:.9em'>" + String(hwCfg.device_name) + " / IP=" +
        WiFi.localIP().toString() + " <a href='/wifi'>[WiFi設定]</a></p>";

    html += "<h2>一括テスト</h2>"
        "<fieldset><legend>構成を選んでテスト実行</legend>"
        "<select id='role'>"
        "<option value='1'" + String(hwCfg.last_role == 1 ? " selected" : "") + ">1: 親機</option>"
        "<option value='2'" + String(hwCfg.last_role == 2 ? " selected" : "") + ">2: 超音波センサ</option>"
        "<option value='3'" + String(hwCfg.last_role == 3 ? " selected" : "") + ">3: ドップラーセンサカメラ</option>"
        "<option value='4'" + String(hwCfg.last_role == 4 ? " selected" : "") + ">4: 薄型カウンタカメラ</option>"
        "</select>"
        "<button onclick='runTests()'>テスト実行</button>"
        "<div id='results'>" + renderResultsTable() + "</div>"
        "</fieldset>";

    html += "<h2>RTC</h2>"
        "<fieldset><button onclick='readRtc()'>現在時刻を読み取る</button>"
        "<p id='rtcResult' style='color:#555'></p></fieldset>";

    html += "<h2>カメラ</h2>"
        "<fieldset><button onclick='capturePhoto()'>撮影してSD保存</button>"
        "<div id='cameraResult'></div>"
        "<p style='color:#555;font-size:.85em'>保存済み画像:</p><ul id='imageList'></ul>"
        "</fieldset>";

    html += "<h2>LED明るさ (ADG728)</h2>"
        "<fieldset>"
        "<label>LED1</label><select id='led1' onchange=\"setLed(1,this.value)\">"
        "<option value='0'>消灯</option><option value='1'>1(最も明るい)</option>"
        "<option value='2'>2</option><option value='3'>3</option><option value='4'>4(最も暗い)</option></select>"
        "<label>LED2</label><select id='led2' onchange=\"setLed(2,this.value)\">"
        "<option value='0'>消灯</option><option value='1'>1(最も明るい)</option>"
        "<option value='2'>2</option><option value='3'>3</option><option value='4'>4(最も暗い)</option></select>"
        "</fieldset>";

    html += "<h2>BLE子機 疎通確認</h2>"
        "<fieldset><button onclick='scanBle()'>近くの子機をスキャン</button>"
        "<ul id='bleList'></ul>"
        "<label>子機device_id (直接指定、空なら最初に見つかった子機)</label>"
        "<input id='bleTarget'><button onclick='checkBle()'>疎通確認</button>"
        "<div id='bleResult'></div></fieldset>";

    html += "<h2>モード</h2>"
        "<fieldset><button class='danger' onclick='backToAp()'>APモードへ戻る</button></fieldset>";

    html += "<div id='status'></div><script>"
        "function showStatus(msg,ok){const e=document.getElementById('status');e.textContent=msg;"
        "e.className=ok?'stok':'sterr';e.style.display='block';setTimeout(()=>e.style.display='none',5000);}"
        "async function runTests(){"
        "showStatus('テスト実行中...',true);"
        "const role=document.getElementById('role').value;"
        "const r=await fetch('/api/run_tests?role='+role,{method:'POST'});"
        "const html=await r.text();"
        "document.getElementById('results').innerHTML=html;"
        "showStatus('テスト完了',true);}"
        "async function readRtc(){"
        "const r=await fetch('/api/rtc/read');const j=await r.json();"
        "document.getElementById('rtcResult').textContent=j.ok?('現在時刻: '+j.datetime):('読み取り失敗: '+j.error);}"
        "async function refreshImages(){"
        "const r=await fetch('/api/camera/list');const j=await r.json();"
        "const ul=document.getElementById('imageList');ul.innerHTML='';"
        "j.images.forEach(n=>{const li=document.createElement('li');"
        "li.innerHTML=\"<a href='/api/camera/image?name=\"+n+\"' target='_blank'>\"+n+\"</a>\";ul.appendChild(li);});}"
        "async function capturePhoto(){"
        "showStatus('撮影中...',true);"
        "const r=await fetch('/api/camera/capture',{method:'POST'});const j=await r.json();"
        "const div=document.getElementById('cameraResult');"
        "if(j.ok){div.innerHTML='保存先: '+j.path+\"<br><img class='thumb' src='/api/camera/image?name=\"+j.name+\"'>\";"
        "showStatus('撮影OK',true);refreshImages();}"
        "else{div.textContent='撮影失敗: '+j.error;showStatus('撮影失敗',false);}}"
        "async function setLed(led,level){"
        "const r=await fetch('/api/led/set?led='+led+'&level='+level);const j=await r.json();"
        "showStatus(j.ok?('LED'+led+': レベル'+level):'LED設定に失敗しました',j.ok);}"
        "async function scanBle(){"
        "showStatus('スキャン中(数秒)...',true);"
        "const r=await fetch('/api/ble/scan');const j=await r.json();"
        "const ul=document.getElementById('bleList');ul.innerHTML='';"
        "if(j.devices.length===0){ul.innerHTML='<li>(見つかりませんでした)</li>';}"
        "j.devices.forEach(d=>{const li=document.createElement('li');"
        "li.innerHTML=\"<a href='#' onclick=\\\"document.getElementById('bleTarget').value='\"+d.name+\"';return false;\\\">\"+"
        "d.name+' (RSSI '+d.rssi+')'+'</a>';ul.appendChild(li);});"
        "showStatus('スキャン完了 '+j.devices.length+'件',true);}"
        "async function checkBle(){"
        "showStatus('接続確認中...',true);"
        "const name=document.getElementById('bleTarget').value;"
        "const r=await fetch('/api/ble/check?name='+encodeURIComponent(name));const j=await r.json();"
        "const div=document.getElementById('bleResult');"
        "if(j.ok){div.innerHTML='device_id='+j.device_id+'<br>fw_version='+j.fw_version+"
        "'<br>battery_id='+j.battery_id+'<br>pending_count='+j.pending_count;showStatus('疎通OK',true);}"
        "else{div.textContent='失敗: '+j.error;showStatus('疎通確認に失敗しました',false);}}"
        "async function backToAp(){"
        "if(!confirm('APモードへ戻ります。よろしいですか?'))return;"
        "await fetch('/api/back_to_ap',{method:'POST'});"
        "showStatus('再起動します...',true);}"
        "refreshImages();"
        "</script>";
    html += HTML_FOOTER;
    sendHtml(html);
}

void handleTestWifiPage() {
    handleApRoot();  // WiFi再設定フォームは共通のものを流用 (保存すると再起動しSTA再接続を試みる)
}

void handleApiRunTests() {
    int role = g_server.hasArg("role") ? g_server.arg("role").toInt() : hwCfg.last_role;
    if (role < 1 || role > 4) role = 1;
    hwCfg.last_role = (uint8_t)role;
    hwtestConfigSave();
    runTestsForRole((BoardRole)role);
    g_server.send(200, "text/html; charset=utf-8", renderResultsTable());
}

void handleApiRtcRead() {
    RTC_PCF8563 rtc;
    if (!rtc.begin()) {
        g_server.send(200, "application/json", "{\"ok\":false,\"error\":\"RTC not found (addr 0x51)\"}");
        return;
    }
    DateTime now = rtc.now();
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
        now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    String out = String("{\"ok\":true,\"datetime\":\"") + buf + "\"}";
    g_server.send(200, "application/json", out);
}

void handleApiCameraCapture() {
    String path, err;
    if (!cameraCaptureAndSave(path, err)) {
        String out = "{\"ok\":false,\"error\":\"" + err + "\"}";
        g_server.send(200, "application/json", out);
        return;
    }
    // pathは "/hwtest_camera/cap_xxx.jpg" 形式。ファイル名部分だけ抜き出す。
    int slash = path.lastIndexOf('/');
    String name = slash >= 0 ? path.substring(slash + 1) : path;
    String out = "{\"ok\":true,\"path\":\"" + path + "\",\"name\":\"" + name + "\"}";
    g_server.send(200, "application/json", out);
}

void handleApiCameraList() {
    std::vector<String> names = cameraListSavedImages(20);
    String out = "{\"images\":[";
    for (size_t i = 0; i < names.size(); i++) {
        if (i > 0) out += ",";
        out += "\"" + names[i] + "\"";
    }
    out += "]}";
    g_server.send(200, "application/json", out);
}

void handleApiCameraImage() {
    if (!g_server.hasArg("name")) {
        g_server.send(400, "text/plain", "name required");
        return;
    }
    String path = String(CAMERA_SAVE_DIR) + "/" + g_server.arg("name");
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) {
        g_server.send(404, "text/plain", "not found");
        return;
    }
    g_server.streamFile(f, "image/jpeg");
    f.close();
}

void handleApiLedSet() {
    if (!g_server.hasArg("led") || !g_server.hasArg("level")) {
        g_server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    int ledArg = g_server.arg("led").toInt();
    int levelArg = g_server.arg("level").toInt();
    if ((ledArg != LIGHT_LED1 && ledArg != LIGHT_LED2) || levelArg < 0 || levelArg > LIGHT_LEVEL_MAX) {
        g_server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    bool ok = lightSetLevel((LightId)ledArg, (uint8_t)levelArg);
    String out = String("{\"ok\":") + (ok ? "true" : "false") + "}";
    g_server.send(ok ? 200 : 500, "application/json", out);
}

void handleApiBleScan() {
    std::vector<BleChildAdv> devices = bleScanChildren(BLE_SCAN_SECONDS);
    String out = "{\"devices\":[";
    for (size_t i = 0; i < devices.size(); i++) {
        if (i > 0) out += ",";
        out += "{\"name\":\"" + devices[i].name + "\",\"rssi\":" + String(devices[i].rssi) + "}";
    }
    out += "]}";
    g_server.send(200, "application/json", out);
}

void handleApiBleCheck() {
    String target = g_server.hasArg("name") ? g_server.arg("name") : "";
    BleChildStatus st = bleCheckChildStatus(target, BLE_SCAN_SECONDS);
    if (!st.ok) {
        g_server.send(200, "application/json", "{\"ok\":false,\"error\":\"" + st.error + "\"}");
        return;
    }
    String out = "{\"ok\":true,\"device_id\":\"" + st.device_id + "\",\"fw_version\":\"" + st.fw_version +
        "\",\"battery_id\":\"" + st.battery_id + "\",\"pending_count\":" + String(st.pending_count) + "}";
    g_server.send(200, "application/json", out);
}

void handleApiBackToAp() {
    hwCfg.wifi_ssid[0] = '\0';
    hwtestConfigSave();
    g_server.send(200, "application/json", "{\"ok\":true}");
    delay(400);
    ESP.restart();
}

bool tryConnectSta() {
    WiFi.mode(WIFI_STA);
    if (hwCfg.use_static_ip && strlen(hwCfg.static_ip) > 0) {
        IPAddress ip, gw, sn, dns;
        ip.fromString(hwCfg.static_ip);
        gw.fromString(hwCfg.static_gateway);
        sn.fromString(hwCfg.static_subnet);
        if (strlen(hwCfg.static_dns) > 0) dns.fromString(hwCfg.static_dns);
        WiFi.config(ip, gw, sn, dns);
    }
    WiFi.begin(hwCfg.wifi_ssid, hwCfg.wifi_pass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < STA_CONNECT_TIMEOUT_MS) {
        delay(200);
    }
    return WiFi.status() == WL_CONNECTED;
}

void startTestMode() {
    g_testMode = true;
    Serial.printf("[HWTEST] テストモード: IP=%s\n", WiFi.localIP().toString().c_str());

    g_server.on("/", HTTP_GET, handleTestRoot);
    g_server.on("/wifi", HTTP_GET, handleTestWifiPage);
    g_server.on("/save", HTTP_POST, handleApSave);
    g_server.on("/api/run_tests", HTTP_POST, handleApiRunTests);
    g_server.on("/api/rtc/read", HTTP_GET, handleApiRtcRead);
    g_server.on("/api/camera/capture", HTTP_POST, handleApiCameraCapture);
    g_server.on("/api/camera/list", HTTP_GET, handleApiCameraList);
    g_server.on("/api/camera/image", HTTP_GET, handleApiCameraImage);
    g_server.on("/api/led/set", HTTP_GET, handleApiLedSet);
    g_server.on("/api/ble/scan", HTTP_GET, handleApiBleScan);
    g_server.on("/api/ble/check", HTTP_GET, handleApiBleCheck);
    g_server.on("/api/back_to_ap", HTTP_POST, handleApiBackToAp);
    g_server.begin();
}

}  // namespace

void hwtestWebServerBegin() {
    if (strlen(hwCfg.wifi_ssid) == 0) {
        Serial.println("[HWTEST] WiFi未設定のためAP設定モードへ入ります");
        startApConfigMode();
        return;
    }

    Serial.printf("[HWTEST] 保存済みWiFiへ接続試行: SSID=%s\n", hwCfg.wifi_ssid);
    if (tryConnectSta()) {
        startTestMode();
    } else {
        Serial.println("[HWTEST] STA接続に失敗しました。AP設定モードにフォールバックします");
        startApConfigMode();
    }
}

void hwtestWebServerLoop() {
    g_server.handleClient();
}
