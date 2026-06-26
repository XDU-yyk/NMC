/**
 * @file    tof.h
 * @brief   VL53L1X ToF distance sensor driver.
 */

#ifndef TOF_H
#define TOF_H

#include "config.h"
#include <Arduino.h>
#include <VL53L1X.h>

struct ToFData {
    uint16_t distance = 0;       // mm
    bool     valid = false;
    uint8_t  rangeStatus = 255;  // 0 = ok, 254 = timeout, 255 = not initialized
    uint32_t lastUpdate = 0;     // ms
    uint32_t errorCount = 0;
    bool     timeout = false;
};

class ToFSensor {
public:
    void begin();
    void update();

    const ToFData& getData() const { return m_data; }
    bool isHealthy() const { return m_data.valid && m_data.rangeStatus == 0; }
    bool isInitialized() const { return m_initialized; }

    uint16_t getAltitudeMM() const { return m_data.distance; }

private:
    VL53L1X m_vl53l1x;
    ToFData m_data;
    bool    m_initialized = false;
    uint32_t m_lastReadAttempt = 0;
};

extern ToFSensor tof;

#endif // TOF_H
