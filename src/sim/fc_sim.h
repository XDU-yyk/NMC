/**
 * @file fc_sim.h
 * @brief Display-only simulated flight-controller telemetry.
 */

#ifndef FC_SIM_H
#define FC_SIM_H

#include <Arduino.h>

enum class SimFcScenario : uint8_t {
    Idle = 0,
    Hover = 1,
    Follow = 2,
    LowBattery = 3,
    Failsafe = 4,
    Manual = 5,     // 方向控制: 响应外部 RC 通道, 积分出位置/朝向
};

struct SimFcState {
    SimFcScenario scenario = SimFcScenario::Idle;
    const char* scenarioName = "idle";
    bool online = true;
    bool simulated = true;
    bool armed = false;
    bool failsafe = false;
    uint8_t flightMode = 0;
    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    float altitudeCm = 0.0f;
    float varioCms = 0.0f;
    float batteryVoltage = 11.4f;
    uint8_t batteryCells = 3;
    uint16_t cycleTimeUs = 1000;
    uint8_t cpuLoad = 18;
    uint8_t linkQuality = 100;
    uint16_t rcRoll = 1500;
    uint16_t rcPitch = 1500;
    uint16_t rcThrottle = 1000;
    uint16_t rcYaw = 1500;

    /* ── 运动学状态 (Manual 场景下由 RC 通道积分得到) ── */
    float posX = 0.0f;      // 机体前后累计位移 (m), +前
    float posY = 0.0f;      // 机体左右累计位移 (m), +右
    float velX = 0.0f;      // 前后速度 (m/s)
    float velY = 0.0f;      // 左右速度 (m/s)
    float headingDeg = 0.0f;// 航向 (0-360), 由 yaw 通道积分
    bool  neutralHold = true; // 是否处于中位保持 (无有效方向指令)
};

class SimFlightController {
public:
    void begin();
    void update();
    bool setScenario(SimFcScenario scenario);
    bool setScenarioByName(const char* name);
    const SimFcState& getState() const { return m_state; }

    /* 方向控制入口: 喂入 RC 通道 (1000-2000) + neutralHold 标志。
       仅在 Manual 场景下驱动运动学积分; 其它场景忽略。 */
    void feedRcChannels(uint16_t roll, uint16_t pitch, uint16_t yaw,
                        uint16_t throttle, bool neutralHold);

    /* 复位运动学累计量 (回到原点) */
    void resetKinematics();

private:
    SimFcState m_state;
    uint32_t m_scenarioStartMs = 0;
    uint32_t m_lastKinematicMs = 0;

    /* 外部喂入的最新 RC 通道 (Manual 场景使用) */
    uint16_t m_inRoll = 1500, m_inPitch = 1500, m_inYaw = 1500, m_inThrottle = 1000;
    bool     m_inNeutralHold = true;

    void applyScenarioBase();
    void integrateKinematics(uint32_t nowMs);
    static const char* scenarioName(SimFcScenario scenario);
};

extern SimFlightController simFc;

#endif
