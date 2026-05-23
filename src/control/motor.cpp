/**
 * @file    motor.cpp
 * @brief   四旋翼电机控制实现
 */

#include "control/motor.h"

MotorController motors;

// static constexpr 成员的定义 (C++17 之前需要)
const uint8_t MotorController::m_pins[4];

// ═══════════════════════════════════════════════════════════
//  初始化
// ═══════════════════════════════════════════════════════════

void MotorController::begin() {
    // 配置 PWM 定时器
    for (uint8_t i = 0; i < 4; i++) {
        ledcSetup(i, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
        ledcAttachPin(m_pins[i], i);
        ledcWrite(i, 0);
    }
    
    // 安全开关
    pinMode(SAFETY_SWITCH_PIN, INPUT_PULLUP);
    
    LOG(LOG_TAG_CTRL, "Motor controller initialized (4 motors, %d Hz PWM)",
        MOTOR_PWM_FREQ);
}

// ═══════════════════════════════════════════════════════════
//  油门混合器
// ═══════════════════════════════════════════════════════════

void MotorController::setMixer(float throttle, float roll, float pitch, float yaw) {
    if (!m_armed) return;
    
    // 安全检查
    if (digitalRead(SAFETY_SWITCH_PIN) == LOW) {
        emergencyStop();
        return;
    }
    
    // 约束
    throttle = constrain(throttle, 0.0f, 1.0f);
    
    // X 布局混合
    float m1 = throttle + roll - pitch - yaw;
    float m2 = throttle - roll - pitch + yaw;
    float m3 = throttle - roll + pitch - yaw;
    float m4 = throttle + roll + pitch + yaw;
    
    // 归一化: 防止任一电机超限
    float maxVal = fmaxf(fmaxf(m1, m2), fmaxf(m3, m4));
    float minVal = fminf(fminf(m1, m2), fminf(m3, m4));
    
    if (maxVal > 1.0f) {
        float scale = 1.0f / maxVal;
        m1 *= scale; m2 *= scale; m3 *= scale; m4 *= scale;
    }
    if (minVal < 0.0f) {
        float offset = -minVal;
        m1 += offset; m2 += offset; m3 += offset; m4 += offset;
    }
    
    // 电池补偿
    m1 *= m_batteryComp;
    m2 *= m_batteryComp;
    m3 *= m_batteryComp;
    m4 *= m_batteryComp;
    
    // 写入 PWM
    writeMotor(0, dutyToPWM(m1));
    writeMotor(1, dutyToPWM(m2));
    writeMotor(2, dutyToPWM(m3));
    writeMotor(3, dutyToPWM(m4));
}

// ═══════════════════════════════════════════════════════════
//  安全控制
// ═══════════════════════════════════════════════════════════

void MotorController::emergencyStop() {
    for (uint8_t i = 0; i < 4; i++) {
        writeMotor(i, 0);
    }
    m_armed = false;
    LOG(LOG_TAG_CTRL, "EMERGENCY STOP — all motors off");
}

void MotorController::idle() {
    if (!m_armed) return;
    uint16_t idlePWM = dutyToPWM((float)MOTOR_IDLE_DUTY / 4096.0f);
    for (uint8_t i = 0; i < 4; i++) {
        writeMotor(i, idlePWM);
    }
}

void MotorController::arm() {
    if (digitalRead(SAFETY_SWITCH_PIN) == LOW) {
        LOG(LOG_TAG_CTRL, "Arm rejected: safety switch engaged");
        return;
    }
    
    m_armed = true;
    // 发送怠速信号 (校准电调)
    idle();
    delay(2000);
    LOG(LOG_TAG_CTRL, "Motors armed");
}

void MotorController::disarm() {
    m_armed = false;
    for (uint8_t i = 0; i < 4; i++) {
        writeMotor(i, 0);
    }
    LOG(LOG_TAG_CTRL, "Motors disarmed");
}

// ═══════════════════════════════════════════════════════════
//  辅助
// ═══════════════════════════════════════════════════════════

uint16_t MotorController::dutyToPWM(float duty) {
    duty = constrain(duty, 0.0f, 1.0f);
    return (uint16_t)(MOTOR_IDLE_DUTY + duty * (MOTOR_MAX_DUTY - MOTOR_IDLE_DUTY));
}

void MotorController::writeMotor(uint8_t index, uint16_t pwm) {
    if (index >= 4) return;
    m_duties[index] = pwm;
    ledcWrite(index, pwm);
}

void MotorController::getDuties(uint16_t& m1, uint16_t& m2,
                                 uint16_t& m3, uint16_t& m4) {
    m1 = m_duties[0]; m2 = m_duties[1];
    m3 = m_duties[2]; m4 = m_duties[3];
}

void MotorController::setBatteryCompensation(float voltage) {
    // 电压越低，补偿系数越大 (保持功率恒定)
    if (voltage > 1.0f) {
        m_batteryComp = 12.6f / voltage;  // 3S LiPo 满电 12.6V
        m_batteryComp = constrain(m_batteryComp, 0.8f, 1.3f);
    }
}
