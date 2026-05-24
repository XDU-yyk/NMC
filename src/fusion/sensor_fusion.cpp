/**
 * @file    sensor_fusion.cpp
 * @brief   多源融合调度实现
 */

#include "fusion/sensor_fusion.h"

SensorFusion fusion;

void SensorFusion::begin() {
    memset(&m_state, 0, sizeof(m_state));
    m_kf.begin(0, 0, 0);
    m_lastPredict = micros();
    LOG(LOG_TAG_FUSION, "Fusion engine init — 9-state EKF ready");
}

void SensorFusion::calibrate() {
    const FCState& fc = fcBridge.getState();
    const GPSData& g  = gps.getData();
    const ToFData& t  = tof.getData();

    m_homeX = m_state.posX;
    m_homeY = m_state.posY;
    m_homeZ = m_state.posZ;

    if (g.valid) {
        m_homeLat = g.lat;
        m_homeLng = g.lng;
    }
    m_homeSet = true;
    LOG(LOG_TAG_FUSION, "Home set: (%.2f, %.2f, %.2f) m", m_homeX, m_homeY, m_homeZ);
}

void SensorFusion::update() {
    uint32_t now = millis();
    const FCState& fc = fcBridge.getState();
    const GPSData& g  = gps.getData();
    const ToFData& t  = tof.getData();

    // ── 1. 飞控姿态同步 ──
    if (fc.lastUpdate > 0) {
        m_state.roll  = fc.roll;
        m_state.pitch = fc.pitch;
        m_state.yaw   = fc.yaw;
        m_state.altitudeBaro = fc.altitude / 100.0f;
        m_state.batteryVoltage = fc.batteryVoltage;
        m_state.batteryCells   = fc.batteryCells;
        m_state.fcOnline = fcBridge.isOnline();
    }

    // ── 2. EKF 预测 (飞控加速度) ──
    float dt = (float)(now - m_lastPredict) / 1000.0f;
    if (dt > 0.001f && dt < 0.2f && fc.lastUpdate > 0) {
        float ax = fc.accRaw[0] * 0.01f;
        float ay = fc.accRaw[1] * 0.01f;
        float az = fc.accRaw[2] * 0.01f - 9.80665f;
        m_kf.predict(dt, ax, ay, az);
        m_lastPredict = now;
        m_state.timestamp = now;
    }

    // ── 3. GPS 更新 ──
    if (g.valid && g.satellites >= 4 && (now - m_lastGPSUpdate >= 1000 / GPS_SAMPLE_RATE)) {
        if (m_homeSet) {
            float gpsX, gpsY;
            GPSDriver::latLngToMeters(m_homeLat, m_homeLng, g.lat, g.lng, gpsX, gpsY);
            float hdopScale = g.hdop > 0 ? g.hdop : 10.0f;
            m_kf.updateGPS(gpsX, gpsY, g.alt, hdopScale);
        } else {
            m_homeLat = g.lat;
            m_homeLng = g.lng;
            m_homeSet = true;
            LOG(LOG_TAG_FUSION, "Auto home from GPS: (%.6f, %.6f)", g.lat, g.lng);
        }
        m_lastGPSUpdate = now;
    }
    m_state.gpsValid      = g.valid;
    m_state.gpsSatellites = g.satellites;

    // ── 4. ToF 更新 ──
    if (t.valid && (now - m_lastToFUpdate >= 1000 / TOF_SAMPLE_RATE)) {
        float tofAltitude = (float)t.distance / 1000.0f;
        m_kf.updateToF(tofAltitude);
        m_lastToFUpdate = now;
    }
    m_state.tofValid = t.valid;

    // ── 5. UWB 更新 ──
    m_state.uwbValid = false;  // 待 UWB 驱动适配后改

    // ── 6. 气压计高度 ──
    if (fc.lastUpdate > 0 && (now - m_lastBaroUpdate >= 200)) {
        m_kf.updateBaro(m_state.altitudeBaro);
        m_lastBaroUpdate = now;
    }

    // ── 7. 同步 EKF 输出 ──
    m_state.posX = m_kf.getX();
    m_state.posY = m_kf.getY();
    m_state.posZ = m_kf.getZ();
    m_state.velX = m_kf.getVX();
    m_state.velY = m_kf.getVY();
    m_state.velZ = m_kf.getVZ();
    m_state.uncertaintyX = m_kf.getUncertaintyX();
    m_state.uncertaintyY = m_kf.getUncertaintyY();
    m_state.uncertaintyZ = m_kf.getUncertaintyZ();
}
