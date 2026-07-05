/**
 * @file fc_sim.cpp
 * @brief Simulated flight-controller telemetry + Manual 方向控制运动学。
 */

#include "sim/fc_sim.h"
#include <string.h>
#include <math.h>

SimFlightController simFc;

void SimFlightController::begin()
{
    setScenario(SimFcScenario::Idle);
}

bool SimFlightController::setScenario(SimFcScenario scenario)
{
    m_state.scenario = scenario;
    m_state.scenarioName = scenarioName(scenario);
    m_scenarioStartMs = millis();
    applyScenarioBase();
    return true;
}

bool SimFlightController::setScenarioByName(const char* name)
{
    if (!name) return false;
    if (strcmp(name, "idle") == 0) return setScenario(SimFcScenario::Idle);
    if (strcmp(name, "hover") == 0) return setScenario(SimFcScenario::Hover);
    if (strcmp(name, "follow") == 0) return setScenario(SimFcScenario::Follow);
    if (strcmp(name, "low_battery") == 0) return setScenario(SimFcScenario::LowBattery);
    if (strcmp(name, "failsafe") == 0) return setScenario(SimFcScenario::Failsafe);
    if (strcmp(name, "manual") == 0) return setScenario(SimFcScenario::Manual);
    return false;
}

void SimFlightController::resetKinematics()
{
    m_state.posX = m_state.posY = 0.0f;
    m_state.velX = m_state.velY = 0.0f;
    m_state.headingDeg = 0.0f;
    m_lastKinematicMs = millis();
}

void SimFlightController::feedRcChannels(uint16_t roll, uint16_t pitch, uint16_t yaw,
                                         uint16_t throttle, bool neutralHold)
{
    m_inRoll = roll;
    m_inPitch = pitch;
    m_inYaw = yaw;
    m_inThrottle = throttle;
    m_inNeutralHold = neutralHold;
}

/* 简化运动学: RC 通道偏离中值 -> 目标速度, 一阶惯性逼近, 再积分成位移。
   纯演示用, 不代表真实飞行动力学; 目的是让 Web 直观看到"按方向移动"。 */
void SimFlightController::integrateKinematics(uint32_t nowMs)
{
    if (m_lastKinematicMs == 0) { m_lastKinematicMs = nowMs; return; }
    float dt = (nowMs - m_lastKinematicMs) * 0.001f;
    m_lastKinematicMs = nowMs;
    if (dt <= 0.0f || dt > 0.5f) dt = 0.05f;

    const float MID = 1500.0f;
    float nPitch = (m_inPitch - MID) / 300.0f;
    float nRoll  = (m_inRoll  - MID) / 300.0f;
    float nYaw   = (m_inYaw   - MID) / 300.0f;

    m_state.neutralHold = m_inNeutralHold;
    if (m_inNeutralHold) {
        m_state.velX *= 0.85f;
        m_state.velY *= 0.85f;
        m_state.roll = 0.0f;
        m_state.pitch = 0.0f;
    } else {
        const float MAX_SPEED = 2.0f;
        float targetVX = nPitch * MAX_SPEED;
        float targetVY = nRoll  * MAX_SPEED;
        float k = dt / (0.4f + dt);
        m_state.velX += (targetVX - m_state.velX) * k;
        m_state.velY += (targetVY - m_state.velY) * k;
        m_state.headingDeg += nYaw * 60.0f * dt;
        while (m_state.headingDeg < 0)    m_state.headingDeg += 360.0f;
        while (m_state.headingDeg >= 360) m_state.headingDeg -= 360.0f;
        m_state.roll  = nRoll  * 12.0f;
        m_state.pitch = nPitch * 12.0f;
    }

    m_state.posX += m_state.velX * dt;
    m_state.posY += m_state.velY * dt;
    m_state.yaw = m_state.headingDeg;

    m_state.rcRoll = m_inRoll;
    m_state.rcPitch = m_inPitch;
    m_state.rcYaw = m_inYaw;
    m_state.rcThrottle = m_inThrottle;
    float nThr = (m_inThrottle - 1500.0f) / 300.0f;
    m_state.altitudeCm = 120.0f + nThr * 40.0f;
    m_state.varioCms = nThr * 20.0f;
}

void SimFlightController::update()
{
    const float t = millis() * 0.001f;
    const float scenarioT = (millis() - m_scenarioStartMs) * 0.001f;

    if (m_state.scenario == SimFcScenario::Manual) {
        applyScenarioBase();
        m_state.armed = true;
        m_state.flightMode = 5;
        integrateKinematics(millis());
        m_state.cycleTimeUs = 1000;
        m_state.cpuLoad = 22;
        return;
    }

    applyScenarioBase();

    switch (m_state.scenario) {
        case SimFcScenario::Idle:
            m_state.roll = 0.5f * sinf(t * 0.7f);
            m_state.pitch = 0.4f * sinf(t * 0.6f + 0.5f);
            m_state.yaw = fmodf(t * 2.0f, 360.0f);
            m_state.altitudeCm = 0.0f;
            m_state.varioCms = 0.0f;
            break;

        case SimFcScenario::Hover:
            m_state.roll = 1.8f * sinf(t * 1.3f);
            m_state.pitch = 1.5f * sinf(t * 1.1f + 0.7f);
            m_state.yaw = fmodf(45.0f + t * 4.0f, 360.0f);
            m_state.altitudeCm = 125.0f + 8.0f * sinf(t * 0.8f);
            m_state.varioCms = 6.4f * cosf(t * 0.8f);
            m_state.rcThrottle = 1510 + static_cast<int16_t>(10.0f * sinf(t * 1.4f));
            break;

        case SimFcScenario::Follow:
            m_state.roll = 4.5f * sinf(t * 0.9f);
            m_state.pitch = 6.0f * sinf(t * 0.65f + 0.4f);
            m_state.yaw = fmodf(120.0f + 12.0f * sinf(t * 0.35f), 360.0f);
            m_state.altitudeCm = 145.0f + 12.0f * sinf(t * 0.7f);
            m_state.varioCms = 8.4f * cosf(t * 0.7f);
            m_state.rcRoll = 1500 + static_cast<int16_t>(42.0f * sinf(t * 0.9f));
            m_state.rcPitch = 1500 + static_cast<int16_t>(65.0f * sinf(t * 0.65f + 0.4f));
            m_state.rcThrottle = 1515 + static_cast<int16_t>(18.0f * sinf(t * 1.2f));
            break;

        case SimFcScenario::LowBattery:
            m_state.roll = 2.0f * sinf(t * 0.9f);
            m_state.pitch = 2.5f * sinf(t * 0.75f + 0.3f);
            m_state.yaw = fmodf(80.0f + t * 2.0f, 360.0f);
            m_state.altitudeCm = 90.0f + 6.0f * sinf(t * 0.5f);
            m_state.varioCms = -8.0f + 2.5f * sinf(t * 0.8f);
            m_state.batteryVoltage = max(9.6f, 10.4f - scenarioT * 0.015f);
            m_state.linkQuality = 92;
            break;

        case SimFcScenario::Failsafe:
            m_state.roll = 7.0f * sinf(t * 1.7f);
            m_state.pitch = 5.0f * sinf(t * 1.4f + 1.5f);
            m_state.yaw = fmodf(200.0f + 20.0f * sinf(t * 0.8f), 360.0f);
            m_state.altitudeCm = max(0.0f, 110.0f - scenarioT * 8.0f);
            m_state.varioCms = -80.0f;
            m_state.linkQuality = static_cast<uint8_t>(max(0.0f, 35.0f - 8.0f * sinf(t * 1.2f)));
            m_state.rcThrottle = 1000;
            break;

        case SimFcScenario::Manual:
            break;
    }

    m_state.cycleTimeUs = 950 + static_cast<uint16_t>(80.0f + 60.0f * sinf(t * 2.3f));
    if (m_state.scenario != SimFcScenario::Failsafe) {
        m_state.cpuLoad = 18 + static_cast<uint8_t>(6.0f + 5.0f * sinf(t * 0.9f));
    }
}

void SimFlightController::applyScenarioBase()
{
    m_state.online = true;
    m_state.simulated = true;
    m_state.failsafe = false;
    m_state.batteryCells = 3;
    m_state.batteryVoltage = 11.4f;
    m_state.linkQuality = 100;
    m_state.cpuLoad = 20;
    m_state.cycleTimeUs = 1000;
    m_state.rcRoll = 1500;
    m_state.rcPitch = 1500;
    m_state.rcThrottle = 1000;
    m_state.rcYaw = 1500;

    switch (m_state.scenario) {
        case SimFcScenario::Idle:
            m_state.armed = false;
            m_state.flightMode = 0;
            break;
        case SimFcScenario::Hover:
            m_state.armed = true;
            m_state.flightMode = 1;
            break;
        case SimFcScenario::Follow:
            m_state.armed = true;
            m_state.flightMode = 2;
            break;
        case SimFcScenario::LowBattery:
            m_state.armed = true;
            m_state.flightMode = 1;
            break;
        case SimFcScenario::Failsafe:
            m_state.armed = false;
            m_state.failsafe = true;
            m_state.flightMode = 4;
            m_state.batteryVoltage = 10.8f;
            m_state.cpuLoad = 35;
            break;
        case SimFcScenario::Manual:
            m_state.armed = true;
            m_state.flightMode = 5;
            break;
    }
}

const char* SimFlightController::scenarioName(SimFcScenario scenario)
{
    switch (scenario) {
        case SimFcScenario::Idle: return "idle";
        case SimFcScenario::Hover: return "hover";
        case SimFcScenario::Follow: return "follow";
        case SimFcScenario::LowBattery: return "low_battery";
        case SimFcScenario::Failsafe: return "failsafe";
        case SimFcScenario::Manual: return "manual";
    }
    return "idle";
}
