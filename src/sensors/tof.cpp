/**
 * @file    tof.cpp
 * @brief   VL53L1X ToF distance sensor driver implementation.
 */

#include "sensors/tof.h"

ToFSensor tof;

static constexpr uint16_t TOF_IO_TIMEOUT_MS = 150;
static constexpr uint32_t TOF_READ_PERIOD_MS = 1000;
static constexpr uint8_t TOF_MAX_CONSECUTIVE_ERRORS = 12;
static constexpr uint32_t TOF_RECOVER_PERIOD_MS = 8000;
static constexpr uint32_t TOF_STALE_MS = 5000;
static constexpr uint32_t TOF_INTERMEASUREMENT_MS = 200;
static constexpr uint16_t TOF_DEFAULT_ADDR = 0x29;
static constexpr uint16_t TOF_MODEL_ID_REG = 0x010F;
static constexpr uint16_t TOF_EXPECTED_MODEL_ID = 0xEACC;
static constexpr uint16_t TOF_SOFT_RESET_REG = 0x0000;
static constexpr uint16_t TOF_RESULT_REG = 0x0089;
static constexpr uint16_t TOF_INTERRUPT_CLEAR_REG = 0x0086;
static constexpr uint8_t TOF_RESULT_LEN = 17;

void ToFSensor::recoverBus() {
    TOF_I2C_PORT.end();
    delay(5);

    pinMode(TOF_SDA, INPUT_PULLUP);
    pinMode(TOF_SCL, INPUT_PULLUP);
    delay(2);

    if (digitalRead(TOF_SDA) == LOW) {
        pinMode(TOF_SCL, OUTPUT_OPEN_DRAIN);
        for (uint8_t i = 0; i < 9 && digitalRead(TOF_SDA) == LOW; i++) {
            digitalWrite(TOF_SCL, LOW);
            delayMicroseconds(8);
            digitalWrite(TOF_SCL, HIGH);
            delayMicroseconds(8);
        }
    }

    pinMode(TOF_SDA, OUTPUT_OPEN_DRAIN);
    digitalWrite(TOF_SDA, LOW);
    delayMicroseconds(8);
    pinMode(TOF_SCL, INPUT_PULLUP);
    delayMicroseconds(8);
    pinMode(TOF_SDA, INPUT_PULLUP);
    delayMicroseconds(8);
}

bool ToFSensor::probeCleanBus() {
    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(TOF_IO_TIMEOUT_MS);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);

    uint8_t ackCount = 0;
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        TOF_I2C_PORT.beginTransmission(TOF_DEFAULT_ADDR);
        const uint8_t err = TOF_I2C_PORT.endTransmission();
        if (err == 0) {
            ackCount++;
            delay(20);
            continue;
        }
        ackCount = 0;
        delay(20);
    }

    if (ackCount >= 2) {
        return true;
    }

    LOG(LOG_TAG_TOF, "VL53L1X address 0x29 did not ACK stably before init");
    m_data.lastFault = "probe_ack";
    return false;
}

bool ToFSensor::writeReg8Checked(uint16_t reg, uint8_t value, const char* fault) {
    TOF_I2C_PORT.beginTransmission(TOF_DEFAULT_ADDR);
    TOF_I2C_PORT.write(static_cast<uint8_t>(reg >> 8));
    TOF_I2C_PORT.write(static_cast<uint8_t>(reg));
    TOF_I2C_PORT.write(value);
    const uint8_t writeErr = TOF_I2C_PORT.endTransmission();
    if (writeErr != 0) {
        m_vl53l1x.last_status = writeErr;
        m_data.lastFault = fault;
        return false;
    }

    m_vl53l1x.last_status = 0;
    return true;
}

bool ToFSensor::readReg16Checked(uint16_t reg, uint16_t& value, const char* fault) {
    value = 0;

    TOF_I2C_PORT.beginTransmission(TOF_DEFAULT_ADDR);
    TOF_I2C_PORT.write(static_cast<uint8_t>(reg >> 8));
    TOF_I2C_PORT.write(static_cast<uint8_t>(reg));
    const uint8_t writeErr = TOF_I2C_PORT.endTransmission();
    if (writeErr != 0) {
        m_vl53l1x.last_status = writeErr;
        m_data.lastFault = fault;
        return false;
    }

    const uint8_t n = TOF_I2C_PORT.requestFrom(
        static_cast<uint8_t>(TOF_DEFAULT_ADDR),
        static_cast<uint8_t>(2));
    if (n != 2) {
        while (TOF_I2C_PORT.available()) {
            (void)TOF_I2C_PORT.read();
        }
        m_vl53l1x.last_status = 5;
        m_data.lastFault = fault;
        return false;
    }

    const int hi = TOF_I2C_PORT.read();
    const int lo = TOF_I2C_PORT.read();
    if (hi < 0 || lo < 0) {
        m_vl53l1x.last_status = 6;
        m_data.lastFault = fault;
        return false;
    }

    value = (static_cast<uint16_t>(hi) << 8) | static_cast<uint8_t>(lo);
    m_vl53l1x.last_status = 0;
    return true;
}

bool ToFSensor::softResetSensor() {
    if (!writeReg8Checked(TOF_SOFT_RESET_REG, 0x00, "soft_reset")) {
        return false;
    }
    delayMicroseconds(100);
    if (!writeReg8Checked(TOF_SOFT_RESET_REG, 0x01, "soft_reset")) {
        return false;
    }
    delay(5);
    return true;
}

bool ToFSensor::configureSensor() {
    m_vl53l1x.setBus(&TOF_I2C_PORT);
    m_vl53l1x.setTimeout(TOF_IO_TIMEOUT_MS);
    m_vl53l1x.last_status = 0;

    uint16_t modelId = 0;
    bool modelOk = false;
    for (uint8_t attempt = 0; attempt < 4 && !modelOk; attempt++) {
        modelOk = readReg16Checked(TOF_MODEL_ID_REG, modelId, "init_model") &&
            modelId == TOF_EXPECTED_MODEL_ID;
        if (modelOk) break;

        if (attempt == 1) {
            (void)softResetSensor();
        } else {
            recoverBus();
        }
        TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
        TOF_I2C_PORT.setTimeOut(TOF_IO_TIMEOUT_MS);
        TOF_I2C_PORT.setClock(TOF_I2C_FREQ);
        delay(30);
    }

    if (!modelOk) {
        LOG(LOG_TAG_TOF, "VL53L1X model check failed: id=0x%04X i2c=%u",
            modelId, m_vl53l1x.last_status);
        m_data.rangeStatus = 255;
        m_data.errorCount++;
        m_data.lastFault = "init_model";
        return false;
    }

    if (!m_vl53l1x.init()) {
        LOG(LOG_TAG_TOF, "VL53L1X init failed, no valid response on SDA=%d SCL=%d",
            TOF_SDA, TOF_SCL);
        m_data.rangeStatus = 255;
        m_data.errorCount++;
        m_data.lastFault = "init_model";
        return false;
    }

    m_vl53l1x.setDistanceMode(VL53L1X::Medium);
    m_vl53l1x.setMeasurementTimingBudget(TOF_TIMING_BUDGET_MS * 1000);
    m_vl53l1x.startContinuous(TOF_INTERMEASUREMENT_MS);
    delay(TOF_INTERMEASUREMENT_MS + 20);
    uint16_t distance = 0;
    uint8_t status = 255;
    (void)readMeasurement(distance, status);

    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);
    return true;
}

uint8_t ToFSensor::convertRangeStatus(uint8_t rawStatus, uint8_t streamCount) const {
    switch (rawStatus) {
        case 17:
        case 2:
        case 1:
        case 3:
            return VL53L1X::HardwareFail;
        case 13:
            return VL53L1X::MinRangeFail;
        case 18:
            return VL53L1X::SynchronizationInt;
        case 5:
            return VL53L1X::OutOfBoundsFail;
        case 4:
            return VL53L1X::SignalFail;
        case 6:
            return VL53L1X::SigmaFail;
        case 7:
            return VL53L1X::WrapTargetFail;
        case 12:
            return VL53L1X::XtalkSignalFail;
        case 8:
            return VL53L1X::RangeValidMinRangeClipped;
        case 9:
            return streamCount == 0
                ? VL53L1X::RangeValidNoWrapCheckFail
                : VL53L1X::RangeValid;
        default:
            return VL53L1X::None;
    }
}

bool ToFSensor::clearInterrupt() {
    TOF_I2C_PORT.beginTransmission(TOF_DEFAULT_ADDR);
    TOF_I2C_PORT.write(static_cast<uint8_t>(TOF_INTERRUPT_CLEAR_REG >> 8));
    TOF_I2C_PORT.write(static_cast<uint8_t>(TOF_INTERRUPT_CLEAR_REG));
    TOF_I2C_PORT.write(static_cast<uint8_t>(0x01));
    return TOF_I2C_PORT.endTransmission() == 0;
}

bool ToFSensor::readMeasurement(uint16_t& distance, uint8_t& rangeStatus) {
    distance = 0;
    rangeStatus = VL53L1X::None;

    TOF_I2C_PORT.beginTransmission(TOF_DEFAULT_ADDR);
    TOF_I2C_PORT.write(static_cast<uint8_t>(TOF_RESULT_REG >> 8));
    TOF_I2C_PORT.write(static_cast<uint8_t>(TOF_RESULT_REG));
    const uint8_t writeErr = TOF_I2C_PORT.endTransmission();
    if (writeErr != 0) {
        m_vl53l1x.last_status = writeErr;
        m_data.lastFault = "read_reg";
        return false;
    }

    uint8_t buf[TOF_RESULT_LEN] = {};
    const uint8_t n = TOF_I2C_PORT.requestFrom(
        static_cast<uint8_t>(TOF_DEFAULT_ADDR),
        static_cast<uint8_t>(TOF_RESULT_LEN));
    if (n != TOF_RESULT_LEN) {
        while (TOF_I2C_PORT.available()) {
            (void)TOF_I2C_PORT.read();
        }
        m_vl53l1x.last_status = 5;
        m_data.lastFault = "read_frame";
        return false;
    }

    for (uint8_t i = 0; i < TOF_RESULT_LEN; i++) {
        const int v = TOF_I2C_PORT.read();
        if (v < 0) {
            m_vl53l1x.last_status = 6;
            m_data.lastFault = "read_byte";
            return false;
        }
        buf[i] = static_cast<uint8_t>(v);
    }

    const uint8_t rawStatus = buf[0];
    const uint8_t streamCount = buf[2];
    const uint16_t rawDistance = (static_cast<uint16_t>(buf[13]) << 8) | buf[14];
    distance = static_cast<uint16_t>(((static_cast<uint32_t>(rawDistance) * 2011U) + 0x0400U) / 0x0800U);
    rangeStatus = convertRangeStatus(rawStatus, streamCount);
    (void)clearInterrupt();
    m_vl53l1x.last_status = 0;
    m_data.lastFault = "none";
    return true;
}

void ToFSensor::begin() {
    const uint32_t recoveries = m_data.recoveries;
    m_data = ToFData{};
    m_data.recoveries = recoveries;
    m_initialized = false;
    m_lastReadAttempt = 0;
    m_lastRecoverAttempt = 0;
    m_lastReadyMissLog = 0;

#if defined(TOF_XSHUT_PIN) && (TOF_XSHUT_PIN >= 0)
    pinMode(TOF_XSHUT_PIN, OUTPUT);
    digitalWrite(TOF_XSHUT_PIN, LOW);
    delay(20);
    digitalWrite(TOF_XSHUT_PIN, HIGH);
    delay(120);
#endif

    recoverBus();
    bool found = false;
    for (uint8_t attempt = 0; attempt < 3 && !found; attempt++) {
        found = probeCleanBus();
        if (!found) {
            recoverBus();
            delay(100);
        }
    }
    if (!found) {
        m_data.rangeStatus = 255;
        m_data.errorCount++;
        m_data.lastFault = "probe_ack";
        return;
    }

    if (!configureSensor()) return;
    m_initialized = true;

    LOG(LOG_TAG_TOF, "VL53L1X initialized, SDA=%d SCL=%d budget=%dms period=%lums i2c=%luHz",
        TOF_SDA, TOF_SCL, TOF_TIMING_BUDGET_MS,
        static_cast<unsigned long>(TOF_INTERMEASUREMENT_MS),
        static_cast<unsigned long>(TOF_I2C_FREQ));
}

void ToFSensor::update() {
    if (!m_initialized) return;

    const uint32_t now = millis();
    if (now - m_lastReadAttempt < TOF_READ_PERIOD_MS) {
        return;
    }
    m_lastReadAttempt = now;

    m_data.timeout = false;
    m_vl53l1x.last_status = 0;

    m_data.totalReads++;
    m_vl53l1x.last_status = 0;
    uint16_t dist = 0;
    uint8_t status = 255;
    const bool readOk = readMeasurement(dist, status);
    const bool badDistance = (dist == 0) || (dist >= 4000);
    const bool i2cError = (m_vl53l1x.last_status != 0);
    const bool timedOut = m_vl53l1x.timeoutOccurred();

    if (!readOk || timedOut || i2cError || badDistance) {
        const bool stale = !m_data.lastUpdate || now - m_data.lastUpdate > TOF_STALE_MS;
        if (stale) {
            m_data.valid = false;
            m_data.rangeStatus = timedOut || i2cError ? 254 : status;
        }
        m_data.timeout = timedOut || i2cError;
        m_data.errorCount++;
        m_data.failedReads++;
        if (m_data.errorCount == TOF_MAX_CONSECUTIVE_ERRORS) {
            LOG(LOG_TAG_TOF, "VL53L1X read error: fault=%s i2c=%u timeout=%d status=%u dist=%u ok=%lu fail=%lu miss=%lu",
                m_data.lastFault,
                m_vl53l1x.last_status, timedOut, status, dist,
                static_cast<unsigned long>(m_data.validReads),
                static_cast<unsigned long>(m_data.failedReads),
                static_cast<unsigned long>(m_data.readyMisses));
        }
        if (m_data.errorCount >= TOF_MAX_CONSECUTIVE_ERRORS &&
            (!m_data.lastUpdate || now - m_data.lastUpdate > TOF_STALE_MS) &&
            now - m_lastRecoverAttempt >= TOF_RECOVER_PERIOD_MS) {
            m_lastRecoverAttempt = now;
            m_data.recoveries++;
            LOG(LOG_TAG_TOF, "Recovering VL53L1X after %lu errors",
                static_cast<unsigned long>(m_data.errorCount));
            begin();
        }
        return;
    }

    m_data.distance = dist;
    m_data.rangeStatus = status;
    m_data.valid = (status == 0);
    m_data.lastUpdate = now;
    m_data.errorCount = m_data.valid ? 0 : (m_data.errorCount + 1);
    if (m_data.valid) {
        m_data.lastFault = "none";
        m_data.validReads++;
    } else {
        m_data.failedReads++;
    }
}
