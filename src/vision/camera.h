/**
 * @file    camera.h
 * @brief   OV2640 DVP 摄像头驱动 (JPEG 图传)
 * 
 * 使用 ESP32-S3 内置 DVP 接口，通过 esp_camera 库驱动 OV2640，
 * 输出 JPEG 帧供 WebSocket 推送。
 */

#ifndef CAMERA_H
#define CAMERA_H

#include "config.h"
#include <Arduino.h>
#include "esp_camera.h"

struct CameraFrame {
    uint8_t* buf;        // JPEG 数据
    size_t   len;        // 数据长度 (字节)
    uint32_t timestamp;  // 采集时间 (ms)
};

class CameraDriver {
public:
    void begin();
    void update();

    /* 获取最新帧 (调用者负责释放返回的帧) */
    bool captureJPEG(CameraFrame& frame);
    void releaseFrame(CameraFrame& frame);

    bool isInitialized() const { return m_initialized; }

private:
    bool            m_initialized = false;
    camera_config_t m_config;
};

extern CameraDriver camera;

#endif // CAMERA_H
