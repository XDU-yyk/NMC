/**
 * @file msp.cpp
 * @brief MSP v1 protocol implementation for F4V3S read-only diagnostics.
 */

#include "comm/msp.h"
#include <string.h>

void MSP::begin(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin)
{
    m_serial = &serial;
    m_serial->begin(baud, SERIAL_8N1, rxPin, txPin);
    memset(&m_diag, 0, sizeof(m_diag));
    strcpy(m_diag.lastError, "none");
}

uint8_t MSP::calcChecksum(const MSPFrame& frame)
{
    uint8_t c = 0;
    c ^= frame.size;
    c ^= frame.cmd;
    for (uint8_t i = 0; i < frame.size; i++) c ^= frame.payload[i];
    return c;
}

bool MSP::sendCommand(uint8_t cmd, const uint8_t* payload, uint8_t size)
{
    if (!m_serial || size > MSP_MAX_PAYLOAD) return false;

    MSPFrame frame;
    frame.cmd = cmd;
    frame.size = size;
    frame.dir = MSP_HEADER_DIR_TO_FC;
    if (payload && size) memcpy(frame.payload, payload, size);
    frame.checksum = calcChecksum(frame);

    uint8_t buf[MSP_MAX_PAYLOAD + 6];
    uint8_t idx = 0;
    buf[idx++] = MSP_HEADER_PREAMBLE;
    buf[idx++] = 'M';
    buf[idx++] = frame.dir;
    buf[idx++] = frame.size;
    buf[idx++] = frame.cmd;
    if (size && payload) {
        memcpy(buf + idx, payload, size);
        idx += size;
    }
    buf[idx++] = frame.checksum;

    size_t written = m_serial->write(buf, idx);
    m_serial->flush();
    m_diag.txFrames++;
    m_diag.txBytes += written;

    return written == idx;
}

bool MSP::request(uint8_t cmd, MSPFrame& frame, uint32_t timeoutMs)
{
    m_diag.requestCount++;
    m_diag.lastCmd = cmd;

    uint32_t rxBytesBefore = m_diag.rxBytes;
    uint32_t rxDollarBefore = m_diag.rxDollarCount;
    uint32_t rxHeaderBefore = m_diag.rxHeaderCount;

    if (!sendCommand(cmd, nullptr, 0)) {
        strcpy(m_diag.lastError, "send fail");
        return false;
    }

    bool ok = expectResponse(cmd, frame, timeoutMs);
    if (ok) {
        m_diag.responseOk++;
        strcpy(m_diag.lastError, "ok");
        return true;
    }

    m_diag.timeoutCount++;
    if (m_diag.rxBytes == rxBytesBefore) {
        strcpy(m_diag.lastError, "rx silence");
    } else if (m_diag.rxDollarCount == rxDollarBefore) {
        strcpy(m_diag.lastError, "noise/no header");
    } else if (m_diag.rxHeaderCount == rxHeaderBefore) {
        strcpy(m_diag.lastError, "header incomplete");
    } else {
        strcpy(m_diag.lastError, "timeout");
    }
    return false;
}

bool MSP::expectResponse(uint8_t cmd, MSPFrame& frame, uint32_t timeoutMs)
{
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        uint32_t remaining = timeoutMs - (millis() - start);
        if (readFrame(frame, remaining)) {
            if (frame.cmd == cmd && frame.dir == MSP_HEADER_DIR_FROM_FC) {
                return true;
            }
            m_diag.frameError++;
        }
    }
    return false;
}

bool MSP::readByteWithTimeout(uint8_t& out, uint32_t deadline)
{
    while (!m_serial || m_serial->available() <= 0) {
        if (deadline && millis() >= deadline) {
            m_diag.frameError++;
            return false;
        }
        delay(1);
    }

    int value = m_serial->read();
    if (value < 0) {
        m_diag.frameError++;
        return false;
    }

    out = (uint8_t)value;
    m_diag.rxBytes++;
    m_diag.rxFirstByteSeen = true;
    m_diag.lastRxByte = out;
    return true;
}

bool MSP::readFrame(MSPFrame& frame, uint32_t timeoutMs)
{
    if (!m_serial) return false;

    uint32_t deadline = timeoutMs ? (millis() + timeoutMs) : 0;
    memset(&frame, 0, sizeof(frame));

    uint8_t b = 0;

    while (true) {
        if (!readByteWithTimeout(b, deadline)) return false;
        if (b == MSP_HEADER_PREAMBLE) {
            m_diag.rxDollarSeen = true;
            m_diag.rxDollarCount++;
            break;
        }
    }

    if (!readByteWithTimeout(b, deadline)) return false;
    if (b != 'M') {
        m_diag.frameError++;
        return false;
    }
    m_diag.rxHeaderSeen = true;
    m_diag.rxHeaderCount++;

    if (!readByteWithTimeout(frame.dir, deadline)) return false;
    if (frame.dir != MSP_HEADER_DIR_FROM_FC && frame.dir != MSP_HEADER_DIR_TO_FC) {
        m_diag.frameError++;
        return false;
    }

    if (!readByteWithTimeout(frame.size, deadline)) return false;
    if (frame.size > MSP_MAX_PAYLOAD) {
        m_diag.frameError++;
        return false;
    }

    if (!readByteWithTimeout(frame.cmd, deadline)) return false;

    for (uint8_t i = 0; i < frame.size; i++) {
        if (!readByteWithTimeout(frame.payload[i], deadline)) return false;
    }

    if (!readByteWithTimeout(frame.checksum, deadline)) return false;
    frame.valid = (frame.checksum == calcChecksum(frame));
    if (!frame.valid) {
        m_diag.checksumFail++;
        strcpy(m_diag.lastError, "checksum fail");
    }
    return frame.valid;
}

bool MSP::readAttitude(float& roll, float& pitch, float& yaw)
{
    MSPFrame f;
    if (!request(MSP_ATTITUDE, f)) return false;
    if (f.size < 6) return false;

    int16_t r = (int16_t)(f.payload[0] | (f.payload[1] << 8));
    int16_t p = (int16_t)(f.payload[2] | (f.payload[3] << 8));
    int16_t h = (int16_t)(f.payload[4] | (f.payload[5] << 8));
    roll = r * 0.1f;
    pitch = p * 0.1f;
    yaw = h;
    return true;
}

bool MSP::readAltitude(float& altCm, float& varioCmS)
{
    MSPFrame f;
    if (!request(MSP_ALTITUDE, f)) return false;
    if (f.size < 6) return false;

    int32_t alt = (int32_t)(f.payload[0] | (f.payload[1] << 8) |
        (f.payload[2] << 16) | (f.payload[3] << 24));
    int16_t vario = (int16_t)(f.payload[4] | (f.payload[5] << 8));
    altCm = (float)alt;
    varioCmS = (float)vario;
    return true;
}

bool MSP::readBattery(uint8_t& cells, float& voltage)
{
    MSPFrame f;
    if (!request(MSP_ANALOG, f)) return false;
    if (f.size < 1) return false;

    uint8_t vbatRaw = f.payload[0];
    voltage = vbatRaw * 0.1f;
    cells = (uint8_t)(voltage / 4.2f + 0.5f);
    return true;
}

bool MSP::readRC(uint16_t channels[16], uint8_t& count)
{
    MSPFrame f;
    if (!request(MSP_RC, f)) return false;

    count = f.size / 2;
    if (count > 16) count = 16;
    for (uint8_t i = 0; i < count; i++) {
        channels[i] = (uint16_t)(f.payload[i * 2] | (f.payload[i * 2 + 1] << 8));
    }
    return true;
}

bool MSP::readIMU(int16_t acc[3], int16_t gyro[3])
{
    MSPFrame f;
    if (!request(MSP_RAW_IMU, f)) return false;
    if (f.size < 18) return false;

    for (int i = 0; i < 3; i++) {
        acc[i] = (int16_t)(f.payload[i * 2] | (f.payload[i * 2 + 1] << 8));
        gyro[i] = (int16_t)(f.payload[6 + i * 2] | (f.payload[6 + i * 2 + 1] << 8));
    }
    return true;
}

bool MSP::readStatus(uint16_t& cycleTime, uint8_t& armingFlags)
{
    MSPFrame f;
    if (!request(MSP_STATUS, f)) return false;
    if (f.size < 11) return false;

    cycleTime = (uint16_t)(f.payload[0] | (f.payload[1] << 8));
    armingFlags = (f.size >= 13) ? f.payload[12] : 0;
    return true;
}

bool MSP::setRawRC(const uint16_t channels[16])
{
    uint8_t buf[32];
    for (int i = 0; i < 8; i++) {
        buf[i * 2] = channels[i] & 0xFF;
        buf[i * 2 + 1] = (channels[i] >> 8) & 0xFF;
    }
    return sendCommand(MSP_SET_RAW_RC, buf, 16);
}

bool MSP::sendArmCommand(bool arm)
{
    MSPFrame f;
    if (!request(MSP_ARMING_CONFIG, f)) return false;
    if (f.size < 1) return false;

    uint8_t buf[4] = {0};
    uint8_t n = f.size < sizeof(buf) ? f.size : sizeof(buf);
    memcpy(buf, f.payload, n);
    if (arm) buf[0] |= 0x01;
    else buf[0] &= ~0x01;

    return sendCommand(MSP_SET_ARMING_CONFIG, buf, f.size);
}
