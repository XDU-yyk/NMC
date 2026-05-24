/**
 * @file    pid.h
 * @brief   PID 控制器桩 (v2.0 — 姿态 PID 移交 F4V3S; 保留位置 PID 供跟随层使用)
 */

#ifndef PID_H
#define PID_H

#include <Arduino.h>

struct PIDConfig {
    float kp, ki, kd;
    float integralLimit;
    float outputLimit;
};

class PID {
public:
    void begin(float p, float i, float d, float outMax) {}
    float compute(float setpoint, float measurement, float dt) { return 0; }
    void reset() {}
};

#endif // PID_H
