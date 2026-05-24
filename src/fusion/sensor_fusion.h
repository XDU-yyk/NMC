/**
 * @file    sensor_fusion.h
 * @brief   多源传感器融合调度器 v2.0
 * 
 * 协调 GPS + UWB + ToF + FC(IMU/气压计) 数据送入 9态 EKF。
 */

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include "config.h"
#include "fusion/kalman.h"
#include "sensors/gps.h"
#include "sensors/tof.h"
#include "comm/fc_bridge.h"
#include <Arduino.h>

struct FusionState {
    float posX, posY, posZ;          // 位置 (m)
    float velX, velY, velZ;          // 速度 (m/s)
    float roll, pitch, yaw;          // 姿态 (°), 直接来自飞控
    float altitudeBaro;              // 气压高度 (m)
    float batteryVoltage;            // 电池电压 (V)
    uint8_t batteryCells;

    float uncertaintyX, uncertaintyY, uncertaintyZ;  // 位置不确定度 (m)

    bool gpsValid, tofValid, uwbValid, fcOnline;
    uint8_t gpsSatellites;

    uint32_t timestamp;              // ms
};

class SensorFusion {
public:
    void begin();
    void update();

    const FusionState& getState() const { return m_state; }
    bool isConverged() const { return m_kf.isConverged(); }

    /* 校准: 将当前位置设为原点 (起飞点) */
    void calibrate();

private:
    KalmanFilter m_kf;
    FusionState  m_state;

    float m_homeX = 0, m_homeY = 0, m_homeZ = 0;  // 起飞点 (m)
    float m_homeLat = 0, m_homeLng = 0;            // 起飞点 GPS 坐标
    bool  m_homeSet = false;

    uint32_t m_lastPredict   = 0;
    uint32_t m_lastGPSUpdate = 0;
    uint32_t m_lastToFUpdate = 0;
    uint32_t m_lastUWBUpdate = 0;
    uint32_t m_lastBaroUpdate = 0;
};

extern SensorFusion fusion;

#endif // SENSOR_FUSION_H
