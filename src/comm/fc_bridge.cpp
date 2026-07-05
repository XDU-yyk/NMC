/**
 * @file    fc_bridge.cpp
 * @brief   飞控桥接层实现 — F4V3S PLUS 通过 MSP 协议通信
 */

#include "comm/fc_bridge.h"
#include "comm/fc_rc_mapping.h"

FCBridge fcBridge;

namespace {
constexpr uint32_t FC_OFFLINE_STATUS_POLL_MS = 500;
}

void FCBridge::begin() {
    memset(&m_state, 0, sizeof(m_state));
    memset(&m_lastOutput, 0, sizeof(m_lastOutput));
    m_outputDiag = FCOutputDiag{};
    m_lastOutput.roll = FC_RC_MID;
    m_lastOutput.pitch = FC_RC_MID;
    m_lastOutput.yaw = FC_RC_MID;
    m_lastOutput.throttle = 1000;
    m_lastOutput.aux1 = FC_RC_MID;
    m_lastOutput.aux2 = FC_RC_MID;
    m_lastOutput.overrideRC = false;
    m_lastOutputUpdate = 0;

    // 初始化 MSP 协议层，连接飞控 UART
    HardwareSerial* fcSerial = &Serial2;  // FC_UART_NUM = 2
    m_msp.begin(*fcSerial, FC_BAUD, FC_RX_PIN, FC_TX_PIN);

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

    // Keep the Web/camera loop responsive during bring-up. A reversed or
    // disconnected UART can make each MSP request wait for its timeout, so
    // offline mode only probes status at a low rate; online mode reads at most
    // one data block per update() call.
    if (!isOnline()) {
        if (now - m_lastReadStatus >= FC_OFFLINE_STATUS_POLL_MS) {
            pollStatus();
            m_lastReadStatus = now;
        }
    } else if (now - m_lastReadStatus >= 200) {
        pollStatus();
        m_lastReadStatus = now;
    } else if (now - m_lastReadRC >= 30) {
        pollRC();
        m_lastReadRC = now;
    } else if (now - m_lastReadBattery >= 500) {
        pollBattery();
        m_lastReadBattery = now;
    } else if (now - m_lastReadAltitude >= 20) {
        pollAltitude();
        m_lastReadAltitude = now;
    } else if (now - m_lastReadAttitude >= 10) {
        pollAttitude();
        m_lastReadAttitude = now;
    }

    // 发送控制指令 (按需, 最大 50Hz)
    const bool outputFresh = (now - m_lastOutputUpdate) <= REAL_FC_OUTPUT_HOLD_MS;
    if (!outputFresh) {
        if (m_lastOutput.overrideRC) {
            m_outputDiag.staleBlocks++;
            m_outputDiag.lastReason = "stale";
        }
        m_lastOutput.overrideRC = false;
    }

    if (m_lastOutput.overrideRC && outputFresh && (now - m_lastSendRC >= 20)) {
#if ENABLE_REAL_FC_OUTPUT
        if (isAssistGateOpen()) {
            sendRawRC(m_lastOutput);
        } else {
            m_outputDiag.gateBlocks++;
            m_outputDiag.lastReason = "gate";
            m_lastSendRC = now;
        }
#else
        m_outputDiag.gateBlocks++;
        m_outputDiag.lastReason = "compile_gate";
        m_lastSendRC = now;
#endif
    }
}

void FCBridge::setOutput(const FCOutput& out) {
    m_lastOutput = out;
    m_outputDiag.setOutputCalls++;
    m_outputDiag.lastSetMs = millis();

    if (!out.overrideRC) {
        m_outputDiag.clearRequests++;
        m_outputDiag.lastReason = "clear";
    } else {
        m_outputDiag.overrideRequests++;
        m_outputDiag.lastReason = "queued";
    }

    // Queue the request; update() is the single 50Hz send/block path.
    m_lastOutputUpdate = m_outputDiag.lastSetMs;
}

bool FCBridge::isOnline() const {
    return m_state.valid && (millis() - m_state.lastUpdate) < FC_MSP_TIMEOUT_MS * 5;
}

bool FCBridge::arm() {
#if ENABLE_ESP32_ARM_DISARM
    bool ok = m_msp.sendArmCommand(true);
    if (ok) m_state.armed = true;
    return ok;
#else
    LOG(LOG_TAG_FC, "arm blocked: ESP32 arming disabled");
    return false;
#endif
}

bool FCBridge::disarm() {
#if ENABLE_ESP32_ARM_DISARM
    bool ok = m_msp.sendArmCommand(false);
    if (ok) m_state.armed = false;
    return ok;
#else
    LOG(LOG_TAG_FC, "disarm blocked: ESP32 disarming disabled");
    return false;
#endif
}

/* ── 分时读取子函数 ── */

void FCBridge::pollAttitude() {
    float r, p, y;
    if (m_msp.readAttitude(r, p, y)) {
        m_state.roll   = r;
        m_state.pitch  = p;
        m_state.yaw    = y;
        m_state.valid = true;
        m_state.lastUpdate = millis();
    }
}

void FCBridge::pollAltitude() {
    float alt, vario;
    if (m_msp.readAltitude(alt, vario)) {
        m_state.altitude = alt;
        m_state.vario    = vario;
        m_state.valid = true;
        m_state.lastUpdate = millis();
    }
}

void FCBridge::pollBattery() {
    if (m_msp.readBattery(m_state.batteryCells, m_state.batteryVoltage)) {
        m_state.valid = true;
        m_state.lastUpdate = millis();
    }
}

void FCBridge::pollRC() {
    if (m_msp.readRC(m_state.rcChannels, m_state.rcChannelCount)) {
        m_state.valid = true;
        m_state.lastUpdate = millis();
    }
}

void FCBridge::pollStatus() {
    uint16_t ct;
    uint8_t  af;
    if (m_msp.readStatus(ct, af)) {
        m_state.cycleTime = ct;
        m_state.armed     = (af & 0x01);
        m_state.valid = true;
        m_state.lastUpdate = millis();
    }
}

bool FCBridge::isAssistGateOpen() const {
    return isAssistGateOpenFromRC(isOnline(),
                                  m_state.armed,
                                  m_state.rcChannels,
                                  m_state.rcChannelCount,
                                  REAL_FC_ASSIST_AUX_CHANNEL,
                                  REAL_FC_ASSIST_AUX_MIN);
}

void FCBridge::sendRawRC(const FCOutput& out) {
    uint16_t ch[16];
    FCRawRCValues values;
    values.roll = out.roll;
    values.pitch = out.pitch;
    values.yaw = out.yaw;
    values.throttle = out.throttle;
    values.aux1 = out.aux1;
    values.aux2 = out.aux2;
    buildBetaflightRawRC(values, ch, FC_RC_MID);
    m_outputDiag.rawRcAttempts++;
    m_outputDiag.lastRoll = ch[FC_RAW_RC_ROLL];
    m_outputDiag.lastPitch = ch[FC_RAW_RC_PITCH];
    m_outputDiag.lastYaw = ch[FC_RAW_RC_YAW];
    m_outputDiag.lastThrottle = ch[FC_RAW_RC_THROTTLE];
    m_outputDiag.lastAux1 = ch[FC_RAW_RC_AUX1];
    m_outputDiag.lastAux2 = ch[FC_RAW_RC_AUX2];
    const bool ok = m_msp.setRawRC(ch);
    if (ok) {
        m_outputDiag.rawRcOk++;
        m_outputDiag.lastReason = "sent";
    } else {
        m_outputDiag.rawRcFail++;
        m_outputDiag.lastReason = "write_fail";
    }
    m_lastSendRC = millis();
    m_outputDiag.lastSendMs = m_lastSendRC;
}
