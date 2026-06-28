/**
 * @file    gps.cpp
 * @brief   NEO-M8N GPS 驱动实现 — 只读诊断版
 * 
 * - Serial1.begin 使用显式 RX/TX 引脚
 * - update() 非阻塞，不等待
 * - 区分 online（有 NMEA）和 valid（有 fix）
 * - 诊断统计：字符数、定位语句数、校验失败数
 */

#include "sensors/gps.h"

GPSDriver gps;

void GPSDriver::begin() {
    m_serial = &Serial1;
    // 显式使用 GPS_RX_PIN / GPS_TX_PIN（config.h 定义）
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

        // 只要有 NMEA 字符就算 online
        if (!m_data.online) {
            m_data.online = true;
            LOG(LOG_TAG_GPS, "NMEA data detected");
        }
    }

    // 更新诊断统计
    m_data.failedChecksum = m_tinyGPS.failedChecksum();

    // 定位有效判断
    if (m_tinyGPS.location.isUpdated() && m_tinyGPS.location.isValid()) {
        m_data.lat         = m_tinyGPS.location.lat();
        m_data.lng         = m_tinyGPS.location.lng();
        m_data.alt         = m_tinyGPS.altitude.meters();
        m_data.speed       = m_tinyGPS.speed.kmph();
        m_data.course      = m_tinyGPS.course.deg();
        m_data.hdop        = m_tinyGPS.hdop.hdop();
        m_data.satellites  = m_tinyGPS.satellites.value();
        m_data.age         = m_tinyGPS.location.age();
        m_data.lastUpdate  = now;
        m_data.lastFixMs   = now;
        m_data.sentencesWithFix++;
        m_data.valid       = true;
    }

    // 如果 age 太大则认为定位失效
    if (m_data.valid && (now - m_data.lastFixMs > 5000)) {
        m_data.valid = false;
    }
}
