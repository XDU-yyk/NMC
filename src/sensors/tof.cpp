/**
 * @file    tof.cpp
 * @brief   VL53L1X ToF distance sensor driver implementation.
 */

#include "sensors/tof.h"

ToFSensor tof;

static constexpr uint16_t TOF_IO_TIMEOUT_MS = 80;
static constexpr uint32_t TOF_READ_PERIOD_MS = 50;
static constexpr uint8_t TOF_MAX_CONSECUTIVE_ERRORS = 3;

void ToFSensor::begin() {
    m_data = ToFData{};
    m_initialized = false;
    m_lastReadAttempt = 0;

#if defined(TOF_XSHUT_PIN) && (TOF_XSHUT_PIN >= 0)
    pinMode(TOF_XSHUT_PIN, OUTPUT);
    digitalWrite(TOF_XSHUT_PIN, LOW);
    delay(10);
    digitalWrite(TOF_XSHUT_PIN, HIGH);
    delay(50);
#endif

    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(TOF_IO_TIMEOUT_MS);
    TOF_I2C_PORT.setClock(100000);

    m_vl53l1x.setBus(&TOF_I2C_PORT);
    m_vl53l1x.setTimeout(TOF_IO_TIMEOUT_MS);

    if (!m_vl53l1x.init()) {
        LOG(LOG_TAG_TOF, "VL53L1X init failed, no valid response on SDA=%d SCL=%d",
            TOF_SDA, TOF_SCL);
        m_data.rangeStatus = 255;
        m_data.errorCount++;
        return;
    }

    m_vl53l1x.setDistanceMode(VL53L1X::Medium);
    m_vl53l1x.setMeasurementTimingBudget(TOF_TIMING_BUDGET_MS * 1000);
    m_vl53l1x.startContinuous(TOF_TIMING_BUDGET_MS);

    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);
    m_initialized = true;

    LOG(LOG_TAG_TOF, "VL53L1X initialized, SDA=%d SCL=%d budget=%dms",
        TOF_SDA, TOF_SCL, TOF_TIMING_BUDGET_MS);
}

void ToFSensor::update() {
    if (!m_initialized) return;

    const uint32_t now = millis();
    if (now - m_lastReadAttempt < TOF_READ_PERIOD_MS) {
        return;
    }
    m_lastReadAttempt = now;

    m_data.timeout = false;

    // read(false) does not wait on dataReady(); this avoids a long stall on
    // modules whose GPIO interrupt/status bit is unreliable.
    const uint16_t dist = m_vl53l1x.read(false);
    if (m_vl53l1x.timeoutOccurred()) {
        m_data.valid = false;
        m_data.timeout = true;
        m_data.rangeStatus = 254;
        m_data.errorCount++;
        if (m_data.errorCount == TOF_MAX_CONSECUTIVE_ERRORS) {
            LOG(LOG_TAG_TOF, "VL53L1X read timeout, using fallback telemetry");
        }
        return;
    }

    m_data.distance = dist;
    m_data.rangeStatus = m_vl53l1x.ranging_data.range_status;
    m_data.valid = (m_data.rangeStatus == 0) && (dist > 0) && (dist < 4000);
    m_data.lastUpdate = now;
    m_data.errorCount = m_data.valid ? 0 : (m_data.errorCount + 1);
}
