/**
 * @file    web_cam_main.cpp
 * @brief   ESP32-S3 Web MVP + Camera — 不含 ToF，避免 I2C 驱动冲突
 * 
 * 由于 ESP32 的 esp_camera 库与 Wire1 (ToF) 存在 I2C 驱动冲突，
 * 此固件仅包含摄像头 + GPS + Web，不包含 ToF。
 * 
 * 要同时使用 ToF 和摄像头，请保持两个独立固件：
 *   esp32-s3-web-mvp          → ToF + GPS + Web
 *   esp32-s3-web-cam-mvp      → Camera + GPS + Web（本文件）
 */

#include <Arduino.h>
#include "config.h"
#include "web/server.h"
#include "sensors/gps.h"
#include "esp_task_wdt.h"
#include <esp_camera.h>

#define WIFI_AP_SSID        "NMC-SmartUmbrella"
#define WIFI_AP_PASSWORD    "12345678"

static uint32_t g_startTime = 0;

// ── 摄像头帧缓存 ──
static uint8_t* g_camBuf = nullptr;
static size_t   g_camLen = 0;
static bool     g_camValid = false;
static uint32_t g_camFrames = 0;
static uint32_t g_camErrors = 0;
static bool     g_camOnline = false;

extern uint8_t* getCamBuf() { return g_camBuf; }
extern size_t   getCamLen() { return g_camLen; }
extern bool     getCamValid() { return g_camValid; }
extern uint32_t getCamFrames() { return g_camFrames; }
extern uint32_t getCamErrors() { return g_camErrors; }

static bool initCamera() {
    camera_config_t cfg = {};
    cfg.ledc_channel    = LEDC_CHANNEL_0;
    cfg.ledc_timer      = LEDC_TIMER_0;
    cfg.pin_d0          = CAM_PIN_Y2;
    cfg.pin_d1          = CAM_PIN_Y3;
    cfg.pin_d2          = CAM_PIN_Y4;
    cfg.pin_d3          = CAM_PIN_Y5;
    cfg.pin_d4          = CAM_PIN_Y6;
    cfg.pin_d5          = CAM_PIN_Y7;
    cfg.pin_d6          = CAM_PIN_Y8;
    cfg.pin_d7          = CAM_PIN_Y9;
    cfg.pin_xclk        = CAM_PIN_XCLK;
    cfg.pin_pclk        = CAM_PIN_PCLK;
    cfg.pin_vsync       = CAM_PIN_VSYNC;
    cfg.pin_href        = CAM_PIN_HREF;
    cfg.pin_sccb_sda    = CAM_PIN_SIOD;
    cfg.pin_sccb_scl    = CAM_PIN_SIOC;
    cfg.pin_pwdn        = CAM_PIN_PWDN;
    cfg.pin_reset       = CAM_PIN_RESET;
    cfg.xclk_freq_hz    = 24000000;
    cfg.pixel_format    = PIXFORMAT_JPEG;
    cfg.frame_size      = FRAMESIZE_QQVGA;
    cfg.jpeg_quality    = 10;
    cfg.fb_count        = 2;
    cfg.fb_location     = CAMERA_FB_IN_DRAM;
    cfg.grab_mode       = CAMERA_GRAB_WHEN_EMPTY;
    return esp_camera_init(&cfg) == ESP_OK;
}

static void captureFrame() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { g_camErrors++; return; }
    g_camFrames++;
    if (!g_camBuf || g_camLen < fb->len) {
        free(g_camBuf);
        g_camBuf = (uint8_t*)malloc(fb->len);
    }
    if (g_camBuf) {
        memcpy(g_camBuf, fb->buf, fb->len);
        g_camLen = fb->len;
        g_camValid = true;
    }
    esp_camera_fb_return(fb);
}

static void fillTelemetry(TelemetryData& data)
{
    uint32_t now = millis();
    float t = now * 0.001f;

    data.roll  = 6.0f * sinf(t * 0.25f);
    data.pitch = 4.0f * sinf(t * 0.20f + 1.2f);
    data.yaw   = 25.0f * sinf(t * 0.10f + 2.5f);
    data.posX = 200.0f * sinf(t * 0.05f);
    data.posY = 150.0f * sinf(t * 0.04f + 0.5f);
    data.posZ = 100.0f + 30.0f * sinf(t * 0.08f);
    data.altitude = 120.0f + 40.0f * sinf(t * 0.10f);
    data.batteryVoltage = 11.4f - 0.02f * (t / 60.0f);
    data.batteryCells = 3;

    // ToF 不可用（I2C 冲突）
    data.tofDistance = 0;
    data.tofOnline = false;
    data.tofStatus = 255;
    data.tofErrors = 0;
    data.tofAgeMs = 0;

    // GPS
    data.gpsOnline = false;
    data.gpsValid = false;
    data.gpsSats = 0;
    data.gpsLat = 0; data.gpsLng = 0;
    data.gpsAlt = 0; data.gpsSpeed = 0;
    data.gpsAgeMs = 0;
    data.gpsChars = 0;
    data.gpsSentences = 0;
    data.gpsFailedChecksum = 0;

    const auto& gd = gps.getData();
    data.gpsOnline = gd.online;
    data.gpsValid  = gd.valid;
    data.gpsSats   = gd.satellites;
    data.gpsLat    = gd.lat;
    data.gpsLng    = gd.lng;
    data.gpsAlt    = gd.alt;
    data.gpsSpeed  = gd.speed;
    data.gpsAgeMs  = gd.age;
    data.gpsChars  = gd.charsProcessed;
    data.gpsSentences = gd.sentencesWithFix;
    data.gpsFailedChecksum = gd.failedChecksum;

    // FC always offline
    data.fcOnline = false;
    data.armed = false;
    data.flightMode = 0;
    data.dataSource = 2;  // gps
    data.errorFlags = 0;
    data.camOnline = g_camOnline;

    data.uptime = now;
    data.freeHeap = ESP.getFreeHeap();
    data.chipTemp = temperatureRead();
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  NMC Smart Umbrella - Web MVP + Camera");
    Serial.println("  GPS + Camera, no ToF (I2C conflict)");
    Serial.println("==============================================");
    Serial.println();

    // Camera
    esp_task_wdt_reset();
    Serial.println("--- Camera ---");
    if (initCamera()) {
        g_camOnline = true;
        Serial.println("  Camera: OK (QQVGA JPEG)");
        captureFrame();
        if (g_camValid) Serial.printf("  Warm-up: %u bytes\n", g_camLen);
        else { Serial.println("  WARM-UP FAILED"); g_camOnline = false; }
    } else {
        Serial.println("  Camera: init FAILED");
    }
    esp_task_wdt_reset();

    // GPS
    Serial.println("--- GPS ---");
    gps.begin();
    Serial.printf("  GPS: UART1 @ %d, RX=%d TX=%d\n", GPS_BAUD, GPS_RX_PIN, GPS_TX_PIN);
    esp_task_wdt_reset();
    Serial.println();

    // Web
    webServer.setTelemetrySource(fillTelemetry);
    if (!webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true)) {
        Serial.println("[FATAL] Web server init failed!");
        while (1) { delay(1000); }
    }

    g_startTime = millis();

    Serial.println("==============================================");
    Serial.println("  SYSTEM READY");
    Serial.printf("  AP: %s @ 192.168.4.1\n", WIFI_AP_SSID);
    Serial.println("  JSON:  http://192.168.4.1/api/telemetry");
    Serial.println("  Cam:   http://192.168.4.1/capture.jpg");
    Serial.println("==============================================");
}

void loop()
{
    gps.update();
    webServer.loop();

    static uint32_t lastCap = 0;
    uint32_t now = millis();
    if (g_camOnline && now - lastCap >= 200) {  // 5 FPS
        lastCap = now;
        captureFrame();
    }

    static uint32_t lastPrint = 0;
    if (now - lastPrint > 5000) {
        lastPrint = now;
        Serial.printf("[SYS] %lu heap=%u cam=%s frames=%u err=%u gps:online=%d valid=%d sats=%u\n",
            (now - g_startTime) / 1000,
            ESP.getFreeHeap(),
            g_camOnline ? "ok" : "off",
            g_camFrames, g_camErrors,
            gps.getData().online, gps.getData().valid, gps.getData().satellites);
    }

    delay(10);
}
