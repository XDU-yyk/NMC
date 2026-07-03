/**
 * @file    fc_uart_probe_main.cpp
 * @brief   F4V3S UART3 MSP 探测固件
 * 
 * - 不启动 WiFi/ToF/GPS
 * - 只发一条 MSP 请求并显示原始 HEX 回包
 * - 验证 ESP32→F4V3S R3→T3→ESP32 链路
 */

#include <Arduino.h>
#include "config.h"

static HardwareSerial* fcSerial = &Serial2;
static const uint8_t MSP_PROBE[] = {0x24, 0x4D, 0x3C, 0x00, 0x01, 0x01}; // $M< + size=0 + cmd=1(FC_VERSION) + chk=1

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  F4V3S UART3 MSP Probe");
    Serial.println("  No WiFi · No ToF · No GPS");
    Serial.println("==============================================");
    Serial.println();
    Serial.println("Wiring (verify before powering):");
    Serial.println("  F4V3S T3/TX3 -> ESP32 GPIO17 (RX)");
    Serial.println("  F4V3S R3/RX3 -> ESP32 GPIO16 (TX)");
    Serial.println("  F4V3S GND    -> ESP32 GND");
    Serial.println();

    fcSerial->begin(FC_BAUD, SERIAL_8N1, FC_RX_PIN, FC_TX_PIN);

    Serial.printf("Serial2: baud=%d, RX=%d, TX=%d\n", FC_BAUD, FC_RX_PIN, FC_TX_PIN);
    Serial.println();
    Serial.println("Sending MSP probe (FC_VERSION)...");
    Serial.println();

    delay(200);

    // 发探测帧
    fcSerial->write(MSP_PROBE, sizeof(MSP_PROBE));

    // 打印发送 HEX
    Serial.print("TX: ");
    for (size_t i = 0; i < sizeof(MSP_PROBE); i++) {
        Serial.printf("%02X ", MSP_PROBE[i]);
    }
    Serial.println();

    // 等 500ms 收回复
    delay(500);

    uint8_t buf[128];
    size_t len = 0;

    while (fcSerial->available() > 0 && len < sizeof(buf)) {
        buf[len++] = fcSerial->read();
    }

    if (len > 0) {
        Serial.print("RX: ");
        for (size_t i = 0; i < len; i++) {
            Serial.printf("%02X ", buf[i]);
        }
        Serial.println();

        // 判断是否以 $M> 开头
        if (len >= 3 && buf[0] == 0x24 && buf[1] == 0x4D && buf[2] == 0x3E) {
            Serial.println();
            Serial.println("*** UART3 MSP LINK ALIVE! ***");
            Serial.println("Proceed to fc-diag.");
        } else {
            Serial.println();
            Serial.println("Data received but not a valid MSP reply.");
            Serial.println("Check: Betaflight UART3 MSP config, wiring, GND.");
        }
    } else {
        Serial.println("RX: (no response)");
        Serial.println();
        Serial.println("*** UART3 SILENT ***");
        Serial.println("1. Check Betaflight CLI: serial 2 1 115200 57600 0 115200");
        Serial.println("2. Verify T3->GPIO17, R3->GPIO16, GND connected");
        Serial.println("3. Verify FC has power (USB or battery)");
    }

    Serial.println();
    Serial.println("Probe complete. Press RST to resend.");
}

void loop() {
    delay(10000);
}
