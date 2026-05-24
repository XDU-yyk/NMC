/**
 * @file    main.cpp
 * @brief   NMC 智能无人机伞 v2.0 — 主入口
 * 
 * 架构: ESP32-S3 (协处理器) + F4V3S PLUS (飞控) 双处理器
 */

#include <Arduino.h>
#include "config.h"
#include "sensors/imu.h"
#include "sensors/barometer.h"
#include "sensors/uwb.h"
#include "sensors/gps.h"
#include "sensors/tof.h"
#include "fusion/sensor_fusion.h"
#include "control/motor.h"
#include "control/mission.h"
#include "follow/follow.h"
#include "comm/fc_bridge.h"
#include "web/server.h"
#include "vision/camera.h"
#include "system/task_manager.h"
#include "esp_task_wdt.h"

// ═══════════════════════════════════════════════════════════
//  WiFi 配置
// ═══════════════════════════════════════════════════════════

#define WIFI_AP_MODE        true
#define WIFI_AP_SSID        "NMC-SmartUmbrella"
#define WIFI_AP_PASSWORD    "12345678"

#define WIFI_STA_SSID       "YourWiFiSSID"
#define WIFI_STA_PASSWORD   "YourWiFiPassword"

// ═══════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════

void setup()
{
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    delay(500);

    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("==============================================");
    DEBUG_SERIAL.println("  NMC 智能无人机伞 v2.0");
    DEBUG_SERIAL.println("  ESP32-S3 + F4V3S PLUS 双处理器架构");
    DEBUG_SERIAL.println("==============================================");
    DEBUG_SERIAL.println();

    pinMode(SAFETY_SWITCH_PIN, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    LOG(LOG_TAG_FUSION, "Chip: ESP32-S3 @ %d MHz", getCpuFrequencyMhz());
    LOG(LOG_TAG_FUSION, "Free heap: %d / PSRAM: %d", ESP.getFreeHeap(), ESP.getPsramSize());

    LOG(LOG_TAG_FUSION, "--- Sensors ---");
    gps.begin();
    LOG(LOG_TAG_GPS, "  GPS (NEO-M8N): init ok");

    // TODO: 连接后取消注释
    // tof.begin();   // I2C SDA=38 SCL=37 — 与 OPI flash 冲突
    // uwb.begin();   // SPI — 需要 DW3000 模块
    // fcBridge.begin();  // UART2 — 需要 F4V3S 飞控
    // camera.begin();    // DVP — 需要 OV2640

    esp_task_wdt_reset();
    LOG(LOG_TAG_FUSION, "--- Fusion Engine ---");
    fusion.begin();

    LOG(LOG_TAG_FOLLOW, "--- Follow Controller ---");
    followCtrl.begin();
    followCtrl.setMode(FollowMode::IDLE);

    LOG(LOG_TAG_MISSION, "--- Mission Planner ---");
    mission.begin();

    LOG(LOG_TAG_FUSION, "--- Calibration ---");
    fusion.calibrate();

    const FusionState& s = fusion.getState();
    followCtrl.setHomePosition(s.posX, s.posY, s.posZ);

    // camera.begin();  // DVP — 需要 OV2640 摄像头

    // Feed WDT before lengthy WiFi/Web init
    esp_task_wdt_reset();

    LOG(LOG_TAG_WEB, "--- Web Server ---");
#if WIFI_AP_MODE
    if (!webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true))
        LOG(LOG_TAG_WEB, "FATAL: Web server init failed!");
#else
    if (!webServer.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD, false)) {
        LOG(LOG_TAG_WEB, "WiFi STA failed, fallback to AP...");
        webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true);
    }
#endif

    webServer.onCommand([](const char* cmd, int value) {
        LOG(LOG_TAG_WEB, "Custom cmd: %s = %d", cmd, value);
    });

    LOG(LOG_TAG_FUSION, "--- Starting Tasks ---");
    taskMgr.begin();

    LOG(LOG_TAG_FUSION, "=======================================");
    LOG(LOG_TAG_FUSION, "  SYSTEM READY");
    LOG(LOG_TAG_FUSION, "  FC:    %s", fcBridge.isOnline() ? "ONLINE" : "OFFLINE");
    LOG(LOG_TAG_FUSION, "  GPS:   %s", gps.isHealthy() ? "LOCK" : "NO FIX");
    LOG(LOG_TAG_FUSION, "  ToF:   %s", tof.isHealthy() ? "OK" : "NO SIGNAL");
    LOG(LOG_TAG_FUSION, "  Cam:   %s", camera.isInitialized() ? "OK" : "OFF");
    LOG(LOG_TAG_FUSION, "  Mode:  IDLE");
    LOG(LOG_TAG_FUSION, "=======================================");

    digitalWrite(LED_BUILTIN, HIGH);
}

// ═══════════════════════════════════════════════════════════
//  loop()
// ═══════════════════════════════════════════════════════════

void loop()
{
    static uint32_t lastReport = 0;
    uint32_t now = millis();

    if (now - lastReport > 5000) {
        lastReport = now;

        MissionPlanner::MissionStatus status;
        mission.getStatus(status);

        DEBUG_SERIAL.printf("\n[SYS] Uptime: %lu s | Cycles: %lu | Errors: %lu\n",
            status.uptime / 1000, status.controlCycles, status.sensorErrors);
        DEBUG_SERIAL.printf("[SYS] Bat: %.1fV (%dS) | FC: %s | GPS: %s | ToF: %s\n",
            status.batteryVoltage, status.batteryCells,
            status.fcOnline ? "ON" : "OFF",
            status.gpsValid ? "LOCK" : "--",
            status.tofValid ? "OK" : "--");
    }

    digitalWrite(LED_BUILTIN, (now / 500) % 2);
    delay(100);
}
