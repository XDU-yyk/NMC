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

    // ESP32-S3 is not allowed to arm motors in this project. Motor output,
    // arming, failsafe, and ESC driving belong to Betaflight/F4V3S.
    void arm() {}
    void disarm() {}
    void emergencyStop() {}
    bool isArmed() const { return false; }
    void setThrottle(float) {}
};

extern MotorController motors;

#endif // MOTOR_H
