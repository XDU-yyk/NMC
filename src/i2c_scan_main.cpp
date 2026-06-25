/**
 * @file    i2c_scan_main.cpp
 * @brief   Standalone I2C scanner for VL53L1X wiring checks.
 */

#include <Arduino.h>
#include <Wire.h>

struct I2CPinPair {
    const char* label;
    int sda;
    int scl;
};

static void testI2C(const I2CPinPair& pins) {
    Serial.printf("\n--- I2C: %s (SDA=%d, SCL=%d) ---\n", pins.label, pins.sda, pins.scl);
    Wire.begin(pins.sda, pins.scl);
    Wire.setClock(100000);
    Wire.setTimeOut(80);

    int found = 0;
    for (int addr = 0x08; addr <= 0x77; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            found++;
            Serial.printf("  found 0x%02X", addr);
            if (addr == 0x29) Serial.print(" (VL53L1X default)");
            Serial.println();
        }
        delay(2);
    }

    if (found == 0) {
        Serial.println("  no device found");
    }

    Wire.end();
    delay(100);
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("=== I2C Scanner ===");
    Serial.printf("Chip: ESP32-S3 rev %d\n", ESP.getChipRevision());

    const I2CPinPair candidates[] = {
        {"ToF confirmed", 41, 42},
        {"Alt 4/5", 4, 5},
        {"Old 38/37", 38, 37},
    };

    for (const auto& pins : candidates) {
        testI2C(pins);
    }

    Serial.println("\n=== Done ===");
}

void loop() {
    delay(30000);
}
