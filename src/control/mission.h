/**
 * @file    mission.h
 * @brief   任务规划器 v2.0 — 飞控命令编排 (替代原 motor.cpp)
 */

#ifndef MISSION_H
#define MISSION_H

#include "config.h"
#include "comm/fc_bridge.h"
#include "follow/follow.h"
#include <Arduino.h>

class MissionPlanner {
public:
    void begin();
    void update();

    /* 飞控控制 */
    bool arm();
    bool disarm();
    bool isArmed() const { return m_armed; }

    /* 安全开关读取 */
    bool isSafetySwitchOn() const;

    /* 系统状态 */
    struct MissionStatus {
        uint32_t uptime;
        uint32_t controlCycles;
        uint32_t sensorErrors;
        float    batteryVoltage;
        uint8_t  batteryCells;
        bool     fcOnline;
        bool     gpsValid;
        bool     tofValid;
    };
    void getStatus(MissionStatus& status) const;

private:
    bool     m_armed   = false;
    uint32_t m_controlCycles = 0;
    uint32_t m_sensorErrors  = 0;

    uint32_t m_lastSafetyCheck = 0;
};

extern MissionPlanner mission;

#endif // MISSION_H
