/**
 * @file    barometer.cpp
 * @brief   气压计驱动实现
 */

#include "sensors/barometer.h"

Barometer baro;

// ═══════════════════════════════════════════════════════════
//  初始化
// ═══════════════════════════════════════════════════════════

bool Barometer::begin() {
    BARO_I2C_PORT.begin(BARO_SDA, BARO_SCL, BARO_I2C_FREQ);
    
    // 读取芯片 ID
    BARO_I2C_PORT.beginTransmission(BARO_I2C_ADDR);
    BARO_I2C_PORT.write(REG_CHIP_ID);
    BARO_I2C_PORT.endTransmission(false);
    BARO_I2C_PORT.requestFrom((int)BARO_I2C_ADDR, 1);
    
    uint8_t chipId = BARO_I2C_PORT.read();
    if (chipId != BMP280_EXPECTED_ID) {
        LOG(LOG_TAG_FUSION, "Barometer ID mismatch: 0x%02X (expected 0x%02X)",
            chipId, BMP280_EXPECTED_ID);
        return false;
    }
    
    // 软复位
    BARO_I2C_PORT.beginTransmission(BARO_I2C_ADDR);
    BARO_I2C_PORT.write(REG_RESET);
    BARO_I2C_PORT.write(0xB6);
    BARO_I2C_PORT.endTransmission();
    delay(10);
    
    // 读取校准参数
    if (!readCalibrationData()) {
        LOG(LOG_TAG_FUSION, "Barometer calibration read failed");
        return false;
    }
    
    // 配置: 正常模式, 气压×16 过采样, 温度×2 过采样, 50Hz ODR
    // ctrl_meas: [7:5]=osrs_t, [4:2]=osrs_p, [1:0]=mode
    // config:   [7:5]=t_sb, [4:2]=filter, [0]=spi3w_en
    BARO_I2C_PORT.beginTransmission(BARO_I2C_ADDR);
    BARO_I2C_PORT.write(REG_CONFIG);
    BARO_I2C_PORT.write(0x08);   // filter=4, t_sb=0.5ms
    BARO_I2C_PORT.endTransmission();
    
    BARO_I2C_PORT.beginTransmission(BARO_I2C_ADDR);
    BARO_I2C_PORT.write(REG_CTRL_MEAS);
    BARO_I2C_PORT.write(0x57);   // osrs_t=2, osrs_p=16, mode=normal
    BARO_I2C_PORT.endTransmission();
    
    // 初始校准零点
    delay(100);
    BaroData tmp;
    if (read(tmp)) {
        m_refPressure = tmp.pressure;
        m_groundAlt = tmp.altitude;
    }
    
    LOG(LOG_TAG_FUSION, "Barometer initialized, ref pressure=%.2f hPa", m_refPressure);
    return true;
}

// ═══════════════════════════════════════════════════════════
//  校准参数读取
// ═══════════════════════════════════════════════════════════

bool Barometer::readCalibrationData() {
    uint8_t calib[24];
    
    BARO_I2C_PORT.beginTransmission(BARO_I2C_ADDR);
    BARO_I2C_PORT.write(0x88);
    BARO_I2C_PORT.endTransmission(false);
    BARO_I2C_PORT.requestFrom((int)BARO_I2C_ADDR, 24);
    
    for (int i = 0; i < 24; i++) {
        if (!BARO_I2C_PORT.available()) return false;
        calib[i] = BARO_I2C_PORT.read();
    }
    
    dig_T1 = calib[0]  | (calib[1]  << 8);
    dig_T2 = calib[2]  | (calib[3]  << 8);
    dig_T3 = calib[4]  | (calib[5]  << 8);
    dig_P1 = calib[6]  | (calib[7]  << 8);
    dig_P2 = calib[8]  | (calib[9]  << 8);
    dig_P3 = calib[10] | (calib[11] << 8);
    dig_P4 = calib[12] | (calib[13] << 8);
    dig_P5 = calib[14] | (calib[15] << 8);
    dig_P6 = calib[16] | (calib[17] << 8);
    dig_P7 = calib[18] | (calib[19] << 8);
    dig_P8 = calib[20] | (calib[21] << 8);
    dig_P9 = calib[22] | (calib[23] << 8);
    
    return true;
}

// ═══════════════════════════════════════════════════════════
//  数据读取
// ═══════════════════════════════════════════════════════════

bool Barometer::read(BaroData& data) {
    // 读取 6 字节: press(3) + temp(3)
    BARO_I2C_PORT.beginTransmission(BARO_I2C_ADDR);
    BARO_I2C_PORT.write(REG_PRESS_MSB);
    BARO_I2C_PORT.endTransmission(false);
    BARO_I2C_PORT.requestFrom((int)BARO_I2C_ADDR, 6);
    
    if (BARO_I2C_PORT.available() < 6) return false;
    
    uint8_t buf[6];
    for (int i = 0; i < 6; i++) buf[i] = BARO_I2C_PORT.read();
    
    int32_t adc_P = ((int32_t)buf[0] << 12) | ((int32_t)buf[1] << 4) | (buf[2] >> 4);
    int32_t adc_T = ((int32_t)buf[3] << 12) | ((int32_t)buf[4] << 4) | (buf[5] >> 4);
    
    int32_t t_fine;
    data.temperature = compensateTemperature(adc_T, t_fine);
    data.pressure = compensatePressure(adc_P, t_fine);
    
    // 高度计算 (相对起飞点)
    data.altitude = pressureToAltitude(data.pressure);
    data.altitudeMm = (data.altitude - m_groundAlt) * 1000.0f;
    data.timestamp = millis();
    
    // IIR 低通滤波
    if (m_lastReadTime == 0) {
        m_altFiltered = data.altitudeMm;
        m_lastAlt = data.altitudeMm;
    } else {
        float alpha = 0.85f;  // 低通系数
        m_altFiltered = alpha * m_altFiltered + (1.0f - alpha) * data.altitudeMm;
        
        // 垂直速度估计 (差分 + 低通)
        float dt = (data.timestamp - m_lastReadTime) / 1000.0f;
        if (dt > 0.01f) {
            float rawSpeed = (data.altitudeMm - m_lastAlt) / dt;
            m_vertSpeedFiltered = 0.9f * m_vertSpeedFiltered + 0.1f * rawSpeed;
        }
        m_lastAlt = data.altitudeMm;
    }
    
    m_lastReadTime = data.timestamp;
    return true;
}

float Barometer::getAltitudeMm() {
    return m_altFiltered;
}

float Barometer::getVerticalSpeed() {
    return m_vertSpeedFiltered;
}

void Barometer::calibrateZero() {
    BaroData tmp;
    if (read(tmp)) {
        m_refPressure = tmp.pressure;
        m_groundAlt = pressureToAltitude(m_refPressure);
        m_altFiltered = 0.0f;
        LOG(LOG_TAG_FUSION, "Barometer zero calibrated: ref=%.2f hPa", m_refPressure);
    }
}

// ═══════════════════════════════════════════════════════════
//  BMP280 补偿公式 (Datasheet §3.11.3)
// ═══════════════════════════════════════════════════════════

float Barometer::compensateTemperature(int32_t adc_T, int32_t& t_fine) {
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) *
            ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) / 25600.0f;
}

float Barometer::compensatePressure(int32_t adc_P, int32_t t_fine) {
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
           ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    
    if (var1 == 0) return 0.0f;
    
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    
    return (float)p / 25600.0f;
}

// ═══════════════════════════════════════════════════════════
//  气压 → 高度 (ISA 标准大气压模型)
// ═══════════════════════════════════════════════════════════

float Barometer::pressureToAltitude(float pressure) {
    // 国际标准大气压公式: h = 44330 * (1 - (P/P0)^(1/5.255))
    if (m_refPressure < 1.0f) return 0.0f;
    
    float ratio = pressure / m_refPressure;
    return 44330.0f * (1.0f - powf(ratio, 1.0f / 5.255f));
}
