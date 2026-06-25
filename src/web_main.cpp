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
#include "sensors/tof.h"
#include "esp_task_wdt.h"

// WiFi 配置
#define WIFI_AP_SSID        "NMC-SmartUmbrella"
#define WIFI_AP_PASSWORD    "12345678"

static uint32_t g_startTime = 0;
static bool     g_tofInit   = false;

/* ── 遥测数据填充：尝试 ToF，失败回退模拟 ── */
static void fillTelemetry(TelemetryData& data)
{
    uint32_t now = millis();
    float t = now * 0.001f;

    // 基础模拟数据（始终有）
    data.roll  = 6.0f * sinf(t * 0.25f);
    data.pitch = 4.0f * sinf(t * 0.20f + 1.2f);
    data.yaw   = 25.0f * sinf(t * 0.10f + 2.5f);
    data.posX = 200.0f * sinf(t * 0.05f);
    data.posY = 150.0f * sinf(t * 0.04f + 0.5f);
    data.posZ = 100.0f + 30.0f * sinf(t * 0.08f);
    data.altitude    = 120.0f + 40.0f * sinf(t * 0.10f);
    data.batteryVoltage = 11.4f - 0.02f * (t / 60.0f);
    data.batteryCells   = 3;
    data.gpsValid = true;
    data.gpsSats  = 8 + (int)(sinf(t * 0.1f) * 2);
    data.gpsLat   = 39.9042;
    data.gpsLng   = 116.4074;
    data.gpsAlt   = 50.0f;
    data.gpsSpeed = 0.5f + 0.3f * sinf(t * 0.1f);
    data.fcOnline    = false;
    data.armed       = false;
    data.flightMode  = 0;
    data.gpsOnline   = true;
    data.dataSource  = 0;
    data.errorFlags  = 0;
    data.uptime      = now;
    data.freeHeap    = ESP.getFreeHeap();
    data.chipTemp    = temperatureRead();

    // ToF：初始化成功后持续尝试读真实值；当前帧异常则回退模拟
    if (g_tofInit) {
        tof.update();
        const auto& td = tof.getData();
        data.tofDistance = td.distance;
        data.tofOnline   = td.valid;
        if (!td.valid) {
            data.tofOnline = false;
            data.errorFlags |= 1;  // bit0 = tof_err
            data.tofDistance = 1500.0f + 700.0f * sinf(t * 0.15f);  // 模拟回退
        } else {
            data.tofOnline = true;
            data.dataSource = 1;  // tof
        }
    } else {
        data.tofDistance = 1500.0f + 700.0f * sinf(t * 0.15f);
        data.tofOnline   = false;
    }
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

    // ToF 传感器 (VL53L1X, I2C SDA=4 SCL=5)
    Serial.println("--- ToF Sensor ---");
    esp_task_wdt_reset();
    {
        // 先探测常见 I2C 地址
        Wire.begin(TOF_SDA, TOF_SCL);
        Wire.setClock(100000);
        pinMode(TOF_SDA, INPUT_PULLUP);
        pinMode(TOF_SCL, INPUT_PULLUP);
        delay(10);
        int foundAddr = -1;
        uint8_t addrs[] = {0x29, 0x30, 0x52, 0x28, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x31, 0x32, 0x50, 0x51, 0x53};
        for (int i = 0; i < (int)(sizeof(addrs)/sizeof(addrs[0])); i++) {
            Wire.beginTransmission(addrs[i]);
            if (Wire.endTransmission() == 0) {
                foundAddr = addrs[i];
                Serial.printf("  I2C found at 0x%02X\n", foundAddr);
                break;
            }
            if (i % 5 == 0) esp_task_wdt_reset();
        }
        if (foundAddr == -1) {
            Serial.println("  No I2C device found on SDA=41 SCL=42");
            g_tofInit = false;
        } else {
            // 用 VL53L1X 库初始化
            tof.begin();
            esp_task_wdt_reset();
            // 等第一帧数据就绪（最多试 5 次）
            for (int retry = 0; retry < 5; retry++) {
                delay(70);
                tof.update();
                if (tof.isHealthy()) break;
                esp_task_wdt_reset();
            }
            g_tofInit = tof.isInitialized();
            Serial.println(g_tofInit ? "  ToF: initialized" : "  ToF: init failed");
        }
    }
    esp_task_wdt_reset();
    Serial.println();

    // 注册数据源（ToF + 模拟混合）
    webServer.setTelemetrySource(fillTelemetry);

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
