/**
 * @file    main.cpp
 * @brief   NMC 智能无人机伞 v2.0 — 主入口
 * 
 * 架构: ESP32-S3 (协处理器) + F4V3S PLUS (飞控) 双处理器
 * 
 * 启动流程:
 * 1. 硬件初始化 (GPIO, SPI, I2C, UART)
 * 2. 传感器驱动初始化 (GPS, ToF, UWB)
 * 3. 飞控通信初始化 (MSP 协议)
 * 4. 传感器融合引擎启动 (9态 EKF)
 * 5. 任务规划器 + 跟随控制器初始化
 * 6. WiFi + Web 服务器启动
 * 7. FreeRTOS 任务调度器启动
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

// ═══════════════════════════════════════════════════════════
//  WiFi 配置
// ═══════════════════════════════════════════════════════════

// AP 模式 (无人机自建热点)
#define WIFI_AP_MODE        true
#define WIFI_AP_SSID        "NMC-SmartUmbrella"
#define WIFI_AP_PASSWORD    "12345678"

// STA 模式 (连接路由器)
#define WIFI_STA_SSID       "YourWiFiSSID"
#define WIFI_STA_PASSWORD   "YourWiFiPassword"

// ═══════════════════════════════════════════════════════════
//  setup()
// ═══════════════════════════════════════════════════════════

void setup()
{
    // ── 1. 串口初始化 ──
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    delay(500);

    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("==============================================");
    DEBUG_SERIAL.println("  NMC 智能无人机伞 v2.0");
    DEBUG_SERIAL.println("  ESP32-S3 + F4V3S PLUS 双处理器架构");
    DEBUG_SERIAL.println("==============================================");
    DEBUG_SERIAL.println();

    // ── 2. GPIO 初始化 ──
    pinMode(SAFETY_SWITCH_PIN, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    LOG(LOG_TAG_FUSION, "Chip: ESP32-S3 @ %d MHz", getCpuFrequencyMhz());
    LOG(LOG_TAG_FUSION, "Free heap: %d / PSRAM: %d", ESP.getFreeHeap(), ESP.getPsramSize());

    // ── 3. 传感器初始化 ──
    LOG(LOG_TAG_FUSION, "--- Sensors ---");

    gps.begin();
    LOG(LOG_TAG_GPS, "  GPS (NEO-M8N): init ok");

    tof.begin();
    LOG(LOG_TAG_TOF, "  ToF (VL53L1X): init ok");

    uwb.begin();
    LOG(LOG_TAG_UWB, "  UWB (DW3000): stub — install arduino-dw3000 lib");

    // ── 4. 飞控通信初始化 ──
    LOG(LOG_TAG_FC, "--- FC Bridge ---");
    fcBridge.begin();

    // ── 5. 融合引擎 ──
    LOG(LOG_TAG_FUSION, "--- Fusion Engine ---");
    fusion.begin();

    // ── 6. 跟随控制器 + 任务规划 ──
    LOG(LOG_TAG_FOLLOW, "--- Follow Controller ---");
    followCtrl.begin();
    followCtrl.setMode(FollowMode::IDLE);

    LOG(LOG_TAG_MISSION, "--- Mission Planner ---");
    mission.begin();

    // ── 7. 校准 ──
    LOG(LOG_TAG_FUSION, "--- Calibration ---");
    fusion.calibrate();

    const FusionState& s = fusion.getState();
    followCtrl.setHomePosition(s.posX, s.posY, s.posZ);

    // ── 8. 摄像头 ──
    camera.begin();

    // ── 9. WiFi + Web 服务 ──
    LOG(LOG_TAG_WEB, "--- Web Server ---");
#if WIFI_AP_MODE
    if (!webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true))
        LOG(LOG_TAG_WEB, "FATAL: Web server init failed!");
#else
    if (!webServer.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD, false)) {
        LOG(LOG_TAG_WEB, "WiFi STA failed, falling back to AP...");
        webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true);
    }
#endif

    // ── 10. 注册 WebSocket 命令回调 ──
    webServer.onCommand([](const char* cmd, int value) {
        LOG(LOG_TAG_WEB, "Custom cmd: %s = %d", cmd, value);
    });

    // ── 11. 启动 FreeRTOS 任务调度 ──
    LOG(LOG_TAG_FUSION, "--- Starting Tasks ---");
    taskMgr.begin();

    // ── 12. 启动完成 ──
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
//  loop() — FreeRTOS 接管后仅处理统计和心跳
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
