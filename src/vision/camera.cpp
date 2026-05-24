/**
 * @file    camera.cpp
 * @brief   OV2640 DVP 摄像头实现 — JPEG 帧采集
 */

#include "vision/camera.h"

CameraDriver camera;

void CameraDriver::begin() {
    memset(&m_config, 0, sizeof(m_config));

    m_config.ledc_channel = LEDC_CHANNEL_0;
    m_config.ledc_timer   = LEDC_TIMER_0;
    m_config.pin_d0       = CAM_PIN_Y2;
    m_config.pin_d1       = CAM_PIN_Y3;
    m_config.pin_d2       = CAM_PIN_Y4;
    m_config.pin_d3       = CAM_PIN_Y5;
    m_config.pin_d4       = CAM_PIN_Y6;
    m_config.pin_d5       = CAM_PIN_Y7;
    m_config.pin_d6       = CAM_PIN_Y8;
    m_config.pin_d7       = CAM_PIN_Y9;
    m_config.pin_xclk     = CAM_PIN_XCLK;
    m_config.pin_pclk     = CAM_PIN_PCLK;
    m_config.pin_vsync    = CAM_PIN_VSYNC;
    m_config.pin_href     = CAM_PIN_HREF;
    m_config.pin_sccb_sda = CAM_PIN_SIOD;
    m_config.pin_sccb_scl = CAM_PIN_SIOC;
    m_config.pin_pwdn     = CAM_PIN_PWDN;
    m_config.pin_reset    = CAM_PIN_RESET;
    m_config.xclk_freq_hz = 10000000;      // 10MHz XCLK
    m_config.pixel_format = PIXFORMAT_JPEG;
    m_config.frame_size   = CAM_FRAME_SIZE;
    m_config.jpeg_quality = CAM_JPEG_QUALITY;
    m_config.fb_count     = CAM_FB_COUNT;

    // 尝试初始化
    esp_err_t err = esp_camera_init(&m_config);
    if (err != ESP_OK) {
        LOG(LOG_TAG_CAM, "Camera init FAILED (err=0x%x)", err);
        m_initialized = false;
        return;
    }

    // 设置传感器参数 (OV2640 特定)
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_brightness(s, 0);     // -2 到 2
        s->set_contrast(s, 0);       // -2 到 2
        s->set_saturation(s, 0);     // -2 到 2
        s->set_special_effect(s, 0); // 0=无特效
        s->set_whitebal(s, 1);       // 自动白平衡
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);        // 自动
        s->set_exposure_ctrl(s, 1);  // 自动曝光
        s->set_aec2(s, 0);
        s->set_gain_ctrl(s, 1);      // 自动增益
        s->set_agc_gain(s, 0);
        s->set_gainceiling(s, (gainceiling_t)0); // 2x
        s->set_bpc(s, 0);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_hmirror(s, 0);
        s->set_vflip(s, 0);
        s->set_dcw(s, 1);            // 缩小到 frame_size
        s->set_colorbar(s, 0);
    }

    m_initialized = true;
    LOG(LOG_TAG_CAM, "OV2640 ok — %dx%d JPEG q=%d",
        (int)(m_config.frame_size == FRAMESIZE_SVGA ? 800 : 640),
        (int)(m_config.frame_size == FRAMESIZE_SVGA ? 600 : 480),
        CAM_JPEG_QUALITY);
}

void CameraDriver::update() {
    // 帧采集由 captureJPEG() 按需触发，这里可做心跳检测
}

bool CameraDriver::captureJPEG(CameraFrame& frame) {
    if (!m_initialized) return false;

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb || fb->format != PIXFORMAT_JPEG) {
        if (fb) esp_camera_fb_return(fb);
        return false;
    }

    frame.buf       = fb->buf;
    frame.len       = fb->len;
    frame.timestamp = millis();

    // 注意: 不调用 esp_camera_fb_return(), 由调用方处理
    return true;
}

void CameraDriver::releaseFrame(CameraFrame& frame) {
    // 找到对应的 frame buffer 并释放
    // 因为 esp_camera_fb_return 需要原始 fb 指针，
    // 实际使用中帧管理由 WebSocket 层的帧队列负责。
    frame.buf = nullptr;
    frame.len = 0;
}
