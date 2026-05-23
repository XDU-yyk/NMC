/**
 * @file    main.cpp
 * @brief   NMC 智能无人机伞 — 主入口
 * 
 * 系统启动流程:
 * 1. 硬件初始化 (GPIO, SPI, I2C, PWM)
 * 2. 传感器驱动初始化 (IMU, 气压计, UWB)
 * 3. 传感器融合引擎启动
 * 4. 电机控制器初始化
 * 5. 跟随控制器初始化
 * 6. WiFi + Web 服务器启动
 * 7. FreeRTOS 任务调度器启动
 * 
 * 作者: yyk
 * 日期: 2025
 */

#include <Arduino.h>
#include "config.h"
#include "sensors/imu.h"
#include "sensors/barometer.h"
#include "sensors/uwb.h"
#include "fusion/sensor_fusion.h"
#include "control/motor.h"
#include "follow/follow.h"
#include "web/server.h"
#include "system/task_manager.h"

// ═══════════════════════════════════════════════════════════
//  WiFi 配置 (根据实际环境修改)
// ═══════════════════════════════════════════════════════════

// AP 模式 (无人机自建热点)
#define WIFI_AP_MODE        true
#define WIFI_AP_SSID        "NMC-SmartUmbrella"
#define WIFI_AP_PASSWORD    "12345678"

// STA 模式 (连接路由器)
#define WIFI_STA_SSID       "YourWiFiSSID"
#define WIFI_STA_PASSWORD   "YourWiFiPassword"

// ═══════════════════════════════════════════════════════════
//  全局变量 (单例已在各模块定义)
// ═══════════════════════════════════════════════════════════

// UWBLocalizer     uwb;       // uwb.cpp
// IMUDriver         imu;       // imu.cpp
// Barometer         baro;      // barometer.cpp
// SensorFusion      fusion;    // sensor_fusion.cpp
// MotorController   motors;    // motor.cpp
// FollowController  followCtrl;// follow.cpp
// WebServerManager  webServer; // server.cpp
// TaskManager       taskMgr;   // task_manager.cpp

// ═══════════════════════════════════════════════════════════
//  setup() — Arduino 标准入口
// ═══════════════════════════════════════════════════════════

void setup() {
    // ── 1. 串口初始化 ──
    DEBUG_SERIAL.begin(DEBUG_BAUD);
    delay(500);  // 等待串口稳定
    
    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("╔══════════════════════════════════════════╗");
    DEBUG_SERIAL.println("║   NMC 智能无人机伞 v1.0.0               ║");
    DEBUG_SERIAL.println("║   ESP32-S3 AI 向量指令集优化            ║");
    DEBUG_SERIAL.println("╚══════════════════════════════════════════╝");
    DEBUG_SERIAL.println();
    
    // ── 2. 硬件引脚初始化 ──
    pinMode(SAFETY_SWITCH_PIN, INPUT_PULLUP);
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    
    LOG(LOG_TAG_FUSION, "Chip: ESP32-S3 @ %d MHz", getCpuFrequencyMhz());
    LOG(LOG_TAG_FUSION, "Free heap: %d bytes", ESP.getFreeHeap());
    LOG(LOG_TAG_FUSION, "PSRAM size: %d bytes", ESP.getPsramSize());
    
    // ── 3. 传感器初始化 ──
    LOG(LOG_TAG_FUSION, "--- Sensor Init ---");
    
    if (!imu.begin(4, 500)) {
        LOG(LOG_TAG_FUSION, "FATAL: IMU initialization failed!");
        while (1) { digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); delay(200); }
    }
    LOG(LOG_TAG_FUSION, "  IMU: OK");
    
    if (!baro.begin()) {
        LOG(LOG_TAG_FUSION, "WARNING: Barometer init failed — altitude hold degraded");
    } else {
        LOG(LOG_TAG_FUSION, "  Barometer: OK");
    }
    
    if (!uwb.begin()) {
        LOG(LOG_TAG_FUSION, "WARNING: UWB init failed — positioning unavailable");
    } else {
        LOG(LOG_TAG_FUSION, "  UWB: OK");
    }
    
    // ── 4. 融合引擎 ──
    LOG(LOG_TAG_FUSION, "--- Fusion Engine ---");
    if (!fusion.begin()) {
        LOG(LOG_TAG_FUSION, "FATAL: Fusion engine init failed!");
        while (1) { delay(1000); }
    }
    
    // ── 5. 电机初始化 ──
    LOG(LOG_TAG_CTRL, "--- Motor Init ---");
    motors.begin();
    
    // ── 6. 跟随控制器 ──
    LOG(LOG_TAG_FOLLOW, "--- Follow Controller ---");
    followCtrl.begin();
    followCtrl.setMode(FollowMode::IDLE);
    
    // ── 7. 校准 ──
    LOG(LOG_TAG_FUSION, "--- Calibration ---");
    fusion.calibrate();
    
    // 设置返航点为当前位置
    const FusionState& s = fusion.getState();
    followCtrl.setHomePosition(s.posX, s.posY, s.posZ);
    
    // ── 8. WiFi + Web 服务 ──
    LOG(LOG_TAG_WEB, "--- Web Server ---");
    
#if WIFI_AP_MODE
    if (!webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true)) {
        LOG(LOG_TAG_WEB, "FATAL: Web server init failed!");
    }
#else
    if (!webServer.begin(WIFI_STA_SSID, WIFI_STA_PASSWORD, false)) {
        LOG(LOG_TAG_WEB, "WiFi STA failed, falling back to AP mode...");
        webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true);
    }
#endif
    
    // ── 9. 注册 WebSocket 命令回调 ──
    webServer.onCommand([](const char* cmd, int value) {
        // 自定义命令处理 (扩展)
        LOG(LOG_TAG_WEB, "Custom cmd: %s = %d", cmd, value);
    });
    
    // ── 10. 启动 FreeRTOS 任务调度 ──
    LOG(LOG_TAG_FUSION, "--- Starting Tasks ---");
    taskMgr.begin();
    
    // ── 11. 启动完成 ──
    LOG(LOG_TAG_FUSION, "=======================================");
    LOG(LOG_TAG_FUSION, "  SYSTEM READY");
    LOG(LOG_TAG_FUSION, "  UWB: %s", uwb.isHealthy() ? "ONLINE" : "OFFLINE");
    LOG(LOG_TAG_FUSION, "  IMU: %s", imu.isCalibrated() ? "CALIBRATED" : "UNCALIBRATED");
    LOG(LOG_TAG_FUSION, "  Mode: IDLE (waiting for command)");
    LOG(LOG_TAG_FUSION, "=======================================");
    
    digitalWrite(LED_BUILTIN, HIGH);  // 系统就绪指示灯
    
    // setup() 返回后, Arduino 框架自动调用 loop()
}

// ═══════════════════════════════════════════════════════════
//  loop() — Arduino 空闲循环
// ═══════════════════════════════════════════════════════════
//
// FreeRTOS 任务接管了所有实时工作，loop() 仅处理:
// - WebSocket 事件泵 (已在独立任务中)
// - 非实时维护任务
// - 统计信息输出

void loop() {
    static uint32_t lastReport = 0;
    uint32_t now = millis();
    
    // 每 5 秒输出系统统计
    if (now - lastReport > 5000) {
        lastReport = now;
        
        SystemStatus status;
        taskMgr.getStatus(status);
        
        DEBUG_SERIAL.printf("\n[SYS] Uptime: %lu s | Heap: %lu | PSRAM: %lu\n",
                            status.uptime / 1000,
                            status.freeHeap,
                            status.freePSRAM);
        DEBUG_SERIAL.printf("[SYS] Cycles: %lu | Errors: %lu | Battery: %.1fV\n",
                            status.controlCycles,
                            status.sensorErrors,
                            status.batteryVoltage);
    }
    
    // 心跳
    digitalWrite(LED_BUILTIN, (now / 500) % 2);
    
    delay(100);
}
