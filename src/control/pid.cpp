/**
 * @file    pid.cpp
 * @brief   PID 控制器实现
 */

#include "control/pid.h"

void PIDController::configure(float kp, float ki, float kd,
                               float outMin, float outMax, float freq) {
    m_kp = kp;
    m_ki = ki;
    m_kd = kd;
    m_outMin = outMin;
    m_outMax = outMax;
    m_defaultDt = 1.0f / freq;
    reset();
}

void PIDController::reset() {
    m_integral = 0;
    m_prevError = 0;
    m_prevDerivative = 0;
    m_output = 0;
    m_pTerm = m_iTerm = m_dTerm = 0;
    m_lastTime = 0;
}

void PIDController::setIntegralSeparation(float threshold) {
    m_integralSepThresh = threshold;
}

void PIDController::setIntegral(float value) {
    m_integral = constrain(value, m_outMin, m_outMax);
}

void PIDController::setFeedforward(float ff) {
    m_feedforward = ff;
}

float PIDController::compute(float setpoint, float measured, float dt) {
    uint32_t now = micros();
    
    // 自动计算 dt
    if (dt <= 0) {
        if (m_lastTime == 0) {
            dt = m_defaultDt;
        } else {
            dt = (now - m_lastTime) / 1000000.0f;
            // 限幅 (防溢出)
            if (dt > 0.1f) dt = m_defaultDt;
            if (dt < 0.0001f) dt = m_defaultDt;
        }
    }
    m_lastTime = now;
    
    // ── 误差 ──
    float error = setpoint - measured;
    
    // ── P 项 ──
    m_pTerm = m_kp * error;
    
    // ── I 项 (带积分分离) ──
    if (fabsf(error) < m_integralSepThresh) {
        m_integral += m_ki * error * dt;
        // 抗积分饱和
        m_integral = constrain(m_integral, m_outMin, m_outMax);
    }
    m_iTerm = m_integral;
    
    // ── D 项 (微分 + 一阶低通滤波) ──
    float rawDeriv = (error - m_prevError) / dt;
    // 低通: alpha * prev + (1-alpha) * raw
    constexpr float alpha = 0.7f;  // 滤波系数
    float filteredDeriv = alpha * m_prevDerivative + (1.0f - alpha) * rawDeriv;
    m_prevDerivative = filteredDeriv;
    m_dTerm = m_kd * filteredDeriv;
    
    m_prevError = error;
    
    // ── 输出合成 ──
    m_output = m_pTerm + m_iTerm + m_dTerm + m_feedforward;
    m_output = constrain(m_output, m_outMin, m_outMax);
    
    return m_output;
}

void PIDController::getComponents(float& p, float& i, float& d) {
    p = m_pTerm;
    i = m_iTerm;
    d = m_dTerm;
}
