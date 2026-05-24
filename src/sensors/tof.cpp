/**
 * @file    tof.cpp
 * @brief   VL53L1X ToF 激光测距实现 (I2C, 下视安装)
 */

#include "sensors/tof.h"

ToFSensor tof;

void ToFSensor::begin() {
    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);

    m_vl53l1x.setBus(&TOF_I2C_PORT);

    if (!m_vl53l1x.init()) {
        LOG(LOG_TAG_TOF, "VL53L1X init FAILED — check I2C wiring (SDA=%d, SCL=%d)",
            TOF_SDA, TOF_SCL);
        m_initialized = false;
        return;
    }

    m_vl53l1x.setTimeout(500);
    m_vl53l1x.setDistanceMode(VL53L1X::Medium);  // 中距模式 (≤3m)
    m_vl53l1x.setMeasurementTimingBudget(TOF_TIMING_BUDGET_MS * 1000);  // us
    m_vl53l1x.startContinuous(TOF_TIMING_BUDGET_MS);

    m_initialized = true;
    memset(&m_data, 0, sizeof(m_data));

    LOG(LOG_TAG_TOF, "VL53L1X ok — %dms timing budget", TOF_TIMING_BUDGET_MS);
}

void ToFSensor::update() {
    if (!m_initialized) return;

    if (!m_vl53l1x.dataReady()) return;

    uint16_t dist = m_vl53l1x.read(false);  // 非阻塞读取

    m_data.distance     = dist;
    m_data.rangeStatus  = m_vl53l1x.ranging_data.range_status;
    m_data.valid        = (m_data.rangeStatus == 0) && (dist > 0) && (dist < 4000);
    m_data.lastUpdate   = millis();
}
