/**
 * @file    fc_bridge.cpp
 * @brief   飞控桥接层实现 — F4V3S PLUS 通过 MSP 协议通信
 */

#include "comm/fc_bridge.h"

FCBridge fcBridge;

void FCBridge::begin() {
    memset(&m_state, 0, sizeof(m_state));
    memset(&m_lastOutput, 0, sizeof(m_lastOutput));

    // 初始化 MSP 协议层，连接飞控 UART
    HardwareSerial* fcSerial = &Serial2;  // FC_UART_NUM = 2
    m_msp.begin(*fcSerial, FC_BAUD);

    LOG(LOG_TAG_FC, "FC Bridge init — UART%d @ %d baud", FC_UART_NUM, FC_BAUD);

    // 初始化解锁状态
    uint16_t ct;
    uint8_t af;
    if (m_msp.readStatus(ct, af)) {
        m_state.armed     = (af & 0x01);
        m_state.cycleTime = ct;
        m_state.valid     = true;
        m_state.lastUpdate = millis();
        LOG(LOG_TAG_FC, "FC online — armed=%d, cycle=%dus", m_state.armed, ct);
    } else {
        LOG(LOG_TAG_FC, "WARNING: FC not responding, check UART wiring");
    }
}

void FCBridge::update() {
    uint32_t now = millis();

    // 分时读取各数据块，避免单次 poll 阻塞太久
    if (now - m_lastReadAttitude >= 10) { pollAttitude();  m_lastReadAttitude = now; }
    if (now - m_lastReadAltitude >= 20) { pollAltitude();  m_lastReadAltitude = now; }
    if (now - m_lastReadBattery  >= 500) { pollBattery();  m_lastReadBattery  = now; }
    if (now - m_lastReadRC       >= 30)  { pollRC();       m_lastReadRC       = now; }
    if (now - m_lastReadStatus   >= 200) { pollStatus();   m_lastReadStatus   = now; }

    // 发送控制指令 (按需, 最大 50Hz)
    if (m_lastOutput.overrideRC && (now - m_lastSendRC >= 20)) {
        uint16_t ch[16] = {0};
        ch[0] = m_lastOutput.roll;
        ch[1] = m_lastOutput.pitch;
        ch[2] = m_lastOutput.throttle;
        ch[3] = m_lastOutput.yaw;
        ch[4] = m_lastOutput.aux1;
        ch[5] = m_lastOutput.aux2;
        m_msp.setRawRC(ch);
        m_lastSendRC = now;
    }
}

void FCBridge::setOutput(const FCOutput& out) {
    m_lastOutput = out;
    // 立即发送一次，后续由 update() 定时续发
    if (out.overrideRC) {
        uint16_t ch[16] = {0};
        ch[0] = out.roll;
        ch[1] = out.pitch;
        ch[2] = out.throttle;
        ch[3] = out.yaw;
        ch[4] = out.aux1;
        ch[5] = out.aux2;
        m_msp.setRawRC(ch);
        m_lastSendRC = millis();
    }
}

bool FCBridge::isOnline() const {
    return (millis() - m_state.lastUpdate) < FC_MSP_TIMEOUT_MS * 3;
}

bool FCBridge::arm() {
    bool ok = m_msp.sendArmCommand(true);
    if (ok) m_state.armed = true;
    return ok;
}

bool FCBridge::disarm() {
    bool ok = m_msp.sendArmCommand(false);
    if (ok) m_state.armed = false;
    return ok;
}

/* ── 分时读取子函数 ── */

void FCBridge::pollAttitude() {
    float r, p, y;
    if (m_msp.readAttitude(r, p, y)) {
        m_state.roll   = r;
        m_state.pitch  = p;
        m_state.yaw    = y;
        m_state.lastUpdate = millis();
    }
}

void FCBridge::pollAltitude() {
    float alt, vario;
    if (m_msp.readAltitude(alt, vario)) {
        m_state.altitude = alt;
        m_state.vario    = vario;
    }
}

void FCBridge::pollBattery() {
    if (m_msp.readBattery(m_state.batteryCells, m_state.batteryVoltage)) {
        // OK
    }
}

void FCBridge::pollRC() {
    m_msp.readRC(m_state.rcChannels, m_state.rcChannelCount);
}

void FCBridge::pollStatus() {
    uint16_t ct;
    uint8_t  af;
    if (m_msp.readStatus(ct, af)) {
        m_state.cycleTime = ct;
        m_state.armed     = (af & 0x01);
    }
}
