/**
 * @file    msp.h
 * @brief   MSP (MultiWii Serial Protocol) 协议层 v2.0
 * 
 * 用于 ESP32-S3 与 F4V3S PLUS 飞控之间的串口通信。
 * 支持 Betaflight / iNav 常用 MSP 消息子集。
 */

#ifndef MSP_H
#define MSP_H

#include <Arduino.h>

/* ── MSP 消息帧结构 ── */
#define MSP_HEADER_PREAMBLE     '$'
#define MSP_HEADER_DIR_TO_FC    '>'
#define MSP_HEADER_DIR_FROM_FC  '<'

#define MSP_MAX_PAYLOAD         64

/* ── MSP 消息 ID (常用子集) ── */
enum MSP_CMD : uint16_t {
    MSP_FC_VERSION          = 1,
    MSP_RAW_IMU             = 102,  // 9× int16: acc[3], gyro[3], mag[3]
    MSP_ATTITUDE            = 108,  // 2× int16: roll, pitch (0.1°), int16 heading
    MSP_ALTITUDE            = 109,  // int32 alt_cm, int16 vario cm/s
    MSP_ANALOG              = 110,  // uint8 vbat, uint16 mAh, uint16 rssi, uint8 amperage
    MSP_RC                  = 105,  // uint16 channels[16]
    MSP_SET_RAW_RC          = 200,  // uint16 channels[16] — 写指令
    MSP_STATUS              = 101,  // uint16 cycleTime, ... uint8 armingFlags, ...
    MSP_ARMING_CONFIG       = 61,
    MSP_SET_ARMING_CONFIG   = 158,
    MSP_RAW_GPS             = 106,  // GPS 原始数据
    MSP_ACC_TRIM            = 240,  // 加速度计微调
    MSP_SET_ACC_TRIM        = 239,
    MSP_SENSOR_ALIGNMENT    = 104,
    MSP_BOXIDS              = 119,  // 飞行模式列表
    MSP_BOX                 = 120,  // 当前激活的飞行模式
};

/* ── MSP 帧 ── */
struct MSPFrame {
    uint8_t  dir;           // MSP_HEADER_DIR_TO_FC or FROM_FC
    uint8_t  cmd;
    uint8_t  size;
    uint8_t  payload[MSP_MAX_PAYLOAD];
    uint8_t  checksum;
    bool     valid;
};

/* ── API ── */
class MSP {
public:
    void begin(HardwareSerial& serial, uint32_t baud);

    /* 发送命令 (写) */
    bool sendCommand(uint8_t cmd, const uint8_t* payload, uint8_t size);

    /* 请求 + 等待响应 (读) */
    bool request(uint8_t cmd, MSPFrame& frame, uint32_t timeoutMs = 100);

    /* 便捷读取函数 */
    bool readAttitude(float& roll, float& pitch, float& yaw);
    bool readAltitude(float& altCm, float& varioCmS);
    bool readBattery(uint8_t& cells, float& voltage);
    bool readRC(uint16_t channels[16], uint8_t& count);
    bool readIMU(int16_t acc[3], int16_t gyro[3]);
    bool readStatus(uint16_t& cycleTime, uint8_t& armingFlags);

    /* 写指令 */
    bool setRawRC(const uint16_t channels[16]);
    bool sendArmCommand(bool arm);

private:
    HardwareSerial* m_serial = nullptr;

    uint8_t calcChecksum(const MSPFrame& frame);
    bool    readFrame(MSPFrame& frame, uint32_t timeoutMs);
    bool    expectResponse(uint8_t cmd, MSPFrame& frame, uint32_t timeoutMs);

    /* 反抖动读帧 */
    bool waitForFrame(uint32_t timeoutMs);
};

#endif // MSP_H
