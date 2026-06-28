/**
 * @file    gps.cpp
 * @brief   NEO-M8N GPS 驱动实现 — 增强诊断版
 * 
 * - 所有 TinyGPSPlus 字段独立更新 (satellites/hdop/alt/speed/course)
 * - passedChecksum / failedChecksum 独立统计
 * - location valid 时才更新 lat/lng/valid
 * - 非阻塞读取
 */

#include "sensors/gps.h"

GPSDriver gps;

void GPSDriver::begin() {
    m_serial = &Serial1;
    m_serial->begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    memset(&m_data, 0, sizeof(m_data));
    LOG(LOG_TAG_GPS, "GPS init — UART1 @ %d baud, RX=%d TX=%d",
        GPS_BAUD, GPS_RX_PIN, GPS_TX_PIN);
}

void GPSDriver::update() {
    if (!m_serial) return;

    uint32_t now = millis();

    // 非阻塞读取所有可用字节
    while (m_serial->available() > 0) {
        char c = m_serial->read();
        m_tinyGPS.encode(c);

        m_data.charsProcessed++;
        m_data.lastCharMs = now;

        if (!m_data.online) {
            m_data.online = true;
            LOG(LOG_TAG_GPS, "NMEA data detected");
        }
    }

    // 更新 lastCharAgeMs
    if (m_data.online) {
        m_data.lastCharAgeMs = now - m_data.lastCharMs;
    }

    // 独立更新各字段（不绑定 location valid）
    // 校验统计
    unsigned long pass = m_tinyGPS.passedChecksum();
    unsigned long fail = m_tinyGPS.failedChecksum();
    m_data.passedChecksum = pass;
    m_data.failedChecksum = fail;

    // 卫星数：只要有更新就读取
    if (m_tinyGPS.satellites.isUpdated()) {
        m_data.satellites = m_tinyGPS.satellites.value();
    }

    // HDOP
    if (m_tinyGPS.hdop.isUpdated() || m_tinyGPS.hdop.isValid()) {
        m_data.hdop = m_tinyGPS.hdop.hdop();
    }

    // 海拔
    if (m_tinyGPS.altitude.isUpdated()) {
        m_data.alt = m_tinyGPS.altitude.meters();
    }

    // 速度
    if (m_tinyGPS.speed.isUpdated()) {
        m_data.speed = m_tinyGPS.speed.kmph();
    }

    // 航向
    if (m_tinyGPS.course.isUpdated()) {
        m_data.course = m_tinyGPS.course.deg();
    }

    // 定位有效：只有 location valid 时才更新 lat/lng/valid
    if (m_tinyGPS.location.isUpdated() && m_tinyGPS.location.isValid()) {
        m_data.lat         = m_tinyGPS.location.lat();
        m_data.lng         = m_tinyGPS.location.lng();
        m_data.age         = m_tinyGPS.location.age();
        m_data.lastUpdate  = now;
        m_data.lastFixMs   = now;
        m_data.sentencesWithFix++;
        m_data.valid       = true;
    }

    // 5 秒无新有效定位则 valid=false（但不清空 satellites）
    if (m_data.valid && (now - m_data.lastFixMs > 5000)) {
        m_data.valid = false;
    }
}
