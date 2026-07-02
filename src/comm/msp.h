/**
 * @file msp.h
 * @brief MSP v1 protocol helper for read-only flight-controller diagnostics.
 */

#ifndef MSP_H
#define MSP_H

#include <Arduino.h>

#define MSP_HEADER_PREAMBLE     '$'
#define MSP_HEADER_DIR_TO_FC    '<'
#define MSP_HEADER_DIR_FROM_FC  '>'
#define MSP_MAX_PAYLOAD         64

enum MSP_CMD : uint16_t {
    MSP_FC_VERSION          = 1,
    MSP_STATUS              = 101,
    MSP_RAW_IMU             = 102,
    MSP_RC                  = 105,
    MSP_ATTITUDE            = 108,
    MSP_ALTITUDE            = 109,
    MSP_ANALOG              = 110,
    MSP_SET_RAW_RC          = 200,
    MSP_ARMING_CONFIG       = 61,
    MSP_SET_ARMING_CONFIG   = 158,
};

struct MSPFrame {
    uint8_t  dir = 0;
    uint8_t  cmd = 0;
    uint8_t  size = 0;
    uint8_t  payload[MSP_MAX_PAYLOAD] = {0};
    uint8_t  checksum = 0;
    bool     valid = false;
};

struct MSPDiag {
    uint32_t requestCount    = 0;
    uint32_t responseOk      = 0;
    uint32_t timeoutCount    = 0;
    uint32_t checksumFail    = 0;
    uint32_t frameError      = 0;

    uint32_t txFrames        = 0;
    uint32_t txBytes         = 0;

    uint32_t rxBytes         = 0;
    uint32_t rxDollarCount   = 0;
    uint32_t rxHeaderCount   = 0;
    bool     rxFirstByteSeen = false;
    bool     rxDollarSeen    = false;
    bool     rxHeaderSeen    = false;
    uint8_t  lastRxByte      = 0;

    uint16_t lastCmd         = 0;
    char     lastError[24]   = {0};
};

class MSP {
public:
    void begin(HardwareSerial& serial, uint32_t baud, int rxPin, int txPin);

    bool sendCommand(uint8_t cmd, const uint8_t* payload, uint8_t size);
    bool request(uint8_t cmd, MSPFrame& frame, uint32_t timeoutMs = 100);

    bool readAttitude(float& roll, float& pitch, float& yaw);
    bool readAltitude(float& altCm, float& varioCmS);
    bool readBattery(uint8_t& cells, float& voltage);
    bool readRC(uint16_t channels[16], uint8_t& count);
    bool readIMU(int16_t acc[3], int16_t gyro[3]);
    bool readStatus(uint16_t& cycleTime, uint8_t& armingFlags);

    bool setRawRC(const uint16_t channels[16]);
    bool sendArmCommand(bool arm);

    const MSPDiag& getDiag() const { return m_diag; }

private:
    HardwareSerial* m_serial = nullptr;
    MSPDiag m_diag;

    uint8_t calcChecksum(const MSPFrame& frame);
    bool readByteWithTimeout(uint8_t& out, uint32_t deadline);
    bool readFrame(MSPFrame& frame, uint32_t timeoutMs);
    bool expectResponse(uint8_t cmd, MSPFrame& frame, uint32_t timeoutMs);
};

#endif // MSP_H
