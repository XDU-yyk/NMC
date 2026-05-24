/**
 * @file    follow.cpp
 * @brief   跟随控制实现 — 位置 PID → MSP 虚拟 RC 通道
 */

#include "follow/follow.h"
#include <cmath>

FollowController followCtrl;

void FollowController::begin() {
    m_pidIntegralX = m_pidIntegralY = m_pidIntegralZ = 0;
    m_pidLastErrorX = m_pidLastErrorY = m_pidLastErrorZ = 0;
    m_mode = FollowMode::IDLE;
    LOG(LOG_TAG_FOLLOW, "Follow controller init — MSP passthrough mode");
}

void FollowController::setHomePosition(float x, float y, float z) {
    m_homeX = x;
    m_homeY = y;
    m_homeZ = z;
    LOG(LOG_TAG_FOLLOW, "Home: (%.2f, %.2f, %.2f) m", x, y, z);
}

void FollowController::setMode(FollowMode mode) {
    if (m_mode == mode) return;
    LOG(LOG_TAG_FOLLOW, "Mode: %d → %d", (int)m_mode, (int)mode);
    m_mode = mode;
    // 模式切换时重置 PID 积分
    m_pidIntegralX = m_pidIntegralY = m_pidIntegralZ = 0;
}

void FollowController::emergencyStop() {
    FCOutput out;
    zeroOutput(out);
    out.throttle = 1000;  // 最低油门
    out.overrideRC = true;
    fcBridge.setOutput(out);
    m_mode = FollowMode::EMERGENCY;
    LOG(LOG_TAG_FOLLOW, "EMERGENCY STOP!");
}

void FollowController::update() {
    uint32_t now = millis();
    if (now - m_lastUpdate < INTERVAL_FOLLOW) return;

    float dt = (float)(now - m_lastUpdate) / 1000.0f;
    m_lastUpdate = now;

    const FusionState& state = fusion.getState();
    FCOutput out;
    zeroOutput(out);

    switch (m_mode) {
        case FollowMode::IDLE:
            out.overrideRC = false;  // 释放 RC 控制，交给遥控器
            break;
        case FollowMode::HOVER:
            out = computeHoverOutput(state);
            break;
        case FollowMode::FOLLOW:
            out = computeFollowOutput(state);
            break;
        case FollowMode::RETURN_HOME:
            out = computeReturnOutput(state);
            break;
        case FollowMode::EMERGENCY:
            out.throttle = 1000;
            out.overrideRC = true;
            break;
    }

    fcBridge.setOutput(out);
}

/* ── PID 计算 ── */
float FollowController::computePID(float error, float& integral, float& lastError,
                                    float& deriv, float kp, float ki, float kd,
                                    float maxOutput, float dt) {
    if (dt <= 0) return 0;

    // 比例
    float p = kp * error;

    // 积分 (带分离)
    if (fabsf(error) < maxOutput * 0.5f) {
        integral += error * dt;
        // 积分限幅
        if (integral > maxOutput / ki) integral = maxOutput / ki;
        if (integral < -maxOutput / ki) integral = -maxOutput / ki;
    }
    float i = ki * integral;

    // 微分 (低通滤波)
    float rawDeriv = (error - lastError) / dt;
    deriv = deriv * 0.7f + rawDeriv * 0.3f;  // α=0.7 一阶低通
    float d = kd * deriv;
    lastError = error;

    float output = p + i + d;
    if (output > maxOutput)  output = maxOutput;
    if (output < -maxOutput) output = -maxOutput;

    return output;
}

/* ── 悬停: 保持当前位置和高度 ── */
FCOutput FollowController::computeHoverOutput(const FusionState& state) {
    FCOutput out;
    zeroOutput(out);

    // 水平位置误差
    float errorX = -state.posX;  // 目标: (0, 0) 即起飞点
    float errorY = -state.posY;
    float errorZ = m_targetHeight / 1000.0f - state.posZ;  // 目标高度

    // 死区
    if (fabsf(errorX) < ALTITUDE_HOLD_DEADBAND / 1000.0f) errorX = 0;
    if (fabsf(errorY) < ALTITUDE_HOLD_DEADBAND / 1000.0f) errorY = 0;
    if (fabsf(errorZ) < ALTITUDE_HOLD_DEADBAND / 1000.0f) errorZ = 0;

    float dt = INTERVAL_FOLLOW / 1000.0f;

    float rollCmd  = computePID(errorY, m_pidIntegralY, m_pidLastErrorY, m_pidDerivY,
                                 POS_PID_P, POS_PID_I, POS_PID_D, POS_PID_MAX, dt);
    float pitchCmd = computePID(errorX, m_pidIntegralX, m_pidLastErrorX, m_pidDerivX,
                                 POS_PID_P, POS_PID_I, POS_PID_D, POS_PID_MAX, dt);
    float altCmd   = computePID(errorZ, m_pidIntegralZ, m_pidLastErrorZ, m_pidDerivZ,
                                 ALT_PID_P, ALT_PID_I, ALT_PID_D, ALT_PID_MAX, dt);

    // 映射到 RC 通道: 中值 ±500
    out.roll     = constrain((uint16_t)(1500 + rollCmd), 1000, 2000);
    out.pitch    = constrain((uint16_t)(1500 + pitchCmd), 1000, 2000);
    out.throttle = constrain((uint16_t)(1500 + altCmd), 1000, 2000);
    out.yaw      = 1500;   // 保持航向
    out.overrideRC = true;

    return out;
}

/* ── 跟随: 保持与目标的相对位置 ── */
FCOutput FollowController::computeFollowOutput(const FusionState& state) {
    // 与悬停类似，但目标位置 (水平方向) 为相对于目标 Tag 的距离
    // 此处假设目标始终在正前方 m_targetDist 处
    // 实际部署时需结合 UWB 目标位置

    FCOutput out;
    zeroOutput(out);

    float targetX = m_targetDist / 1000.0f;  // 目标在正前方
    float targetY = 0;
    float targetZ = m_targetHeight / 1000.0f;

    float errorX = targetX - state.posX;
    float errorY = targetY - state.posY;
    float errorZ = targetZ - state.posZ;

    // 距离限制
    float dist = sqrtf(state.posX * state.posX + state.posY * state.posY);
    if (dist > FOLLOW_MAX_DIST / 1000.0f) {
        // 目标太远，减速靠近
        errorX *= 0.5f;
        errorY *= 0.5f;
    }

    float dt = INTERVAL_FOLLOW / 1000.0f;

    float rollCmd  = computePID(errorY, m_pidIntegralY, m_pidLastErrorY, m_pidDerivY,
                                 POS_PID_P, POS_PID_I, POS_PID_D, POS_PID_MAX, dt);
    float pitchCmd = computePID(errorX, m_pidIntegralX, m_pidLastErrorX, m_pidDerivX,
                                 POS_PID_P, POS_PID_I, POS_PID_D, POS_PID_MAX, dt);
    float altCmd   = computePID(errorZ, m_pidIntegralZ, m_pidLastErrorZ, m_pidDerivZ,
                                 ALT_PID_P, ALT_PID_I, ALT_PID_D, ALT_PID_MAX, dt);

    out.roll     = constrain((uint16_t)(1500 + rollCmd), 1000, 2000);
    out.pitch    = constrain((uint16_t)(1500 + pitchCmd), 1000, 2000);
    out.throttle = constrain((uint16_t)(1500 + altCmd), 1000, 2000);
    out.yaw      = 1500;
    out.overrideRC = true;

    return out;
}

/* ── 返航: 飞回原点后降落 ── */
FCOutput FollowController::computeReturnOutput(const FusionState& state) {
    FCOutput out;
    zeroOutput(out);

    float errorX = m_homeX - state.posX;
    float errorY = m_homeY - state.posY;
    float errorZ = m_homeZ - state.posZ;  // 回到起飞高度后降落

    float distHome = sqrtf(errorX * errorX + errorY * errorY);

    if (distHome < 0.5f && fabsf(errorZ) < 0.3f) {
        // 已到家，缓慢降落
        out.throttle = 1300;
        out.overrideRC = true;
        return out;
    }

    float dt = INTERVAL_FOLLOW / 1000.0f;

    float rollCmd  = computePID(errorY, m_pidIntegralY, m_pidLastErrorY, m_pidDerivY,
                                 POS_PID_P, POS_PID_I, POS_PID_D, POS_PID_MAX * 0.5f, dt);
    float pitchCmd = computePID(errorX, m_pidIntegralX, m_pidLastErrorX, m_pidDerivX,
                                 POS_PID_P, POS_PID_I, POS_PID_D, POS_PID_MAX * 0.5f, dt);
    float altCmd   = computePID(errorZ, m_pidIntegralZ, m_pidLastErrorZ, m_pidDerivZ,
                                 ALT_PID_P, ALT_PID_I, ALT_PID_D, ALT_PID_MAX * 0.5f, dt);

    out.roll     = constrain((uint16_t)(1500 + rollCmd), 1200, 1800);
    out.pitch    = constrain((uint16_t)(1500 + pitchCmd), 1200, 1800);
    out.throttle = constrain((uint16_t)(1500 + altCmd), 1100, 1900);
    out.yaw      = 1500;
    out.overrideRC = true;

    return out;
}

void FollowController::zeroOutput(FCOutput& out) {
    out.roll      = 1500;
    out.pitch     = 1500;
    out.throttle  = 1000;  // 油门最低 (安全)
    out.yaw       = 1500;
    out.aux1      = 1500;
    out.aux2      = 1500;
    out.overrideRC = false;
}
