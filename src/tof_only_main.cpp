/**
 * @file    tof_only_main.cpp
 * @brief   VL53L1X-only diagnostic firmware.
 */

#include <Arduino.h>
#include "config.h"
#include "sensors/tof.h"

static uint32_t g_startTime = 0;
static uint32_t g_lastInitAttempt = 0;

static bool scanForTof()
{
    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(150);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);

    Serial.printf("  Scanning I2C on SDA=%d SCL=%d\n", TOF_SDA, TOF_SCL);
    uint8_t foundCount = 0;
    bool found = false;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        TOF_I2C_PORT.beginTransmission(addr);
        uint8_t err = TOF_I2C_PORT.endTransmission();
        if (err == 0) {
            foundCount++;
            Serial.printf("  I2C device found: 0x%02X%s\n",
                addr, addr == 0x29 ? " (VL53L1X default)" : "");
            found = found || (addr == 0x29);
        }
    }
    if (found && foundCount != 1) {
        Serial.printf("  ToF: unstable I2C scan (%u devices), retry later\n", foundCount);
        return false;
    }
    return found;
}

static void initTof()
{
    if (!scanForTof()) {
        Serial.println("  ToF: 0x29 not found");
        return;
    }

    tof.begin();
    const auto& td = tof.getData();
    Serial.printf("  ToF: %s valid=%d status=%u dist=%u err=%lu fault=%s\n",
        tof.isInitialized() ? "initialized" : "init failed",
        td.valid,
        td.rangeStatus,
        td.distance,
        static_cast<unsigned long>(td.errorCount),
        td.lastFault);
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  NMC ToF-only diagnostic");
    Serial.println("  No WiFi, no GPS, no Web");
    Serial.println("==============================================");

    initTof();
    g_startTime = millis();
}

void loop()
{
    if (tof.isInitialized()) {
        tof.update();
    } else if (millis() - g_lastInitAttempt >= 5000) {
        g_lastInitAttempt = millis();
        initTof();
    }

    static uint32_t lastPrint = 0;
    const uint32_t now = millis();
    if (now - lastPrint >= 5000) {
        lastPrint = now;
        const auto& td = tof.getData();
        Serial.printf("[TOF_ONLY] %lus init=%d valid=%d status=%u dist=%u err=%lu age=%lu ok=%lu fail=%lu miss=%lu rec=%lu fault=%s heap=%u\n",
            (now - g_startTime) / 1000,
            tof.isInitialized(),
            td.valid,
            td.rangeStatus,
            td.distance,
            static_cast<unsigned long>(td.errorCount),
            static_cast<unsigned long>(td.lastUpdate ? now - td.lastUpdate : 0),
            static_cast<unsigned long>(td.validReads),
            static_cast<unsigned long>(td.failedReads),
            static_cast<unsigned long>(td.readyMisses),
            static_cast<unsigned long>(td.recoveries),
            td.lastFault,
            ESP.getFreeHeap());
    }

    delay(10);
}
