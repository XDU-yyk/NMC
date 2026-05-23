/**
 * @file    imu.h
 * @brief   IMU 惯性测量单元驱动 (MPU6050 / ICM-20948)
 * 
 * 提供 6/9 轴惯性数据采集，包含:
 * - 三轴加速度
 * - 三轴陀螺仪
 * - 板载温度
 * - 姿态解算 (欧拉角/四元数)
 * 
 * ESP32-S3 优化:
 * - I2C DMA 传输减少 CPU 占用
 * - 16 字节对齐 + SIMD 加速 Madgwick 滤波
 */

#ifndef IMU_H
#define IMU_H

#include "config.h"
#include <Arduino.h>
#include <Wire.h>

/**
 * @brief IMU 原始数据
 */
struct IMUData {
    // 加速度 (m/s²)
    float accelX, accelY, accelZ;
    // 陀螺仪 (rad/s)
    float gyroX, gyroY, gyroZ;
    // 温度 (°C)
    float temperature;
    // 时间戳 (ms)
    uint32_t timestamp;
} __attribute__((aligned(16)));

/**
 * @brief 姿态数据
 */
struct Attitude {
    float roll;      // 横滚角 (度)
    float pitch;     // 俯仰角 (度)
    float yaw;       // 偏航角 (度)
    
    // 四元数 (w + xi + yj + zk)
    float qw, qx, qy, qz;
} __attribute__((aligned(16)));

/**
 * @brief IMU 驱动类
 */
class IMUDriver {
public:
    /**
     * @brief 初始化 IMU
     * @param accelRange 加速度量程: 2/4/8/16g
     * @param gyroRange  陀螺仪量程: 250/500/1000/2000dps
     */
    bool begin(uint8_t accelRange = 2, uint16_t gyroRange = 500);
    
    /**
     * @brief 读取原始传感器数据
     * @return true 数据有效
     */
    bool readRaw(IMUData& data);
    
    /**
     * @brief 触发陀螺仪零偏校准
     * 
     * 采集 500 样本取均值。校准时需保持静止。
     */
    void calibrateGyro();
    
    /**
     * @brief 获取当前姿态 (已融合)
     * @param dt 自上次更新以来的时间 (秒)
     */
    void getAttitude(float dt, Attitude& att);
    
    /**
     * @brief 获取加速度矢量 (用于互补滤波)
     */
    void getAccelVec(float& ax, float& ay, float& az);
    
    /**
     * @brief 获取陀螺仪角速度
     */
    void getGyroVec(float& gx, float& gy, float& gz);
    
    /**
     * @brief IMU 是否校准完成
     */
    bool isCalibrated() { return m_calibrated; }
    
private:
    // 零偏
    float m_gyroBiasX = 0, m_gyroBiasY = 0, m_gyroBiasZ = 0;
    bool  m_calibrated = false;
    
    // 量程转换因子
    float m_accelScale = 1.0f;   // LSB → m/s²
    float m_gyroScale  = 1.0f;   // LSB → rad/s
    
    // 姿态四元数 (当前)
    float m_qw = 1.0f, m_qx = 0.0f, m_qy = 0.0f, m_qz = 0.0f;
    
    // 原始数据缓存
    IMUData m_rawData;
    
    /**
     * @brief Madgwick AHRS 算法 — SIMD 优化版
     * 
     * 利用 ESP32-S3 向量指令集加速四元数乘法。
     * 数据结构 16 字节对齐以允许单指令加载。
     * 
     * @param ax, ay, az  加速度 (归一化)
     * @param gx, gy, gz  陀螺仪 (rad/s)
     * @param dt          时间步长 (秒)
     */
    void madgwickUpdate(float ax, float ay, float az,
                        float gx, float gy, float gz,
                        float dt);
    
    /**
     * @brief 四元数 → 欧拉角
     */
    void quaternionToEuler(float& roll, float& pitch, float& yaw);
};

// 全局单例
extern IMUDriver imu;

#endif // IMU_H
