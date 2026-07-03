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
TunableParams g_params;

static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GATEWAY(192, 168, 4, 1);
static const IPAddress AP_SUBNET(255, 255, 255, 0);
static constexpr uint8_t AP_CHANNEL = 1;
static constexpr uint8_t AP_MAX_CLIENTS = 4;


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
    const char* page = INDEX_HTML;
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

    if (requestLine.startsWith("GET /ping ") || requestLine.startsWith("GET /ping?"))
    {
        sendText(client, "pong\n");
    }
    else if (requestLine.startsWith("GET /api/telemetry ") || requestLine.startsWith("GET /api/telemetry?"))
    {
        JsonDocument doc;
        buildTelemetryJson(doc);
        doc["apStations"] = WiFi.softAPgetStationNum();

        String out;
        serializeJson(doc, out);
        sendJson(client, out);
    }
    else if (requestLine.startsWith("GET /api/params ") || requestLine.startsWith("GET /api/params?"))
    {
        JsonDocument doc;
        doc["webPollMs"]       = g_params.webPollMs;
        doc["followDistanceM"] = g_params.followDistanceM;
        doc["maxPitchDeg"]     = g_params.maxPitchDeg;
        doc["maxRollDeg"]      = g_params.maxRollDeg;
        String out;
        serializeJson(doc, out);
        sendJson(client, out);
    }
    else if (requestLine.startsWith("POST /api/params ") || requestLine.startsWith("POST /api/params?"))
    {
        // 等 body 到达
        delay(50);
        String body;
        while (client.connected() && client.available()) {
            char c = client.read();
            if (c == -1) break;
            body += c;
        }
        LOG(LOG_TAG_WEB, "POST body: %s", body.c_str());
        JsonDocument doc;
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            if (doc["webPollMs"].is<uint16_t>()) {
                uint16_t v = doc["webPollMs"].as<uint16_t>();
                if (v >= 200 && v <= 5000) {
                    g_params.webPollMs = v;
                    LOG(LOG_TAG_WEB, "param webPollMs=%u", v);
                }
            }
            if (doc["followDistanceM"].is<float>()) {
                float v = doc["followDistanceM"].as<float>();
                if (v >= 0.5f && v <= 5.0f) {
                    g_params.followDistanceM = v;
                    LOG(LOG_TAG_WEB, "param followDistanceM=%.1f", v);
                }
            }
            if (doc["maxPitchDeg"].is<float>()) {
                float v = doc["maxPitchDeg"].as<float>();
                if (v >= 1.0f && v <= 15.0f) {
                    g_params.maxPitchDeg = v;
                    LOG(LOG_TAG_WEB, "param maxPitchDeg=%.1f", v);
                }
            }
            if (doc["maxRollDeg"].is<float>()) {
                float v = doc["maxRollDeg"].as<float>();
                if (v >= 1.0f && v <= 15.0f) {
                    g_params.maxRollDeg = v;
                    LOG(LOG_TAG_WEB, "param maxRollDeg=%.1f", v);
                }
            }
            sendText(client, "ok\n");
        } else {
            sendText(client, "bad json\n");
        }
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

    // GPS — 默认 offline/no fix
    data.gpsValid = false;
    data.gpsSats  = 0;
    data.gpsLat   = 0;
    data.gpsLng   = 0;
    data.gpsAlt   = 0;
    data.gpsSpeed = 0;
    data.gpsOnline = false;
    data.gpsAgeMs = 0;
    data.gpsChars = 0;
    data.gpsSentences = 0;
    data.gpsFailedChecksum = 0;

    // 飞控 — 默认离线
    data.fcOnline    = false;
    data.armed       = false;
    data.flightMode  = 0;

    // 数据源在线状态（无硬件时模拟离线）
    data.tofOnline   = false;
    data.gpsOnline   = true;   // GPS 模拟 fix（模拟数据源在线）
    data.dataSource  = 0;      // sim
    data.errorFlags  = 0;
    data.tofStatus   = 255;    // not initialized
    data.tofErrors   = 0;
    data.tofAgeMs    = 0;

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
    doc["gpsAgeMs"] = data.gpsAgeMs;
    doc["gpsChars"] = data.gpsChars;
    doc["gpsSentences"] = data.gpsSentences;
    doc["gpsFailedChecksum"] = data.gpsFailedChecksum;

    // 飞控
    doc["fcOnline"]   = data.fcOnline;
    doc["armed"]      = data.armed;
    doc["flightMode"] = data.flightMode;

    // 数据源在线状态
    doc["tofOnline"]   = data.tofOnline;
    doc["gpsOnline"]   = data.gpsOnline;
    doc["dataSource"]  = data.dataSource;
    doc["errorFlags"]  = data.errorFlags;
    doc["tofStatus"]  = data.tofStatus;
    doc["tofErrors"]  = data.tofErrors;
    doc["tofAgeMs"]   = data.tofAgeMs;

    // 系统
    doc["uptime"]    = data.uptime;
    doc["freeHeap"]  = data.freeHeap;
    doc["chipTemp"]  = data.chipTemp;
    doc["tofStatus"]   = data.tofStatus;
    doc["tofErrors"]   = data.tofErrors;
    doc["tofAgeMs"]    = data.tofAgeMs;
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
