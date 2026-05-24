/**
 * @file    imu.h / imu.cpp
 * @brief   IMU 驱动桩 (v2.0 — IMU 数据改从 F4V3S 飞控 MSP 读取)
 * 
 * 保留供旧代码兼容。实际姿态数据通过 FCBridge::getState() 获取。
 */

#ifndef IMU_H
#define IMU_H

#include "config.h"
#include <Arduino.h>

struct IMUData {
    float accelX, accelY, accelZ;
    float gyroX,  gyroY,  gyroZ;
    float roll, pitch, yaw;
    bool  calibrated = false;
};

class IMUDriver {
public:
    bool begin(int sda, int scl) { return true; }
    void update() {}
    bool isCalibrated() const { return true; }
    const IMUData& getData() const { return m_data; }
private:
    IMUData m_data;
};

extern IMUDriver imu;

#endif // IMU_H
