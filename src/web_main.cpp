/**
 * @file    web_main.cpp
 * @brief   ESP32-S3 Web MVP — GPS 只读诊断 + ToF
 * 
 * 当前阶段：GPS NEO-M8N 只读诊断，不接飞控，不做控制，不做融合。
 * GPS 默认 offline/no fix，收到 NMEA 后 online=true，定位有效后 valid=true。
 */

#include <Arduino.h>
#include "config.h"
#include "web/server.h"
#include "sensors/tof.h"
#include "sensors/gps.h"
#include "esp_task_wdt.h"

#define WIFI_AP_SSID        "NMC-SmartUmbrella"
#define WIFI_AP_PASSWORD    "12345678"

static uint32_t g_startTime = 0;
static bool g_tofInit = false;
static uint32_t g_lastTofRetry = 0;

static float simTofDistance(float t) {
    return 1500.0f + 700.0f * sinf(t * 0.15f);
}

static void fillTelemetry(TelemetryData& data)
{
    uint32_t now = millis();
    float t = now * 0.001f;

    // 模拟姿态/电池（始终有）
    data.roll  = 6.0f * sinf(t * 0.25f);
    data.pitch = 4.0f * sinf(t * 0.20f + 1.2f);
    data.yaw   = 25.0f * sinf(t * 0.10f + 2.5f);
    data.posX = 200.0f * sinf(t * 0.05f);
    data.posY = 150.0f * sinf(t * 0.04f + 0.5f);
    data.posZ = 100.0f + 30.0f * sinf(t * 0.08f);
    data.altitude = 120.0f + 40.0f * sinf(t * 0.10f);
    data.batteryVoltage = 11.4f - 0.02f * (t / 60.0f);
    data.batteryCells = 3;

    // GPS — 默认 offline/no fix
    data.gpsOnline = false;
    data.gpsValid = false;
    data.gpsSats = 0;
    data.gpsLat = 0; data.gpsLng = 0;
    data.gpsAlt = 0; data.gpsSpeed = 0;
    data.gpsAgeMs = 0;
    data.gpsChars = 0;
    data.gpsSentences = 0;
    data.gpsFailedChecksum = 0;

    // 尝试读真实 GPS
    const auto& gd = gps.getData();
    data.gpsOnline = gd.online;
    data.gpsValid  = gd.valid;
    data.gpsSats   = gd.satellites;
    data.gpsLat    = gd.lat;
    data.gpsLng    = gd.lng;
    data.gpsAlt    = gd.alt;
    data.gpsSpeed  = gd.speed;
    data.gpsAgeMs  = gd.age;
    data.gpsChars  = gd.charsProcessed;
    data.gpsSentences = gd.sentencesWithFix;
    data.gpsFailedChecksum = gd.failedChecksum;

    // ToF
    data.fcOnline = false;
    data.armed = false;
    data.flightMode = 0;
    data.dataSource = 0;
    data.errorFlags = 0;

    if (g_tofInit) {
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
    TOF_I2C_PORT.setTimeOut(150);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);

    Serial.printf("  Scanning I2C on SDA=%d SCL=%d\n", TOF_SDA, TOF_SCL);
    uint8_t foundCount = 0;
    bool foundTof = false;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        TOF_I2C_PORT.beginTransmission(addr);
        uint8_t err = TOF_I2C_PORT.endTransmission();
        if (err == 0) {
            foundCount++;
            Serial.printf("  I2C device found: 0x%02X%s\n",
                addr, addr == 0x29 ? " (VL53L1X default)" : "");
            foundTof = foundTof || (addr == 0x29);
        }
        if ((addr & 0x0F) == 0) esp_task_wdt_reset();
    }
    if (foundTof && foundCount == 1) {
        return true;
    }
    if (foundTof) {
        Serial.printf("  ToF: unstable I2C scan (%u devices), retry later\n", foundCount);
    }
    return false;
}

static void initTof()
{
    Serial.println("--- ToF Sensor ---");
    bool found = false;
    for (uint8_t attempt = 0; attempt < 5 && !found; attempt++) {
        found = scanForTof();
        if (!found) {
            delay(250);
            esp_task_wdt_reset();
        }
    }
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
    Serial.println("  NMC Smart Umbrella - Web MVP");
    Serial.println("  GPS diagnostic + ToF, no FC");
    Serial.println("==============================================");
    Serial.println();

    esp_task_wdt_reset();
    initTof();
    esp_task_wdt_reset();

    // GPS (UART1, RX=18 TX=15)
    Serial.println("--- GPS ---");
    gps.begin();
    Serial.printf("  GPS: UART1 @ %d, RX=%d TX=%d\n", GPS_BAUD, GPS_RX_PIN, GPS_TX_PIN);
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
    gps.update();
    g_tofInit = tof.isInitialized();
    if (g_tofInit) {
        tof.update();
    } else if (millis() - g_lastTofRetry >= 5000) {
        g_lastTofRetry = millis();
        Serial.println("[TOF] Retry init");
        tof.begin();
        g_tofInit = tof.isInitialized();
    }

    webServer.loop();

    static uint32_t lastPrint = 0;
    uint32_t now = millis();
    if (now - lastPrint > 5000) {
        lastPrint = now;
        const auto& td = tof.getData();
        const auto& gd = gps.getData();
        Serial.printf("[SYS] %lus heap=%u stations=%u "
            "tof:init=%d valid=%d status=%u dist=%u err=%lu age=%lu ok=%lu fail=%lu miss=%lu rec=%lu fault=%s "
            "gps:online=%d valid=%d sats=%u chars=%u\n",
            (now - g_startTime) / 1000,
            ESP.getFreeHeap(),
            WiFi.softAPgetStationNum(),
            g_tofInit, td.valid, td.rangeStatus, td.distance,
            static_cast<unsigned long>(td.errorCount),
            static_cast<unsigned long>(td.lastUpdate ? now - td.lastUpdate : 0),
            static_cast<unsigned long>(td.validReads),
            static_cast<unsigned long>(td.failedReads),
            static_cast<unsigned long>(td.readyMisses),
            static_cast<unsigned long>(td.recoveries),
            td.lastFault,
            gd.online, gd.valid, gd.satellites, gd.charsProcessed);
    }

    delay(10);
}
