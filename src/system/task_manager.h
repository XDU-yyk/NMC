/**
 * @file    task_manager.h
 * @brief   FreeRTOS 任务调度器 v2.0
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "config.h"
#include <Arduino.h>

struct SystemStatus {
    uint32_t uptime;
    size_t   freeHeap;
    size_t   freePSRAM;
    uint32_t controlCycles;
    uint32_t sensorErrors;
    float    batteryVoltage;
};

class TaskManager {
public:
    void begin();
    void getStatus(SystemStatus& status);

    /* 任务句柄 (供外部暂停/恢复) */
    void* getTaskFusion() const { return m_taskFusion; }
    void* getTaskFollow() const { return m_taskFollow; }

private:
    void* m_taskFusion   = nullptr;
    void* m_taskFollow   = nullptr;
    void* m_taskSafety   = nullptr;
    void* m_taskTelemetry = nullptr;
    void* m_taskWeb      = nullptr;
    void* m_taskFC       = nullptr;
    void* m_taskGPS      = nullptr;
    void* m_taskToF      = nullptr;
    void* m_taskCamera   = nullptr;

    static void taskFusion(void* pv);
    static void taskFollow(void* pv);
    static void taskSafety(void* pv);
    static void taskTelemetry(void* pv);
    static void taskWeb(void* pv);
    static void taskFCComm(void* pv);
    static void taskGPS(void* pv);
    static void taskToF(void* pv);
    static void taskCamera(void* pv);
};

extern TaskManager taskMgr;

#endif // TASK_MANAGER_H
