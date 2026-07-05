/**
 * @file    manual_control.cpp
 * @brief   方向控制实现 — 方向意图 → RC 通道, 含死区/限幅/超时归中/接管
 *
 * 纯映射函数 (mapAxisToRC / mapThrottleToRC / clampRC / mapCommandToChannels)
 * 不依赖 Arduino, 可在 host 上用 g++ 直接编译测试 (见 test/test_manual_control.cpp)。
 * ManualController 仅额外持有时间戳/状态, 时间由调用方传入 (nowMs), 便于测试。
 */

#include "control/manual_control.h"

/* ── 简单工具: 不引入 <algorithm>, 保持轻量 ── */
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float sanitizeNorm(float norm) {
    return norm == norm ? norm : 0.0f;
}

static inline float applyDeadzone(float norm, float deadzone) {
    if (norm > -deadzone && norm < deadzone) return 0.0f;
    // 死区外线性重映射, 保证从死区边界平滑起步 (无跳变)
    float sign = norm > 0.0f ? 1.0f : -1.0f;
    float mag  = (norm > 0 ? norm : -norm);
    float scaled = (mag - deadzone) / (1.0f - deadzone);
    return sign * clampf(scaled, 0.0f, 1.0f);
}

uint16_t clampRC(int value) {
    if (value < MC_RC_MIN) return MC_RC_MIN;
    if (value > MC_RC_MAX) return MC_RC_MAX;
    return (uint16_t)value;
}

uint16_t mapAxisToRC(float norm, int span, float deadzone) {
    float n = clampf(sanitizeNorm(norm), -1.0f, 1.0f);
    n = applyDeadzone(n, deadzone);
    int offset = (int)(n * (float)span);
    return clampRC(MC_RC_MID + offset);
}

uint16_t mapThrottleToRC(float norm, float deadzone) {
    float n = clampf(sanitizeNorm(norm), -1.0f, 1.0f);
    n = applyDeadzone(n, deadzone);
    // 以悬停为中心, 上下各取到 MAX/MIN 的一半行程
    int up   = MC_THROTTLE_MAX - MC_THROTTLE_HOVER;
    int down = MC_THROTTLE_HOVER - MC_THROTTLE_MIN;
    int offset = (int)(n * (float)(n >= 0 ? up : down));
    int v = MC_THROTTLE_HOVER + offset;
    if (v < MC_THROTTLE_MIN) v = MC_THROTTLE_MIN;
    if (v > MC_THROTTLE_MAX) v = MC_THROTTLE_MAX;
    return clampRC(v);
}

RcChannels mapCommandToChannels(const DirectionCommand& cmd) {
    RcChannels ch;
    ch.roll     = mapAxisToRC(cmd.right,   MC_AXIS_SPAN, MC_DEADZONE);
    ch.pitch    = mapAxisToRC(cmd.forward, MC_AXIS_SPAN, MC_DEADZONE);
    ch.yaw      = mapAxisToRC(cmd.yaw,     MC_AXIS_SPAN, MC_DEADZONE);
    ch.throttle = mapThrottleToRC(cmd.throttle, MC_DEADZONE);
    return ch;
}

/* 中位保持: 所有姿态轴归中, 油门降到受限最小 (安全态) */
static RcChannels neutralChannels() {
    RcChannels ch;
    ch.roll     = MC_RC_MID;
    ch.pitch    = MC_RC_MID;
    ch.yaw      = MC_RC_MID;
    ch.throttle = MC_THROTTLE_MIN;
    return ch;
}

/* ================================================================
 * ManualController
 * ================================================================ */
void ManualController::begin() {
    m_cmd = DirectionCommand{};
    m_last = neutralChannels();
    m_lastCmdMs = 0;
    m_pilotOverride = false;
    m_neutralHold = true;
    m_hasCmd = false;
}

void ManualController::setCommand(const DirectionCommand& cmd, uint32_t nowMs) {
    m_cmd = cmd;
    m_lastCmdMs = nowMs;
    m_hasCmd = true;
}

RcChannels ManualController::update(uint32_t nowMs) {
    // Web 暂停/遥控接管优先: 直接归中位, 把控制权交还遥控/飞控
    if (m_pilotOverride) {
        m_neutralHold = true;
        m_last = neutralChannels();
        return m_last;
    }

    // 输入超时保护: 长时间无新方向意图 → 归中位 (失控兜底)
    bool timedOut = !m_hasCmd || (nowMs - m_lastCmdMs > MC_INPUT_TIMEOUT_MS);
    if (timedOut) {
        m_neutralHold = true;
        m_last = neutralChannels();
        return m_last;
    }

    m_neutralHold = false;
    m_last = mapCommandToChannels(m_cmd);
    return m_last;
}
