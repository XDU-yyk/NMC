/**
 * @file    follow.h
 * @brief   自主跟随算法
 * 
 * 基于 UWB 定位的智能跟随系统, 使无人机伞自动锁定
 * 用户位置并保持安全距离与高度。
 * 
 * 跟随策略:
 * 1. 目标锁定: 用户携带 UWB Tag, 系统通过锚点解算 Tag 位置
 * 2. 路径规划: 根据当前位姿与目标位姿计算期望速度矢量
 * 3. 避障: 基于 IMU 加速度突变检测障碍 (简化)
 * 4. 安全边界: 超距/信号丢失 → 悬停等待或返航
 * 
 * 控制架构 (级联 PID):
 *   位置环 (外环) → 速度环 (内环) → 姿态环 → 电机混合器
 */

#ifndef FOLLOW_H
#define FOLLOW_H

#include "config.h"
#include "control/pid.h"
#include "fusion/sensor_fusion.h"
#include <Arduino.h>

/**
 * @brief 跟随模式枚举
 */
enum class FollowMode : uint8_t {
    IDLE,           // 空闲 — 手动控制
    HOVER,          // 悬停 — 保持当前位置
    FOLLOW,         // 跟随 — 锁定目标行进
    RETURN_HOME,    // 返航 — 返回起飞点
    LOST_SIGNAL     // 信号丢失 — 悬停等待
};

/**
 * @brief 跟随目标信息
 */
struct FollowTarget {
    float posX, posY, posZ;   // 目标三维坐标 (mm)
    float velX, velY, velZ;   // 目标速度估计 (mm/s)
    uint32_t lastSeen;        // 最后可见时间
    bool valid;               // 目标有效
};

/**
 * @brief 跟随控制输出
 */
struct FollowOutput {
    float targetRoll;      // 目标 roll 角 (度)
    float targetPitch;     // 目标 pitch 角 (度)
    float targetYaw;       // 目标 yaw 角 (度)
    float targetThrottle;  // 目标油门 (0–1)
    FollowMode mode;       // 当前模式
};

/**
 * @brief 自主跟随控制器
 */
class FollowController {
public:
    /**
     * @brief 初始化跟随控制器
     */
    void begin();
    
    /**
     * @brief 更新跟随控制 (每 50ms 调用)
     * @param state 当前融合状态
     * @param target 目标位置 (UWB Tag)
     * @param output 输出控制量
     */
    void update(const FusionState& state, const FollowTarget& target,
                FollowOutput& output);
    
    /**
     * @brief 设置跟随模式
     */
    void setMode(FollowMode mode);
    FollowMode getMode() const { return m_mode; }
    
    /**
     * @brief 设置跟随参数
     */
    void setTargetDistance(float distMm) { m_targetDist = distMm; }
    void setTargetHeight(float heightMm) { m_targetHeight = heightMm; }
    void setMaxSpeed(float speed) { m_maxSpeed = speed; }
    
    /**
     * @brief 获取设定距离和高度
     */
    float getTargetDistance() const { return m_targetDist; }
    float getTargetHeight() const  { return m_targetHeight; }
    
    /**
     * @brief 设置返航点
     */
    void setHomePosition(float x, float y, float z);
    
private:
    // 级联 PID
    PIDController m_pidPosX;     // 位置 X → 速度 X
    PIDController m_pidPosY;     // 位置 Y → 速度 Y
    PIDController m_pidPosZ;     // 位置 Z → 速度 Z (高度)
    PIDController m_pidVelX;     // 速度 X → 角度 X (roll)
    PIDController m_pidVelY;     // 速度 Y → 角度 Y (pitch)
    PIDController m_pidYaw;      // 偏航
    
    // 模式状态
    FollowMode m_mode = FollowMode::IDLE;
    
    // 参数
    float m_targetDist   = FOLLOW_TARGET_DIST;
    float m_targetHeight = FOLLOW_TARGET_HEIGHT;
    float m_maxSpeed     = FOLLOW_MAX_SPEED;
    
    // 返航点
    float m_homeX = 0, m_homeY = 0, m_homeZ = 0;
    bool  m_homeSet = false;
    
    // 目标丢失计时
    uint32_t m_targetLostTime = 0;
    bool m_targetWasValid = false;
    
    // 悬停油门估计 (自适应)
    float m_hoverThrottle = 0.25f;  // 初始估计 25%
    
    /**
     * @brief 更新悬停油门估计 (基于高度误差积分)
     */
    void updateHoverThrottle(float heightError);
    
    /**
     * @brief 目标位置 → 期望机体速度 (考虑安全距离)
     */
    void computeDesiredVelocity(const FusionState& state,
                                 const FollowTarget& target,
                                 float& vx, float& vy, float& vz);
    /**
     * @brief 期望速度 → 期望姿态 (倾斜角)
     */
    void velocityToAttitude(float vx, float vy,
                             float& roll, float& pitch);
};

// 全局单例
extern FollowController followCtrl;

#endif // FOLLOW_H
