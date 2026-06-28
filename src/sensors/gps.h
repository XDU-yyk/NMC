/**
 * @file    gps.h
 * @brief   NEO-M8N GPS 驱动 (基于 TinyGPSPlus)
 * 
 * 只读诊断阶段：不参与控制，不做融合。
 * 区分"串口有 NMEA 数据"(online) 和"定位有效"(valid)。
 */

#ifndef GPS_H
#define GPS_H

#include "config.h"
#include <Arduino.h>
#include <TinyGPSPlus.h>

struct GPSData {
    // 连接状态
    bool     online = false;      // 串口收到过 NMEA 字符
    bool     valid  = false;      // TinyGPS location valid 且 age 合理
    // 定位数据
    double   lat    = 0;          // 纬度 (°)
    double   lng    = 0;          // 经度 (°)
    double   alt    = 0;          // 海拔 (m)
    double   speed  = 0;          // 地速 (km/h)
    double   course = 0;          // 航向 (°)
    float    hdop   = 0;          // 水平精度因子
    uint8_t  satellites = 0;      // 卫星数
    uint32_t age    = 0;          // 定位数据龄期 (ms)
    uint32_t lastUpdate = 0;      // 最后更新 (ms)
    // 诊断统计
    uint32_t charsProcessed  = 0; // 已处理 NMEA 字符数
    uint32_t sentencesWithFix = 0;// 含定位的语句数
    uint32_t failedChecksum  = 0; // 校验失败数
    uint32_t lastCharMs      = 0; // 最后收到字符时间戳
    uint32_t lastFixMs       = 0; // 最后有效定位时间戳
};

class GPSDriver {
public:
    void begin();
    void update();

    const GPSData& getData() const { return m_data; }
    bool isOnline() const { return m_data.online; }
    bool hasFix() const { return m_data.valid && m_data.satellites >= 4; }

private:
    TinyGPSPlus m_tinyGPS;
    GPSData     m_data;
    HardwareSerial* m_serial = nullptr;
};

extern GPSDriver gps;

#endif // GPS_H
