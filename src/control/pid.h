/**
 * @file    pid.h
 * @brief   PID 控制器 (带抗积分饱和与微分滤波)
 * 
 * 特性:
 * - 比例 (P): 快速响应
 * - 积分 (I): 消除稳态误差, 带积分分离和抗饱和
 * - 微分 (D): 抑制超调, 带一阶低通滤波降噪
 * - 输出限幅与平滑
 */

#ifndef PID_H
#define PID_H

#include "config.h"
#include <Arduino.h>

/**
 * @brief PID 控制器
 */
class PIDController {
public:
    /**
     * @brief 配置 PID 增益
     * @param kp 比例增益
     * @param ki 积分增益
     * @param kd 微分增益
     * @param outMin, outMax 输出限幅
     * @param freq 控制器频率, 用于计算 dt
     */
    void configure(float kp, float ki, float kd,
                   float outMin, float outMax, float freq);
    
    /**
     * @brief 计算控制输出
     * @param setpoint  目标值
     * @param measured  当前测量值
     * @param dt        自上次更新的时间 (秒), 0 表示自动计算
     * @return 控制输出
     */
    float compute(float setpoint, float measured, float dt = 0);
    
    /**
     * @brief 重置控制器 (清除积分和微分历史)
     */
    void reset();
    
    /**
     * @brief 设置积分分离阈值
     * @param threshold 误差超过此值则禁用积分项
     */
    void setIntegralSeparation(float threshold);
    
    /**
     * @brief 强制设置积分项 (用于初始化解耦)
     */
    void setIntegral(float value);
    
    /**
     * @brief 获取当前 P/I/D 分量 (调试用)
     */
    void getComponents(float& p, float& i, float& d);
    
    /**
     * @brief 设置输出前馈 (用于重力补偿等)
     */
    void setFeedforward(float ff);
    
private:
    float m_kp = 0, m_ki = 0, m_kd = 0;
    float m_outMin = -1000, m_outMax = 1000;
    float m_integralSepThresh = 100.0f;
    
    float m_integral = 0;
    float m_prevError = 0;
    float m_prevDerivative = 0;   // 微分低通滤波
    
    float m_output = 0;
    float m_pTerm = 0, m_iTerm = 0, m_dTerm = 0;
    float m_feedforward = 0;
    
    uint32_t m_lastTime = 0;
    float m_defaultDt = 0.01f;    // 默认步长 (100Hz)
};

#endif // PID_H
