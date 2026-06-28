/**
 * @file    gps_diag_main.cpp
 * @brief   GPS 诊断固件 — 不启动 WiFi/ToF，纯 NMEA 分析
 * 
 * 功能:
 * - 打印前 20 条原始 NMEA 行
 * - 每 5 秒打印 TinyGPSPlus 统计
 * - 波特率探测: 9600→38400→57600→115200 各跑 10 秒
 * 
 * 编译: pio run -e esp32-s3-gps-diag -t upload
 */

#include <Arduino.h>
#include <TinyGPSPlus.h>
#include "config.h"

static TinyGPSPlus gpsEngine;
static HardwareSerial* gpsSerial = &Serial1;

static uint32_t g_chars    = 0;
static uint32_t g_sentences = 0;
static uint32_t g_baud     = GPS_BAUD;

static void printStats(uint32_t now) {
    Serial.println();
    Serial.printf("=== Stats @ baud=%u, runtime=%lus ===\n",
        g_baud, now / 1000);
    Serial.printf("  chars:         %u\n", g_chars);
    Serial.printf("  passedChecksum: %lu\n", gpsEngine.passedChecksum());
    Serial.printf("  failedChecksum: %lu\n", gpsEngine.failedChecksum());
    Serial.printf("  sentencesFix:  %lu\n", gpsEngine.sentencesWithFix());
    Serial.printf("  satellites:    %u\n", gpsEngine.satellites.value());
    Serial.printf("  hdop:          %.1f\n", gpsEngine.hdop.hdop());
    Serial.printf("  valid:         %u\n", gpsEngine.location.isValid());
    Serial.printf("  lat/lng:       %.6f / %.6f\n",
        gpsEngine.location.lat(), gpsEngine.location.lng());
    Serial.printf("  alt:           %.1f m\n", gpsEngine.altitude.meters());
    Serial.printf("  speed:         %.1f km/h\n", gpsEngine.speed.kmph());
    Serial.printf("  age:           %lu ms\n", gpsEngine.location.age());
}

static void testBaud(uint32_t baud, const char* label) {
    g_baud = baud;
    g_chars = 0;
    gpsEngine = TinyGPSPlus();

    Serial.printf("\n--- Testing %s (%u baud) ---\n", label, baud);
    gpsSerial->begin(baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    uint32_t start = millis();
    while (millis() - start < 10000) {
        while (gpsSerial->available() > 0) {
            char c = gpsSerial->read();
            gpsEngine.encode(c);
            g_chars++;
        }
        delay(1);
    }

    printStats(millis());
    Serial.printf("  >> passedChecksum=%lu  (higher = better baud)\n",
        gpsEngine.passedChecksum());
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  GPS Diagnostic Tool");
    Serial.printf("  GPS pins: RX=%d TX=%d\n", GPS_RX_PIN, GPS_TX_PIN);
    Serial.println("==============================================");

    // --- 阶段 1: 当前配置测试 ---
    g_baud = GPS_BAUD;
    Serial.printf("\n--- Phase 1: Current config (%u baud) ---\n", GPS_BAUD);
    gpsSerial->begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    Serial.println("Raw NMEA (first 20 lines):");
    Serial.println("------------------------");
    char line[256];
    int lineIdx = 0;
    int lineCount = 0;
    uint32_t phaseStart = millis();

    while (lineCount < 20 && millis() - phaseStart < 30000) {
        while (gpsSerial->available() > 0 && lineCount < 20) {
            char c = gpsSerial->read();
            gpsEngine.encode(c);
            g_chars++;
            g_sentences = gpsEngine.sentencesWithFix();

            if (c == '\n') {
                if (lineIdx > 0) {
                    line[lineIdx] = 0;
                    // 只看 $G 开头的
                    if (line[0] == '$' && line[1] == 'G') {
                        Serial.println(line);
                        lineCount++;
                    }
                }
                lineIdx = 0;
            } else if (lineIdx < 250 && c != '\r') {
                line[lineIdx++] = c;
            }
        }
        delay(5);
    }
    Serial.println("------------------------");

    printStats(millis());

    // --- 阶段 2: 波特率探测 ---
    Serial.println("\n--- Phase 2: Baud rate scan ---");
    gpsSerial->end();

    testBaud(9600, "9600");
    gpsSerial->end();
    testBaud(38400, "38400");
    gpsSerial->end();
    testBaud(57600, "57600");
    gpsSerial->end();
    testBaud(115200, "115200");

    Serial.println("\n=== Done ===");
    Serial.println("Recommendation: use the baud rate with highest passedChecksum");
}

void loop() {
    delay(30000);
}
