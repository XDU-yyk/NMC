/**
 * @file    kalman.h
 * @brief   扩展卡尔曼滤波器 (EKF) — 6 状态
 * 
 * 状态向量 X = [x, y, z, vx, vy, vz]^T
 * - 位置 (x, y, z): 世界坐标系 (mm)
 * - 速度 (vx, vy, vz): 世界坐标系 (mm/s)
 * 
 * 观测模型:
 * - UWB: 直接观测 x, y, z (测量噪声较大)
 * - 气压计: 直接观测 z (测量噪声小)
 * - IMU: 提供加速度作为控制输入 (u = [ax, ay, az])
 * 
 * 利用 ESP32-S3 AI 向量指令集:
 * - 矩阵乘法使用 ESP-DSP 的 dsps_mul_f32()
 * - 所有矩阵 16 字节对齐 → SIMD 加载/存储
 * - 协方差更新利用对称性减少运算量
 */

#ifndef KALMAN_H
#define KALMAN_H

#include "config.h"
#include <Arduino.h>

/**
 * @brief 6 状态 EKF
 */
class KalmanFilter6D {
public:
    /**
     * @brief 初始化滤波器
     * @param dt 预测步长 (秒)
     */
    void init(float dt);
    
    /**
     * @brief 预测步: 基于 IMU 加速度推算
     * @param ax, ay, az 加速度 (mm/s²) — 已转换到世界系
     */
    void predict(float ax, float ay, float az);
    
    /**
     * @brief UWB 观测更新: 三维位置
     * @param x, y, z  测量值 (mm)
     * @param confidence 置信度 0–1, 用于动态调整 R
     */
    void updateUWB(float x, float y, float z, float confidence);
    
    /**
     * @brief 气压计观测更新: 仅高度
     * @param z 测量高度 (mm)
     */
    void updateBaro(float z);
    
    /**
     * @brief 获取当前状态估计
     */
    void getState(float& x, float& y, float& z,
                  float& vx, float& vy, float& vz);
    
    /**
     * @brief 获取协方差矩阵对角线 (不确定度)
     */
    void getUncertainty(float& ux, float& uy, float& uz);
    
    /**
     * @brief 重置状态
     */
    void reset();
    
private:
    static constexpr uint8_t N = 6;   // 状态维度
    
    // 状态向量 ─ 16 字节对齐
    float m_X[N] __attribute__((aligned(16)));
    
    // 协方差矩阵 P (N×N) ─ 16 字节对齐
    float m_P[N * N] __attribute__((aligned(16)));
    
    // 状态转移矩阵 F (N×N)
    float m_F[N * N] __attribute__((aligned(16)));
    
    // 控制矩阵 B (N×3)
    float m_B[N * 3] __attribute__((aligned(16)));
    
    // 过程噪声协方差 Q (N×N)
    float m_Q[N * N] __attribute__((aligned(16)));
    
    // UWB 观测矩阵 H_uwb (3×N) 及噪声 R_uwb (3×3)
    float m_H_uwb[3 * N] __attribute__((aligned(16)));
    float m_R_uwb[3 * 3] __attribute__((aligned(16)));
    
    // 气压计观测矩阵 H_baro (1×N) 及噪声 R_baro
    float m_H_baro[N] __attribute__((aligned(16)));
    float m_R_baro;
    
    float m_dt;
    bool  m_initialized = false;
    
    // ───── 矩阵运算辅助 (SIMD 路径) ─────
    
    /**
     * @brief 通用卡尔曼更新: K = P*H' * inv(H*P*H' + R)
     * 
     * 使用 ESP-DSP 矩阵求逆 + 乘法。观测维度 m ≤ 3，
     * 小矩阵直接展开比通用求逆更快。
     */
    void kalmanUpdate(const float* H, const float* R,
                      const float* Z, uint8_t m);
};

#endif // KALMAN_H
