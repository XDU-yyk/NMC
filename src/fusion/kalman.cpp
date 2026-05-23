/**
 * @file    kalman.cpp
 * @brief   6 状态 EKF 实现
 */

#include "fusion/kalman.h"

// ═══════════════════════════════════════════════════════════
//  初始化
// ═══════════════════════════════════════════════════════════

void KalmanFilter6D::init(float dt) {
    m_dt = dt;
    
    // ── 状态向量清零 ──
    for (uint8_t i = 0; i < N; i++) m_X[i] = 0.0f;
    
    // ── 协方差矩阵初始化 (先验不确定度) ──
    for (uint8_t i = 0; i < N * N; i++) m_P[i] = 0.0f;
    // 位置不确定度 — 初始较高 (未定位)
    m_P[0 * N + 0] = 1000000.0f;   // var(x)
    m_P[1 * N + 1] = 1000000.0f;   // var(y)
    m_P[2 * N + 2] =  100000.0f;   // var(z)
    m_P[3 * N + 3] =   10000.0f;   // var(vx)
    m_P[4 * N + 4] =   10000.0f;   // var(vy)
    m_P[5 * N + 5] =    1000.0f;   // var(vz)
    
    // ── 状态转移矩阵 F (匀速模型) ──
    // X_k+1 = F * X_k + B * u_k
    // F = [I_3, dt*I_3; 0_3, I_3]
    for (uint8_t i = 0; i < N * N; i++) m_F[i] = 0.0f;
    for (uint8_t i = 0; i < N; i++)  m_F[i * N + i] = 1.0f;
    m_F[0 * N + 3] = dt;   // x += dt * vx
    m_F[1 * N + 4] = dt;   // y += dt * vy
    m_F[2 * N + 5] = dt;   // z += dt * vz
    
    // ── 控制矩阵 B ──
    // B = [0.5*dt^2 * I_3; dt * I_3]
    for (uint8_t i = 0; i < N * 3; i++) m_B[i] = 0.0f;
    float dt2h = 0.5f * dt * dt;
    m_B[0 * 3 + 0] = dt2h;   m_B[3 * 3 + 0] = dt;
    m_B[1 * 3 + 1] = dt2h;   m_B[4 * 3 + 1] = dt;
    m_B[2 * 3 + 2] = dt2h;   m_B[5 * 3 + 2] = dt;
    
    // ── 过程噪声 Q (加速度模型噪声 × 离散化) ──
    float q_accel = KF_PROCESS_NOISE_Q;  // (mm/s²)²
    float dt3_3 = dt*dt*dt / 3.0f;
    float dt2_2 = dt*dt / 2.0f;
    for (uint8_t i = 0; i < N * N; i++) m_Q[i] = 0.0f;
    for (uint8_t i = 0; i < 3; i++) {
        m_Q[i * N + i]               = q_accel * dt3_3;
        m_Q[i * N + (i + 3)]         = q_accel * dt2_2;
        m_Q[(i + 3) * N + i]         = q_accel * dt2_2;
        m_Q[(i + 3) * N + (i + 3)]  = q_accel * dt;
    }
    
    // ── UWB 观测矩阵 H_uwb: 直接观测位置 ──
    for (uint8_t i = 0; i < 3 * N; i++) m_H_uwb[i] = 0.0f;
    m_H_uwb[0 * N + 0] = 1.0f;
    m_H_uwb[1 * N + 1] = 1.0f;
    m_H_uwb[2 * N + 2] = 1.0f;
    
    // H_baro: 仅观测 z
    for (uint8_t i = 0; i < N; i++) m_H_baro[i] = 0.0f;
    m_H_baro[2] = 1.0f;
    
    m_initialized = true;
}

// ═══════════════════════════════════════════════════════════
//  预测步
// ═══════════════════════════════════════════════════════════

void KalmanFilter6D::predict(float ax, float ay, float az) {
    if (!m_initialized) return;
    
    // ── 1. 状态预测: X = F*X + B*u ──
    float X_new[N] __attribute__((aligned(16)));
    
    for (uint8_t i = 0; i < N; i++) {
        X_new[i] = 0.0f;
        for (uint8_t j = 0; j < N; j++) {
            X_new[i] += m_F[i * N + j] * m_X[j];
        }
        // 控制输入
        if (i < 3) {
            X_new[i] += m_B[i * 3 + i] * (i==0 ? ax : (i==1 ? ay : az));
        } else {
            X_new[i] += m_B[i * 3 + (i-3)] * (i==3 ? ax : (i==4 ? ay : az));
        }
    }
    
    // ── 2. 协方差预测: P = F*P*F' + Q ──
    // 分两步: temp = F*P, 然后 P = temp*F' + Q
    float temp[N * N] __attribute__((aligned(16))) = {0};
    
    for (uint8_t i = 0; i < N; i++) {
        for (uint8_t j = 0; j < N; j++) {
            float sum = 0;
            for (uint8_t k = 0; k < N; k++) {
                sum += m_F[i * N + k] * m_P[k * N + j];
            }
            temp[i * N + j] = sum;
        }
    }
    
    // P = temp * F' + Q
    for (uint8_t i = 0; i < N; i++) {
        for (uint8_t j = 0; j < N; j++) {
            float sum = 0;
            for (uint8_t k = 0; k < N; k++) {
                sum += temp[i * N + k] * m_F[j * N + k];  // F' = F(j,k) at (k,j)
            }
            m_P[i * N + j] = sum + m_Q[i * N + j];
        }
    }
    
    // 更新状态
    for (uint8_t i = 0; i < N; i++) m_X[i] = X_new[i];
}

// ═══════════════════════════════════════════════════════════
//  UWB 更新
// ═══════════════════════════════════════════════════════════

void KalmanFilter6D::updateUWB(float x, float y, float z, float confidence) {
    if (!m_initialized) return;
    
    // 动态调整测量噪声: 置信度低 → 噪声大
    float r_scale = 1.0f / (confidence + 0.1f);  // 映射到 [1, 10]
    float r_val = KF_MEASURE_NOISE_R * r_scale;
    
    // 构建 R 矩阵
    float R[9] __attribute__((aligned(16))) = {0};
    R[0] = r_val;  R[4] = r_val;  R[8] = r_val;
    
    float Z[3] __attribute__((aligned(16))) = {x, y, z};
    
    kalmanUpdate(m_H_uwb, R, Z, 3);
}

// ═══════════════════════════════════════════════════════════
//  气压计更新 (仅高度)
// ═══════════════════════════════════════════════════════════

void KalmanFilter6D::updateBaro(float z) {
    if (!m_initialized) return;
    
    float R = KF_BARO_NOISE_R;
    float Z = z;
    
    kalmanUpdate(m_H_baro, &R, &Z, 1);
}

// ═══════════════════════════════════════════════════════════
//  通用卡尔曼更新
// ═══════════════════════════════════════════════════════════

void KalmanFilter6D::kalmanUpdate(const float* H, const float* R,
                                   const float* Z, uint8_t m) {
    // ── 1. 计算残差 y = Z - H*X ──
    float y[m] __attribute__((aligned(16)));
    for (uint8_t i = 0; i < m; i++) {
        float hx = 0;
        for (uint8_t j = 0; j < N; j++) {
            hx += H[i * N + j] * m_X[j];
        }
        y[i] = Z[i] - hx;
    }
    
    // ── 2. 计算 S = H*P*H' + R (创新协方差) ──
    // 先算 PH'  (N×m)
    float PHt[N * 3] __attribute__((aligned(16))) = {0};
    for (uint8_t i = 0; i < N; i++) {
        for (uint8_t j = 0; j < m; j++) {
            float sum = 0;
            for (uint8_t k = 0; k < N; k++) {
                sum += m_P[i * N + k] * H[j * N + k];
            }
            PHt[i * m + j] = sum;
        }
    }
    
    // S = H * PH' (m×m)
    float S[9] __attribute__((aligned(16))) = {0};
    for (uint8_t i = 0; i < m; i++) {
        for (uint8_t j = 0; j < m; j++) {
            float sum = 0;
            for (uint8_t k = 0; k < N; k++) {
                sum += H[i * N + k] * PHt[k * m + j];
            }
            S[i * m + j] = sum + R[i * m + j];
        }
    }
    
    // ── 3. 计算卡尔曼增益: K = PH' * S^{-1} ──
    // m ≤ 3, 直接求逆 (伴随矩阵法)
    float S_inv[9] __attribute__((aligned(16))) = {0};
    
    if (m == 1) {
        // 标量
        S_inv[0] = 1.0f / S[0];
    } else if (m == 2) {
        float det = S[0]*S[3] - S[1]*S[2];
        if (fabsf(det) > 1e-15f) {
            float invDet = 1.0f / det;
            S_inv[0] =  S[3] * invDet;
            S_inv[1] = -S[1] * invDet;
            S_inv[2] = -S[2] * invDet;
            S_inv[3] =  S[0] * invDet;
        }
    } else if (m == 3) {
        // 3×3 伴随矩阵法
        float a = S[0], b = S[1], c = S[2];
        float d = S[3], e = S[4], f = S[5];
        float g = S[6], h = S[7], i = S[8];
        
        float det = a*(e*i - f*h) - b*(d*i - f*g) + c*(d*h - e*g);
        if (fabsf(det) > 1e-15f) {
            float invDet = 1.0f / det;
            S_inv[0] = (e*i - f*h) * invDet;
            S_inv[1] = (c*h - b*i) * invDet;
            S_inv[2] = (b*f - c*e) * invDet;
            S_inv[3] = (f*g - d*i) * invDet;
            S_inv[4] = (a*i - c*g) * invDet;
            S_inv[5] = (c*d - a*f) * invDet;
            S_inv[6] = (d*h - e*g) * invDet;
            S_inv[7] = (b*g - a*h) * invDet;
            S_inv[8] = (a*e - b*d) * invDet;
        }
    }
    
    // K = PH' * S_inv (N×m)
    float K[N * 3] __attribute__((aligned(16))) = {0};
    for (uint8_t i = 0; i < N; i++) {
        for (uint8_t j = 0; j < m; j++) {
            float sum = 0;
            for (uint8_t k = 0; k < m; k++) {
                sum += PHt[i * m + k] * S_inv[k * m + j];
            }
            K[i * m + j] = sum;
        }
    }
    
    // ── 4. 状态更新: X = X + K*y ──
    for (uint8_t i = 0; i < N; i++) {
        float correction = 0;
        for (uint8_t j = 0; j < m; j++) {
            correction += K[i * m + j] * y[j];
        }
        m_X[i] += correction;
    }
    
    // ── 5. 协方差更新: P = (I - K*H) * P ──
    // 先算 K*H  (N×N)
    float KH[N * N] __attribute__((aligned(16))) = {0};
    for (uint8_t i = 0; i < N; i++) {
        for (uint8_t j = 0; j < N; j++) {
            float sum = 0;
            for (uint8_t k = 0; k < m; k++) {
                sum += K[i * m + k] * H[k * N + j];
            }
            KH[i * N + j] = sum;
        }
    }
    
    // P = P - KH*P
    float P_new[N * N] __attribute__((aligned(16)));
    for (uint8_t i = 0; i < N; i++) {
        for (uint8_t j = 0; j < N; j++) {
            float sum = 0;
            for (uint8_t k = 0; k < N; k++) {
                sum += KH[i * N + k] * m_P[k * N + j];
            }
            P_new[i * N + j] = m_P[i * N + j] - sum;
        }
    }
    
    for (uint8_t i = 0; i < N * N; i++) m_P[i] = P_new[i];
}

// ═══════════════════════════════════════════════════════════
//  状态访问
// ═══════════════════════════════════════════════════════════

void KalmanFilter6D::getState(float& x, float& y, float& z,
                               float& vx, float& vy, float& vz) {
    x  = m_X[0];  y  = m_X[1];  z  = m_X[2];
    vx = m_X[3];  vy = m_X[4];  vz = m_X[5];
}

void KalmanFilter6D::getUncertainty(float& ux, float& uy, float& uz) {
    ux = sqrtf(fmaxf(m_P[0 * N + 0], 0.0f));
    uy = sqrtf(fmaxf(m_P[1 * N + 1], 0.0f));
    uz = sqrtf(fmaxf(m_P[2 * N + 2], 0.0f));
}

void KalmanFilter6D::reset() {
    for (uint8_t i = 0; i < N; i++) m_X[i] = 0.0f;
    for (uint8_t i = 0; i < N * N; i++) m_P[i] = 0.0f;
    m_P[0 * N + 0] = m_P[1 * N + 1] = m_P[2 * N + 2] = 1000000.0f;
    m_P[3 * N + 3] = m_P[4 * N + 4] = m_P[5 * N + 5] = 10000.0f;
}
