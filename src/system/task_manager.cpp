/**
 * @file    task_manager.cpp
 * @brief   FreeRTOS 双核任务调度实现 v2.0
 * 
 * Core 0 (I/O 密集型): 传感器采集 (GPS/ToF/UWB/FC) + 遥测
 * Core 1 (计算密集型): 融合 + 跟随 + 任务规划 + 安全 + Web
 */

#include "system/task_manager.h"
#include "fusion/sensor_fusion.h"
#include "follow/follow.h"
#include "control/mission.h"
#include "comm/fc_bridge.h"
#include "sensors/gps.h"
#include "sensors/tof.h"
#include "sensors/uwb.h"
#include "vision/camera.h"
#include "web/server.h"

TaskManager taskMgr;

void TaskManager::begin() {
    // Core 0 — I/O 密集型
    xTaskCreatePinnedToCore(taskFCComm,    "FC_Comm",    TASK_STACK_MEDIUM,  this, PRIO_FC_COMM,    &m_taskFC,    0);
    xTaskCreatePinnedToCore(taskGPS,       "GPS",        TASK_STACK_SHALLOW, this, PRIO_GPS_READ,   &m_taskGPS,   0);
    xTaskCreatePinnedToCore(taskToF,       "ToF",        TASK_STACK_SHALLOW, this, PRIO_TOF_READ,   &m_taskToF,   0);
    xTaskCreatePinnedToCore(taskTelemetry, "Telemetry",  TASK_STACK_SHALLOW, this, PRIO_TELEMETRY,  &m_taskTelemetry, 0);

    // Core 1 — 计算密集型
    xTaskCreatePinnedToCore(taskFusion,    "Fusion",     TASK_STACK_DEEP,    this, PRIO_FUSION,     &m_taskFusion,  1);
    xTaskCreatePinnedToCore(taskFollow,    "Follow",     TASK_STACK_MEDIUM,  this, PRIO_FOLLOW,     &m_taskFollow,  1);
    xTaskCreatePinnedToCore(taskSafety,    "Safety",     TASK_STACK_MEDIUM,  this, PRIO_SAFETY,     &m_taskSafety,  1);
    xTaskCreatePinnedToCore(taskWeb,       "Web",        TASK_STACK_MEDIUM,  this, PRIO_WEB_SERVER, &m_taskWeb,     1);
    xTaskCreatePinnedToCore(taskCamera,    "Camera",     TASK_STACK_DEEP,    this, PRIO_CAMERA,     &m_taskCamera,  1);

    LOG(LOG_TAG_FUSION, "Tasks: 9 tasks across 2 cores");
}

void TaskManager::getStatus(SystemStatus& st) {
    st.uptime        = millis();
    st.freeHeap      = ESP.getFreeHeap();
    st.freePSRAM     = ESP.getPsramSize();
    st.controlCycles = 0;
    st.sensorErrors  = 0;
    st.batteryVoltage = 0;
}

/* ── Core 0 任务 ── */

void TaskManager::taskFCComm(void* pv) {
    for (;;) {
        fcBridge.update();
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_FC_COMM));
    }
}

void TaskManager::taskGPS(void* pv) {
    for (;;) {
        gps.update();
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_GPS));
    }
}

void TaskManager::taskToF(void* pv) {
    for (;;) {
        tof.update();
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_TOF));
    }
}

void TaskManager::taskTelemetry(void* pv) {
    for (;;) {
        webServer.broadcastTelemetry();
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_TELEMETRY));
    }
}

/* ── Core 1 任务 ── */

void TaskManager::taskFusion(void* pv) {
    for (;;) {
        fusion.update();
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_FUSION));
    }
}

void TaskManager::taskFollow(void* pv) {
    for (;;) {
        followCtrl.update();
        mission.update();
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_FOLLOW));
    }
}

void TaskManager::taskSafety(void* pv) {
    for (;;) {
        mission.update();
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_SAFETY));
    }
}

void TaskManager::taskWeb(void* pv) {
    for (;;) {
        webServer.loop();
        vTaskDelay(pdMS_TO_TICKS(5));  // Web 事件泵 ~200Hz
    }
}

void TaskManager::taskCamera(void* pv) {
    for (;;) {
        camera.update();
        vTaskDelay(pdMS_TO_TICKS(INTERVAL_CAMERA));
    }
}
