/**
 * @file    web_main.cpp
 * @brief   ESP32-S3 Web MVP entry with ToF diagnostics.
 */

#include <Arduino.h>
#include "config.h"
#include "web/server.h"
#include "sensors/tof.h"
#include "comm/fc_bridge.h"
#include "esp_task_wdt.h"

#define WIFI_AP_SSID        "NMC-SmartUmbrella"
#define WIFI_AP_PASSWORD    "12345678"

static uint32_t g_startTime = 0;
static bool g_tofInit = false;

static float simTofDistance(float t) {
    return 1500.0f + 700.0f * sinf(t * 0.15f);
}

static void fillTelemetry(TelemetryData& data)
{
    uint32_t now = millis();
    float t = now * 0.001f;

    data.roll  = 6.0f * sinf(t * 0.25f);
    data.pitch = 4.0f * sinf(t * 0.20f + 1.2f);
    data.yaw   = 25.0f * sinf(t * 0.10f + 2.5f);
    data.posX = 200.0f * sinf(t * 0.05f);
    data.posY = 150.0f * sinf(t * 0.04f + 0.5f);
    data.posZ = 100.0f + 30.0f * sinf(t * 0.08f);
    data.altitude = 120.0f + 40.0f * sinf(t * 0.10f);
    data.batteryVoltage = 11.4f - 0.02f * (t / 60.0f);
    data.batteryCells = 3;
    data.gpsValid = true;
    data.gpsSats  = 8 + (int)(sinf(t * 0.1f) * 2);
    data.gpsLat   = 39.9042;
    data.gpsLng   = 116.4074;
    data.gpsAlt   = 50.0f;
    data.gpsSpeed = 0.5f + 0.3f * sinf(t * 0.1f);
    data.gpsOnline = true;
    // 飞控 — 尝试读真实数据
    fcBridge.update();
    if (fcBridge.isOnline()) {
        const auto& fc = fcBridge.getState();
        data.roll     = fc.roll;
        data.pitch    = fc.pitch;
        data.yaw      = fc.yaw;
        data.altitude = fc.altitude;
        data.batteryVoltage = fc.batteryVoltage;
        data.batteryCells   = fc.batteryCells;
        data.armed    = fc.armed;
        data.fcOnline = true;
        data.dataSource = 3;
    }
    data.gpsOnline = true;
    data.fcOnline = false;
    data.armed = false;
    data.flightMode = 0;
    data.dataSource = 0;
    data.errorFlags = 0;

    if (g_tofInit) {
        tof.update();
        const auto& td = tof.getData();
        data.tofStatus = td.rangeStatus;
        data.tofErrors = td.errorCount;
        data.tofAgeMs = td.lastUpdate ? (now - td.lastUpdate) : 0;

        if (td.valid) {
            data.tofDistance = td.distance;
            data.tofOnline = true;
            data.dataSource = 1;
        } else {
            data.tofDistance = simTofDistance(t);
            data.tofOnline = false;
            data.errorFlags |= 1;
        }
    } else {
        data.tofDistance = simTofDistance(t);
        data.tofOnline = false;
        data.tofStatus = 255;
        data.errorFlags |= 1;
    }

    data.uptime = now;
    data.freeHeap = ESP.getFreeHeap();
    data.chipTemp = temperatureRead();
}

static bool scanForTof()
{
    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(80);
    TOF_I2C_PORT.setClock(100000);

    Serial.printf("  Scanning I2C on SDA=%d SCL=%d\n", TOF_SDA, TOF_SCL);
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        TOF_I2C_PORT.beginTransmission(addr);
        uint8_t err = TOF_I2C_PORT.endTransmission();
        if (err == 0) {
            Serial.printf("  I2C device found: 0x%02X%s\n",
                addr, addr == 0x29 ? " (VL53L1X default)" : "");
            if (addr == 0x29) return true;
        }
        if ((addr & 0x0F) == 0) esp_task_wdt_reset();
    }
    return false;
}

static void initTof()
{
    Serial.println("--- ToF Sensor ---");
    bool found = scanForTof();
    if (!found) {
        Serial.println("  ToF: 0x29 not found, using simulated distance");
        g_tofInit = false;
        return;
    }

    tof.begin();
    for (int retry = 0; retry < 8; retry++) {
        delay(60);
        tof.update();
        if (tof.isHealthy()) break;
        esp_task_wdt_reset();
    }

    g_tofInit = tof.isInitialized();
    const auto& td = tof.getData();
    Serial.printf("  ToF: %s, valid=%d status=%u dist=%u errors=%lu\n",
        g_tofInit ? "initialized" : "init failed",
        td.valid,
        td.rangeStatus,
        td.distance,
        static_cast<unsigned long>(td.errorCount));
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  NMC Smart Umbrella - Web MVP ToF Diagnostic");
    Serial.println("  ESP32-S3, HTTP polling, ToF optional");
    Serial.println("==============================================");
    Serial.println();

    esp_task_wdt_reset();
    initTof();
    esp_task_wdt_reset();

    // 飞控 MSP 通信
    Serial.println("--- Flight Controller ---");
    fcBridge.begin();
    Serial.printf("  FC: %s\n", fcBridge.isOnline() ? "ONLINE" : "OFFLINE (check UART wiring)");
    esp_task_wdt_reset();
    Serial.println();

    webServer.setTelemetrySource(fillTelemetry);

    if (!webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true)) {
        Serial.println("[FATAL] Web server init failed!");
        while (1) { delay(1000); }
    }

    webServer.onCommand([](const char* cmd, int value) {
        Serial.printf("[CMD] %s = %d\n", cmd, value);
    });

    g_startTime = millis();

    Serial.println("==============================================");
    Serial.println("  SYSTEM READY");
    Serial.printf("  AP: %s @ 192.168.4.1\n", WIFI_AP_SSID);
    Serial.println("  Visit http://192.168.4.1");
    Serial.println("  JSON:  http://192.168.4.1/api/telemetry");
    Serial.println("==============================================");
}

void loop()
{
    webServer.loop();

    static uint32_t lastPrint = 0;
    uint32_t now = millis();
    if (now - lastPrint > 5000) {
        lastPrint = now;
        const auto& td = tof.getData();
        Serial.printf("[SYS] %lus heap=%u stations=%u tofInit=%d valid=%d status=%u dist=%u errors=%lu age=%lu\n",
            (now - g_startTime) / 1000,
            ESP.getFreeHeap(),
            WiFi.softAPgetStationNum(),
            g_tofInit,
            td.valid,
            td.rangeStatus,
            td.distance,
            static_cast<unsigned long>(td.errorCount),
            static_cast<unsigned long>(td.lastUpdate ? now - td.lastUpdate : 0));
    }

    delay(10);
}
