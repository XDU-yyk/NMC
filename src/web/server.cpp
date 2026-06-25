/**
 * @file    server.cpp
 * @brief   WebSocket Web 服务器 v2.0 — 解耦版
 * 
 * 遥测数据通过 TelemetryCallback 注入；
 * 未注册回调时自动使用内部模拟数据 (fillSimData)。
 */

#include "web/server.h"
#include "web/index_html.h"

WebServerManager webServer;
WebServerManager* g_webServerInstance = nullptr;

static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);
static constexpr uint8_t AP_CHANNEL = 1;
static constexpr uint8_t AP_MAX_CLIENTS = 4;

#ifdef WEB_HTTP_ONLY
static const char HTTP_ONLY_INDEX[] = R"rawliteral(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NMC Web MVP</title>
<style>
:root{color-scheme:dark;--bg:#0a0e17;--panel:#111827;--line:#334155;--text:#e2e8f0;--muted:#94a3b8;--ok:#22c55e;--warn:#f59e0b;--blue:#60a5fa;}
*{box-sizing:border-box}
body{margin:0;font-family:Arial,sans-serif;background:var(--bg);color:var(--text);padding:18px;line-height:1.45}
.wrap{max-width:980px;margin:0 auto}
header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:14px}
h1{font-size:1.35rem;margin:0}
.pill{border:1px solid var(--line);border-radius:999px;padding:6px 10px;color:var(--muted);font-size:.82rem}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:10px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:12px;min-height:92px}
.label{font-size:.78rem;color:var(--muted);margin-bottom:6px}
.value{font-size:1.45rem;font-weight:700}
.unit{font-size:.82rem;color:var(--muted);font-weight:400;margin-left:3px}
.ok{color:var(--ok)}.warn{color:var(--warn)}.blue{color:var(--blue)}
.log{margin-top:12px;color:var(--muted);font-size:.85rem}
button{background:#1d4ed8;color:white;border:0;border-radius:6px;padding:9px 12px;font-weight:700}
</style>
</head>
<body>
<div class="wrap">
<header>
  <div>
    <h1>NMC Web MVP</h1>
    <div class="log">HTTP polling mode · ESP32-S3</div>
  </div>
  <div class="pill" id="net">connecting</div>
</header>
<div class="grid">
  <div class="card"><div class="label">System</div><div class="value ok" id="sys">OK</div><div class="log" id="uptime">--</div></div>
  <div class="card"><div class="label">ToF Distance</div><div class="value" id="tof">--<span class="unit">mm</span></div></div>
  <div class="card"><div class="label">Altitude</div><div class="value" id="alt">--<span class="unit">cm</span></div></div>
  <div class="card"><div class="label">Battery</div><div class="value" id="bat">--<span class="unit">V</span></div></div>
  <div class="card"><div class="label">Attitude</div><div class="value blue" id="att">--</div></div>
  <div class="card"><div class="label">GPS</div><div class="value" id="gps">--</div></div>
  <div class="card"><div class="label">Flight Controller</div><div class="value warn" id="fc">offline</div></div>
  <div class="card"><div class="label">Heap</div><div class="value" id="heap">--<span class="unit">B</span></div></div>
</div>
<div class="log" id="log">Waiting for telemetry...</div>
</div>
<script>
const $=id=>document.getElementById(id);
function n(v,d=1){return Number(v||0).toFixed(d)}
function tick(){
 fetch('/api/telemetry?ts='+Date.now(),{cache:'no-store'})
  .then(r=>r.json())
  .then(d=>{
    $('net').textContent='online';
    $('net').className='pill ok';
    $('tof').innerHTML=n(d.tofDist,0)+'<span class="unit">mm</span>';
    $('alt').innerHTML=n(d.baroAlt,0)+'<span class="unit">cm</span>';
    $('bat').innerHTML=n(d.batV,2)+'<span class="unit">V</span>';
    $('att').textContent='R '+n(d.roll)+' / P '+n(d.pitch)+' / Y '+n(d.yaw);
    $('gps').textContent=d.gpsValid ? (d.gpsSats+' sats') : 'no fix';
    $('fc').textContent=d.fcOnline ? 'online' : 'offline';
    $('fc').className=d.fcOnline?'value ok':'value warn';
    $('heap').innerHTML=(d.freeHeap||0)+'<span class="unit">B</span>';
    $('uptime').textContent=Math.floor((d.uptime||0)/1000)+' s';
    $('log').textContent='Telemetry updated: '+new Date().toLocaleTimeString();
  })
  .catch(e=>{
    $('net').textContent='retrying';
    $('net').className='pill warn';
    $('log').textContent='Telemetry request failed';
  });
}
tick();
setInterval(tick,1000);
</script>
</body>
</html>)rawliteral";
#endif

bool WebServerManager::begin(const char* ssid, const char* password, bool apMode)
{
    m_apMode = apMode;
    g_webServerInstance = this;

    if (apMode)
    {
        WiFi.persistent(false);
        WiFi.disconnect(true, true);
        WiFi.mode(WIFI_OFF);
        delay(300);

        WiFi.mode(WIFI_AP);
        WiFi.setSleep(false);

        if (!WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET))
        {
            LOG(LOG_TAG_WEB, "softAPConfig failed");
            return false;
        }

        bool ok = WiFi.softAP(ssid, password, AP_CHANNEL, false, AP_MAX_CLIENTS);
        if (!ok)
        {
            LOG(LOG_TAG_WEB, "softAP failed");
            return false;
        }

        delay(100);
        LOG(LOG_TAG_WEB, "AP: %s @ %s ch=%u",
            ssid,
            WiFi.softAPIP().toString().c_str(),
            AP_CHANNEL);
    }
    else
    {
        WiFi.begin(ssid, password);
        LOG(LOG_TAG_WEB, "WiFi: %s", ssid);
        int a = 0;
        while (WiFi.status() != WL_CONNECTED && a < 30) { delay(500); DEBUG_SERIAL.print("."); a++; }
        if (WiFi.status() != WL_CONNECTED) { LOG(LOG_TAG_WEB, "WiFi failed!"); return false; }
        LOG(LOG_TAG_WEB, "IP: %s", WiFi.localIP().toString().c_str());
    }

    if (!apMode) setupMDNS();

    m_httpServer.begin();
    m_httpServer.setNoDelay(true);

#ifndef WEB_HTTP_ONLY
    m_wsServer.listen(WS_SERVER_PORT);
#endif

    LOG(LOG_TAG_WEB, "Ready → http://%s.local", DEVICE_MDNS);
    return true;
}

void WebServerManager::sendText(WiFiClient& client, const char* body)
{
    client.print("HTTP/1.0 200 OK\r\n");
    client.print("Content-Type: text/plain; charset=utf-8\r\n");
    client.print("Cache-Control: no-store\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Length: ");
    client.print(strlen(body));
    client.print("\r\n\r\n");
    client.print(body);
    client.flush();
}

void WebServerManager::sendJson(WiFiClient& client, const String& body)
{
    client.print("HTTP/1.0 200 OK\r\n");
    client.print("Content-Type: application/json; charset=utf-8\r\n");
    client.print("Cache-Control: no-store\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Length: ");
    client.print(body.length());
    client.print("\r\n\r\n");
    client.print(body);
    client.flush();
}

void WebServerManager::sendIndex(WiFiClient& client)
{
#ifdef WEB_HTTP_ONLY
    const char* page = HTTP_ONLY_INDEX;
#else
    const char* page = INDEX_HTML;
#endif
    size_t length = strlen(page);

    client.print("HTTP/1.0 200 OK\r\n");
    client.print("Content-Type: text/html; charset=utf-8\r\n");
    client.print("Cache-Control: no-store\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Length: ");
    client.print(length);
    client.print("\r\n\r\n");
    client.write(reinterpret_cast<const uint8_t*>(page), length);
    client.flush();
}

bool WebServerManager::setupMDNS()
{
    if (!MDNS.begin(DEVICE_MDNS)) return false;
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
    return true;
}

void WebServerManager::handleHttpClient()
{
    WiFiClient client = m_httpServer.available();
    if (!client) return;

    handleHttpRequest(client);
    delay(10);
    client.stop();
}

void WebServerManager::handleHttpRequest(WiFiClient& client)
{
    client.setTimeout(1500);

    String requestLine = client.readStringUntil('\n');
    requestLine.trim();

    while (client.connected() && client.available())
    {
        String header = client.readStringUntil('\n');
        if (header == "\r" || header.length() == 0) break;
    }

    LOG(LOG_TAG_WEB, "HTTP %s from %s",
        requestLine.c_str(),
        client.remoteIP().toString().c_str());

    if (requestLine.startsWith("GET /ping "))
    {
        sendText(client, "pong\n");
    }
    else if (requestLine.startsWith("GET /api/telemetry "))
    {
        JsonDocument doc;
        buildTelemetryJson(doc);
        doc["apStations"] = WiFi.softAPgetStationNum();

        String out;
        serializeJson(doc, out);
        sendJson(client, out);
    }
    else
    {
        sendIndex(client);
    }
}

#ifndef WEB_HTTP_ONLY
void WebServerManager::acceptNewClients()
{
    m_wsServer.poll();
    while (m_wsServer.available())
    {
        auto client = m_wsServer.accept();
        LOG(LOG_TAG_WEB, "WS client connected (#%zu)", m_clients.size() + 1);

        // 使用回调模式处理消息（避免 readBlocking 阻塞问题）
        client.onMessage([this](WebsocketsClient& cl, WebsocketsMessage msg) {
            this->handleClientMessage(cl, msg);
        });

        m_clients.push_back(std::move(client));
    }

    auto it = m_clients.begin();
    while (it != m_clients.end())
    {
        if (!it->available())
        {
            LOG(LOG_TAG_WEB, "WS client removed");
            it = m_clients.erase(it);
        }
        else { ++it; }
    }
}

void WebServerManager::handleClientMessage(WebsocketsClient& client, WebsocketsMessage msg)
{
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msg.data());
    if (err) { LOG(LOG_TAG_WEB, "JSON err: %s", err.c_str()); return; }

    const char* cmd = doc["cmd"];
    int value = doc["value"] | 0;
    LOG(LOG_TAG_WEB, "cmd: %s (val=%d)", cmd, value);

    // 通用命令转发 — 具体处理由注册方通过 onCommand 完成
    if (m_cmdCallback) m_cmdCallback(cmd, value);
}
#endif

/* ── 内部模拟数据生成器（无注册回调时自动使用） ── */
void WebServerManager::fillSimData(TelemetryData& data)
{
    uint32_t now = millis();
    float t = now * 0.001f;

    // 姿态 — 缓慢正弦摆动
    data.roll  = 6.0f * sinf(t * 0.25f);
    data.pitch = 4.0f * sinf(t * 0.20f + 1.2f);
    data.yaw   = 25.0f * sinf(t * 0.10f + 2.5f);

    // 位置 — 模拟小幅漂移
    data.posX = 200.0f * sinf(t * 0.05f);
    data.posY = 150.0f * sinf(t * 0.04f + 0.5f);
    data.posZ = 100.0f + 30.0f * sinf(t * 0.08f);

    // ToF — 在 800~2500mm 间变化
    data.tofDistance = 1500.0f + 700.0f * sinf(t * 0.15f);
    data.altitude    = 120.0f + 40.0f * sinf(t * 0.10f);

    // 电池 — 缓慢下降
    data.batteryVoltage = 11.4f - 0.02f * (t / 60.0f);
    data.batteryCells   = 3;

    // GPS — 显示固定坐标（模拟已定位）
    data.gpsValid = true;
    data.gpsSats  = 8 + (int)(sinf(t * 0.1f) * 2);
    data.gpsLat   = 39.9042;
    data.gpsLng   = 116.4074;
    data.gpsAlt   = 50.0f;
    data.gpsSpeed = 0.5f + 0.3f * sinf(t * 0.1f);

    // 飞控 — 默认离线
    data.fcOnline    = false;
    data.armed       = false;
    data.flightMode  = 0;

    // 系统
    data.uptime      = now;
    data.freeHeap    = ESP.getFreeHeap();
    data.chipTemp    = temperatureRead();
#ifdef WEB_HTTP_ONLY
    data.clientCount = 0;
#else
    data.clientCount = m_clients.size();
#endif
}

void WebServerManager::buildTelemetryJson(JsonDocument& doc)
{
    TelemetryData data;

    if (m_telemetryCb) {
        m_telemetryCb(data);      // 外部数据源
    } else {
        fillSimData(data);        // 内部模拟回退
    }

    // 姿态
    doc["roll"]  = data.roll;
    doc["pitch"] = data.pitch;
    doc["yaw"]   = data.yaw;

    // 位置
    doc["posX"] = data.posX;
    doc["posY"] = data.posY;
    doc["posZ"] = data.posZ;

    // 传感器
    doc["tofDist"]  = data.tofDistance;
    doc["baroAlt"]  = data.altitude;
    doc["batV"]     = data.batteryVoltage;
    doc["batCells"] = data.batteryCells;

    // GPS
    doc["gpsValid"] = data.gpsValid;
    doc["gpsSats"]  = data.gpsSats;
    doc["gpsLat"]   = data.gpsLat;
    doc["gpsLng"]   = data.gpsLng;
    doc["gpsAlt"]   = data.gpsAlt;
    doc["gpsSpeed"] = data.gpsSpeed;

    // 飞控
    doc["fcOnline"]   = data.fcOnline;
    doc["armed"]      = data.armed;
    doc["flightMode"] = data.flightMode;

    // 系统
    doc["uptime"]    = data.uptime;
    doc["freeHeap"]  = data.freeHeap;
    doc["chipTemp"]  = data.chipTemp;
    doc["clients"]   = data.clientCount;

    doc["ts"] = millis();
}

void WebServerManager::loop()
{
    handleHttpClient();
#ifndef WEB_HTTP_ONLY
    acceptNewClients();

    // poll() 会触发 onMessage 回调，无需手动 readBlocking
    for (auto& client : m_clients)
    {
        client.poll();
    }
#endif
}

void WebServerManager::broadcastTelemetry()
{
#ifdef WEB_HTTP_ONLY
    return;
#else
    if (m_clients.empty()) return;

    JsonDocument doc;
    buildTelemetryJson(doc);

    String out; serializeJson(doc, out);
    for (auto& client : m_clients)
    {
        if (client.available()) client.send(out);
    }
#endif
}
