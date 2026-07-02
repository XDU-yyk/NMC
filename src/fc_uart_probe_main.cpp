/**
 * @file fc_uart_probe_main.cpp
 * @brief Raw UART6/MSP probe for F4V3S R6/T6 debugging.
 *
 * Sends read-only MSP v1 request frames on ESP32 Serial2 and dumps raw bytes
 * received from the flight controller. No WiFi, sensors, Web, or control output.
 */

#include <Arduino.h>
#include "config.h"
#include "comm/msp.h"

struct ProbeCommand {
    uint8_t cmd;
    const char* name;
};

static const ProbeCommand kCommands[] = {
    {MSP_FC_VERSION, "FC_VERSION"},
    {MSP_STATUS, "STATUS"},
    {MSP_ATTITUDE, "ATTITUDE"},
    {MSP_ALTITUDE, "ALTITUDE"},
    {MSP_ANALOG, "ANALOG"},
    {MSP_RC, "RC"},
};

static uint8_t checksumFor(uint8_t size, uint8_t cmd, const uint8_t* payload)
{
    uint8_t c = size ^ cmd;
    for (uint8_t i = 0; i < size; i++) c ^= payload[i];
    return c;
}

static void buildRequest(uint8_t cmd, uint8_t* out, uint8_t& len)
{
    len = 0;
    out[len++] = '$';
    out[len++] = 'M';
    out[len++] = '<';
    out[len++] = 0;
    out[len++] = cmd;
    out[len++] = checksumFor(0, cmd, nullptr);
}

static void printHexByte(uint8_t b)
{
    if (b < 16) Serial.print('0');
    Serial.print(b, HEX);
}

static void printFrame(const uint8_t* data, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++) {
        printHexByte(data[i]);
        if (i + 1 < len) Serial.print(' ');
    }
}

static size_t dumpRxFor(uint32_t windowMs)
{
    uint32_t start = millis();
    size_t count = 0;
    while (millis() - start < windowMs) {
        while (Serial2.available() > 0) {
            uint8_t b = (uint8_t)Serial2.read();
            if (count == 0) Serial.print(" rx=");
            printHexByte(b);
            Serial.print(' ');
            count++;
        }
        delay(1);
    }
    return count;
}

void setup()
{
    Serial.begin(DEBUG_BAUD);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  F4V3S UART6 Raw MSP Probe");
    Serial.println("  Read-only MSP requests, raw RX byte dump");
    Serial.println("==============================================");
    Serial.printf("ESP32 Serial2: baud=%d RX=%d TX=%d\n", FC_BAUD, FC_RX_PIN, FC_TX_PIN);
    Serial.println("Wire: F4 T6/TX6 -> ESP32 RX17, F4 R6/RX6 -> ESP32 TX16, GND direct.");
    Serial.println("MSP request examples for USB-TTL serial assistant:");

    for (const auto& item : kCommands) {
        uint8_t frame[6];
        uint8_t len = 0;
        buildRequest(item.cmd, frame, len);
        Serial.printf("  %-10s cmd=%3u hex=", item.name, item.cmd);
        printFrame(frame, len);
        Serial.println();
    }
    Serial.println();

    Serial2.begin(FC_BAUD, SERIAL_8N1, FC_RX_PIN, FC_TX_PIN);
}

void loop()
{
    static uint8_t index = 0;
    static uint32_t round = 0;

    const ProbeCommand& item = kCommands[index];
    uint8_t frame[6];
    uint8_t len = 0;
    buildRequest(item.cmd, frame, len);

    Serial.printf("[PROBE] round=%lu cmd=%s(%u) tx=", round, item.name, item.cmd);
    printFrame(frame, len);

    Serial2.write(frame, len);
    Serial2.flush();

    size_t rxCount = dumpRxFor(250);
    if (rxCount == 0) Serial.print(" rx=<none>");
    Serial.println();

    index = (index + 1) % (sizeof(kCommands) / sizeof(kCommands[0]));
    if (index == 0) round++;
    delay(750);
}
