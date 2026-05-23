/**
 * @file    server.cpp
 * @brief   WebSocket 嵌入式 Web 服务器实现
 */

#include "web/server.h"
#include "web/index_html.h"

WebServerManager webServer;
WebServerManager* g_webServerInstance = nullptr;

bool WebServerManager::begin(const char* ssid, const char* password, bool apMode) {
    m_apMode = apMode;
    g_webServerInstance = this;
    
    if (apMode) {
        WiFi.softAP(ssid, password);
        LOG(LOG_TAG_WEB, "AP: %s @ %s", ssid, WiFi.softAPIP().toString().c_str());
    } else {
        WiFi.begin(ssid, password);
        LOG(LOG_TAG_WEB, "WiFi: %s", ssid);
        int a = 0;
        while (WiFi.status() != WL_CONNECTED && a < 30) { delay(500); DEBUG_SERIAL.print("."); a++; }
        if (WiFi.status() != WL_CONNECTED) { LOG(LOG_TAG_WEB, "WiFi failed!"); return false; }
        LOG(LOG_TAG_WEB, "IP: %s", WiFi.localIP().toString().c_str());
    }
    
    setupMDNS();
    
    m_httpServer.on("/", HTTP_GET, []() {
        g_webServerInstance->m_httpServer.send(200, "text/html", INDEX_HTML);
    });
    m_httpServer.onNotFound([]() {
        g_webServerInstance->m_httpServer.send(404, "text/plain", "Not Found");
    });
    m_httpServer.begin();
    
    m_wsServer.listen(81);
    
    LOG(LOG_TAG_WEB, "Ready → http://%s.local", DEVICE_MDNS);
    return true;
}

bool WebServerManager::setupMDNS() {
    if (!MDNS.begin(DEVICE_MDNS)) return false;
    MDNS.addService("http", "tcp", 80);
    return true;
}

// ── 接受新 WebSocket 客户端 ──
void WebServerManager::acceptNewClients() {
    m_wsServer.poll();
    while (m_wsServer.available()) {
        auto client = m_wsServer.accept();
        LOG(LOG_TAG_WEB, "WS client connected (#%zu)", m_clients.size() + 1);
        m_clients.push_back(std::move(client));
    }
    
    // 清理断开的客户端
    auto it = m_clients.begin();
    while (it != m_clients.end()) {
        if (!it->available()) {
            LOG(LOG_TAG_WEB, "WS client removed");
            it = m_clients.erase(it);
        } else {
            ++it;
        }
    }
}

// ── 处理客户端消息 ──
void WebServerManager::handleClientMessage(WebsocketsClient& client, WebsocketsMessage msg) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, msg.data());
    if (err) { LOG(LOG_TAG_WEB, "JSON err: %s", err.c_str()); return; }
    
    const char* cmd = doc["cmd"];
    int value = doc["value"] | 0;
    LOG(LOG_TAG_WEB, "cmd: %s (val=%d)", cmd, value);
    
    if      (strcmp(cmd, "hover")  == 0) followCtrl.setMode(FollowMode::HOVER);
    else if (strcmp(cmd, "follow") == 0) followCtrl.setMode(FollowMode::FOLLOW);
    else if (strcmp(cmd, "return") == 0) followCtrl.setMode(FollowMode::RETURN_HOME);
    else if (strcmp(cmd, "stop")   == 0) { motors.emergencyStop(); followCtrl.setMode(FollowMode::IDLE); }
    else if (strcmp(cmd, "arm")    == 0) { motors.arm(); followCtrl.setMode(FollowMode::HOVER); }
    else if (strcmp(cmd, "disarm") == 0) { motors.disarm(); followCtrl.setMode(FollowMode::IDLE); }
    else if (strcmp(cmd, "setDist")   == 0) followCtrl.setTargetDistance(value);
    else if (strcmp(cmd, "setHeight") == 0) followCtrl.setTargetHeight(value);
    
    if (m_cmdCallback) m_cmdCallback(cmd, value);
}

void WebServerManager::loop() {
    m_httpServer.handleClient();
    acceptNewClients();
    
    // 轮询每个客户端
    for (auto& client : m_clients) {
        client.poll();
        if (client.available()) {
            auto msg = client.readBlocking();
            handleClientMessage(client, msg);
        }
    }
}

void WebServerManager::broadcastTelemetry() {
    if (m_clients.empty()) return;
    
    JsonDocument doc;
    const FusionState& s = fusion.getState();
    doc["posX"] = s.posX; doc["posY"] = s.posY; doc["posZ"] = s.posZ;
    doc["unc"] = (s.uncertaintyX + s.uncertaintyY + s.uncertaintyZ) / 3.0f;
    doc["converged"] = fusion.isConverged();
    doc["roll"] = s.roll; doc["pitch"] = s.pitch; doc["yaw"] = s.yaw;
    doc["vz"] = s.vertSpeed;
    doc["mode"] = (int)followCtrl.getMode();
    doc["armed"] = motors.isArmed();
    doc["tgtX"] = s.posX; doc["tgtY"] = s.posY;
    doc["tgtDist"] = sqrtf(s.posX*s.posX + s.posY*s.posY);
    doc["tgtValid"] = (millis() - s.timestamp) < SIGNAL_LOST_TIMEOUT;
    doc["imuOk"] = imu.isCalibrated(); doc["baroOk"] = true; doc["uwbOk"] = uwb.isHealthy();
    doc["temp"] = 25.0f; doc["press"] = 1013.25f; doc["ts"] = s.timestamp;
    
    String out; serializeJson(doc, out);
    for (auto& client : m_clients) {
        if (client.available()) client.send(out);
    }
}
