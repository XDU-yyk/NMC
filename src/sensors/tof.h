/**
 * @file    tof.h
 * @brief   VL53L1X ToF 激光测距驱动 (下视 — 低空高度)
 */

#ifndef TOF_H
#define TOF_H

#include "config.h"
#include <Arduino.h>
#include <VL53L1X.h>

struct ToFData {
    uint16_t distance;       // 测距值 (mm)
    bool     valid;          // 数据有效
    uint8_t  rangeStatus;    // 0 = 正常, 其他 = 错误码
    uint32_t lastUpdate;     // ms
    uint32_t errorCount;     // 连续读取失败次数
};

class ToFSensor {
public:
    void begin();
    void update();

    const ToFData& getData() const { return m_data; }
    bool isHealthy() const { return m_data.valid && m_data.rangeStatus == 0; }
    bool isInitialized() const { return m_initialized; }

    /* 获取当前高度 (mm) */
    uint16_t getAltitudeMM() const { return m_data.distance; }

private:
    VL53L1X m_vl53l1x;
    ToFData m_data;
    bool    m_initialized = false;
};

extern ToFSensor tof;

#endif // TOF_H
