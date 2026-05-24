/**
 * @file    kalman.h
 * @brief   9状态扩展卡尔曼滤波器 (EKF v2)
 * 
 * 状态向量: X = [x, y, z, vx, vy, vz, ax_bias, ay_bias, az_bias]
 * 观测源: GPS(xyz) + UWB(xyz) + ToF(z) + 飞控气压计(z)
 * 
 * 所有单位: 国际单位制 (m, m/s, m/s²)
 */

#ifndef KALMAN_H
#define KALMAN_H

#include "config.h"
#include <Arduino.h>

#define KF_N 9   // 状态向量维度

class KalmanFilter {
public:
    /* 初始化滤波器 (设置初始位置和初始协方差) */
    void begin(float initX, float initY, float initZ);

    /* 预测步 (传入 dt 秒, 加速度 [ax, ay, az] 世界坐标系 m/s²) */
    void predict(float dt, float ax, float ay, float az);

    /* ── 更新步 (分传感器类型) ── */

    /* GPS 绝对位置更新 (xyz, 噪声根据 HDOP 放大) */
    void updateGPS(float x, float y, float z, float hdopScale);

    /* UWB 相对位置更新 */
    void updateUWB(float x, float y, float z);

    /* ToF 激光高度更新 (z 轴, 低噪声) */
    void updateToF(float z);

    /* 飞控气压计高度更新 (z 轴, 中噪声) */
    void updateBaro(float z);

    /* ── 状态获取 ── */

    float getX()     const { return m_X[0]; }
    float getY()     const { return m_X[1]; }
    float getZ()     const { return m_X[2]; }
    float getVX()    const { return m_X[3]; }
    float getVY()    const { return m_X[4]; }
    float getVZ()    const { return m_X[5]; }
    float getBiasX() const { return m_X[6]; }
    float getBiasY() const { return m_X[7]; }
    float getBiasZ() const { return m_X[8]; }

    /* 位置协方差 (不确定度) */
    float getUncertaintyX() const { return sqrtf(fmaxf(0, m_P[0*KF_N + 0])); }
    float getUncertaintyY() const { return sqrtf(fmaxf(0, m_P[1*KF_N + 1])); }
    float getUncertaintyZ() const { return sqrtf(fmaxf(0, m_P[2*KF_N + 2])); }

    /* 滤波器是否收敛 */
    bool isConverged() const { return m_converged; }

    /* 重置滤波器 */
    void reset(float initX, float initY, float initZ);

private:
    float m_X[KF_N];           // 状态向量
    float m_P[KF_N * KF_N];    // 协方差矩阵

    bool  m_converged = false;
    uint32_t m_lastPredict = 0;

    /* 矩阵运算辅助函数 */
    void matMultiply(const float* A, const float* B, float* C, int n);
    void matTranspose(const float* A, float* AT, int n);
    void matAdd(const float* A, const float* B, float* C, int n);
    void matSubtract(const float* A, const float* B, float* C, int n);
    float matDeterminant(const float* A, int n);
    bool  matInverse(const float* A, float* Ainv, int n);
    void matSetIdentity(float* A, int n);
    void matSetZero(float* A, int n);
    void matScale(float* A, float s, int n);
    void matCopy(const float* src, float* dst, int n);

    /* 通用更新函数 (单行观测) */
    void updateScalar(const float* H, int H_len, float z, float R);
};

#endif // KALMAN_H
