/**
 * @file    mission.cpp
 * @brief   任务规划器实现 — 安全检测 + 飞控命令编排
 */

#include "control/mission.h"

MissionPlanner mission;

void MissionPlanner::begin() {
    m_armed = false;
    m_controlCycles = 0;
    m_sensorErrors = 0;
    LOG(LOG_TAG_MISSION, "Mission planner init");
}

void MissionPlanner::update() {
    m_controlCycles++;

    uint32_t now = millis();
    if (now - m_lastSafetyCheck < INTERVAL_SAFETY) return;
    m_lastSafetyCheck = now;

    const FusionState& s = fusion.getState();

    // ── 安全检查 ──
    if (m_armed) {
        // 1. 姿态超限
        if (fabsf(s.roll) > MAX_TILT_ANGLE || fabsf(s.pitch) > MAX_TILT_ANGLE) {
            LOG(LOG_TAG_MISSION, "SAFETY: Tilt exceed! R=%.1f P=%.1f", s.roll, s.pitch);
            followCtrl.setMode(FollowMode::HOVER);  // 强制悬停回稳
            m_sensorErrors++;
        }

        // 2. 电池低电压
        float minCellV = BATTERY_CELL_MIN;
        if (s.batteryVoltage > 0 && s.batteryCells > 0) {
            float cellV = s.batteryVoltage / (float)s.batteryCells;
            if (cellV < CRITICAL_BATTERY) {
                LOG(LOG_TAG_MISSION, "SAFETY: Critical battery! %.1fV/cell", cellV);
                followCtrl.emergencyStop();
                disarm();
            } else if (cellV < LOW_BATTERY_THRESH / (float)s.batteryCells) {
                LOG(LOG_TAG_MISSION, "WARNING: Low battery %.1fV/cell", cellV);
            }
        }

        // 3. 飞控离线
        if (!fcBridge.isOnline()) {
            LOG(LOG_TAG_MISSION, "SAFETY: FC offline!");
            followCtrl.emergencyStop();
            m_sensorErrors++;
        }

        // 4. 安全开关
        if (!isSafetySwitchOn()) {
            LOG(LOG_TAG_MISSION, "SAFETY: Switch off!");
            followCtrl.emergencyStop();
            disarm();
        }
    }
}

bool MissionPlanner::arm() {
    if (!fcBridge.isOnline()) {
        LOG(LOG_TAG_MISSION, "Arm FAILED: FC offline");
        return false;
    }
    if (!isSafetySwitchOn()) {
        LOG(LOG_TAG_MISSION, "Arm FAILED: Safety switch off");
        return false;
    }
    if (fcBridge.arm()) {
        m_armed = true;
        LOG(LOG_TAG_MISSION, "Armed OK");
        return true;
    }
    LOG(LOG_TAG_MISSION, "Arm FAILED: FC rejected");
    return false;
}

bool MissionPlanner::disarm() {
    if (fcBridge.disarm()) {
        m_armed = false;
        LOG(LOG_TAG_MISSION, "Disarmed");
        return true;
    }
    return false;
}

bool MissionPlanner::isSafetySwitchOn() const {
    return digitalRead(SAFETY_SWITCH_PIN) == HIGH;
}

void MissionPlanner::getStatus(MissionStatus& st) const {
    st.uptime         = millis();
    st.controlCycles  = m_controlCycles;
    st.sensorErrors   = m_sensorErrors;
    st.batteryVoltage = fusion.getState().batteryVoltage;
    st.batteryCells   = fusion.getState().batteryCells;
    st.fcOnline       = fcBridge.isOnline();
    st.gpsValid       = fusion.getState().gpsValid;
    st.tofValid       = fusion.getState().tofValid;
}
