/**
 * @file    gps.h
 * @brief   NEO-M8N GPS 驱动 (基于 TinyGPSPlus)
 */

#ifndef GPS_H
#define GPS_H

#include "config.h"
#include <Arduino.h>
#include <TinyGPSPlus.h>

struct GPSData {
    bool     valid;
    double   lat;           // 纬度 (°)
    double   lng;           // 经度 (°)
    double   alt;           // 海拔 (m)
    double   speed;         // 地速 (km/h)
    double   course;        // 航向 (°)
    float    hdop;          // 水平精度因子
    uint8_t  satellites;    // 卫星数
    uint32_t age;           // 数据年龄 (ms)
    uint32_t lastUpdate;    // 最后更新时间 (ms)
};

class GPSDriver {
public:
    void begin();
    void update();

    const GPSData& getData() const { return m_data; }
    bool isHealthy() const { return m_data.valid && m_data.satellites >= 4; }

    /* 将经纬度差转为平面距离 (简易, 适合短距离) */
    static void latLngToMeters(double latRef, double lngRef,
                               double lat,    double lng,
                               float& xMeters, float& yMeters);

private:
    TinyGPSPlus m_tinyGPS;
    GPSData     m_data;
    HardwareSerial* m_serial = nullptr;
};

extern GPSDriver gps;

#endif // GPS_H
