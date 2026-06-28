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
    uint32_t totalReads = 0;
    uint32_t validReads = 0;
    uint32_t failedReads = 0;
    uint32_t readyMisses = 0;
    uint32_t recoveries = 0;
    const char* lastFault = "none";
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
    void recoverBus();
    bool probeCleanBus();
    bool configureSensor();
    bool writeReg8Checked(uint16_t reg, uint8_t value, const char* fault);
    bool readReg16Checked(uint16_t reg, uint16_t& value, const char* fault);
    bool softResetSensor();
    bool readMeasurement(uint16_t& distance, uint8_t& rangeStatus);
    bool clearInterrupt();
    uint8_t convertRangeStatus(uint8_t rawStatus, uint8_t streamCount) const;

    VL53L1X m_vl53l1x;
    ToFData m_data;
    bool    m_initialized = false;
    uint32_t m_lastReadAttempt = 0;
    uint32_t m_lastRecoverAttempt = 0;
    uint32_t m_lastReadyMissLog = 0;
};

extern ToFSensor tof;

#endif // TOF_H
