/**
 * @file    barometer.h
 * @brief   气压计驱动 (BMP280)
 * 
 * 提供高精度海拔高度测量:
 * - 气压测量精度 ±0.12 hPa (≈ ±1m)
 * - 温度测量精度 ±0.5°C
 * - IIR 滤波器降噪
 * - 与 IMU 数据互补用于高度保持
 */

#ifndef BAROMETER_H
#define BAROMETER_H

#include "config.h"
#include <Arduino.h>
#include <Wire.h>

/**
 * @brief 气压计数据
 */
struct BaroData {
    float pressure;      // 气压 (hPa)
    float temperature;   // 温度 (°C)
    float altitude;      // 海拔高度 (m)
    float altitudeMm;    // 高度 (mm) — 以起飞点为零点
    uint32_t timestamp;
};

/**
 * @brief BMP280 驱动类
 */
class Barometer {
public:
    /**
     * @brief 初始化气压计
     * @return true 成功
     */
    bool begin();
    
    /**
     * @brief 读取气压和温度数据
     */
    bool read(BaroData& data);
    
    /**
     * @brief 获取当前高度 (mm, 相对起飞点)
     */
    float getAltitudeMm();
    
    /**
     * @brief 校准零点 — 将当前气压设为参考海平面气压
     */
    void calibrateZero();
    
    /**
     * @brief 获取垂直速度估计 (mm/s)
     * 
     * 基于气压高度差分 + 低通滤波
     */
    float getVerticalSpeed();
    
private:
    // BMP280 寄存器
    enum Reg : uint8_t {
        REG_CHIP_ID    = 0xD0,
        REG_CTRL_MEAS  = 0xF4,
        REG_CONFIG     = 0xF5,
        REG_PRESS_MSB  = 0xF7,
        REG_TEMP_MSB   = 0xFA,
        REG_RESET      = 0xE0
    };
    
    static constexpr uint8_t BMP280_EXPECTED_ID = 0x58;
    
    // 校准参数
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    
    // 参考气压 (起飞点)
    float m_refPressure = 1013.25f;
    float m_groundAlt = 0.0f;
    
    // 高度滤波
    float m_altFiltered = 0.0f;
    float m_lastAlt = 0.0f;
    uint32_t m_lastReadTime = 0;
    
    // 垂直速度低通
    float m_vertSpeedFiltered = 0.0f;
    
    /**
     * @brief 读取 BMP280 校准参数
     */
    bool readCalibrationData();
    
    /**
     * @brief 补偿温度 (BMP280 公式)
     */
    float compensateTemperature(int32_t adc_T, int32_t& t_fine);
    
    /**
     * @brief 补偿气压 (BMP280 公式)
     */
    float compensatePressure(int32_t adc_P, int32_t t_fine);
    
    /**
     * @brief 气压 → 高度 (国际标准大气压公式)
     */
    float pressureToAltitude(float pressure);
};

// 全局单例
extern Barometer baro;

#endif // BAROMETER_H
