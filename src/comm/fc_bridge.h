/**
 * @file    fc_bridge.h
 * @brief   飞控桥接层 — 封装 F4V3S PLUS 飞控数据读写
 * 
 * 功能: 
 *   - 定时读取飞控姿态/高度/电池/RC 状态
 *   - 通过虚拟 RC 通道控制飞控 (位置 → MSP_SET_RAW_RC)
 *   - 飞控连接健康检测
 */

#ifndef FC_BRIDGE_H
#define FC_BRIDGE_H

#include "config.h"
#include "comm/msp.h"
#include <Arduino.h>

/* ── 飞控完整状态 (一次读全部) ── */
struct FCState {
    float    roll, pitch, yaw;         // 姿态 (°)
    float    altitude;                 // 气压高度 (cm)
    float    vario;                    // 升降速率 (cm/s)
    int16_t  accRaw[3], gyroRaw[3];    // 原始 IMU
    float    batteryVoltage;           // 电池电压 (V)
    uint8_t  batteryCells;            // 推算电芯数

    uint16_t rcChannels[16];
    uint8_t  rcChannelCount;

    uint16_t cycleTime;               // 飞控循环周期 (us)
    bool     armed;                   // 解锁状态

    bool     valid;                   // 数据有效标志
    uint32_t lastUpdate;              // 最后更新时间 (ms)
};

/* ── 控制输出 (ESP32 → 飞控) ── */
struct FCOutput {
    uint16_t roll;        // 1000-2000, 中值 1500
    uint16_t pitch;
    uint16_t throttle;
    uint16_t yaw;
    uint16_t aux1;        // 飞行模式开关
    uint16_t aux2;
    bool     overrideRC;  // 是否接管 RC 控制
};

class FCBridge {
public:
    void begin();

    /* 轮询: 读取飞控最新数据 (每次调用只读一部分, 避免阻塞) */
    void update();

    /* 获取缓存的飞控状态 */
    const FCState& getState() const { return m_state; }

    /* 发送控制指令到飞控 */
    void setOutput(const FCOutput& out);

    /* 飞控是否在线 (最近 N ms 内收到数据) */
    bool isOnline() const;

    /* 解锁/上锁飞控 */
    bool arm();
    bool disarm();

    /* 底层 MSP 接口 (供高级控制使用) */
    MSP& getMSP() { return m_msp; }

private:
    MSP     m_msp;
    FCState m_state;
    FCOutput m_lastOutput;

    uint32_t m_lastReadAttitude  = 0;
    uint32_t m_lastReadAltitude  = 0;
    uint32_t m_lastReadBattery   = 0;
    uint32_t m_lastReadRC        = 0;
    uint32_t m_lastReadStatus    = 0;
    uint32_t m_lastSendRC        = 0;

    /* 分时读取子函数 */
    void pollAttitude();
    void pollAltitude();
    void pollBattery();
    void pollRC();
    void pollStatus();
};

extern FCBridge fcBridge;

#endif // FC_BRIDGE_H
