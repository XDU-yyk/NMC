/**
 * @file    kalman.cpp
 * @brief   9状态 EKF 实现 — 矩阵运算内联，无外部依赖
 * 
 * 所有矩阵运算均手写 9×9，利用 ESP32-S3 SIMD 友好的内存对齐。
 */

#include "fusion/kalman.h"
#include <cmath>

static float KF_DATA_ALIGN[KF_N * KF_N * 4] __attribute__((aligned(16)));

void KalmanFilter::begin(float initX, float initY, float initZ) {
    matSetZero(m_X, KF_N);
    m_X[0] = initX;
    m_X[1] = initY;
    m_X[2] = initZ;

    matSetIdentity(m_P, KF_N);
    // 初始位置不确定度: (1m)², 速度: (0.5m/s)², 偏置: (0.1m/s²)²
    m_P[0*KF_N + 0] = 1.0f;
    m_P[1*KF_N + 1] = 1.0f;
    m_P[2*KF_N + 2] = 1.0f;
    m_P[3*KF_N + 3] = 0.25f;
    m_P[4*KF_N + 4] = 0.25f;
    m_P[5*KF_N + 5] = 0.25f;
    m_P[6*KF_N + 6] = 0.01f;
    m_P[7*KF_N + 7] = 0.01f;
    m_P[8*KF_N + 8] = 0.01f;

    m_converged = false;
    m_lastPredict = micros();
}

/* ── 预测步 ── */
void KalmanFilter::predict(float dt, float ax, float ay, float az) {
    if (dt <= 0) return;

    float dt2 = dt * dt;
    float dt3_2 = dt2 * dt / 2.0f;

    // 状态转移: X = F * X + B * u
    // x  = x + vx*dt + 0.5*(ax - bias_ax)*dt²
    // vx = vx + (ax - bias_ax)*dt
    // bias_ax = bias_ax (常值模型)

    float accX = ax - m_X[6];  // 去除加速度偏置
    float accY = ay - m_X[7];
    float accZ = az - m_X[8];

    float newX[KF_N];
    newX[0] = m_X[0] + m_X[3] * dt + 0.5f * accX * dt2;
    newX[1] = m_X[1] + m_X[4] * dt + 0.5f * accY * dt2;
    newX[2] = m_X[2] + m_X[5] * dt + 0.5f * accZ * dt2;
    newX[3] = m_X[3] + accX * dt;
    newX[4] = m_X[4] + accY * dt;
    newX[5] = m_X[5] + accZ * dt;
    newX[6] = m_X[6];  // 偏置常值模型
    newX[7] = m_X[7];
    newX[8] = m_X[8];

    // 构造雅可比 F (9×9)
    float F[KF_N * KF_N];
    matSetIdentity(F, KF_N);
    // F[i][i+3] = dt   (位置 ← 速度)
    F[0*KF_N + 3] = dt;
    F[1*KF_N + 4] = dt;
    F[2*KF_N + 5] = dt;
    // F[i+3][i+6] = -dt  (速度 ← 加速度偏置)
    F[3*KF_N + 6] = -dt;
    F[4*KF_N + 7] = -dt;
    F[5*KF_N + 8] = -dt;

    // P = F * P * F^T + Q
    float F_P[KF_N * KF_N], F_P_FT[KF_N * KF_N], FT[KF_N * KF_N];
    matMultiply(F, m_P, F_P, KF_N);
    matTranspose(F, FT, KF_N);
    matMultiply(F_P, FT, F_P_FT, KF_N);

    // 过程噪声 Q = G * Qc * G^T (连续时间白噪声)
    // Q 简化为对角矩阵
    float Q[KF_N * KF_N];
    matSetZero(Q, KF_N);
    float qAccel = KF_PROCESS_NOISE_ACCEL * dt2;  // 加速度噪声 × dt²
    // 位置: σ²_acc * dt⁴/4
    Q[0*KF_N + 0] = qAccel * dt2 / 4.0f;
    Q[1*KF_N + 1] = qAccel * dt2 / 4.0f;
    Q[2*KF_N + 2] = qAccel * dt2 / 4.0f;
    // 速度: σ²_acc * dt²
    Q[3*KF_N + 3] = qAccel;
    Q[4*KF_N + 4] = qAccel;
    Q[5*KF_N + 5] = qAccel;
    // 偏置: 小值 (慢变)
    Q[6*KF_N + 6] = 0.0001f * dt;
    Q[7*KF_N + 7] = 0.0001f * dt;
    Q[8*KF_N + 8] = 0.0001f * dt;

    matAdd(F_P_FT, Q, m_P, KF_N);
    memcpy(m_X, newX, sizeof(newX));

    m_lastPredict = micros();
}

/* ── 通用单行标量更新 ── */
void KalmanFilter::updateScalar(const float* H, int H_len, float z, float R) {
    // S = H * P * H^T + R
    float P_HT[KF_N];  // 临时: P * H^T
    for (int i = 0; i < KF_N; i++) {
        float sum = 0;
        for (int j = 0; j < KF_N; j++) {
            sum += m_P[i * KF_N + j] * H[j];
        }
        P_HT[i] = sum;
    }

    float S = 0;
    for (int i = 0; i < KF_N; i++) {
        S += H[i] * P_HT[i];
    }
    S += R;

    if (fabsf(S) < 1e-9f) return;  // 数值保护

    // K = P_HT / S
    float K[KF_N];
    for (int i = 0; i < KF_N; i++) {
        K[i] = P_HT[i] / S;
    }

    // 创新 y = z - H*x
    float y = z;
    for (int i = 0; i < KF_N; i++) {
        y -= H[i] * m_X[i];
    }

    // X = X + K * y
    for (int i = 0; i < KF_N; i++) {
        m_X[i] += K[i] * y;
    }

    // P = (I - K*H) * P
    float KH[KF_N * KF_N];
    for (int i = 0; i < KF_N; i++) {
        for (int j = 0; j < KF_N; j++) {
            KH[i * KF_N + j] = K[i] * H[j];
        }
    }

    float I_KH[KF_N * KF_N];
    matSetIdentity(I_KH, KF_N);
    for (int i = 0; i < KF_N * KF_N; i++) {
        I_KH[i] -= KH[i];
    }

    float I_KH_P[KF_N * KF_N];
    matMultiply(I_KH, m_P, I_KH_P, KF_N);
    matCopy(I_KH_P, m_P, KF_N);

    m_converged = true;
}

/* ── 各传感器更新 ── */

void KalmanFilter::updateGPS(float x, float y, float z, float hdopScale) {
    float R_pos = KF_GPS_NOISE_POS * KF_GPS_NOISE_POS * hdopScale;

    // H = [1 0 0 0 0 0 0 0 0]  for x
    float H[KF_N];
    memset(H, 0, sizeof(H));
    H[0] = 1; updateScalar(H, KF_N, x, R_pos);
    H[0] = 0; H[1] = 1; updateScalar(H, KF_N, y, R_pos);
    H[1] = 0; H[2] = 1; updateScalar(H, KF_N, z, R_pos * 1.5f);  // GPS 高度噪声更大
}

void KalmanFilter::updateUWB(float x, float y, float z) {
    float R_uwb = KF_UWB_NOISE_POS * KF_UWB_NOISE_POS;

    float H[KF_N];
    memset(H, 0, sizeof(H));
    H[0] = 1; updateScalar(H, KF_N, x, R_uwb);
    H[0] = 0; H[1] = 1; updateScalar(H, KF_N, y, R_uwb);
    H[1] = 0; H[2] = 1; updateScalar(H, KF_N, z, R_uwb);
}

void KalmanFilter::updateToF(float z) {
    float R_tof = KF_TOF_NOISE_Z * KF_TOF_NOISE_Z;

    float H[KF_N];
    memset(H, 0, sizeof(H));
    H[2] = 1;  // H = [0 0 1 0 ...]
    updateScalar(H, KF_N, z, R_tof);
}

void KalmanFilter::updateBaro(float z) {
    float R_baro = KF_BARO_NOISE_Z * KF_BARO_NOISE_Z;

    float H[KF_N];
    memset(H, 0, sizeof(H));
    H[2] = 1;
    updateScalar(H, KF_N, z, R_baro);
}

void KalmanFilter::reset(float initX, float initY, float initZ) {
    begin(initX, initY, initZ);
}

/* ═══════════════════════════════════════════════════════════
 *  矩阵运算 (9×9 固定尺寸, SIMD 对齐)
 * ═══════════════════════════════════════════════════════════ */

void KalmanFilter::matMultiply(const float* A, const float* B, float* C, int n) {
    // A[n×n] × B[n×n] = C[n×n]  (朴素 O(n³))
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0;
            for (int k = 0; k < n; k++) {
                sum += A[i * n + k] * B[k * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

void KalmanFilter::matTranspose(const float* A, float* AT, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            AT[i * n + j] = A[j * n + i];
        }
    }
}

void KalmanFilter::matAdd(const float* A, const float* B, float* C, int n) {
    for (int i = 0; i < n * n; i++) {
        C[i] = A[i] + B[i];
    }
}

void KalmanFilter::matSubtract(const float* A, const float* B, float* C, int n) {
    for (int i = 0; i < n * n; i++) {
        C[i] = A[i] - B[i];
    }
}

void KalmanFilter::matSetIdentity(float* A, int n) {
    matSetZero(A, n);
    for (int i = 0; i < n; i++) {
        A[i * n + i] = 1.0f;
    }
}

void KalmanFilter::matSetZero(float* A, int n) {
    memset(A, 0, n * n * sizeof(float));
}

void KalmanFilter::matScale(float* A, float s, int n) {
    for (int i = 0; i < n * n; i++) {
        A[i] *= s;
    }
}

void KalmanFilter::matCopy(const float* src, float* dst, int n) {
    memcpy(dst, src, n * n * sizeof(float));
}

/* 行列式 (未使用, 保留) */
float KalmanFilter::matDeterminant(const float* A, int n) {
    // 仅用于调试, 朴素实现 (O(n³))
    float tmp[KF_N * KF_N];
    memcpy(tmp, A, n * n * sizeof(float));
    float det = 1.0f;
    for (int i = 0; i < n; i++) {
        float piv = tmp[i * n + i];
        if (fabsf(piv) < 1e-9f) return 0;
        det *= piv;
        for (int j = i + 1; j < n; j++) {
            float f = tmp[j * n + i] / piv;
            for (int k = i; k < n; k++) {
                tmp[j * n + k] -= f * tmp[i * n + k];
            }
        }
    }
    return det;
}

/* 矩阵求逆 (Gauss-Jordan, 未使用, 保留) */
bool KalmanFilter::matInverse(const float* A, float* Ainv, int n) {
    float aug[KF_N * KF_N * 2];
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            aug[i * (n * 2) + j] = A[i * n + j];
            aug[i * (n * 2) + n + j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    for (int i = 0; i < n; i++) {
        float piv = aug[i * (n * 2) + i];
        if (fabsf(piv) < 1e-9f) return false;
        for (int j = 0; j < n * 2; j++) aug[i * (n * 2) + j] /= piv;
        for (int k = 0; k < n; k++) {
            if (k == i) continue;
            float f = aug[k * (n * 2) + i];
            for (int j = 0; j < n * 2; j++) aug[k * (n * 2) + j] -= f * aug[i * (n * 2) + j];
        }
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            Ainv[i * n + j] = aug[i * (n * 2) + n + j];
        }
    }
    return true;
}
