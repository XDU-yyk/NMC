/**
 * @file    manual_control.h
 * @brief   方向控制 v0.1 — 遥控/Web 方向输入 → RC 通道 (1000-2000)
 *
 * 作用:
 *   把"方向意图"(前后/左右/偏航/油门, 归一化 -1~+1) 翻译成飞控可理解的
 *   RC 通道值 (1000-2000, 中值 1500), 供 fcBridge.setOutput() / 仿真使用。
 *
 * 定位 (务必先读):
 *   - 这是"自动跟随"的前置台阶: 跟随算法将来算出的方向量, 走的是同一条
 *     "方向量 → RC 通道" 映射, 因此本模块的输出接口与 follow 保持一致。
 *   - MC6C 手柄的定位是"安全接管层"(飞控固件通道配置), 不在本模块内。
 *   - 真机输出默认禁用: 见 ENABLE_REAL_FC_OUTPUT 与运行时安全门。
 *
 * 安全约束 (来自 AGENTS.md):
 *   - 飞控损坏期间禁止任何真机 MSP 写入 / RC override / 解锁 / 电机测试。
 *   - 本模块在飞控修复前只驱动仿真 (fc_sim), 绝不写真飞控。
 *
 * 设计要点:
 *   - 核心映射函数 mapAxisToRC / clampRC 为纯函数, 不依赖 Arduino, 可在
 *     host (g++) 上单元测试, 这是"无硬件可验证"的关键。
 */

#ifndef MANUAL_CONTROL_H
#define MANUAL_CONTROL_H

#include <stdint.h>

/* ================================================================
 * 纯逻辑常量 (host 与 target 共用, 不依赖 Arduino)
 * ================================================================ */
#define MC_RC_MIN            1000
#define MC_RC_MID            1500
#define MC_RC_MAX            2000

/* 方向轴限幅: 手动阶段限制在温和范围, 避免大姿态 (对齐 AGENTS.md ~10° 建议) */
#define MC_AXIS_SPAN         300      // 单轴最大偏离中值 (±300 → 1200~1800)
#define MC_DEADZONE          0.05f    // 摇杆死区 (归一化), 抑制零点漂移

/* 油门: 手动阶段给一个受限的悬停附近范围 */
#define MC_THROTTLE_MIN      1100
#define MC_THROTTLE_HOVER    1500
#define MC_THROTTLE_MAX      1800

/* 输入超时: 超过该时间没收到新方向指令 → 自动归中位 (失控保护) */
#define MC_INPUT_TIMEOUT_MS  500

/* ================================================================
 * 方向意图 (归一化输入, -1.0 ~ +1.0)
 * ================================================================ */
struct DirectionCommand {
    float forward = 0.0f;   // +前 / -后   → pitch
    float right   = 0.0f;   // +右 / -左   → roll
    float yaw     = 0.0f;   // +顺时针/-逆 → yaw
    float throttle = 0.0f;  // +上 / -下 (相对悬停) → throttle
};

/* RC 通道输出 (按 Betaflight 功能名: roll, pitch, yaw, throttle) */
struct RcChannels {
    uint16_t roll     = MC_RC_MID;
    uint16_t pitch    = MC_RC_MID;
    uint16_t yaw      = MC_RC_MID;
    uint16_t throttle = MC_THROTTLE_MIN;
};

/* ================================================================
 * 纯函数 (可 host 测试) — 不含任何 Arduino / 时间依赖
 * ================================================================ */

/* 单轴归一化量 (-1~+1) → RC 值, 含死区与限幅。span 为最大偏离中值。 */
uint16_t mapAxisToRC(float norm, int span, float deadzone);

/* 油门专用: 归一化 (-1~+1) 相对悬停量 → [min, max] 内的 RC 值。 */
uint16_t mapThrottleToRC(float norm, float deadzone);

/* 通道限幅保护, 任何情况下都不越界 [1000,2000]。 */
uint16_t clampRC(int value);

/* 把方向意图整体映射为四通道 (纯函数, 便于测试)。 */
RcChannels mapCommandToChannels(const DirectionCommand& cmd);

/* ================================================================
 * 控制器 (target 使用; 持有超时/接管状态)
 * ================================================================ */
class ManualController {
public:
    void begin();

    /* 设置最新方向意图 (来自 Web / 遥控解析), 记录时间戳。 */
    void setCommand(const DirectionCommand& cmd, uint32_t nowMs);

    /* Web 暂停/遥控接管: 置真则忽略方向意图, 输出归中位 (交还飞控/遥控)。 */
    void setPilotOverride(bool takeover) { m_pilotOverride = takeover; }
    bool isPilotOverride() const { return m_pilotOverride; }

    /* 每帧调用: 处理输入超时归中, 返回当前应输出的 RC 通道。 */
    RcChannels update(uint32_t nowMs);

    /* 最近一次输出 (供遥测/仿真读取)。 */
    const RcChannels& lastChannels() const { return m_last; }

    /* 是否因超时/接管而处于"中位保持"安全态。 */
    bool isNeutralHold() const { return m_neutralHold; }

private:
    DirectionCommand m_cmd;
    RcChannels       m_last;
    uint32_t         m_lastCmdMs   = 0;
    bool             m_pilotOverride = false;
    bool             m_neutralHold   = true;
    bool             m_hasCmd        = false;
};

#endif // MANUAL_CONTROL_H
