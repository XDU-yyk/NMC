/**
 * @file    i2c_scan_main.cpp
 * @brief   Standalone I2C scanner for the configured VL53L1X ToF bus.
 */

#include <Arduino.h>
#include <Wire.h>
#include "config.h"

static constexpr uint8_t VL53L1X_ADDR = 0x29;
static constexpr uint16_t VL53L1X_MODEL_ID_REG = 0x010F;
static constexpr uint16_t VL53L1X_MODEL_ID = 0xEACC;
static constexpr uint16_t VL53L1X_SOFT_RESET_REG = 0x0000;
static constexpr uint8_t ACK_TRIALS = 30;
static constexpr uint8_t MODEL_TRIALS = 20;

static void preparePinsForIdleRead()
{
    TOF_I2C_PORT.end();
    delay(5);
    pinMode(TOF_SDA, INPUT_PULLUP);
    pinMode(TOF_SCL, INPUT_PULLUP);
    delay(5);
}

static void printIdleLevels(const char* label)
{
    preparePinsForIdleRead();
    Serial.printf("  idle[%s]: SDA=%d SCL=%d\n",
        label, digitalRead(TOF_SDA), digitalRead(TOF_SCL));
}

static void beginTofBus()
{
    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(150);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);
}

static uint8_t probeAddr(uint8_t addr)
{
    TOF_I2C_PORT.beginTransmission(addr);
    return TOF_I2C_PORT.endTransmission();
}

static bool writeReg8Checked(uint8_t addr, uint16_t reg, uint8_t value)
{
    TOF_I2C_PORT.beginTransmission(addr);
    TOF_I2C_PORT.write(static_cast<uint8_t>(reg >> 8));
    TOF_I2C_PORT.write(static_cast<uint8_t>(reg));
    TOF_I2C_PORT.write(value);
    const uint8_t writeErr = TOF_I2C_PORT.endTransmission();
    if (writeErr != 0) {
        Serial.printf("  write 0x%04X failed: i2c=%u\n", reg, writeErr);
        return false;
    }
    return true;
}

static bool readReg16Checked(uint8_t addr, uint16_t reg, uint16_t& value, uint8_t& writeErr, uint8_t& readBytes)
{
    value = 0;
    writeErr = 0;
    readBytes = 0;

    TOF_I2C_PORT.beginTransmission(addr);
    TOF_I2C_PORT.write(static_cast<uint8_t>(reg >> 8));
    TOF_I2C_PORT.write(static_cast<uint8_t>(reg));
    writeErr = TOF_I2C_PORT.endTransmission();
    if (writeErr != 0) {
        return false;
    }

    readBytes = TOF_I2C_PORT.requestFrom(addr, static_cast<uint8_t>(2));
    if (readBytes != 2) {
        while (TOF_I2C_PORT.available()) {
            (void)TOF_I2C_PORT.read();
        }
        return false;
    }

    const int hi = TOF_I2C_PORT.read();
    const int lo = TOF_I2C_PORT.read();
    if (hi < 0 || lo < 0) {
        Serial.println("  model read failed: short buffer");
        return false;
    }

    value = (static_cast<uint16_t>(hi) << 8) | static_cast<uint8_t>(lo);
    return true;
}

static void recoverBus()
{
    TOF_I2C_PORT.end();
    delay(5);

    pinMode(TOF_SDA, INPUT_PULLUP);
    pinMode(TOF_SCL, INPUT_PULLUP);
    delay(2);

    pinMode(TOF_SCL, OUTPUT_OPEN_DRAIN);
    for (uint8_t i = 0; i < 9; i++) {
        digitalWrite(TOF_SCL, LOW);
        delayMicroseconds(8);
        digitalWrite(TOF_SCL, HIGH);
        delayMicroseconds(8);
    }

    pinMode(TOF_SDA, OUTPUT_OPEN_DRAIN);
    digitalWrite(TOF_SDA, LOW);
    delayMicroseconds(8);
    pinMode(TOF_SCL, INPUT_PULLUP);
    delayMicroseconds(8);
    pinMode(TOF_SDA, INPUT_PULLUP);
    delayMicroseconds(8);
}

static void softResetTof()
{
    Serial.println("  soft reset attempt");
    if (!writeReg8Checked(VL53L1X_ADDR, VL53L1X_SOFT_RESET_REG, 0x00)) {
        return;
    }
    delayMicroseconds(100);
    if (!writeReg8Checked(VL53L1X_ADDR, VL53L1X_SOFT_RESET_REG, 0x01)) {
        return;
    }
    delay(10);
}

static void probeTofStability()
{
    uint8_t ackOk = 0;
    uint8_t errCounts[8] = {};
    for (uint8_t i = 0; i < ACK_TRIALS; i++) {
        const uint8_t err = probeAddr(VL53L1X_ADDR);
        if (err == 0) {
            ackOk++;
        } else if (err < 8) {
            errCounts[err]++;
        }
        delay(10);
    }

    Serial.printf("  0x%02X ACK: %u/%u", VL53L1X_ADDR, ackOk, ACK_TRIALS);
    for (uint8_t i = 1; i < 8; i++) {
        if (errCounts[i]) Serial.printf(" err%u=%u", i, errCounts[i]);
    }
    Serial.println();
}

static void sampleModelId()
{
    uint8_t okCount = 0;
    uint8_t expectedCount = 0;
    uint8_t writeFailures = 0;
    uint8_t shortReads = 0;
    uint16_t lastBad = 0;

    for (uint8_t i = 0; i < MODEL_TRIALS; i++) {
        uint16_t modelId = 0;
        uint8_t writeErr = 0;
        uint8_t readBytes = 0;
        const bool ok = readReg16Checked(
            VL53L1X_ADDR, VL53L1X_MODEL_ID_REG, modelId, writeErr, readBytes);
        if (ok) {
            okCount++;
            if (modelId == VL53L1X_MODEL_ID) {
                expectedCount++;
            } else {
                lastBad = modelId;
            }
            Serial.printf("  model[%02u]: ok id=0x%04X%s\n",
                i, modelId, modelId == VL53L1X_MODEL_ID ? " expected" : " BAD");
        } else {
            if (writeErr != 0) {
                writeFailures++;
                Serial.printf("  model[%02u]: write failed i2c=%u\n", i, writeErr);
            } else {
                shortReads++;
                Serial.printf("  model[%02u]: read failed bytes=%u\n", i, readBytes);
            }
        }
        delay(20);
    }

    Serial.printf("  model summary: ok=%u/%u expected=%u bad_last=0x%04X write_fail=%u short_read=%u\n",
        okCount, MODEL_TRIALS, expectedCount, lastBad, writeFailures, shortReads);
}

static void scanTofBus()
{
    Serial.printf("\n--- ToF I2C bus: SDA=%d SCL=%d freq=%luHz ---\n",
        TOF_SDA, TOF_SCL, static_cast<unsigned long>(TOF_I2C_FREQ));
    printIdleLevels("before");
    recoverBus();
    printIdleLevels("after-recover");
    beginTofBus();

    uint8_t foundCount = 0;
    bool foundTof = false;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        TOF_I2C_PORT.beginTransmission(addr);
        const uint8_t err = TOF_I2C_PORT.endTransmission();
        if (err == 0) {
            foundCount++;
            Serial.printf("  found 0x%02X%s\n",
                addr, addr == VL53L1X_ADDR ? " (VL53L1X default)" : "");
            foundTof = foundTof || (addr == VL53L1X_ADDR);
        }
        delay(2);
    }
    probeTofStability();

    if (foundCount == 0) {
        Serial.println("  no I2C devices found");
        return;
    }

    if (!foundTof) {
        Serial.printf("  0x%02X not found\n", VL53L1X_ADDR);
        return;
    }

    sampleModelId();
    softResetTof();
    sampleModelId();
    printIdleLevels("after");
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("=== ToF I2C Scanner ===");
    Serial.printf("Chip: ESP32-S3 rev %d\n", ESP.getChipRevision());
    scanTofBus();
    Serial.println("\n=== Done ===");
}

void loop()
{
    delay(30000);
    scanTofBus();
}
