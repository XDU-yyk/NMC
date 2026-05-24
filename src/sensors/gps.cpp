/**
 * @file    gps.cpp
 * @brief   NEO-M8N GPS 驱动实现
 */

#include "sensors/gps.h"

GPSDriver gps;

void GPSDriver::begin() {
    m_serial = &Serial1;  // GPS_UART_NUM = 1
    m_serial->begin(GPS_BAUD, SERIAL_8N1);

    memset(&m_data, 0, sizeof(m_data));
    LOG(LOG_TAG_GPS, "GPS init — UART1 @ %d baud", GPS_BAUD);
}

void GPSDriver::update() {
    if (!m_serial) return;

    // 读入所有可用字节给 TinyGPSPlus
    while (m_serial->available() > 0) {
        char c = m_serial->read();
        m_tinyGPS.encode(c);
    }

    // 更新缓存数据
    if (m_tinyGPS.location.isUpdated() || m_tinyGPS.altitude.isUpdated()) {
        m_data.lat  = m_tinyGPS.location.lat();
        m_data.lng  = m_tinyGPS.location.lng();
        m_data.alt  = m_tinyGPS.altitude.meters();
        m_data.speed = m_tinyGPS.speed.kmph();
        m_data.course = m_tinyGPS.course.deg();
        m_data.hdop = m_tinyGPS.hdop.hdop();
        m_data.satellites = m_tinyGPS.satellites.value();
        m_data.valid = m_tinyGPS.location.isValid();
        m_data.age   = m_tinyGPS.location.age();
        m_data.lastUpdate = millis();
    }
}

void GPSDriver::latLngToMeters(double latRef, double lngRef,
                                double lat,    double lng,
                                float& xMeters, float& yMeters) {
    // 简易球面近似: 纬度 1° ≈ 111320m, 经度 1° ≈ 111320m × cos(lat)
    const float METERS_PER_DEG_LAT = 111320.0f;
    float metersPerDegLng = METERS_PER_DEG_LAT * cosf(latRef * DEG_TO_RAD);

    yMeters = (float)((lat - latRef) * METERS_PER_DEG_LAT);
    xMeters = (float)((lng - lngRef) * metersPerDegLng);
}
