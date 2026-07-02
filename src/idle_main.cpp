/**
 * @file idle_main.cpp
 * @brief Idle firmware used to keep ESP32 quiet during external UART tests.
 */

#include <Arduino.h>

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println("NMC idle firmware: Serial2/MSP disabled.");
}

void loop()
{
    static uint32_t lastPrint = 0;
    uint32_t now = millis();
    if (now - lastPrint >= 5000) {
        lastPrint = now;
        Serial.printf("[IDLE] %lus\n", now / 1000);
    }
    delay(50);
}
