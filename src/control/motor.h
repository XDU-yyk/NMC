/**
 * @file    motor.h
 * @brief   电机控制桩 (v2.0 — 电机由 F4V3S 飞控直接驱动, ESP32 不再直连电调)
 */

#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

class MotorController {
public:
    void begin() {}
    void arm()   { m_armed = true; }
    void disarm(){ m_armed = false; }
    void emergencyStop() { m_armed = false; }
    bool isArmed() const { return m_armed; }
    void setThrottle(float t) {}
private:
    bool m_armed = false;
};

extern MotorController motors;

#endif // MOTOR_H
