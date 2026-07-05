/**
 * @file    follow.h
 * @brief   自主跟随控制 v2.0 (legacy PID; real FC output disabled by default)
 */

#ifndef FOLLOW_H
#define FOLLOW_H

#include "config.h"
#include "fusion/sensor_fusion.h"
#include "comm/fc_bridge.h"
#include <Arduino.h>

enum class FollowMode : uint8_t {
    IDLE        = 0,
    HOVER       = 1,
    FOLLOW      = 2,
    RETURN_HOME = 3,
    EMERGENCY   = 4
};

class FollowController {
public:
    void begin();

    /* 每帧调用。默认不发送真实飞控输出，见 ENABLE_LEGACY_FOLLOW_FC_OUTPUT。 */
    void update();

    /* 模式切换 */
    void setMode(FollowMode mode);
    FollowMode getMode() const { return m_mode; }

    /* 参数设置 */
    void setTargetDistance(float distMm) { m_targetDist = distMm; }
    void setTargetHeight(float heightMm) { m_targetHeight = heightMm; }
    void setHomePosition(float x, float y, float z);

    /* 紧急停机 */
    void emergencyStop();

private:
    FollowMode m_mode = FollowMode::IDLE;

    float m_targetDist   = FOLLOW_TARGET_DIST;
    float m_targetHeight = FOLLOW_TARGET_HEIGHT;

    float m_homeX = 0, m_homeY = 0, m_homeZ = 0;

    /* PID 状态 */
    float m_pidIntegralX = 0, m_pidIntegralY = 0, m_pidIntegralZ = 0;
    float m_pidLastErrorX = 0, m_pidLastErrorY = 0, m_pidLastErrorZ = 0;
    float m_pidDerivX = 0, m_pidDerivY = 0, m_pidDerivZ = 0;

    uint32_t m_lastUpdate = 0;

    /* 内部函数 */
    float computePID(float error, float& integral, float& lastError, float& deriv,
                     float kp, float ki, float kd, float maxOutput, float dt);
    FCOutput computeHoverOutput(const FusionState& state);
    FCOutput computeFollowOutput(const FusionState& state);
    FCOutput computeReturnOutput(const FusionState& state);

    void zeroOutput(FCOutput& out);
};

extern FollowController followCtrl;

#endif // FOLLOW_H
