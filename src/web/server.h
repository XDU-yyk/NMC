/**
 * @file    server.h
 * @brief   WebSocket 嵌入式 Web 服务器 v2.0
 * 
 * 重构: 移除对传感器/飞控模块的硬依赖，通过回调接口解耦。
 *       MVP 模式下使用内部模拟数据生成器。
 */

#ifndef SERVER_H
#define SERVER_H

#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#ifndef WEB_HTTP_ONLY
#include <ArduinoWebsockets.h>
#include <vector>

using namespace websockets;
#endif

/* ── 可调参数（白名单） ── */
struct TunableParams {
    uint16_t webPollMs = 1000;      // 前端轮询间隔 (ms)
    float followDistanceM = 1.5f;   // 跟随目标距离 (m)
    float maxPitchDeg = 5.0f;       // 最大辅助俯仰角 (°)
    float maxRollDeg = 5.0f;        // 最大辅助横滚角 (°)
};

extern TunableParams g_params;

/* ── 遥测数据结构（无硬件依赖） ── */
struct TelemetryData {
    float pitch = 0, roll = 0, yaw = 0;
    // 位置 (mm)
    float posX = 0, posY = 0, posZ = 0;
    // 传感器
    float tofDistance = 0;       // mm
    float altitude = 0;          // cm
    float batteryVoltage = 0;    // V
    uint8_t batteryCells = 3;
    // GPS
    bool   gpsValid = false;
    int    gpsSats = 0;
    double gpsLat = 0, gpsLng = 0;
    float  gpsAlt = 0, gpsSpeed = 0;
    uint32_t gpsAgeMs = 0;
    uint32_t gpsChars = 0;
    uint32_t gpsSentences = 0;
    uint32_t gpsFailedChecksum = 0;
    // 飞控
    bool   fcOnline = false;
    bool   armed = false;
    int    flightMode = 0;       // 0=IDLE,1=HOVER,2=FOLLOW,3=RTH,4=LOST
    // 各数据源在线状态
    bool   tofOnline = false;
    bool   gpsOnline = false;
    bool   camOnline = false;     // 摄像头在线
    int    dataSource = 0;       // 0=sim, 1=tof, 2=gps, 3=fc, 4=mixed
    uint32_t errorFlags = 0;     // bit0=tof_err, bit1=gps_err, bit2=fc_err
    uint8_t  tofStatus = 255;    // 0=ok, 254=timeout, 255=not initialized
    uint32_t tofErrors = 0;
    uint32_t tofAgeMs = 0;
    // 系统
    uint32_t uptime = 0;
    size_t   freeHeap = 0;
    float    chipTemp = 0;
    uint8_t  clientCount = 0;
};

typedef void (*TelemetryCallback)(TelemetryData& data);
typedef void (*CommandCallback)(const char* cmd, int value);

class WebServerManager
{
public:
    bool begin(const char* ssid, const char* password, bool apMode = false);
    void loop();
    void broadcastTelemetry();
    void onCommand(CommandCallback callback) { m_cmdCallback = callback; }
    bool isConnected() const { return WiFi.isConnected(); }
#ifdef WEB_HTTP_ONLY
    uint8_t getClientCount() const { return 0; }
#else
    uint8_t getClientCount() const { return m_clients.size(); }
#endif

    /* 注册外部遥测数据源；未注册时使用内部模拟数据 */
    void setTelemetrySource(TelemetryCallback cb) { m_telemetryCb = cb; }

private:
    WiFiServer      m_httpServer{ WEB_SERVER_PORT };
#ifndef WEB_HTTP_ONLY
    WebsocketsServer m_wsServer;
    std::vector<WebsocketsClient> m_clients;
#endif

    CommandCallback  m_cmdCallback = nullptr;
    TelemetryCallback m_telemetryCb = nullptr;
    bool m_apMode = false;

    void handleHttpClient();
    void handleHttpRequest(WiFiClient& client);
#ifndef WEB_HTTP_ONLY
    void acceptNewClients();
    void handleClientMessage(WebsocketsClient& client, WebsocketsMessage msg);
#endif
    void buildTelemetryJson(JsonDocument& doc);
    bool setupMDNS();
    void sendText(WiFiClient& client, const char* body);
    void sendJson(WiFiClient& client, const String& body);
    void sendIndex(WiFiClient& client);

    /* 内部模拟数据生成器（回退方案） */
    void fillSimData(TelemetryData& data);
};

extern WebServerManager webServer;
extern WebServerManager* g_webServerInstance;

#endif // SERVER_H
