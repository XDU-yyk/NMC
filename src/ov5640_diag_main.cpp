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
#include <freertos/semphr.h>
#include <string.h>
#include "config.h"

#define AP_SSID     "NMC-Camera"
#define AP_PASS     "12345678"
#define AP_CHANNEL  1
#define STREAM_BOUNDARY "jpgframe"
#define STREAM_INTERVAL_MS 200
#define STREAM_CLIENT_MAX_MS 60000
#define HTTP_LINE_TIMEOUT_MS 400
#define JPEG_WRITE_TIMEOUT_MS 1200
#define CAMERA_CAPTURE_FAIL_LIMIT 3
#define CAMERA_RESTART_DELAY_MS 500
#define CAMERA_RETRY_MS 3000
#define CAMERA_STALE_RESTART_MS 5000
#define FW_TAG "camdiag-20260715-worker"

static WiFiServer g_server(80);
static uint32_t g_frameCount = 0;
static uint32_t g_errorCount = 0;
static uint32_t g_startTime  = 0;

// ── 帧缓存 ──
static uint8_t* g_cacheBuf = nullptr;
static size_t   g_cacheLen = 0;
static size_t   g_cacheCap = 0;
static bool     g_cacheValid = false;
static bool     g_cameraReady = false;
static bool     g_cameraDriverActive = false;
static bool     g_cameraFirstFrameLogged = false;
static bool     g_cameraRestartPending = false;
static bool     g_apStarted = false;
static uint32_t g_lastFrameMs = 0;
static uint32_t g_httpRequests = 0;
static uint32_t g_jpegRequests = 0;
static uint32_t g_statusRequests = 0;
static uint32_t g_statusSeq = 0;
static uint32_t g_writeTimeouts = 0;
static uint32_t g_badRequests = 0;
static uint32_t g_captureFailStreak = 0;
static uint32_t g_cameraRestartRequests = 0;
static int8_t g_camSiod = CAM_PIN_SIOD;
static int8_t g_camSioc = CAM_PIN_SIOC;
static uint32_t g_cameraRecoveries = 0;
static uint32_t g_nextCameraStartMs = 0;
static uint32_t g_lastCameraRestartMs = 0;
static SemaphoreHandle_t g_cacheMutex = nullptr;
static TaskHandle_t g_cameraTask = nullptr;

static bool writeClient(WiFiClient& c, const uint8_t* data, size_t len, uint32_t timeoutMs);

static bool lockCache(TickType_t waitTicks = portMAX_DELAY) {
    return !g_cacheMutex || xSemaphoreTake(g_cacheMutex, waitTicks) == pdTRUE;
}

static void unlockCache() {
    if (g_cacheMutex) xSemaphoreGive(g_cacheMutex);
}

static void readCacheStatus(size_t& len, size_t& cap, bool& valid, uint32_t& lastFrameMs) {
    len = 0;
    cap = 0;
    valid = false;
    lastFrameMs = 0;
    if (!lockCache(pdMS_TO_TICKS(20))) return;
    len = g_cacheLen;
    cap = g_cacheCap;
    valid = g_cacheValid;
    lastFrameMs = g_lastFrameMs;
    unlockCache();
}

static const char INDEX_HTML[] =
"<!doctype html><html><head>"
"<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<meta http-equiv='Cache-Control' content='no-store'>"
"<meta http-equiv='Pragma' content='no-cache'>"
"<title>NMC Camera</title>"
"<style>"
"body{background:#000;color:#fff;text-align:center;padding:10px;font-family:sans-serif}"
"img{display:block;max-width:100%;min-height:120px;margin:0 auto;border-radius:8px;background:#111}"
"a{color:#8cf}#s{font-size:12px;line-height:1.35;word-break:break-all}#tag{font-size:12px;color:#aaa}"
"</style></head><body>"
"<h2>NMC Camera</h2>"
"<img id='f' alt='camera frame' src='/capture.jpg?boot=1'>"
"<p id='s'>status: loading</p>"
"<p id='tag'>fw: " FW_TAG "</p>"
"<p><a href='/stream'>stream</a> | <a href='/capture.jpg?manual=1'>capture.jpg</a></p>"
"<script>"
"var img=document.getElementById('f');"
"var s=document.getElementById('s');"
"var busy=false,timer=0,seq=0,statBusy=false,fail=0;"
"function next(ms){clearTimeout(timer);timer=setTimeout(refresh,ms);}"
"function done(ms){busy=false;next(ms);}"
"function refresh(){if(busy)return;busy=true;var finished=false;"
"var guard=setTimeout(function(){if(finished)return;finished=true;fail++;done(1200);},1800);"
"img.onload=function(){if(finished)return;finished=true;clearTimeout(guard);fail=0;done(220);};"
"img.onerror=function(){if(finished)return;finished=true;clearTimeout(guard);fail++;done(fail>3?1500:900);};"
"img.src='/capture.jpg?_='+(++seq)+'_'+(new Date()).getTime();}"
"function stat(){if(statBusy)return;statBusy=true;var x=new XMLHttpRequest();x.timeout=1200;"
"x.onreadystatechange=function(){if(x.readyState==4){statBusy=false;s.textContent=(x.status==200?x.responseText:('status http '+x.status));}};"
"x.ontimeout=function(){statBusy=false;s.textContent='status timeout';};"
"x.open('GET','/status?_='+(new Date()).getTime(),true);x.send();}"
"next(200);"
"setInterval(stat,2000);stat();"
"</script>"
"</body></html>";

static void sendStatus(WiFiClient& c) {
    uint32_t uptime = millis();
    float fps = g_frameCount / (uptime > g_startTime ? ((uptime - g_startTime) / 1000.0f) : 1.0f);
    size_t cacheLen = 0;
    size_t cacheCap = 0;
    bool cacheValid = false;
    uint32_t lastFrameMs = 0;
    readCacheStatus(cacheLen, cacheCap, cacheValid, lastFrameMs);
    char body[768];
    int bodyLen = snprintf(body, sizeof(body),
        "{\"seq\":%lu,\"fw\":\"%s\",\"frames\":%lu,\"fps\":%.1f,\"errors\":%lu,\"cache\":%u,\"cap\":%u,\"valid\":%s,\"camera\":%s,\"ap\":%s,\"stations\":%u,\"ip\":\"%s\",\"channel\":%d,\"http\":%lu,\"jpg\":%lu,\"stat\":%lu,\"timeouts\":%lu,\"bad\":%lu,\"age\":%lu,\"cfail\":%lu,\"creq\":%lu,\"crec\":%lu,\"heap\":%u,\"uptime\":%lu}\n",
        (unsigned long)(++g_statusSeq),
        FW_TAG,
        (unsigned long)g_frameCount,
        fps,
        (unsigned long)g_errorCount,
        (unsigned)cacheLen,
        (unsigned)cacheCap,
        cacheValid ? "true" : "false",
        g_cameraReady ? "true" : "false",
        g_apStarted ? "true" : "false",
        (unsigned)WiFi.softAPgetStationNum(),
        WiFi.softAPIP().toString().c_str(),
        WiFi.channel(),
        (unsigned long)g_httpRequests,
        (unsigned long)g_jpegRequests,
        (unsigned long)g_statusRequests,
        (unsigned long)g_writeTimeouts,
        (unsigned long)g_badRequests,
        lastFrameMs ? (unsigned long)(uptime - lastFrameMs) : 0UL,
        (unsigned long)g_captureFailStreak,
        (unsigned long)g_cameraRestartRequests,
        (unsigned long)g_cameraRecoveries,
        ESP.getFreeHeap(),
        (unsigned long)uptime);
    if (bodyLen < 0) return;
    if (bodyLen >= (int)sizeof(body)) bodyLen = (int)sizeof(body) - 1;

    c.printf("HTTP/1.0 200 OK\r\n");
    c.printf("Content-Type: application/json\r\n");
    c.printf("Cache-Control: no-store, no-cache, must-revalidate, proxy-revalidate, max-age=0\r\n");
    c.printf("Pragma: no-cache\r\n");
    c.printf("Expires: 0\r\n");
    c.printf("Access-Control-Allow-Origin: *\r\n");
    c.printf("Connection: close\r\n");
    c.printf("Content-Length: %u\r\n\r\n", (unsigned)bodyLen);
    writeClient(c, (const uint8_t*)body, (size_t)bodyLen, 500);
    c.flush();
}

static void sendHtml(WiFiClient& c) {
    c.setNoDelay(true);
    const size_t bodyLen = sizeof(INDEX_HTML) - 1;
    char head[256];
    int hlen = snprintf(head, sizeof(head),
        "HTTP/1.0 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-store, no-cache, must-revalidate, proxy-revalidate, max-age=0\r\n"
        "Pragma: no-cache\r\n"
        "Expires: 0\r\n"
        "Connection: close\r\n"
        "Content-Length: %u\r\n\r\n",
        (unsigned)bodyLen);
    if (hlen > 0 && hlen < (int)sizeof(head)) {
        writeClient(c, (const uint8_t*)head, (size_t)hlen, 500);
        writeClient(c, (const uint8_t*)INDEX_HTML, bodyLen, 1000);
    }
    c.flush();
}

// ── 摄像头初始化 ──
static void recoverSccbBus() {
    if (g_camSiod < 0 || g_camSioc < 0) return;

    pinMode(g_camSiod, INPUT_PULLUP);
    pinMode(g_camSioc, OUTPUT_OPEN_DRAIN);
    digitalWrite(g_camSioc, HIGH);
    delayMicroseconds(10);

    for (uint8_t i = 0; i < 16; i++) {
        digitalWrite(g_camSioc, LOW);
        delayMicroseconds(10);
        digitalWrite(g_camSioc, HIGH);
        delayMicroseconds(10);
    }

    pinMode(g_camSiod, OUTPUT_OPEN_DRAIN);
    digitalWrite(g_camSiod, LOW);
    delayMicroseconds(10);
    digitalWrite(g_camSioc, HIGH);
    delayMicroseconds(10);
    digitalWrite(g_camSiod, HIGH);
    delayMicroseconds(10);

    pinMode(g_camSiod, INPUT_PULLUP);
    pinMode(g_camSioc, INPUT_PULLUP);
    delay(20);
}

static bool initCamera() {
    recoverSccbBus();

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
    cfg.pin_sccb_sda    = g_camSiod;
    cfg.pin_sccb_scl    = g_camSioc;
    cfg.pin_pwdn        = CAM_PIN_PWDN;
    cfg.pin_reset       = CAM_PIN_RESET;
    cfg.xclk_freq_hz    = 24000000;
    cfg.pixel_format    = PIXFORMAT_JPEG;
    cfg.frame_size      = FRAMESIZE_QQVGA;
    cfg.jpeg_quality    = 15;
    cfg.fb_count        = 2;
    cfg.fb_location     = CAMERA_FB_IN_DRAM;
    cfg.grab_mode       = CAMERA_GRAB_LATEST;
    return esp_camera_init(&cfg) == ESP_OK;
}

static void stopCameraDriver() {
    if (g_cameraDriverActive || g_cameraReady) {
        esp_camera_deinit();
    }
    g_cameraDriverActive = false;
    g_cameraReady = false;
}

static bool startCamera(uint8_t attempts = 5, bool countRecovery = false) {
    for (uint8_t i = 1; i <= attempts; i++) {
        Serial.printf("  Camera init attempt %u/%u (SIOD=%d SIOC=%d)\n", i, attempts, g_camSiod, g_camSioc);
        if (initCamera()) {
            g_cameraDriverActive = true;
            g_cameraReady = true;
            g_cameraFirstFrameLogged = false;
            g_cameraRestartPending = false;
            g_captureFailStreak = 0;
            if (countRecovery) g_cameraRecoveries++;
            sensor_t* s = esp_camera_sensor_get();
            if (s) {
                Serial.printf("  PID: 0x%04X (SIOD=%d SIOC=%d)\n", s->id.PID, g_camSiod, g_camSioc);
            }
            return true;
        }
        // Swap SIOD/SIOC on next attempt if pins differ
        if (i < attempts && g_camSiod != g_camSioc) {
            int8_t tmp = g_camSiod; g_camSiod = g_camSioc; g_camSioc = tmp;
            Serial.printf("  Swap SIOD/SIOC -> SIOD=%d SIOC=%d\n", g_camSiod, g_camSioc);
        }
        g_cameraReady = false;
        g_cameraDriverActive = false;
        g_errorCount++;
        esp_camera_deinit();
        delay(500);
    }
    Serial.println("  CAM INIT FAILED - will keep AP up and retry");
    return false;
}

static void requestCameraRestart(const char* reason) {
    if (g_cameraRestartPending) return;
    uint32_t now = millis();
    uint32_t age = g_lastFrameMs ? (now - g_lastFrameMs) : 0UL;
    g_cameraRestartRequests++;
    g_lastCameraRestartMs = now;
    g_nextCameraStartMs = now + CAMERA_RESTART_DELAY_MS;
    g_cameraRestartPending = true;
    Serial.printf("[CAM] restart requested reason=%s fail=%lu age=%lu req=%lu\n",
        reason,
        (unsigned long)g_captureFailStreak,
        (unsigned long)age,
        (unsigned long)g_cameraRestartRequests);
    stopCameraDriver();
}

static bool writeClient(WiFiClient& c, const uint8_t* data, size_t len, uint32_t timeoutMs = 300) {
    size_t sent = 0;
    uint32_t lastProgress = millis();
    while (sent < len) {
        if (!c.connected()) return false;
        size_t chunk = len - sent;
        if (chunk > 512) chunk = 512;

        size_t wrote = c.write(data + sent, chunk);
        if (wrote == 0) {
            if (millis() - lastProgress > timeoutMs) return false;
            delay(2);
            yield();
            continue;
        }

        sent += wrote;
        lastProgress = millis();
        yield();
    }
    return sent == len;
}

static bool readLine(WiFiClient& c, char* out, size_t outLen, uint32_t timeoutMs) {
    if (outLen == 0) return false;

    size_t len = 0;
    uint32_t start = millis();
    while (millis() - start < timeoutMs && c.connected()) {
        while (c.available()) {
            char ch = (char)c.read();
            if (ch == '\r') continue;
            if (ch == '\n') {
                out[len] = '\0';
                return true;
            }
            if (len + 1 < outLen) out[len++] = ch;
        }
        delay(1);
        yield();
    }

    out[len] = '\0';
    return len > 0;
}

static void drainHeaders(WiFiClient& c) {
    char line[128];
    for (uint8_t i = 0; i < 20; i++) {
        if (!readLine(c, line, sizeof(line), 50)) return;
        if (line[0] == '\0') return;
    }
}

// ── 主循环抓帧（无阻塞） ──
static void captureFrame() {
    if (!g_cameraReady) return;
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        g_errorCount++;
        g_captureFailStreak++;
        if (g_captureFailStreak >= CAMERA_CAPTURE_FAIL_LIMIT) {
            requestCameraRestart("fb_get");
        }
        return;
    }
    g_frameCount++;
    g_captureFailStreak = 0;

    // 更新缓存
    if (!lockCache()) {
        g_errorCount++;
        esp_camera_fb_return(fb);
        return;
    }

    if (fb->len > g_cacheCap) {
        uint8_t* next = (uint8_t*)realloc(g_cacheBuf, fb->len);
        if (!next) {
            g_errorCount++;
            unlockCache();
            esp_camera_fb_return(fb);
            return;
        }
        g_cacheBuf = next;
        g_cacheCap = fb->len;
    }
    bool firstFrame = false;
    size_t frameLen = fb->len;
    if (g_cacheBuf) {
        memcpy(g_cacheBuf, fb->buf, fb->len);
        g_cacheLen = fb->len;
        g_cacheValid = true;
        g_lastFrameMs = millis();
        if (!g_cameraFirstFrameLogged) {
            g_cameraFirstFrameLogged = true;
            firstFrame = true;
        }
    }
    unlockCache();
    esp_camera_fb_return(fb);
    if (firstFrame) {
        Serial.printf("  Camera first frame: OK (%u bytes)\n", (unsigned)frameLen);
    }
}

// ── 从缓存发送（无缓存时实时抓取） ──
static bool copyCachedFrame(uint8_t*& jpeg, size_t& jpegLen) {
    jpeg = nullptr;
    jpegLen = 0;
    if (!lockCache(pdMS_TO_TICKS(50))) return false;
    if (g_cacheValid && g_cacheBuf && g_cacheLen > 0) {
        jpeg = (uint8_t*)malloc(g_cacheLen);
        if (jpeg) {
            memcpy(jpeg, g_cacheBuf, g_cacheLen);
            jpegLen = g_cacheLen;
        }
    }
    unlockCache();
    return jpeg != nullptr;
}

static void cameraWorker(void*) {
    Serial.printf("[CAM] worker started on core %d\n", xPortGetCoreID());
    startCamera();

    uint32_t lastCaptureMs = 0;
    uint32_t lastRetryMs = millis();
    for (;;) {
        uint32_t now = millis();
        if (g_cameraRestartPending && now >= g_nextCameraStartMs) {
            g_cameraRestartPending = false;
            lastRetryMs = now;
            startCamera(2, true);
        } else if (!g_cameraReady && !g_cameraRestartPending && now - lastRetryMs >= CAMERA_RETRY_MS) {
            lastRetryMs = now;
            stopCameraDriver();
            startCamera(1, true);
        }

        if (g_cameraReady && now - lastCaptureMs >= STREAM_INTERVAL_MS) {
            lastCaptureMs = now;
            captureFrame();
        }

        size_t cacheLen = 0;
        size_t cacheCap = 0;
        bool cacheValid = false;
        uint32_t lastFrameMs = 0;
        readCacheStatus(cacheLen, cacheCap, cacheValid, lastFrameMs);
        if (g_cameraReady && cacheValid && lastFrameMs && now - lastFrameMs > CAMERA_STALE_RESTART_MS) {
            requestCameraRestart("stale");
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void sendJpeg(WiFiClient& c, const char* contentType) {
    c.setNoDelay(true);
    g_jpegRequests++;
    uint8_t* jpeg = nullptr;
    size_t jpegLen = 0;
    if (!copyCachedFrame(jpeg, jpegLen)) {
        c.printf("HTTP/1.0 503\r\n\r\nno frame\n");
        return;
    }

    c.printf("HTTP/1.0 200 OK\r\n");
    c.printf("Content-Type: %s\r\n", contentType);
    c.printf("Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n");
    c.printf("Pragma: no-cache\r\n");
    c.printf("Connection: close\r\n");
    c.printf("Content-Length: %u\r\n\r\n", (unsigned)jpegLen);
    if (!writeClient(c, jpeg, jpegLen, JPEG_WRITE_TIMEOUT_MS)) {
        g_errorCount++;
        g_writeTimeouts++;
    }
    free(jpeg);
    c.flush();
}

// ── MJPEG 流（每次实时抓取） ──
static void streamMjpeg(WiFiClient& c) {
    c.setNoDelay(true);
    c.setTimeout(500);
    c.printf("HTTP/1.0 200 OK\r\n");
    c.printf("Content-Type: multipart/x-mixed-replace;boundary=%s\r\n", STREAM_BOUNDARY);
    c.printf("Access-Control-Allow-Origin: *\r\n");
    c.printf("Cache-Control: no-cache\r\n");
    c.printf("Connection: close\r\n\r\n");

    char head[256];
    uint32_t lastSend = 0;
    uint32_t streamStart = millis();
    uint32_t sentFrames = 0;
    while (c.connected() && g_cameraReady) {
        if (millis() - streamStart > STREAM_CLIENT_MAX_MS) break;
        if (millis() - lastSend < STREAM_INTERVAL_MS) { delay(1); continue; }
        uint8_t* jpeg = nullptr;
        size_t jpegLen = 0;
        if (!copyCachedFrame(jpeg, jpegLen)) { delay(10); continue; }
        sentFrames++;
        lastSend = millis();

        int hlen = snprintf(head, sizeof(head),
            "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            STREAM_BOUNDARY, (unsigned)jpegLen);
        bool ok = writeClient(c, (uint8_t*)head, hlen, 300)
            && writeClient(c, jpeg, jpegLen, 500);
        free(jpeg);
        if (!ok) {
            g_errorCount++;
            break;
        }
        yield();
    }
    c.stop();
    Serial.printf("[CAM] stream closed frames=%lu ms=%lu\n",
        (unsigned long)sentFrames, (unsigned long)(millis() - streamStart));
}

// ── HTTP ──
static void handle(WiFiClient& c) {
    c.setNoDelay(true);
    c.setTimeout(HTTP_LINE_TIMEOUT_MS);
    g_httpRequests++;

    char req[160];
    if (!readLine(c, req, sizeof(req), HTTP_LINE_TIMEOUT_MS)) {
        g_badRequests++;
        c.printf("HTTP/1.0 408 Request Timeout\r\nConnection: close\r\n\r\n");
        c.stop();
        return;
    }
    drainHeaders(c);

    if (strncmp(req, "GET /ping ", 10) == 0) {
        c.printf("HTTP/1.0 200 OK\r\nConnection: close\r\n\r\npong\n");
    } else if (strncmp(req, "GET /capture.jpg", 16) == 0 || strncmp(req, "GET /capture.jpeg", 17) == 0) {
        sendJpeg(c, "image/jpeg");
    } else if (strncmp(req, "GET /status", 11) == 0) {
        g_statusRequests++;
        sendStatus(c);
    } else if (strncmp(req, "GET /stream", 11) == 0) {
        streamMjpeg(c);
        return;
    } else if (strncmp(req, "GET /favicon.ico", 16) == 0) {
        c.printf("HTTP/1.0 204 No Content\r\nConnection: close\r\n\r\n");
    } else {
        sendHtml(c);
    }
    c.stop();
}

// ── 设置 ──
void setup() {
    Serial.begin(115200); delay(500);
    Serial.printf("\n========== OV2640 Web Camera ==========\n");
    Serial.printf("  FW: %s\n", FW_TAG);
    Serial.printf("  AP: %s · 192.168.4.1\n", AP_SSID);

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    bool apConfigOk = WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
    bool apOk = WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, false, 4);
    g_apStarted = apConfigOk && apOk;
    g_server.begin();
    Serial.printf("  AP start: config=%s softAP=%s ip=%s channel=%d\n",
        apConfigOk ? "OK" : "FAIL",
        apOk ? "OK" : "FAIL",
        WiFi.softAPIP().toString().c_str(),
        WiFi.channel());

    // 预热：WiFi 启动后再抓帧验证 DMA 兼容
    g_startTime = millis();
    g_cacheMutex = xSemaphoreCreateMutex();
    BaseType_t taskOk = pdFAIL;
    if (g_cacheMutex) {
        taskOk = xTaskCreatePinnedToCore(
            cameraWorker, "cam_worker", 16384, nullptr, 1, &g_cameraTask, 0);
    } else {
        Serial.println("[CAM] cache mutex allocation failed");
    }
    Serial.printf("  Camera worker: %s\n", taskOk == pdPASS ? "OK" : "FAIL");
    Serial.println("=== Ready ===\n");
}

// ── 主循环 ──
void loop() {
    uint32_t now = millis();
    WiFiClient c = g_server.available();
    if (c) handle(c);

    static uint32_t tLog = 0;
    if (now - tLog >= 10000) {
        tLog = now;
        size_t cacheLen = 0;
        size_t cacheCap = 0;
        bool cacheValid = false;
        uint32_t lastFrameMs = 0;
        readCacheStatus(cacheLen, cacheCap, cacheValid, lastFrameMs);
        float fps = g_frameCount / ((now - g_startTime) / 1000.0f);
        Serial.printf("[CAM] f=%lu fps=%.1f heap=%u err=%lu cache=%u cap=%u ap=%s sta=%u ip=%s ch=%d http=%lu jpg=%lu stat=%lu to=%lu bad=%lu age=%lu cfail=%lu creq=%lu crec=%lu\n",
            (unsigned long)g_frameCount,
            fps,
            ESP.getFreeHeap(),
            (unsigned long)g_errorCount,
            (unsigned)cacheLen,
            (unsigned)cacheCap,
            g_apStarted ? "ok" : "fail",
            (unsigned)WiFi.softAPgetStationNum(),
            WiFi.softAPIP().toString().c_str(),
            WiFi.channel(),
            (unsigned long)g_httpRequests,
            (unsigned long)g_jpegRequests,
            (unsigned long)g_statusRequests,
            (unsigned long)g_writeTimeouts,
            (unsigned long)g_badRequests,
            lastFrameMs ? (unsigned long)(now - lastFrameMs) : 0UL,
            (unsigned long)g_captureFailStreak,
            (unsigned long)g_cameraRestartRequests,
            (unsigned long)g_cameraRecoveries);
    }
    delay(5);
}
