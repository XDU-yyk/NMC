/**
 * @file    uwb.h
 * @brief   UWB 驱动桩 (v2.0 — 待 arduino-dw3000 库手动安装后启用)
 */

#ifndef UWB_H
#define UWB_H

#include "config.h"
#include <Arduino.h>

struct UWBData {
    float anchorDist[4];
    float posX, posY, posZ;
    bool  healthy;
};

class UWBLocalizer {
public:
    bool begin() { return true; }
    void update() {}
    bool isHealthy() const { return false; }
private:
    UWBData m_data;
};

extern UWBLocalizer uwb;

#endif // UWB_H
