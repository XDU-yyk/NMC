/**
 * @file    web_main.cpp
 * @brief   Web 面板 MVP 独立入口 — 仅 ESP32-S3，零硬件依赖
 * 
 * 用途: 在无飞控/传感器的条件下验证 Web 面板功能。
 * 编译: pio run -e esp32-s3-web-mvp
 */

#include <Arduino.h>
#include "config.h"
#include "web/server.h"

// WiFi 配置
#define WIFI_AP_SSID        "NMC-SmartUmbrella"
#define WIFI_AP_PASSWORD    "12345678"

static uint32_t g_startTime = 0;

/* ── 模拟数据填充 ── */
static void fillSimTelemetry(TelemetryData& data)
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

    // GPS — 固定坐标（模拟已定位）
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
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  NMC 智能无人机伞 — Web MVP");
    Serial.println("  仅 ESP32-S3 · 零硬件依赖 · 模拟数据");
    Serial.println("==============================================");
    Serial.println();

    // 注册模拟数据源
    webServer.setTelemetrySource(fillSimTelemetry);

    // 启动 WiFi AP + Web 服务器
    if (!webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true)) {
        Serial.println("[FATAL] Web server init failed!");
        while (1) { delay(1000); }
    }

    // 注册命令回调（仅日志记录）
    webServer.onCommand([](const char* cmd, int value) {
        Serial.printf("[CMD] %s = %d\n", cmd, value);
    });

    g_startTime = millis();

    Serial.println("==============================================");
    Serial.println("  SYSTEM READY");
    Serial.printf("  AP: %s @ 192.168.4.1\n", WIFI_AP_SSID);
    Serial.println("  Visit http://192.168.4.1 in your browser");
    Serial.println("==============================================");
}

void loop()
{
    webServer.loop();

    // 每 5 秒打印一次系统状态
    static uint32_t lastPrint = 0;
    uint32_t now = millis();
    if (now - lastPrint > 5000) {
        lastPrint = now;
        Serial.printf("[SYS] Uptime: %lus | Heap: %u | WS Clients: %u | AP Stations: %u | AP IP: %s\n",
            (now - g_startTime) / 1000,
            ESP.getFreeHeap(),
            webServer.getClientCount(),
            WiFi.softAPgetStationNum(),
            WiFi.softAPIP().toString().c_str());
    }

    delay(10);
}
