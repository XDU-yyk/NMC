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
    bool   fcSimulated = false;
    bool   armed = false;
    int    flightMode = 0;       // 0=IDLE,1=HOVER,2=FOLLOW,3=RTH,4=LOST
    int    fcScenario = 0;
    const char* fcScenarioName = "offline";
    bool   fcFailsafe = false;
    uint16_t fcCycleTimeUs = 0;
    uint8_t  fcCpuLoad = 0;
    uint8_t  fcLinkQuality = 0;
    float    fcVario = 0;
    uint16_t rcRoll = 1500;
    uint16_t rcPitch = 1500;
    uint16_t rcThrottle = 1000;
    uint16_t rcYaw = 1500;
    uint16_t rcAux1 = 1500;
    uint16_t rcAux2 = 1000;
    uint8_t  rcChannelCount = 0;
    bool     fcAssistSwitch = false;
    bool     fcAssistGateOpen = false;
    bool     fcRealOutputCompiled = false;
    bool     fcRealOnline = false;
    bool     rxHasSixChannels = false;
    bool     rxCenterOk = false;
    bool     rxThrottleLow = false;
    bool     rxAux1Valid = false;
    bool     rxAux2Valid = false;
    bool     rxAux1Low = false;
    bool     rxAux2Low = false;
    bool     rxBenchReady = false;
    uint32_t fcOutSetCalls = 0;
    uint32_t fcOutOverrideRequests = 0;
    uint32_t fcOutClearRequests = 0;
    uint32_t fcOutRawAttempts = 0;
    uint32_t fcOutRawOk = 0;
    uint32_t fcOutRawFail = 0;
    uint32_t fcOutGateBlocks = 0;
    uint32_t fcOutStaleBlocks = 0;
    uint32_t fcOutLastSetAgeMs = 0;
    uint32_t fcOutLastSendAgeMs = 0;
    uint16_t fcOutRoll = 1500;
    uint16_t fcOutPitch = 1500;
    uint16_t fcOutYaw = 1500;
    uint16_t fcOutThrottle = 1000;
    uint16_t fcOutAux1 = 1500;
    uint16_t fcOutAux2 = 1000;
    const char* fcOutReason = "n/a";
    uint32_t fcMspTxFrames = 0;
    uint32_t fcMspTxBytes = 0;
    uint32_t fcMspRxBytes = 0;
    uint32_t fcMspTimeouts = 0;
    const char* fcMspLastError = "n/a";
    // 各数据源在线状态
    bool   tofOnline = false;
    bool   gpsOnline = false;
    bool   camOnline = false;     // 摄像头在线
    bool   camValid = false;
    uint32_t camFrames = 0;
    uint32_t camErrors = 0;
    uint32_t camAgeMs = 0;
    uint32_t camBytes = 0;
    uint32_t camRecoveries = 0;
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

/* 方向意图回调: 各轴归一化 -1.0~+1.0; pilotOverride=true 表示网页暂停/交还遥控。
   由前端虚拟摇杆/方向按钮通过 WS 消息 {"cmd":"dir",...} 触发。 */
typedef void (*DirectionCallback)(float forward, float right, float yaw,
                                  float throttle, bool pilotOverride);

class WebServerManager
{
public:
    bool begin(const char* ssid, const char* password, bool apMode = false);
    void loop();
    void broadcastTelemetry();
    void onCommand(CommandCallback callback) { m_cmdCallback = callback; }
    void onDirection(DirectionCallback callback) { m_dirCallback = callback; }
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
    DirectionCallback m_dirCallback = nullptr;
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
