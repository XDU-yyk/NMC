/**
 * @file    motor.h
 * @brief   四旋翼电机控制
 * 
 * X 型四旋翼布局:
 *   M4 (前左) ──── M1 (前右)
 *       \         /
 *         \     /
 *          \ /
 *          / \
 *         /     \
 *       /         \
 *   M3 (后左) ──── M2 (后右)
 * 
 * 控制分配:
 *   M1 = throttle + roll_PID - pitch_PID - yaw_PID
 *   M2 = throttle - roll_PID - pitch_PID + yaw_PID
 *   M3 = throttle - roll_PID + pitch_PID - yaw_PID
 *   M4 = throttle + roll_PID + pitch_PID + yaw_PID
 */

#ifndef MOTOR_H
#define MOTOR_H

#include "config.h"
#include <Arduino.h>

/**
 * @brief 电机控制类
 */
class MotorController {
public:
    /**
     * @brief 初始化 PWM 输出
     */
    void begin();
    
    /**
     * @brief 设置油门和姿态控制量
     * @param throttle  基础油门 (0–1)
     * @param roll      roll 轴控制量
     * @param pitch     pitch 轴控制量
     * @param yaw       yaw 轴控制量
     */
    void setMixer(float throttle, float roll, float pitch, float yaw);
    
    /**
     * @brief 紧急停机 — 立即关闭所有电机
     */
    void emergencyStop();
    
    /**
     * @brief 怠速 — 最低转速 (不解锁)
     */
    void idle();
    
    /**
     * @brief 解锁电机 (上电后调用)
     */
    void arm();
    
    /**
     * @brief 锁定电机
     */
    void disarm();
    
    /**
     * @brief 是否已解锁
     */
    bool isArmed() const { return m_armed; }
    
    /**
     * @brief 获取各电机当前占空比 (调试)
     */
    void getDuties(uint16_t& m1, uint16_t& m2, uint16_t& m3, uint16_t& m4);
    
    /**
     * @brief 设置电池电压补偿系数
     */
    void setBatteryCompensation(float voltage);
    
private:
    bool m_armed = false;
    uint16_t m_duties[4] = {0};
    float m_batteryComp = 1.0f;
    
    // PWM 通道
    static constexpr uint8_t m_pins[4] = {MOTOR1_PIN, MOTOR2_PIN, MOTOR3_PIN, MOTOR4_PIN};
    
    /**
     * @brief 将归一化占空比 (0–1) 转为 PWM 值
     */
    uint16_t dutyToPWM(float duty);
    
    /**
     * @brief 写入单个电机 PWM
     */
    void writeMotor(uint8_t index, uint16_t pwm);
};

// 全局单例
extern MotorController motors;

#endif // MOTOR_H
