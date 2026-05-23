/**
 * @file    task_manager.h
 * @brief   FreeRTOS 任务管理器
 * 
 * 将系统功能划分为独立任务，利用双核架构实现实时调度:
 * 
 * Core 0 (PRO CPU):
 *   - 传感器采集任务 (200Hz)
 *   - UWB 测距任务 (20Hz)
 *   - 串口遥测输出 (10Hz)
 * 
 * Core 1 (APP CPU):
 *   - 传感器融合更新 (100Hz)
 *   - 姿态控制任务 (200Hz) — 最高优先级
 *   - 跟随决策任务 (20Hz)
 *   - Web + WebSocket 任务
 *   - 安全监控任务 (50Hz)
 */

#ifndef TASK_MANAGER_H
#define TASK_MANAGER_H

#include "config.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

/**
 * @brief 系统运行状态
 */
struct SystemStatus {
    uint32_t uptime;           // 运行时间 (ms)
    float    cpuUsageCore0;    // CPU 使用率 (0–100%)
    float    cpuUsageCore1;
    uint32_t freeHeap;         // 可用堆内存 (bytes)
    uint32_t freePSRAM;        // 可用 PSRAM (bytes)
    float    batteryVoltage;   // 电池电压 (V)
    bool     safetySwitchOk;   // 安全开关状态
    uint32_t sensorErrors;     // 传感器错误计数
    uint32_t controlCycles;    // 控制循环计数
};

/**
 * @brief 任务管理器
 * 
 * 初始化所有 FreeRTOS 任务并管理信号量同步。
 */
class TaskManager {
public:
    /**
     * @brief 创建所有任务并启动调度器
     */
    void begin();
    
    /**
     * @brief 获取系统状态快照
     */
    void getStatus(SystemStatus& status);
    
    /**
     * @brief 获取传感器数据就绪信号量
     */
    SemaphoreHandle_t getSensorReadySem() { return m_sensorReady; }
    
    /**
     * @brief 获取融合数据就绪信号量
     */
    SemaphoreHandle_t getFusionReadySem() { return m_fusionReady; }
    
private:
    // FreeRTOS 任务句柄
    TaskHandle_t m_taskSensorRead  = nullptr;
    TaskHandle_t m_taskFusion      = nullptr;
    TaskHandle_t m_taskControl     = nullptr;
    TaskHandle_t m_taskFollow      = nullptr;
    TaskHandle_t m_taskWebServer   = nullptr;
    TaskHandle_t m_taskSafety      = nullptr;
    TaskHandle_t m_taskTelemetry   = nullptr;
    
    // 信号量 (二值)
    SemaphoreHandle_t m_sensorReady = nullptr;
    SemaphoreHandle_t m_fusionReady = nullptr;
    
    // 互斥锁 (保护共享数据)
    SemaphoreHandle_t m_stateMutex = nullptr;
    
    // 运行标志
    volatile bool m_running = false;
    
    // 统计
    volatile uint32_t m_sensorErrors = 0;
    volatile uint32_t m_controlCycles = 0;
    volatile float m_batteryVoltage = 12.6f;
    
    // ───── 静态任务函数 (FreeRTOS 要求) ─────
    
    static void taskSensorRead(void* param);
    static void taskFusion(void* param);
    static void taskControl(void* param);
    static void taskFollow(void* param);
    static void taskWebServer(void* param);
    static void taskSafety(void* param);
    static void taskTelemetry(void* param);
};

// 全局单例
extern TaskManager taskMgr;

#endif // TASK_MANAGER_H
