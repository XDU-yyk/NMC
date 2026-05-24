/**
 * @file    msp.cpp
 * @brief   MSP 协议实现 — 二进制帧编解码
 */

#include "comm/msp.h"
#include "config.h"

void MSP::begin(HardwareSerial& serial, uint32_t baud) {
    m_serial = &serial;
    m_serial->begin(baud, SERIAL_8N1);
}

/* ── 计算 XOR 校验和 ── */
uint8_t MSP::calcChecksum(const MSPFrame& frame) {
    uint8_t c = 0;
    c ^= frame.size;
    c ^= frame.cmd;
    for (uint8_t i = 0; i < frame.size; i++) {
        c ^= frame.payload[i];
    }
    return c;
}

/* ── 发送命令帧 ── */
bool MSP::sendCommand(uint8_t cmd, const uint8_t* payload, uint8_t size) {
    if (!m_serial) return false;

    MSPFrame frame;
    frame.cmd  = cmd;
    frame.size = size;
    frame.dir  = MSP_HEADER_DIR_TO_FC;
    if (payload && size) memcpy(frame.payload, payload, size);
    frame.checksum = calcChecksum(frame);

    m_serial->write(MSP_HEADER_PREAMBLE);
    m_serial->write('M');
    m_serial->write(frame.dir);
    m_serial->write(frame.size);
    m_serial->write(frame.cmd);
    if (size) m_serial->write(frame.payload, size);
    m_serial->write(frame.checksum);

    return true;
}

/* ── 请求命令 + 等待响应 ── */
bool MSP::request(uint8_t cmd, MSPFrame& frame, uint32_t timeoutMs) {
    // 发送空载荷请求
    if (!sendCommand(cmd, nullptr, 0)) return false;
    // 等待飞控回包
    return expectResponse(cmd, frame, timeoutMs);
}

/* ── 等待飞控响应 ── */
bool MSP::expectResponse(uint8_t cmd, MSPFrame& frame, uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (readFrame(frame, 0)) {
            if (frame.cmd == cmd && frame.dir == MSP_HEADER_DIR_FROM_FC) {
                return true;
            }
            // 消息不匹配，继续等待
        }
    }
    return false;
}

/* ── 从串口读一帧 ── */
bool MSP::readFrame(MSPFrame& frame, uint32_t timeoutMs) {
    if (!m_serial || !m_serial->available()) return false;

    // 找帧头 '$M'
    while (m_serial->read() != '$') {
        if (timeoutMs && (millis() > timeoutMs)) return false;
        if (!m_serial->available()) return false;
    }
    if (m_serial->read() != 'M') return false;  // 第二个头字节

    frame.dir = m_serial->read();
    if (frame.dir != MSP_HEADER_DIR_FROM_FC && frame.dir != MSP_HEADER_DIR_TO_FC)
        return false;

    frame.size = m_serial->read();
    if (frame.size > MSP_MAX_PAYLOAD) return false;

    frame.cmd = m_serial->read();

    // 读载荷
    for (uint8_t i = 0; i < frame.size; i++) {
        frame.payload[i] = m_serial->read();
    }

    frame.checksum = m_serial->read();
    frame.valid = (frame.checksum == calcChecksum(frame));

    return frame.valid;
}

/* ── 等待帧就绪 (不清空缓冲区) ── */
bool MSP::waitForFrame(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (m_serial && m_serial->available()) return true;
        delay(1);
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════
 *  便捷读取函数
 * ═══════════════════════════════════════════════════════════ */

bool MSP::readAttitude(float& roll, float& pitch, float& yaw) {
    MSPFrame f;
    if (!request(MSP_ATTITUDE, f)) return false;
    if (f.size < 6) return false;

    int16_t r = (int16_t)(f.payload[0] | (f.payload[1] << 8));
    int16_t p = (int16_t)(f.payload[2] | (f.payload[3] << 8));
    int16_t h = (int16_t)(f.payload[4] | (f.payload[5] << 8));

    roll  = r * 0.1f;   // 0.1° 分辨率
    pitch = p * 0.1f;
    yaw   = h;
    return true;
}

bool MSP::readAltitude(float& altCm, float& varioCmS) {
    MSPFrame f;
    if (!request(MSP_ALTITUDE, f)) return false;
    if (f.size < 6) return false;

    int32_t alt   = (int32_t)(f.payload[0] | (f.payload[1] << 8) |
                              (f.payload[2] << 16) | (f.payload[3] << 24));
    int16_t vario = (int16_t)(f.payload[4] | (f.payload[5] << 8));

    altCm    = (float)alt;
    varioCmS = (float)vario;
    return true;
}

bool MSP::readBattery(uint8_t& cells, float& voltage) {
    MSPFrame f;
    if (!request(MSP_ANALOG, f)) return false;
    if (f.size < 9) return false;

    uint8_t vbatRaw = f.payload[0];         // 0-255 → 0-25.5V
    voltage = vbatRaw * 0.1f;
    cells = roundf(voltage / BATTERY_CELL_MAX);  // 推断电芯数
    return true;
}

bool MSP::readRC(uint16_t channels[16], uint8_t& count) {
    MSPFrame f;
    if (!request(MSP_RC, f)) return false;

    count = f.size / 2;
    if (count > 16) count = 16;

    for (uint8_t i = 0; i < count; i++) {
        channels[i] = (uint16_t)(f.payload[i * 2] | (f.payload[i * 2 + 1] << 8));
    }
    return true;
}

bool MSP::readIMU(int16_t acc[3], int16_t gyro[3]) {
    MSPFrame f;
    if (!request(MSP_RAW_IMU, f)) return false;
    if (f.size < 18) return false;

    for (int i = 0; i < 3; i++) {
        acc[i]  = (int16_t)(f.payload[i * 2] | (f.payload[i * 2 + 1] << 8));
        gyro[i] = (int16_t)(f.payload[6 + i * 2] | (f.payload[6 + i * 2 + 1] << 8));
    }
    return true;
}

bool MSP::readStatus(uint16_t& cycleTime, uint8_t& armingFlags) {
    MSPFrame f;
    if (!request(MSP_STATUS, f)) return false;
    if (f.size < 13) return false;

    cycleTime   = (uint16_t)(f.payload[0] | (f.payload[1] << 8));
    armingFlags = f.payload[12];  // bit0 = armed
    return true;
}

/* ═══════════════════════════════════════════════════════════
 *  写指令
 * ═══════════════════════════════════════════════════════════ */

bool MSP::setRawRC(const uint16_t channels[16]) {
    uint8_t buf[32];
    for (int i = 0; i < 8; i++) {
        buf[i * 2]     = channels[i] & 0xFF;
        buf[i * 2 + 1] = (channels[i] >> 8) & 0xFF;
    }
    return sendCommand(MSP_SET_RAW_RC, buf, 16);
}

bool MSP::sendArmCommand(bool arm) {
    MSPFrame f;
    if (!request(MSP_ARMING_CONFIG, f)) return false;
    if (f.size < 1) return false;

    uint8_t buf[4];
    memcpy(buf, f.payload, f.size < 4 ? f.size : 4);

    if (arm) {
        buf[0] |= 0x01;   // 设置解锁标志
    } else {
        buf[0] &= ~0x01;
    }

    return sendCommand(MSP_SET_ARMING_CONFIG, buf, f.size);
}
