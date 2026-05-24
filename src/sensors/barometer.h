/**
 * @file    barometer.h
 * @brief   气压计驱动桩 (v2.0 — 数据改从 F4V3S 飞控 MSP 读取)
 */

#ifndef BAROMETER_H
#define BAROMETER_H

#include "config.h"
#include <Arduino.h>

struct BaroData {
    float temperature;
    float pressure;
    float altitude;
    bool  valid;
};

class Barometer {
public:
    bool begin() { return true; }
    void update() {}
    const BaroData& getData() const { return m_data; }
private:
    BaroData m_data;
};

extern Barometer baro;

#endif // BAROMETER_H
