/**
 * @file    task_manager.cpp
 * @brief   FreeRTOS 任务管理器实现
 */

#include "system/task_manager.h"
#include "sensors/imu.h"
#include "sensors/barometer.h"
#include "sensors/uwb.h"
#include "fusion/sensor_fusion.h"
#include "control/pid.h"
#include "control/motor.h"
#include "follow/follow.h"
#include "web/server.h"

TaskManager taskMgr;

// ═══════════════════════════════════════════════════════════
//  初始化 — 创建所有任务
// ═══════════════════════════════════════════════════════════

void TaskManager::begin() {
    // 创建信号量
    m_sensorReady = xSemaphoreCreateBinary();
    m_fusionReady = xSemaphoreCreateBinary();
    m_stateMutex  = xSemaphoreCreateMutex();
    
    m_running = true;
    
    // ── Core 0 任务 ──
    xTaskCreatePinnedToCore(
        taskSensorRead, "SensorRead", TASK_STACK_DEEP,
        this, PRIO_SENSOR_READ,
        &m_taskSensorRead, 0);  // Core 0
    
    xTaskCreatePinnedToCore(
        taskTelemetry, "Telemetry", TASK_STACK_SHALLOW,
        this, PRIO_TELEMETRY,
        &m_taskTelemetry, 0);
    
    // ── Core 1 任务 ──
    xTaskCreatePinnedToCore(
        taskFusion, "Fusion", TASK_STACK_DEEP,
        this, PRIO_FUSION,
        &m_taskFusion, 1);  // Core 1
    
    xTaskCreatePinnedToCore(
        taskControl, "Control", TASK_STACK_MEDIUM,
        this, PRIO_CONTROL,
        &m_taskControl, 1);
    
    xTaskCreatePinnedToCore(
        taskFollow, "Follow", TASK_STACK_MEDIUM,
        this, PRIO_FOLLOW,
        &m_taskFollow, 1);
    
    xTaskCreatePinnedToCore(
        taskWebServer, "WebServer", TASK_STACK_DEEP,
        this, PRIO_WEB_SERVER,
        &m_taskWebServer, 1);
    
    xTaskCreatePinnedToCore(
        taskSafety, "Safety", TASK_STACK_SHALLOW,
        this, PRIO_SAFETY,
        &m_taskSafety, 1);
    
    LOG(LOG_TAG_FUSION, "Task Manager: 7 tasks created (Core0:2, Core1:5)");
}

// ═══════════════════════════════════════════════════════════
//  系统状态
// ═══════════════════════════════════════════════════════════

void TaskManager::getStatus(SystemStatus& status) {
    status.uptime = millis();
    status.freeHeap = ESP.getFreeHeap();
    status.freePSRAM = ESP.getFreePsram();
    status.batteryVoltage = m_batteryVoltage;
    status.safetySwitchOk = (digitalRead(SAFETY_SWITCH_PIN) == HIGH);
    status.sensorErrors = m_sensorErrors;
    status.controlCycles = m_controlCycles;
}

// ═══════════════════════════════════════════════════════════
//  任务 1: 传感器采集 (200Hz, Core 0)
// ═══════════════════════════════════════════════════════════

void TaskManager::taskSensorRead(void* param) {
    TaskManager* self = (TaskManager*)param;
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(INTERVAL_SENSOR_READ);
    
    while (self->m_running) {
        // 读取 IMU (200Hz)
        IMUData imuData;
        if (!imu.readRaw(imuData)) {
            self->m_sensorErrors++;
        }
        
        // 气压计 (降采样到 50Hz — 每 4 次读一次)
        static int baroCounter = 0;
        if (++baroCounter >= 4) {
            baroCounter = 0;
            BaroData baroData;
            baro.read(baroData);
        }
        
        // UWB 测距触发 (降采样到 20Hz)
        static int uwbCounter = 0;
        if (++uwbCounter >= 10) {
            uwbCounter = 0;
            uwb.startRanging();
        }
        
        // 通知融合任务数据就绪
        xSemaphoreGive(self->m_sensorReady);
        
        vTaskDelayUntil(&lastWake, interval);
    }
    
    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════
//  任务 2: 传感器融合 (100Hz, Core 1)
// ═══════════════════════════════════════════════════════════

void TaskManager::taskFusion(void* param) {
    TaskManager* self = (TaskManager*)param;
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(INTERVAL_FUSION);
    
    while (self->m_running) {
        // 等待传感器数据
        if (xSemaphoreTake(self->m_sensorReady, pdMS_TO_TICKS(20)) == pdTRUE) {
            fusion.update();
            xSemaphoreGive(self->m_fusionReady);
        }
        
        vTaskDelayUntil(&lastWake, interval);
    }
    
    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════
//  任务 3: 姿态控制 (200Hz, Core 1) — 最高优先级
// ═══════════════════════════════════════════════════════════

void TaskManager::taskControl(void* param) {
    TaskManager* self = (TaskManager*)param;
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(INTERVAL_CONTROL);
    
    // 姿态 PID 控制器
    static PIDController pidRoll, pidPitch, pidYaw;
    pidRoll.configure(PID_ROLL_P, PID_ROLL_I, PID_ROLL_D,
                      -PID_ROLL_MAX, PID_ROLL_MAX, 200.0f);
    pidPitch.configure(PID_PITCH_P, PID_PITCH_I, PID_PITCH_D,
                       -PID_PITCH_MAX, PID_PITCH_MAX, 200.0f);
    pidYaw.configure(PID_YAW_P, PID_YAW_I, PID_YAW_D,
                     -PID_YAW_MAX, PID_YAW_MAX, 200.0f);
    
    while (self->m_running) {
        if (xSemaphoreTake(self->m_fusionReady, pdMS_TO_TICKS(10)) == pdTRUE) {
            const FusionState& s = fusion.getState();
            
            // 获取跟随输出
            FollowTarget target;
            // target 在 follow 任务中更新，这里读取
            FollowOutput fo;
            // fo 由 follow 任务生成，通过共享内存获取
            
            // PID 计算
            float pitchCtrl = pidPitch.compute(0, s.pitch);  // 目标 vs 当前
            float rollCtrl  = pidRoll.compute(0, s.roll);
            float yawCtrl   = pidYaw.compute(0, s.yaw);
            
            // 油门 (悬停 + 高度修正)
            float throttle = 0.25f;  // 基础悬停油门
            
            // 写入电机混合器
            motors.setMixer(throttle, rollCtrl, pitchCtrl, yawCtrl);
            
            self->m_controlCycles++;
        }
        
        vTaskDelayUntil(&lastWake, interval);
    }
    
    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════
//  任务 4: 跟随决策 (20Hz, Core 1)
// ═══════════════════════════════════════════════════════════

void TaskManager::taskFollow(void* param) {
    TaskManager* self = (TaskManager*)param;
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(INTERVAL_FOLLOW);
    
    while (self->m_running) {
        const FusionState& s = fusion.getState();
        
        // 构建目标 (从 UWB Tag 位置获取)
        FollowTarget target;
        Vec3 tagPos;
        float conf;
        if (uwb.getPosition(tagPos, conf) && conf > 0.3f) {
            target.posX = tagPos.x;
            target.posY = tagPos.y;
            target.posZ = tagPos.z;
            target.valid = true;
            target.lastSeen = millis();
        } else {
            target.valid = false;
        }
        
        // 更新跟随控制
        FollowOutput output;
        followCtrl.update(s, target, output);
        
        // 输出写入共享内存 (供控制任务读取)
        // (实际使用队列或互斥保护)
        
        vTaskDelayUntil(&lastWake, interval);
    }
    
    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════
//  任务 5: Web 服务器
// ═══════════════════════════════════════════════════════════

void TaskManager::taskWebServer(void* param) {
    TaskManager* self = (TaskManager*)param;
    
    while (self->m_running) {
        webServer.loop();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════
//  任务 6: 安全监控 (50Hz, Core 1)
// ═══════════════════════════════════════════════════════════

void TaskManager::taskSafety(void* param) {
    TaskManager* self = (TaskManager*)param;
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(INTERVAL_SAFETY);
    
    while (self->m_running) {
        // 安全开关检查
        if (digitalRead(SAFETY_SWITCH_PIN) == LOW && motors.isArmed()) {
            motors.emergencyStop();
            followCtrl.setMode(FollowMode::IDLE);
            LOG(LOG_TAG_CTRL, "SAFETY: Kill switch engaged");
        }
        
        // 姿态超限检查
        const FusionState& s = fusion.getState();
        if (fabsf(s.roll) > MAX_TILT_ANGLE || fabsf(s.pitch) > MAX_TILT_ANGLE) {
            if (motors.isArmed()) {
                LOG(LOG_TAG_CTRL, "SAFETY: Tilt exceeded %.0f°", MAX_TILT_ANGLE);
                followCtrl.setMode(FollowMode::HOVER);  // 强制回稳
            }
        }
        
        // 电池电压检测 (简化)
        // float vbat = analogRead(BATTERY_PIN) * voltage_divider;
        // self->m_batteryVoltage = vbat;
        
        // 心跳 LED
        static bool ledState = false;
        ledState = !ledState;
        // digitalWrite(LED_BUILTIN, ledState);
        
        vTaskDelayUntil(&lastWake, interval);
    }
    
    vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════
//  任务 7: 遥测广播 (10Hz, Core 0)
// ═══════════════════════════════════════════════════════════

void TaskManager::taskTelemetry(void* param) {
    TaskManager* self = (TaskManager*)param;
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t interval = pdMS_TO_TICKS(INTERVAL_TELEMETRY);
    
    while (self->m_running) {
        // WebSocket 广播 (到所有连接客户端)
        webServer.broadcastTelemetry();
        
        // 串口遥测 (调试)
        #if DEBUG_ENABLE
        const FusionState& s = fusion.getState();
        DEBUG_SERIAL.printf("[TEL] X:%.0f Y:%.0f Z:%.0f | R:%.1f P:%.1f Y:%.1f | "
                            "Mode:%d Clients:%d\n",
                            s.posX, s.posY, s.posZ,
                            s.roll, s.pitch, s.yaw,
                            (int)followCtrl.getMode(),
                            webServer.getClientCount());
        #endif
        
        vTaskDelayUntil(&lastWake, interval);
    }
    
    vTaskDelete(NULL);
}
