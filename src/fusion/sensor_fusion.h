/**
 * @file    sensor_fusion.h
 * @brief   多源传感器融合引擎
 * 
 * 整合 UWB、IMU 和气压计数据，通过 EKF 输出高精度位姿估计。
 * 
 * 核心创新 — ESP32-S3 AI 向量指令集优化:
 * 
 * 1. 矩阵运算加速:
 *    - EKF 预测/更新中的矩阵乘法使用 SIMD 指令
 *    - 16 字节对齐确保单周期 128-bit 加载
 *    - 协方差更新利用对称性：P = 0.5*(P + P') 保证数值稳定
 * 
 * 2. 自适应噪声调节:
 *    - UWB 置信度 → 动态 R 矩阵缩放
 *    - 加速度幅值 → 过程噪声自适应 (运动越剧烈, Q 越大)
 *    - 气压计漂移补偿: 长时间无 UWB 更新时增大 R_baro
 * 
 * 3. 数据对齐与时间戳同步:
 *    - 基于硬件定时器的微秒级时间戳
 *    - 传感器数据缓存队列, 延迟补偿
 */

#ifndef SENSOR_FUSION_H
#define SENSOR_FUSION_H

#include "config.h"
#include "fusion/kalman.h"
#include "sensors/uwb.h"
#include "sensors/imu.h"
#include "sensors/barometer.h"
#include <Arduino.h>

/**
 * @brief 融合状态输出
 */
struct FusionState {
    // 位置 (世界坐标系, mm)
    float posX, posY, posZ;
    // 速度 (mm/s)
    float velX, velY, velZ;
    // 姿态 (度)
    float roll, pitch, yaw;
    // 高度 (mm, 相对地面)
    float altitude;
    // 垂直速度 (mm/s)
    float vertSpeed;
    // 位置不确定度 (mm)
    float uncertaintyX, uncertaintyY, uncertaintyZ;
    // 最后更新时间
    uint32_t timestamp;
};

/**
 * @brief 传感器融合引擎
 * 
 * 运行流程:
 * 1. 读取 IMU → 姿态解算 + 加速度 (世界系)
 * 2. 读取气压计 → 高度 + 垂直速度
 * 3. 读取 UWB → 三维位置 + 置信度
 * 4. EKF 预测 (IMU 加速度)
 * 5. EKF 更新 (UWB + 气压计)
 * 6. 输出融合状态
 */
class SensorFusion {
public:
    /**
     * @brief 初始化所有传感器和滤波器
     */
    bool begin();
    
    /**
     * @brief 执行一次融合更新 (应每 10ms 调用一次)
     * @return true 更新成功
     */
    bool update();
    
    /**
     * @brief 获取融合状态
     */
    const FusionState& getState() const { return m_state; }
    
    /**
     * @brief 执行校准流程 (陀螺仪 + 气压计零点)
     */
    void calibrate();
    
    /**
     * @brief EKF 是否已收敛
     */
    bool isConverged() const { return m_converged; }
    
private:
    // 融合滤波器
    KalmanFilter6D m_ekf;
    
    // 融合输出
    FusionState m_state;
    
    // 收敛判断
    bool m_converged = false;
    uint32_t m_convergeStart = 0;
    
    // 上一次 IMU 数据 (用于计算加速度增量)
    float m_lastAccelX = 0, m_lastAccelY = 0, m_lastAccelZ = 0;
    
    // 时间管理
    uint32_t m_lastFusionTime = 0;
    uint32_t m_lastUWBUpdate = 0;
    
    /**
     * @brief 将 IMU 加速度从机体坐标系转换到世界坐标系
     */
    void bodyToWorld(float ax, float ay, float az,
                     float& wx, float& wy, float& wz);
};

// 全局单例
extern SensorFusion fusion;

#endif // SENSOR_FUSION_H
