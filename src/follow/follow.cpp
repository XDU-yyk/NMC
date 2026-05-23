/**
 * @file    follow.cpp
 * @brief   自主跟随算法实现
 */

#include "follow/follow.h"

FollowController followCtrl;

// ═══════════════════════════════════════════════════════════
//  初始化
// ═══════════════════════════════════════════════════════════

void FollowController::begin() {
    // 位置环 PID (外环): 位置误差 → 期望速度
    m_pidPosX.configure(0.8f, 0.02f, 0.05f, -m_maxSpeed, m_maxSpeed, 20.0f);
    m_pidPosY.configure(0.8f, 0.02f, 0.05f, -m_maxSpeed, m_maxSpeed, 20.0f);
    m_pidPosZ.configure(1.2f, 0.05f, 0.15f,
                        -ALTITUDE_MAX_RATE, ALTITUDE_MAX_RATE, 20.0f);
    
    // 速度环 PID (内环): 速度误差 → 期望角度
    m_pidVelX.configure(PID_ROLL_P, PID_ROLL_I, PID_ROLL_D,
                        -MAX_TILT_ANGLE, MAX_TILT_ANGLE, 50.0f);
    m_pidVelY.configure(PID_PITCH_P, PID_PITCH_I, PID_PITCH_D,
                        -MAX_TILT_ANGLE, MAX_TILT_ANGLE, 50.0f);
    
    // 偏航 PID: 位置偏差在水平面投影的方向角
    m_pidYaw.configure(PID_YAW_P, PID_YAW_I, PID_YAW_D,
                       -180.0f, 180.0f, 20.0f);
    
    LOG(LOG_TAG_FOLLOW, "Follow controller initialized");
}

// ═══════════════════════════════════════════════════════════
//  主更新
// ═══════════════════════════════════════════════════════════

void FollowController::update(const FusionState& state,
                               const FollowTarget& target,
                               FollowOutput& output) {
    output.mode = m_mode;
    
    switch (m_mode) {
        case FollowMode::IDLE:
            // 不输出任何控制量
            output.targetRoll = 0;
            output.targetPitch = 0;
            output.targetYaw = state.yaw;
            output.targetThrottle = 0;
            return;
            
        case FollowMode::HOVER:
            // 位置保持
            {
                float dummyVelX = 0, dummyVelY = 0, dummyVelZ = 0;
                // 位置 PID 以当前位置作为 setpoint
                float targetVx = m_pidPosX.compute(state.posX, state.posX);
                float targetVy = m_pidPosY.compute(state.posY, state.posY);
                float targetVz = m_pidPosZ.compute(m_targetHeight, state.altitude);
                
                velocityToAttitude(targetVx, targetVy,
                                   output.targetRoll, output.targetPitch);
                output.targetYaw = state.yaw;
                output.targetThrottle = m_hoverThrottle;
            }
            return;
            
        case FollowMode::FOLLOW:
            // 跟随模式 — 核心逻辑
            if (!target.valid) {
                // 目标丢失
                if (m_targetWasValid) {
                    m_targetLostTime = millis();
                    m_targetWasValid = false;
                }
                
                // 超时 → 悬停
                if (millis() - m_targetLostTime > SIGNAL_LOST_TIMEOUT) {
                    m_mode = FollowMode::LOST_SIGNAL;
                    LOG(LOG_TAG_FOLLOW, "Target lost > %d ms, entering LOST_SIGNAL",
                        SIGNAL_LOST_TIMEOUT);
                    return;
                }
                
                // 短暂丢失 → 保持最后一个位置
                output.targetRoll = 0;
                output.targetPitch = 0;
                output.targetYaw = state.yaw;
                output.targetThrottle = m_hoverThrottle;
                return;
            }
            
            m_targetWasValid = true;
            
            // 1. 计算期望速度 (位置 → 速度)
            {
            float desVx, desVy, desVz;
            computeDesiredVelocity(state, target, desVx, desVy, desVz);
            
            // 2. 速度 → 姿态 (倾斜角)
            velocityToAttitude(desVx, desVy,
                               output.targetRoll, output.targetPitch);
            }
            
            // 3. 偏航: 始终面向目标
            {
            float dx = target.posX - state.posX;
            float dy = target.posY - state.posY;
            float targetYaw = atan2f(dy, dx) * RAD_TO_DEG;
            output.targetYaw = targetYaw;
            }
            
            // 4. 油门: 高度 PID
            {
            float heightError = m_targetHeight - state.altitude;
            float dzCtrl = m_pidPosZ.compute(m_targetHeight, state.altitude);
            
            // 悬停油门 + 高度修正
            output.targetThrottle = m_hoverThrottle + dzCtrl * 0.001f;
            output.targetThrottle = constrain(output.targetThrottle, 0.0f, 1.0f);
            
            // 更新悬停油门估计
            updateHoverThrottle(heightError);
            }
            break;
            
        case FollowMode::LOST_SIGNAL:
            // 信号丢失: 原地悬停
            output.targetRoll = 0;
            output.targetPitch = 0;
            output.targetYaw = state.yaw;
            output.targetThrottle = m_hoverThrottle;
            break;
            
        case FollowMode::RETURN_HOME:
            // 返航: 以当前位置为起点, home 为目标
            if (m_homeSet) {
                FollowTarget homeTarget;
                homeTarget.posX = m_homeX;
                homeTarget.posY = m_homeY;
                homeTarget.posZ = m_homeZ;
                homeTarget.valid = true;
                homeTarget.lastSeen = millis();
                
                float desVx, desVy, desVz;  // ← 本地变量声明
                computeDesiredVelocity(state, homeTarget, desVx, desVy, desVz);
                velocityToAttitude(desVx, desVy,
                                   output.targetRoll, output.targetPitch);
                output.targetYaw = atan2f(m_homeY - state.posY,
                                          m_homeX - state.posX) * RAD_TO_DEG;
                
                float homeH = m_pidPosZ.compute(m_homeZ, state.posZ);
                output.targetThrottle = m_hoverThrottle + homeH * 0.001f;
                output.targetThrottle = constrain(output.targetThrottle, 0.0f, 1.0f);
                
                // 判断是否到达
                float distHome = sqrtf((m_homeX - state.posX) * (m_homeX - state.posX) +
                                       (m_homeY - state.posY) * (m_homeY - state.posY));
                if (distHome < 200.0f && fabsf(m_homeZ - state.posZ) < 100.0f) {
                    m_mode = FollowMode::HOVER;
                    LOG(LOG_TAG_FOLLOW, "Returned home, switching to HOVER");
                }
            }
            break;
    }
}

// ═══════════════════════════════════════════════════════════
//  期望速度计算
// ═══════════════════════════════════════════════════════════

void FollowController::computeDesiredVelocity(const FusionState& state,
                                               const FollowTarget& target,
                                               float& vx, float& vy, float& vz) {
    // 水平面位置误差
    float dx = target.posX - state.posX;
    float dy = target.posY - state.posY;
    float dist = sqrtf(dx*dx + dy*dy);
    
    // 安全距离保持
    float distError = dist - m_targetDist;
    
    // 死区: 在目标距离 ±100mm 内不修正
    if (fabsf(distError) < 100.0f) {
        vx = 0;
        vy = 0;
    } else {
        // 沿目标方向的速度矢量
        float dirX = (dist > 1.0f) ? dx / dist : 0.0f;
        float dirY = (dist > 1.0f) ? dy / dist : 0.0f;
        
        // 超距: 向目标移动; 过近: 后退
        float speed = m_pidPosX.compute(m_targetDist, dist);
        // 注意: PID 的 setpoint=期望距离, measured=实际距离
        
        vx = speed * dirX;
        vy = speed * dirY;
        
        // 速度限幅
        float spd = sqrtf(vx*vx + vy*vy);
        if (spd > m_maxSpeed) {
            float scale = m_maxSpeed / spd;
            vx *= scale; vy *= scale;
        }
    }
    
    // 垂直速度 (高度 PID)
    vz = m_pidPosZ.compute(m_targetHeight, state.altitude);
}

// ═══════════════════════════════════════════════════════════
//  速度 → 姿态
// ═══════════════════════════════════════════════════════════
//
// 原理: 四旋翼通过倾斜产生水平推力
//  tilt ≈ atan(a / g) 其中 a 是期望加速度
//  速度误差通过 PID 转换为期望加速度，再映射为倾角

void FollowController::velocityToAttitude(float vx, float vy,
                                           float& roll, float& pitch) {
    // 机体坐标系: roll (横滚) → X 轴侧向力, pitch (俯仰) → Y 轴前向力
    // 实际代码需根据当前偏航角旋转速度矢量
    // 简化: 假设偏航锁定 (机体 X 向前)
    
    float accelX = vx * 2.0f;   // 速度 → 加速度 (增益系数)
    float accelY = vy * 2.0f;
    
    // 期望倾角 ≈ atan(accel/g), 限制到 MAX_TILT_ANGLE
    pitch = atanf(accelY / 9.81f) * RAD_TO_DEG;
    roll  = atanf(accelX / 9.81f) * RAD_TO_DEG;
    
    pitch = constrain(pitch, -MAX_TILT_ANGLE, MAX_TILT_ANGLE);
    roll  = constrain(roll,  -MAX_TILT_ANGLE, MAX_TILT_ANGLE);
}

// ═══════════════════════════════════════════════════════════
//  模式 & 参数
// ═══════════════════════════════════════════════════════════

void FollowController::setMode(FollowMode mode) {
    if (m_mode != mode) {
        LOG(LOG_TAG_FOLLOW, "Mode: %d → %d", (int)m_mode, (int)mode);
        m_mode = mode;
        
        // 模式切换时重置 PID
        m_pidPosX.reset();
        m_pidPosY.reset();
        m_pidPosZ.reset();
        m_pidVelX.reset();
        m_pidVelY.reset();
        m_pidYaw.reset();
    }
}

void FollowController::setHomePosition(float x, float y, float z) {
    m_homeX = x; m_homeY = y; m_homeZ = z;
    m_homeSet = true;
    LOG(LOG_TAG_FOLLOW, "Home set: (%.0f, %.0f, %.0f)", x, y, z);
}

void FollowController::updateHoverThrottle(float heightError) {
    // 缓慢自适应: 长期高度误差修正悬停油门估计
    // 防止推力估计漂移
    if (fabsf(heightError) > ALTITUDE_HOLD_DEADBAND) {
        float adj = heightError * 0.00001f;  // 极小修正率
        m_hoverThrottle += adj;
        m_hoverThrottle = constrain(m_hoverThrottle, 0.1f, 0.8f);
    }
}
