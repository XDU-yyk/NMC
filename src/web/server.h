/**
 * @file    server.h
 * @brief   WebSocket 嵌入式 Web 服务器 (gilmaimon/ArduinoWebsockets)
 */

#ifndef SERVER_H
#define SERVER_H

#include "config.h"
#include "fusion/sensor_fusion.h"
#include "follow/follow.h"
#include "control/motor.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoWebsockets.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <vector>

using namespace websockets;

typedef void (*CommandCallback)(const char* cmd, int value);

class WebServerManager {
public:
    bool begin(const char* ssid, const char* password, bool apMode = false);
    void loop();
    void broadcastTelemetry();
    void onCommand(CommandCallback callback) { m_cmdCallback = callback; }
    bool isConnected() const { return WiFi.isConnected(); }
    uint8_t getClientCount() const { return m_clients.size(); }

private:
    WebServer        m_httpServer{WEB_SERVER_PORT};
    WebsocketsServer m_wsServer;
    std::vector<WebsocketsClient> m_clients;
    
    CommandCallback  m_cmdCallback = nullptr;
    bool m_apMode = false;
    
    void acceptNewClients();
    void handleClientMessage(WebsocketsClient& client, WebsocketsMessage msg);
    void buildTelemetryJson(JsonDocument& doc);
    bool setupMDNS();
};

extern WebServerManager webServer;
extern WebServerManager* g_webServerInstance;

#endif // SERVER_H
