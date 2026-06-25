/**
 * @file    tof.cpp
 * @brief   VL53L1X ToF driver implementation.
 */

#include "sensors/tof.h"

ToFSensor tof;

static constexpr uint16_t TOF_READ_TIMEOUT_MS = 500;
static constexpr uint8_t TOF_MAX_CONSECUTIVE_ERRORS = 3;

void ToFSensor::begin() {
    memset(&m_data, 0, sizeof(m_data));
    m_initialized = false;

#if defined(TOF_XSHUT_PIN) && (TOF_XSHUT_PIN >= 0)
    pinMode(TOF_XSHUT_PIN, OUTPUT);
    digitalWrite(TOF_XSHUT_PIN, LOW);
    delay(10);
    digitalWrite(TOF_XSHUT_PIN, HIGH);
    delay(20);
#endif

    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(TOF_READ_TIMEOUT_MS);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);

    // 启用内部上拉（某些 ESP32-S3 板 I2C 需要）
    pinMode(TOF_SDA, INPUT_PULLUP);
    pinMode(TOF_SCL, INPUT_PULLUP);

    m_vl53l1x.setBus(&TOF_I2C_PORT);
    m_vl53l1x.setTimeout(TOF_READ_TIMEOUT_MS);

    if (!m_vl53l1x.init()) {
        LOG(LOG_TAG_TOF, "VL53L1X init failed — module not responding (SDA=%d, SCL=%d)",
            TOF_SDA, TOF_SCL);
        LOG(LOG_TAG_TOF, "Check: wiring, XSHUT=3.3V, or module may be defective");
            m_data.valid = false;
            m_data.rangeStatus = 255;
            m_data.errorCount++;
            return;
        }

    m_vl53l1x.setDistanceMode(VL53L1X::Medium);
    m_vl53l1x.setMeasurementTimingBudget(TOF_TIMING_BUDGET_MS * 1000);
    m_vl53l1x.startContinuous(TOF_TIMING_BUDGET_MS);

    m_initialized = true;

    LOG(LOG_TAG_TOF, "VL53L1X ok, SDA=%d SCL=%d budget=%dms",
        TOF_SDA, TOF_SCL, TOF_TIMING_BUDGET_MS);
}

void ToFSensor::update() {
    if (!m_initialized) return;

    uint16_t dist = m_vl53l1x.read(true);
    if (m_vl53l1x.timeoutOccurred()) {
        m_data.valid = false;
        m_data.rangeStatus = 254;
        m_data.errorCount++;
        if (m_data.errorCount == TOF_MAX_CONSECUTIVE_ERRORS) {
            LOG(LOG_TAG_TOF, "VL53L1X read timeout, falling back until data recovers");
        }
        return;
    }

    m_data.distance = dist;
    m_data.rangeStatus = m_vl53l1x.ranging_data.range_status;
    m_data.valid = (m_data.rangeStatus == 0) && (dist > 0) && (dist < 4000);
    m_data.lastUpdate = millis();
    m_data.errorCount = m_data.valid ? 0 : (m_data.errorCount + 1);
}
