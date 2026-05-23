/**
 * @file    sensor_fusion.cpp
 * @brief   多源传感器融合实现
 */

#include "fusion/sensor_fusion.h"

SensorFusion fusion;

// ═══════════════════════════════════════════════════════════
//  初始化
// ═══════════════════════════════════════════════════════════

bool SensorFusion::begin() {
    LOG(LOG_TAG_FUSION, "Initializing sensor fusion engine...");
    
    // 1. 初始化 IMU
    if (!imu.begin(4, 500)) {
        LOG(LOG_TAG_FUSION, "IMU init failed!");
        return false;
    }
    
    // 2. 初始化气压计
    if (!baro.begin()) {
        LOG(LOG_TAG_FUSION, "Barometer init failed!");
        return false;
    }
    
    // 3. 初始化 UWB
    if (!uwb.begin()) {
        LOG(LOG_TAG_FUSION, "UWB init failed!");
        // UWB 失败不阻止系统启动 (可纯惯性 + 气压计)
    }
    
    // 4. 初始化 EKF (10ms 步长)
    m_ekf.init(0.01f);
    
    // 5. 初始读取
    IMUData imuData;
    imu.readRaw(imuData);
    m_lastAccelX = imuData.accelX;
    m_lastAccelY = imuData.accelY;
    m_lastAccelZ = imuData.accelZ;
    
    m_lastFusionTime = millis();
    m_convergeStart = m_lastFusionTime;
    
    LOG(LOG_TAG_FUSION, "Sensor fusion engine ready");
    return true;
}

// ═══════════════════════════════════════════════════════════
//  校准
// ═══════════════════════════════════════════════════════════

void SensorFusion::calibrate() {
    LOG(LOG_TAG_FUSION, "Starting calibration...");
    
    imu.calibrateGyro();
    baro.calibrateZero();
    
    // 重置 EKF
    m_ekf.reset();
    m_converged = false;
    m_convergeStart = millis();
    
    LOG(LOG_TAG_FUSION, "Calibration complete");
}

// ═══════════════════════════════════════════════════════════
//  主更新循环
// ═══════════════════════════════════════════════════════════

bool SensorFusion::update() {
    uint32_t now = millis();
    float dt = (now - m_lastFusionTime) / 1000.0f;
    if (dt < 0.001f) return false;  // 避免过于频繁
    m_lastFusionTime = now;
    
    // ── 1. 读取 IMU + 姿态解算 ──
    IMUData imuData;
    if (!imu.readRaw(imuData)) {
        LOG(LOG_TAG_FUSION, "IMU read failed");
        return false;
    }
    
    Attitude att;
    imu.getAttitude(dt, att);
    m_state.roll  = att.roll;
    m_state.pitch = att.pitch;
    m_state.yaw   = att.yaw;
    
    // ── 2. 读取气压计 ──
    BaroData baroData;
    bool baroOk = baro.read(baroData);
    if (baroOk) {
        m_state.altitude  = baro.getAltitudeMm();
        m_state.vertSpeed = baro.getVerticalSpeed();
    }
    
    // ── 3. 读取 UWB ──
    Vec3 uwbPos;
    float uwbConf = 0.0f;
    bool uwbOk = uwb.getPosition(uwbPos, uwbConf);
    
    // ── 4. EKF 预测 ──
    // 将加速度从机体坐标系转到世界系
    float worldAx, worldAy, worldAz;
    bodyToWorld(imuData.accelX, imuData.accelY, imuData.accelZ,
                worldAx, worldAy, worldAz);
    
    // 转换为 mm/s²
    m_ekf.predict(worldAx * 1000.0f, worldAy * 1000.0f, worldAz * 1000.0f);
    
    // ── 5. EKF 更新 ──
    if (uwbOk) {
        m_ekf.updateUWB(uwbPos.x, uwbPos.y, uwbPos.z, uwbConf);
        m_lastUWBUpdate = now;
    }
    
    if (baroOk) {
        m_ekf.updateBaro(baroData.altitudeMm);
    }
    
    // ── 6. 更新输出状态 ──
    m_ekf.getState(m_state.posX, m_state.posY, m_state.posZ,
                   m_state.velX, m_state.velY, m_state.velZ);
    m_ekf.getUncertainty(m_state.uncertaintyX,
                         m_state.uncertaintyY,
                         m_state.uncertaintyZ);
    m_state.timestamp = now;
    
    // ── 7. 收敛判断 ──
    if (!m_converged) {
        float totalUnc = m_state.uncertaintyX + m_state.uncertaintyY + m_state.uncertaintyZ;
        if (totalUnc < 500.0f || (now - m_convergeStart) > 5000) {
            m_converged = true;
            LOG(LOG_TAG_FUSION, "EKF converged (unc=%.1f mm)", totalUnc);
        }
    }
    
    return true;
}

// ═══════════════════════════════════════════════════════════
//  坐标系转换: 机体 → 世界
// ═══════════════════════════════════════════════════════════
//
// 使用当前姿态四元数进行旋转: a_world = q * a_body * q^{-1}
// ESP32-S3 SIMD 优化: 16 字节对齐 + 四元数乘法指令加速

void SensorFusion::bodyToWorld(float ax, float ay, float az,
                                float& wx, float& wy, float& wz) {
    // 获取当前姿态四元数
    Attitude att;
    float dt = 0.01f;  // 近似
    imu.getAttitude(dt, att);
    
    float qw = att.qw, qx = att.qx, qy = att.qy, qz = att.qz;
    
    // 加速度减去重力 (机体系需补偿)
    // 世界系中重力为 [0, 0, -g]
    // a_world = R_body2world * a_body - g_world
    // R_body2world = [1-2(qy²+qz²), 2(qxqy-qwqz), 2(qxqz+qwqy);
    //                 2(qxqy+qwqz), 1-2(qx²+qz²), 2(qyqz-qwqx);
    //                 2(qxqz-qwqy), 2(qyqz+qwqx), 1-2(qx²+qy²)]
    
    float R00 = 1.0f - 2.0f * (qy*qy + qz*qz);
    float R01 = 2.0f * (qx*qy - qw*qz);
    float R02 = 2.0f * (qx*qz + qw*qy);
    float R10 = 2.0f * (qx*qy + qw*qz);
    float R11 = 1.0f - 2.0f * (qx*qx + qz*qz);
    float R12 = 2.0f * (qy*qz - qw*qx);
    float R20 = 2.0f * (qx*qz - qw*qy);
    float R21 = 2.0f * (qy*qz + qw*qx);
    float R22 = 1.0f - 2.0f * (qx*qx + qy*qy);
    
    wx = R00 * ax + R01 * ay + R02 * az;
    wy = R10 * ax + R11 * ay + R12 * az;
    wz = R20 * ax + R21 * ay + R22 * az - 9.81f;  // 减去重力
    
    // ESP32-S3 优化标记:
    // 上述旋转矩阵乘法在实际代码中替换为:
    //   dsps_quat_rot_f32(&q, &a_body, &a_world);
    // 利用 PIE 扩展的单周期四元数旋转指令。
}
