/**
 * @file    fc_diag_main.cpp
 * @brief   F4V3S 飞控 MSP 只读诊断固件
 * 
 * - 不启动 WiFi/ToF/GPS
 * - 只启动 Serial + FC UART (Serial2)
 * - 每 1 秒轮询所有 MSP 命令
 * - 每 5 秒打印 MSP 诊断统计
 * - 全程不发送写指令
 */

#include <Arduino.h>
#include "config.h"
#include "comm/msp.h"
#include "comm/fc_bridge.h"

static uint32_t g_startTime = 0;
static uint32_t g_loopCount = 0;

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  F4V3S Flight Controller MSP Diagnostic");
    Serial.println("  Read-only · No WiFi · No ToF · No GPS");
    Serial.println("==============================================");

    // FC Bridge 内部会初始化 MSP + Serial2
    Serial.printf("FC UART: Serial2 @ %d baud, RX=%d TX=%d\n",
        FC_BAUD, FC_RX_PIN, FC_TX_PIN);
    Serial.println("  F4V3S T3/TX3 -> ESP32 GPIO17 (RX)");
    Serial.println("  F4V3S R3/RX3 -> ESP32 GPIO16 (TX)");
    Serial.println("  UART6 R6/T6 reserved for CRSF receiver");

    fcBridge.begin();

    g_startTime = millis();
    Serial.println();
}

void loop()
{
    uint32_t now = millis();
    g_loopCount++;

    // 每 1 秒轮询一次
    static uint32_t lastPoll = 0;
    if (now - lastPoll >= 1000) {
        lastPoll = now;

        fcBridge.update();

        const auto& fc = fcBridge.getState();
        const auto& diag = fcBridge.getMSPDiag();

        bool online = fcBridge.isOnline();

        if (online) {
            // 打印一行完整状态
            Serial.printf("[FC] online=%d armed=%d roll=%.1f pitch=%.1f yaw=%.1f "
                "alt=%.0f vbat=%.2f rc=%u,%u,%u,%u\n",
                online, fc.armed,
                fc.roll, fc.pitch, fc.yaw,
                fc.altitude, fc.batteryVoltage,
                fc.rcChannels[0], fc.rcChannels[1],
                fc.rcChannels[2], fc.rcChannels[3]);
        } else {
            // 每 3 秒提示一次 offline（避免刷屏）
            static uint32_t lastOfflinePrint = 0;
            if (now - lastOfflinePrint >= 3000) {
                lastOfflinePrint = now;
                Serial.printf("[FC] offline — check wiring: TX→17 RX→16 GND→GND + FC power\n");
                Serial.printf("      MSP: txF=%lu txB=%lu rxB=%lu $=%lu $M=%lu ok=%lu timeout=%lu fErr=%lu lastCmd=%u lastRx=0x%02X err=%s\n",
                    diag.txFrames, diag.txBytes, diag.rxBytes,
                    diag.rxDollarCount, diag.rxHeaderCount,
                    diag.responseOk, diag.timeoutCount,
                    diag.frameError, diag.lastCmd, diag.lastRxByte, diag.lastError);
            }
        }

        // 每 5 秒打印 MSP 诊断
        if (g_loopCount % 5 == 0 && online) {
            Serial.printf("[MSP] txF=%lu txB=%lu rxB=%lu $=%lu $M=%lu "
                "ok=%lu timeout=%lu ckFail=%lu fErr=%lu lastCmd=%u lastRx=0x%02X err=%s\n",
                diag.txFrames, diag.txBytes, diag.rxBytes,
                diag.rxDollarCount, diag.rxHeaderCount,
                diag.responseOk, diag.timeoutCount,
                diag.checksumFail, diag.frameError,
                diag.lastCmd, diag.lastRxByte, diag.lastError);
        }
        // 第 3 秒也打一次（用于 offline 时能看到 rx 统计）
        if (g_loopCount % 3 == 0) {
            Serial.printf("[MSP] txF=%lu txB=%lu rxB=%lu $=%lu $M=%lu "
                "ok=%lu timeout=%lu ckFail=%lu fErr=%lu lastCmd=%u lastRx=0x%02X err=%s\n",
                diag.txFrames, diag.txBytes, diag.rxBytes,
                diag.rxDollarCount, diag.rxHeaderCount,
                diag.responseOk, diag.timeoutCount,
                diag.checksumFail, diag.frameError,
                diag.lastCmd, diag.lastRxByte, diag.lastError);
        }

        // 每 30 秒打印系统摘要
        if (g_loopCount % 30 == 0) {
            Serial.printf("[SYS] uptime=%lus loops=%lu\n",
                (now - g_startTime) / 1000, g_loopCount);
        }
    }

    delay(10);
}
