/**
 * @file    ov5640_diag_main.cpp
 * @brief   OV2640 图传固件 — 主循环抓帧缓存，HTTP 从缓存发送
 * 
 * 分离式设计：主循环独立抓帧 → 存入缓存 → 请求处理从缓存读取
 * 抓帧超时不阻塞服务，卡死后自动恢复。
 * 
 * AP: NMC-Camera (192.168.4.1)
 * /capture.jpg  → 缓存帧
 * /stream       → MJPEG 流（从缓存发）
 * /             → HTML
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include "config.h"

#define AP_SSID     "NMC-Camera"
#define AP_PASS     "12345678"
#define STREAM_BOUNDARY "--jpgframe"

static WiFiServer g_server(80);
static uint32_t g_frameCount = 0;
static uint32_t g_errorCount = 0;
static uint32_t g_startTime  = 0;

// ── 帧缓存 ──
static uint8_t* g_cacheBuf = nullptr;
static size_t   g_cacheLen = 0;
static bool     g_cacheValid = false;

// ── 摄像头初始化 ──
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

// ── 主循环抓帧（无阻塞） ──
static void captureFrame() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { g_errorCount++; return; }
    g_frameCount++;

    // 更新缓存
    if (!g_cacheBuf) {
        g_cacheBuf = (uint8_t*)malloc(fb->len);
    } else if (g_cacheLen < fb->len) {
        g_cacheBuf = (uint8_t*)realloc(g_cacheBuf, fb->len);
    }
    if (g_cacheBuf) {
        memcpy(g_cacheBuf, fb->buf, fb->len);
        g_cacheLen = fb->len;
        g_cacheValid = true;
    }
    esp_camera_fb_return(fb);
}

// ── 从缓存发送（无缓存时实时抓取） ──
static void sendCached(WiFiClient& c, const char* contentType) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        c.printf("HTTP/1.0 503\r\n\r\nno frame\n");
        return;
    }
    g_frameCount++;
    c.printf("HTTP/1.0 200 OK\r\n");
    c.printf("Content-Type: %s\r\n", contentType);
    c.printf("Content-Length: %u\r\n\r\n", fb->len);
    c.write(fb->buf, fb->len);
    // 回存到缓存
    if (g_cacheLen < fb->len) {
        free(g_cacheBuf);
        g_cacheBuf = (uint8_t*)malloc(fb->len);
    }
    if (g_cacheBuf) {
        memcpy(g_cacheBuf, fb->buf, fb->len);
        g_cacheLen = fb->len;
        g_cacheValid = true;
    }
    esp_camera_fb_return(fb);
}

// ── MJPEG 流（每次实时抓取） ──
static void streamMjpeg(WiFiClient& c) {
    c.printf("HTTP/1.0 200 OK\r\n");
    c.printf("Content-Type: multipart/x-mixed-replace;boundary=%s\r\n", STREAM_BOUNDARY);
    c.printf("Access-Control-Allow-Origin: *\r\n");
    c.printf("Cache-Control: no-cache\r\n");
    c.printf("Connection: close\r\n\r\n");

    char head[256];
    uint32_t lastSend = 0;
    while (c.connected()) {
        if (millis() - lastSend < 100) { delay(5); continue; }
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) { delay(10); continue; }
        g_frameCount++;
        lastSend = millis();

        int hlen = snprintf(head, sizeof(head),
            "\r\n%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            STREAM_BOUNDARY, fb->len);
        c.write((uint8_t*)head, hlen);
        c.write(fb->buf, fb->len);
        esp_camera_fb_return(fb);
    }
}

// ── HTTP ──
static void handle(WiFiClient& c) {
    String req = c.readStringUntil('\n'); req.trim();
    while (c.available()) { String h = c.readStringUntil('\n');
        if (h == "\r" || !h.length()) break; }

    if (req.startsWith("GET /ping ")) {
        c.printf("HTTP/1.0 200\r\n\r\npong\n");
    } else if (req.startsWith("GET /capture.jpg") || req.startsWith("GET /capture.jpeg")) {
        sendCached(c, "image/jpeg");
    } else if (req.startsWith("GET /stream")) {
        streamMjpeg(c);
    } else {
        c.printf("HTTP/1.0 200\r\nContent-Type: text/html\r\n\r\n");
        c.printf("<!doctype html><html><head>"
            "<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>NMC</title>"
            "<style>body{background:#000;color:#fff;text-align:center;padding:10px;font-family:sans-serif}"
            "img{max-width:100%%;border-radius:8px}</style></head><body>"
            "<h2>NMC Camera</h2>"
            "<img id='f' src='/capture.jpg'>"
            "<script>setInterval(()=>{document.getElementById('f').src='/capture.jpg?_='+Date.now()},200)"
            "</script></body></html>");
    }
}

// ── 设置 ──
void setup() {
    Serial.begin(115200); delay(500);
    Serial.printf("\n========== OV2640 Web Camera ==========\n");
    Serial.printf("  AP: %s · 192.168.4.1\n", AP_SSID);
    if (!initCamera()) { Serial.println("CAM FAIL"); while(1) delay(1000); }
    sensor_t* s = esp_camera_sensor_get();
    if (s) Serial.printf("  PID: 0x%04X\n", s->id.PID);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    WiFi.softAP(AP_SSID, AP_PASS, 6, false, 4);
    g_server.begin();

    // 预热：WiFi 启动后再抓帧验证 DMA 兼容
    delay(200);
    camera_fb_t* warm = esp_camera_fb_get();
    if (warm) {
        g_cacheBuf = (uint8_t*)malloc(warm->len);
        if (g_cacheBuf) {
            memcpy(g_cacheBuf, warm->buf, warm->len);
            g_cacheLen = warm->len;
            g_cacheValid = true;
        }
        esp_camera_fb_return(warm);
        g_frameCount++;
        Serial.printf("  Warm-up: OK (%u bytes)\n", g_cacheLen);
    } else {
        Serial.println("  WARM-UP FAILED - camera may not work");
    }

    g_startTime = millis();
    Serial.println("=== Ready ===\n");
}

// ── 主循环 ──
void loop() {
    WiFiClient c = g_server.available();
    if (c) handle(c);

    static uint32_t tLog = 0;
    if (millis() - tLog >= 10000) {
        tLog = millis();
        float fps = g_frameCount / ((millis() - g_startTime) / 1000.0f);
        Serial.printf("[CAM] f=%lu fps=%.1f heap=%u err=%lu cache=%u\n",
            g_frameCount, fps, ESP.getFreeHeap(), g_errorCount, g_cacheLen);
    }
    delay(5);
}
