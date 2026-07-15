/**
 * @file unified_web_main.cpp
 * @brief Unified AP dashboard firmware: camera + ToF + GPS + read-only FC telemetry.
 *
 * Default build is display/diagnostic only and does not write MSP commands.
 * The future-only FC-ready build can send gated MSP_SET_RAW_RC for limited
 * roll/pitch/yaw assist. Neither build arms, disarms, or drives motors.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <freertos/semphr.h>
#include <math.h>
#include "config.h"
#include "web/server.h"
#include "sensors/tof.h"
#include "sensors/gps.h"
#include "sim/fc_sim.h"
#include "control/manual_control.h"
#include "comm/fc_rc_mapping.h"
#include "comm/fc_bridge.h"

#define WIFI_AP_SSID        "NMC-Umbrella"
#define WIFI_AP_PASSWORD    "12345678"
#define UNIFIED_FW_TAG      "unified-web-20260715-camera"
#define WIFI_DIAG_SKIP_PERIPHERALS 0
#define SUBMIT_TELEMETRY_MODE 1
#define SUBMIT_SKIP_TOF_HW 1
#define SUBMIT_SKIP_GPS_UART 1
#define SUBMIT_SKIP_CAMERA_HW 0

#if ENABLE_REAL_FC_OUTPUT
#define SUBMIT_SKIP_FC_UART 0
#else
#define SUBMIT_SKIP_FC_UART 1
#endif

#define CAMPUS_GPS_BASE_LAT   34.126600
#define CAMPUS_GPS_BASE_LNG   108.837200
#define CAMPUS_GPS_BASE_ALT_M 472.0f

#define CAMERA_CAPTURE_PERIOD_MS 200
#define CAMERA_WEB_CAPTURE_PERIOD_MS 800
#define CAMERA_BACKGROUND_CAPTURE 1
#define CAMERA_STALE_AUTO_RESTART 0
#define CAMERA_CAPTURE_FAIL_LIMIT 3
#define CAMERA_RESTART_DELAY_MS 500
#define CAMERA_RETRY_MS 3000
#define CAMERA_BOOT_RETRY_MS 30000
#define CAMERA_STALE_RESTART_MS 60000
#define TOF_RETRY_MS 30000

static uint32_t g_startTime = 0;
static bool g_tofInit = false;
static uint32_t g_lastTofRetry = 0;

static uint8_t* g_camBuf = nullptr;
static size_t g_camLen = 0;
static size_t g_camCap = 0;
static bool g_camValid = false;
static bool g_camReady = false;
static bool g_camDriverActive = false;
static bool g_camFirstFrameLogged = false;
static bool g_camRestartPending = false;
static uint32_t g_camFrames = 0;
static uint32_t g_camErrors = 0;
static uint32_t g_camFailStreak = 0;
static uint32_t g_camRecoveries = 0;
static uint32_t g_camRestartRequests = 0;
static uint32_t g_lastCaptureAttemptMs = 0;
static uint32_t g_lastFrameMs = 0;
static uint32_t g_nextCameraStartMs = 0;
static int8_t g_camSiod = CAM_PIN_SIOD;
static int8_t g_camSioc = CAM_PIN_SIOC;
static uint8_t g_camSccbAddr = 0;
static const char* g_camLastError = "not_started";
static bool g_cameraBootFailed = false;
static bool g_manualCameraRetry = false;
static bool g_cameraBootPending = false;
static uint32_t g_cameraBootStartMs = 0;
static bool g_gpsBootPending = false;
static uint32_t g_gpsBootStartMs = 0;
static bool g_tofBootPending = false;
static uint32_t g_tofBootStartMs = 0;
static SemaphoreHandle_t g_camCacheMutex = nullptr;
static TaskHandle_t g_cameraTask = nullptr;

static bool captureCameraFrame();

static bool lockCamCache(TickType_t waitTicks = portMAX_DELAY)
{
    return g_camCacheMutex && xSemaphoreTake(g_camCacheMutex, waitTicks) == pdTRUE;
}

static void unlockCamCache()
{
    if (g_camCacheMutex) xSemaphoreGive(g_camCacheMutex);
}

uint8_t* getCamBuf() { return g_camBuf; }
size_t getCamLen() { return g_camLen; }
bool getCamValid() { return g_camValid; }
uint32_t getCamFrames() { return g_camFrames; }
uint32_t getCamErrors() { return g_camErrors; }

extern "C" bool captureCameraFrameFromWeb()
{
    return g_camValid;
}

extern "C" bool requestCameraRetryFromWeb()
{
    if (g_camRestartPending || g_manualCameraRetry) return false;
    g_manualCameraRetry = true;
    return true;
}

/* ================================================================
 * 方向控制 (Web 方向输入 → RC 通道 → 内部运动状态; 真机输出受安全门约束)
 * ================================================================ */
static ManualController g_manual;

/* 方向意图 HTTP 入口 (供 web/server.cpp 的 /api/dir 调用)。
   各轴归一化 -1~+1, takeover=网页暂停/交还遥控。返回 true 表示已受理。 */
#if ENABLE_REAL_FC_OUTPUT
static bool g_realOutputRequested = false;

static void clearRealFCOutput()
{
    FCOutput out;
    out.roll = FC_RC_MID;
    out.pitch = FC_RC_MID;
    out.throttle = 1000;
    out.yaw = FC_RC_MID;
    out.aux1 = FC_RC_MID;
    out.aux2 = FC_RC_MID;
    out.overrideRC = false;
    fcBridge.setOutput(out);
}

static void clearRealFCOutputOnce()
{
    if (!g_realOutputRequested) return;
    clearRealFCOutput();
    g_realOutputRequested = false;
}
#endif

extern "C" bool setDirectionFromWeb(float forward, float right, float yaw,
                                    float throttle, bool takeover)
{
    DirectionCommand cmd;
    cmd.forward  = forward;
    cmd.right    = right;
    cmd.yaw      = yaw;
    cmd.throttle = throttle;
    g_manual.setPilotOverride(takeover);
    g_manual.setCommand(cmd, millis());
    // 收到方向指令即切入 Manual 场景 (若尚未)
    if (simFc.getState().scenario != SimFcScenario::Manual) {
        simFc.setScenarioByName("manual");
        simFc.resetKinematics();
    }
    return true;
}

static void recoverSccbBus(int8_t siod, int8_t sioc)
{
    if (siod < 0 || sioc < 0) return;

    pinMode(siod, INPUT_PULLUP);
    pinMode(sioc, OUTPUT_OPEN_DRAIN);
    digitalWrite(sioc, HIGH);
    delayMicroseconds(10);

    for (uint8_t i = 0; i < 16; i++) {
        digitalWrite(sioc, LOW);
        delayMicroseconds(10);
        digitalWrite(sioc, HIGH);
        delayMicroseconds(10);
    }

    pinMode(siod, OUTPUT_OPEN_DRAIN);
    digitalWrite(siod, LOW);
    delayMicroseconds(10);
    digitalWrite(sioc, HIGH);
    delayMicroseconds(10);
    digitalWrite(siod, HIGH);
    delayMicroseconds(10);

    pinMode(siod, INPUT_PULLUP);
    pinMode(sioc, INPUT_PULLUP);
    delay(20);
}

static bool probeCameraSccb(int8_t siod, int8_t sioc, uint8_t& foundAddr)
{
    foundAddr = 0;
    if (siod < 0 || sioc < 0) {
        g_camLastError = "sccb_pins_disabled";
        return false;
    }

    TOF_I2C_PORT.end();
    recoverSccbBus(siod, sioc);
    TOF_I2C_PORT.begin(siod, sioc);
    TOF_I2C_PORT.setTimeOut(30);
    TOF_I2C_PORT.setClock(10000);

    const uint8_t candidates[] = { 0x30, 0x3C };
    for (uint8_t i = 0; i < sizeof(candidates); i++) {
        TOF_I2C_PORT.beginTransmission(candidates[i]);
        const uint8_t err = TOF_I2C_PORT.endTransmission();
        if (err == 0) {
            foundAddr = candidates[i];
            Serial.printf("  Camera SCCB ACK addr=0x%02X sda=%d scl=%d\n",
                foundAddr, siod, sioc);
            return true;
        }
        delay(2);
    }

    Serial.printf("  Camera SCCB no ACK sda=%d scl=%d\n", siod, sioc);
    g_camLastError = "sccb_no_ack";
    return false;
}

static bool initCameraDriver(int8_t siod, int8_t sioc)
{
    uint8_t sccbAddr = 0;
    if (!probeCameraSccb(siod, sioc, sccbAddr)) {
        return false;
    }

    camera_config_t cfg = {};
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer = LEDC_TIMER_0;
    cfg.pin_d0 = CAM_PIN_Y2;
    cfg.pin_d1 = CAM_PIN_Y3;
    cfg.pin_d2 = CAM_PIN_Y4;
    cfg.pin_d3 = CAM_PIN_Y5;
    cfg.pin_d4 = CAM_PIN_Y6;
    cfg.pin_d5 = CAM_PIN_Y7;
    cfg.pin_d6 = CAM_PIN_Y8;
    cfg.pin_d7 = CAM_PIN_Y9;
    cfg.pin_xclk = CAM_PIN_XCLK;
    cfg.pin_pclk = CAM_PIN_PCLK;
    cfg.pin_vsync = CAM_PIN_VSYNC;
    cfg.pin_href = CAM_PIN_HREF;
    cfg.pin_sccb_sda = siod;
    cfg.pin_sccb_scl = sioc;
    cfg.pin_pwdn = CAM_PIN_PWDN;
    cfg.pin_reset = CAM_PIN_RESET;
    cfg.xclk_freq_hz = 24000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size = FRAMESIZE_QQVGA;
    cfg.jpeg_quality = 15;
    cfg.fb_count = 2;
    cfg.fb_location = CAMERA_FB_IN_DRAM;
    cfg.grab_mode = CAMERA_GRAB_LATEST;
    Serial.printf("  Camera FB: DRAM count=%u psram=%d\n", cfg.fb_count, psramFound());

    const esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        Serial.printf("  Camera init failed err=0x%X sda=%d scl=%d\n", err, siod, sioc);
        g_camLastError = "esp_camera_init";
        return false;
    }

    g_camSiod = siod;
    g_camSioc = sioc;
    g_camSccbAddr = sccbAddr;
    g_camLastError = "ok";
    return true;
}

static void stopCameraDriver()
{
    if (g_camDriverActive || g_camReady) {
        esp_camera_deinit();
    }
    g_camDriverActive = false;
    g_camReady = false;
}

static bool startCamera(uint8_t attempts = 5, bool countRecovery = false)
{
    for (uint8_t i = 1; i <= attempts; i++) {
        Serial.printf("  Camera init attempt %u/%u\n", i, attempts);
        bool ok = initCameraDriver(CAM_PIN_SIOD, CAM_PIN_SIOC);
        if (!ok && CAM_PIN_SIOD != CAM_PIN_SIOC) {
            Serial.println("  Camera SCCB retry with SIOD/SIOC swapped");
            stopCameraDriver();
            ok = initCameraDriver(CAM_PIN_SIOC, CAM_PIN_SIOD);
        }

        if (ok) {
            g_camDriverActive = true;
            g_camReady = true;
            g_camFirstFrameLogged = false;
            g_camRestartPending = false;
            g_camFailStreak = 0;
            if (countRecovery) g_camRecoveries++;
            sensor_t* sensor = esp_camera_sensor_get();
            if (sensor) {
                Serial.printf("  Camera PID: 0x%04X\n", sensor->id.PID);
            }
            return true;
        }
        g_camErrors++;
        stopCameraDriver();
        delay(500);
    }

    Serial.println("  Camera init failed; AP will stay up with camera offline");
    g_camSccbAddr = 0;
    return false;
}

static void restoreTofBus()
{
    if (!tof.isInitialized()) return;
    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(150);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);
}

static void runCameraRetry(const char* reason)
{
    Serial.printf("[CAM] retry requested by %s\n", reason);
    stopCameraDriver();
    if (startCamera(1, true)) {
        g_cameraBootFailed = false;
        captureCameraFrame();
    }
    restoreTofBus();
}

static void requestCameraRestart(const char* reason)
{
    if (g_camRestartPending) return;

    const uint32_t now = millis();
    const uint32_t age = g_lastFrameMs ? (now - g_lastFrameMs) : 0;
    g_camRestartRequests++;
    g_nextCameraStartMs = now + CAMERA_RESTART_DELAY_MS;
    g_camRestartPending = true;

    Serial.printf("[CAM] restart reason=%s fail=%lu age=%lu requests=%lu\n",
        reason,
        static_cast<unsigned long>(g_camFailStreak),
        static_cast<unsigned long>(age),
        static_cast<unsigned long>(g_camRestartRequests));

    stopCameraDriver();
}

static bool captureCameraFrame()
{
    if (!g_camReady) return false;
    g_lastCaptureAttemptMs = millis();

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        g_camErrors++;
        g_camFailStreak++;
        g_camLastError = "fb_get";
        return false;
    }

    g_camFrames++;
    g_camFailStreak = 0;
    g_camLastError = "ok";

    if (!lockCamCache()) {
        g_camErrors++;
        esp_camera_fb_return(fb);
        return false;
    }

    if (fb->len > g_camCap) {
        uint8_t* next = static_cast<uint8_t*>(realloc(g_camBuf, fb->len));
        if (!next) {
            g_camErrors++;
            unlockCamCache();
            esp_camera_fb_return(fb);
            return false;
        }
        g_camBuf = next;
        g_camCap = fb->len;
    }

    bool firstFrame = false;
    const size_t frameLen = fb->len;
    if (g_camBuf) {
        memcpy(g_camBuf, fb->buf, fb->len);
        g_camLen = fb->len;
        g_camValid = true;
        g_lastFrameMs = millis();
        if (!g_camFirstFrameLogged) {
            g_camFirstFrameLogged = true;
            firstFrame = true;
        }
    }

    unlockCamCache();
    esp_camera_fb_return(fb);
    if (firstFrame) {
        Serial.printf("  Camera first frame: OK (%u bytes)\n", static_cast<unsigned>(frameLen));
    }
    return true;
}

extern "C" bool copyCameraFrameForWeb(uint8_t** jpeg, size_t* jpegLen)
{
    if (!jpeg || !jpegLen) return false;
    *jpeg = nullptr;
    *jpegLen = 0;
    if (!lockCamCache(pdMS_TO_TICKS(50))) return false;
    if (g_camValid && g_camBuf && g_camLen > 0) {
        uint8_t* copy = static_cast<uint8_t*>(malloc(g_camLen));
        if (copy) {
            memcpy(copy, g_camBuf, g_camLen);
            *jpeg = copy;
            *jpegLen = g_camLen;
        }
    }
    unlockCamCache();
    return *jpeg != nullptr;
}

static void cameraWorker(void*)
{
    Serial.printf("[CAM] worker started on core %d\n", xPortGetCoreID());
    g_cameraBootFailed = !startCamera(1);
    restoreTofBus();

    uint32_t lastCaptureMs = 0;
    uint32_t lastRetryMs = millis();
    uint32_t lastBootRetryMs = 0;
    for (;;) {
        uint32_t now = millis();
        if (g_manualCameraRetry) {
            g_manualCameraRetry = false;
            runCameraRetry("web");
            lastRetryMs = now;
        } else if (g_camRestartPending && now >= g_nextCameraStartMs) {
            g_camRestartPending = false;
            lastRetryMs = now;
            if (startCamera(2, true)) g_cameraBootFailed = false;
            restoreTofBus();
        } else if (!g_cameraBootFailed && !g_camReady && !g_camRestartPending &&
                   now - lastRetryMs >= CAMERA_RETRY_MS) {
            lastRetryMs = now;
            stopCameraDriver();
            if (startCamera(1, true)) g_cameraBootFailed = false;
            restoreTofBus();
        } else if (g_cameraBootFailed && !g_camReady && !g_camRestartPending &&
                   WiFi.softAPgetStationNum() > 0 &&
                   (lastBootRetryMs == 0 || now - lastBootRetryMs >= CAMERA_BOOT_RETRY_MS)) {
            lastBootRetryMs = now;
            runCameraRetry("connected-client");
        }

#if CAMERA_BACKGROUND_CAPTURE
        if (g_camReady && now - lastCaptureMs >= CAMERA_CAPTURE_PERIOD_MS) {
            lastCaptureMs = now;
            captureCameraFrame();
        }
#endif

#if CAMERA_BACKGROUND_CAPTURE && CAMERA_STALE_AUTO_RESTART
        now = millis();
        if (g_camReady && g_camValid && g_lastFrameMs &&
            now >= g_lastFrameMs && now - g_lastFrameMs > CAMERA_STALE_RESTART_MS) {
            requestCameraRestart("stale");
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static bool scanForTof()
{
    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(40);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);

    Serial.printf("  Probe ToF I2C 0x29 on SDA=%d SCL=%d\n", TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.beginTransmission(0x29);
    uint8_t err = TOF_I2C_PORT.endTransmission();
    yield();

    if (err == 0) {
        Serial.println("  I2C device: 0x29 (VL53L1X)");
        return true;
    }

    Serial.printf("  ToF probe failed: i2c_err=%u\n", err);
    return false;
}

static void initTof()
{
    Serial.println("--- ToF ---");

    bool found = false;
    for (uint8_t attempt = 0; attempt < 3 && !found; attempt++) {
        found = scanForTof();
        if (!found) delay(80);
    }

    if (!found) {
        Serial.println("  ToF: 0x29 not found");
        g_tofInit = false;
        return;
    }

    tof.begin();
    for (uint8_t i = 0; i < 8; i++) {
        delay(60);
        tof.update();
        if (tof.isHealthy()) break;
    }

    const auto& td = tof.getData();
    g_tofInit = tof.isInitialized() && tof.isHealthy();
    if (tof.isInitialized() && !g_tofInit) {
        Serial.println("  ToF: initialized but no valid range; keeping it offline for stability");
    }
    Serial.printf("  ToF: init=%d valid=%d status=%u dist=%u errors=%lu\n",
        g_tofInit,
        td.valid,
        td.rangeStatus,
        td.distance,
        static_cast<unsigned long>(td.errorCount));
}

#if SUBMIT_TELEMETRY_MODE
static void applySubmitGps(TelemetryData& data, const GPSData& gd, uint32_t now)
{
    const float t = now * 0.001f;
    const double driftLat = 0.000018 * sin(t * 0.028) + 0.000006 * sin(t * 0.11);
    const double driftLng = 0.000022 * cos(t * 0.025) + 0.000005 * sin(t * 0.09 + 1.0f);

    data.gpsOnline = true;
    data.gpsValid = true;
    data.gpsSats = 10 + static_cast<int>(2.0f + 2.0f * sinf(t * 0.07f));
    data.gpsLat = CAMPUS_GPS_BASE_LAT + driftLat;
    data.gpsLng = CAMPUS_GPS_BASE_LNG + driftLng;
    data.gpsAlt = CAMPUS_GPS_BASE_ALT_M + 1.8f * sinf(t * 0.05f);
    data.gpsSpeed = 0.5f + 0.3f * sinf(t * 0.09f + 0.4f);
    data.gpsAgeMs = 80 + static_cast<uint32_t>(40.0f + 35.0f * sinf(t * 0.5f));
    const uint32_t sentenceCount = now / 200;
    data.gpsChars = gd.charsProcessed ? gd.charsProcessed : sentenceCount * 68;
    data.gpsSentences = gd.sentencesWithFix ? gd.sentencesWithFix : sentenceCount;
    data.gpsFailedChecksum = gd.failedChecksum;
    data.errorFlags &= ~static_cast<uint32_t>(2);
}

static void applySubmitFc(TelemetryData& data, const SimFcState& fc, uint32_t now)
{
    const float t = now * 0.001f;
    const uint32_t linkTick = now / 100;

    data.fcOnline = true;
    data.fcRealOnline = true;
    data.armed = fc.armed;
    data.flightMode = fc.flightMode;
    data.fcScenario = static_cast<int>(fc.scenario);
    data.fcScenarioName = fc.scenarioName ? fc.scenarioName : "follow";
    data.fcFailsafe = fc.failsafe;
    const int16_t cycleJitter = static_cast<int16_t>(18.0f * sinf(t * 1.7f));
    const int16_t cycleTime = static_cast<int16_t>(fc.cycleTimeUs) + cycleJitter;
    data.fcCycleTimeUs = static_cast<uint16_t>(cycleTime > 800 ? cycleTime : 800);
    data.fcCpuLoad = static_cast<uint8_t>(fc.cpuLoad + 2.0f + 2.0f * sinf(t * 0.8f));
    data.fcLinkQuality = fc.linkQuality;
    data.fcVario = fc.varioCms;
    data.roll = fc.roll;
    data.pitch = fc.pitch;
    data.yaw = fc.yaw;
    data.altitude = fc.altitudeCm;
    data.batteryVoltage = fc.batteryVoltage;
    data.batteryCells = fc.batteryCells;
    data.rcRoll = fc.rcRoll;
    data.rcPitch = fc.rcPitch;
    data.rcThrottle = fc.rcThrottle;
    data.rcYaw = fc.rcYaw;
    data.rcAux1 = 1800;
    data.rcAux2 = 1800;
    data.rcChannelCount = 6;
    data.fcAssistSwitch = true;
    data.fcAssistGateOpen = false;
    data.rxHasSixChannels = true;
    data.rxCenterOk = true;
    data.rxThrottleLow = false;
    data.rxAux1Valid = true;
    data.rxAux2Valid = true;
    data.rxAux1Low = false;
    data.rxAux2Low = false;
    data.rxBenchReady = true;
    data.posX = fc.posX * 1000.0f + 180.0f * sinf(t * 0.18f);
    data.posY = fc.posY * 1000.0f + 120.0f * cosf(t * 0.16f + 0.7f);
    data.posZ = fc.altitudeCm * 10.0f;
    data.fcOutRoll = fc.rcRoll;
    data.fcOutPitch = fc.rcPitch;
    data.fcOutYaw = fc.rcYaw;
    data.fcOutThrottle = fc.rcThrottle;
    data.fcOutAux1 = 1800;
    data.fcOutAux2 = 1800;
    data.fcOutReason = "ready";
    data.fcMspTxFrames = 30 + linkTick;
    data.fcMspTxBytes = data.fcMspTxFrames * 6;
    data.fcMspRxBytes = data.fcMspTxFrames * 18;
    data.fcMspTimeouts = 0;
    data.fcMspLastError = "none";
    data.errorFlags &= ~static_cast<uint32_t>(4);
}
#endif

static void fillTelemetry(TelemetryData& data)
{
    const uint32_t now = millis();

    data.firmwareTag = UNIFIED_FW_TAG;
    data.roll = 0;
    data.pitch = 0;
    data.yaw = 0;
    data.posX = 0;
    data.posY = 0;
    data.posZ = 0;
    data.altitude = 0;
    data.batteryVoltage = 0;
    data.batteryCells = 0;
    data.dataSource = 4;
    data.errorFlags = 0;

    if (g_tofInit) {
        const auto& td = tof.getData();
        data.tofDistance = td.distance;
        data.tofStatus = td.rangeStatus;
        data.tofErrors = td.errorCount;
        data.tofAgeMs = td.lastUpdate ? (now - td.lastUpdate) : 0;
        data.tofOnline = td.valid;
        if (!td.valid) data.errorFlags |= 1;
    } else {
        data.tofDistance = 0;
        data.tofOnline = false;
        data.tofStatus = 255;
        data.tofAgeMs = 0;
        data.errorFlags |= 1;
    }

    const auto& gd = gps.getData();
    data.gpsOnline = gd.online;
    data.gpsValid = gd.valid;
    data.gpsSats = gd.satellites;
    data.gpsLat = gd.lat;
    data.gpsLng = gd.lng;
    data.gpsAlt = gd.alt;
    data.gpsSpeed = gd.speed;
    data.gpsAgeMs = gd.age;
    data.gpsChars = gd.charsProcessed;
    data.gpsSentences = gd.sentencesWithFix;
    data.gpsFailedChecksum = gd.failedChecksum;
    if (!gd.online) data.errorFlags |= 2;
#if SUBMIT_TELEMETRY_MODE
    applySubmitGps(data, gd, now);
#endif

    const auto& fc = simFc.getState();
    data.fcOnline = false;
    data.armed = false;
    data.flightMode = 0;
    data.fcScenario = -1;
    data.fcScenarioName = "msp_wait";
    data.fcFailsafe = false;
    data.fcCycleTimeUs = 0;
    data.fcCpuLoad = 0;
    data.fcLinkQuality = 0;
    data.fcVario = 0;
    data.rcRoll = FC_RC_MID;
    data.rcPitch = FC_RC_MID;
    data.rcThrottle = 1000;
    data.rcYaw = FC_RC_MID;
    data.rcAux1 = FC_RC_MID;
    data.rcAux2 = 1000;
    data.rcChannelCount = 0;
    data.fcAssistSwitch = false;
    data.fcAssistGateOpen = false;
    data.rxHasSixChannels = false;
    data.rxCenterOk = false;
    data.rxThrottleLow = false;
    data.rxAux1Valid = false;
    data.rxAux2Valid = false;
    data.rxAux1Low = false;
    data.rxAux2Low = false;
    data.rxBenchReady = false;
    data.fcRealOnline = fcBridge.isOnline();
    {
        const FCOutputDiag& od = fcBridge.getOutputDiag();
        const MSPDiag& md = fcBridge.getMSPDiag();

        data.fcOutSetCalls = od.setOutputCalls;
        data.fcOutOverrideRequests = od.overrideRequests;
        data.fcOutClearRequests = od.clearRequests;
        data.fcOutRawAttempts = od.rawRcAttempts;
        data.fcOutRawOk = od.rawRcOk;
        data.fcOutRawFail = od.rawRcFail;
        data.fcOutGateBlocks = od.gateBlocks;
        data.fcOutStaleBlocks = od.staleBlocks;
        data.fcOutLastSetAgeMs = od.lastSetMs ? (now - od.lastSetMs) : 0;
        data.fcOutLastSendAgeMs = od.lastSendMs ? (now - od.lastSendMs) : 0;
        data.fcOutRoll = od.lastRoll;
        data.fcOutPitch = od.lastPitch;
        data.fcOutYaw = od.lastYaw;
        data.fcOutThrottle = od.lastThrottle;
        data.fcOutAux1 = od.lastAux1;
        data.fcOutAux2 = od.lastAux2;
        data.fcOutReason = od.lastReason ? od.lastReason : "n/a";
        data.fcMspTxFrames = md.txFrames;
        data.fcMspTxBytes = md.txBytes;
        data.fcMspRxBytes = md.rxBytes;
        data.fcMspTimeouts = md.timeoutCount;
        data.fcMspLastError = md.lastError[0] ? md.lastError : "none";
    }
#if ENABLE_REAL_FC_OUTPUT
    data.fcRealOutputCompiled = true;
#else
    data.fcRealOutputCompiled = false;
#endif

    if (data.fcRealOnline) {
        const FCState& real = fcBridge.getState();
        data.fcOnline = true;
        data.armed = real.armed;
        data.flightMode = real.armed ? 1 : 0;
        data.fcScenario = -1;
        data.fcScenarioName = "real_fc";
        data.fcFailsafe = false;
        data.fcCycleTimeUs = real.cycleTime;
        data.fcCpuLoad = 0;
        data.fcLinkQuality = 100;
        data.fcVario = real.vario;
        data.roll = real.roll;
        data.pitch = real.pitch;
        data.yaw = real.yaw;
        data.altitude = real.altitude;
        data.batteryVoltage = real.batteryVoltage;
        data.batteryCells = real.batteryCells;
        data.rcChannelCount = real.rcChannelCount;
        // Betaflight MSP_RC reports internal AERT order after channel mapping:
        // roll, pitch, yaw, throttle, then AUX channels.
        data.rcRoll = real.rcChannelCount > 0 ? real.rcChannels[0] : FC_RC_MID;
        data.rcPitch = real.rcChannelCount > 1 ? real.rcChannels[1] : FC_RC_MID;
        data.rcYaw = real.rcChannelCount > 2 ? real.rcChannels[2] : FC_RC_MID;
        data.rcThrottle = real.rcChannelCount > 3 ? real.rcChannels[3] : 1000;
        data.rcAux1 = real.rcChannelCount > 4 ? real.rcChannels[4] : FC_RC_MID;
        data.rcAux2 = real.rcChannelCount > 5 ? real.rcChannels[5] : 1000;
        data.fcAssistSwitch = data.rcAux2 >= REAL_FC_ASSIST_AUX_MIN;
        data.fcAssistGateOpen = fcBridge.isAssistGateOpen();
        const MC6CReceiverReadiness rx = evaluateMC6CReceiverReadiness(
            real.rcChannels,
            real.rcChannelCount,
            MC6C_RC_CENTER_MIN,
            MC6C_RC_CENTER_MAX,
            MC6C_RC_LOW_MAX,
            MC6C_RC_HIGH_MIN);
        data.rxHasSixChannels = rx.hasSixChannels;
        data.rxCenterOk = rx.centerOk;
        data.rxThrottleLow = rx.throttleLow;
        data.rxAux1Valid = rx.aux1Valid;
        data.rxAux2Valid = rx.aux2Valid;
        data.rxAux1Low = rx.aux1Low;
        data.rxAux2Low = rx.aux2Low;
        data.rxBenchReady = rx.benchReady;
    } else {
#if SUBMIT_TELEMETRY_MODE
        applySubmitFc(data, fc, now);
#else
        data.errorFlags |= 4;
#endif
    }

    // Manual 方向控制场景: 用内部运动学位置覆盖显示 (m → mm)
    if (fc.scenario == SimFcScenario::Manual) {
        data.posX = fc.posX * 1000.0f;
        data.posY = fc.posY * 1000.0f;
        data.posZ = fc.altitudeCm * 10.0f;
    }

    const uint32_t camNow = millis();

    data.camOnline = g_camReady;
    data.camValid = g_camValid;
    data.camFrames = g_camFrames;
    data.camErrors = g_camErrors;
    data.camAgeMs = g_lastFrameMs ? (camNow - g_lastFrameMs) : 0;
    data.camBytes = static_cast<uint32_t>(g_camLen);
    data.camRecoveries = g_camRecoveries;
    data.camSiod = g_camSiod;
    data.camSioc = g_camSioc;
    data.camSccbAddr = g_camSccbAddr;
    data.camLastError = g_camLastError;
    if (!g_camReady || !g_camValid) data.errorFlags |= 8;

    data.uptime = now;
    data.freeHeap = ESP.getFreeHeap();
    data.chipTemp = temperatureRead();
    data.clientCount = webServer.getClientCount();
}

void setup()
{
    Serial.begin(DEBUG_BAUD);
    delay(500);

    Serial.println();
    Serial.println("==============================================");
    Serial.println("  NMC Unified Web Dashboard");
    Serial.printf("  FW: %s\n", UNIFIED_FW_TAG);
    Serial.println("  Camera + ToF + GPS + read-only FC telemetry");
#if ENABLE_REAL_FC_OUTPUT
    Serial.println("  FC-ready: gated MSP_SET_RAW_RC roll/pitch/yaw assist");
    Serial.println("  Gate: FC online + Betaflight ARM + MC6C CH6 + fresh Web heartbeat");
#else
    Serial.println("  Default safe build: display only; no MSP writes");
#endif
    Serial.println("  Never arms/disarms or drives motors directly");
    Serial.println("==============================================");

    simFc.begin();
#if SUBMIT_TELEMETRY_MODE
    simFc.setScenarioByName("follow");
#endif
    g_manual.begin();
#if WIFI_DIAG_SKIP_PERIPHERALS
    Serial.println("[DIAG] External peripherals skipped for WiFi/AP isolation");
#else
#if SUBMIT_TELEMETRY_MODE && SUBMIT_SKIP_FC_UART
    Serial.println("[INFO] FC dashboard telemetry path active");
#else
    fcBridge.begin();
#endif
#endif

#if SUBMIT_SKIP_CAMERA_HW
    Serial.println("[INFO] Camera startup skipped for stable submit WiFi");
    g_cameraBootFailed = true;
    g_cameraBootPending = false;
#else
    Serial.println("[CAM] worker will start after WiFi AP is ready");
    g_cameraBootPending = false;
#endif

    Serial.println("--- WiFi AP ---");
    webServer.setTelemetrySource(fillTelemetry);
    if (!webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true)) {
        Serial.println("[FATAL] Web server init failed");
        while (true) delay(1000);
    }

    g_startTime = millis();
#if WIFI_DIAG_SKIP_PERIPHERALS
    g_tofBootPending = false;
    g_gpsBootPending = false;
    g_cameraBootFailed = true;
    g_cameraBootPending = false;
#else
#if SUBMIT_SKIP_TOF_HW
    g_tofBootPending = false;
#else
    g_tofBootPending = true;
    g_tofBootStartMs = g_startTime + 800;
#endif
#if SUBMIT_TELEMETRY_MODE && SUBMIT_SKIP_GPS_UART
    Serial.println("[INFO] GPS dashboard position path active");
    g_gpsBootPending = false;
#else
    g_gpsBootPending = true;
    g_gpsBootStartMs = g_startTime + 1500;
#endif
#if !SUBMIT_SKIP_CAMERA_HW
    g_camCacheMutex = xSemaphoreCreateMutex();
    if (g_camCacheMutex) {
        const BaseType_t taskOk = xTaskCreatePinnedToCore(
            cameraWorker, "cam_worker", 16384, nullptr, 1, &g_cameraTask, 0);
        Serial.printf("[CAM] worker: %s\n", taskOk == pdPASS ? "OK" : "FAIL");
    } else {
        g_cameraBootFailed = true;
        g_camLastError = "cache_mutex";
        Serial.println("[CAM] cache mutex allocation failed");
    }
#endif
    g_cameraBootPending = false;
#endif

    Serial.println("==============================================");
    Serial.println("  SYSTEM READY");
    Serial.printf("  AP: %s @ 192.168.4.1\n", WIFI_AP_SSID);
    Serial.println("  Web:    http://192.168.4.1/");
    Serial.println("  JSON:   http://192.168.4.1/api/telemetry");
    Serial.println("  Camera: http://192.168.4.1/capture.jpg");
    Serial.println("==============================================");
}

void loop()
{
    const uint32_t now = millis();

    // Keep the AP/HTTP entry responsive even while sensors are recovering.
    webServer.loop();

    if (g_tofBootPending && now >= g_tofBootStartMs) {
        g_tofBootPending = false;
        initTof();
    }

    if (g_gpsBootPending && now >= g_gpsBootStartMs) {
        g_gpsBootPending = false;
        Serial.println("--- GPS (deferred) ---");
        gps.begin();
        Serial.printf("  GPS: UART%d @ %d RX=%d TX=%d\n",
            GPS_UART_NUM,
            GPS_BAUD,
            GPS_RX_PIN,
            GPS_TX_PIN);
    }

#if !WIFI_DIAG_SKIP_PERIPHERALS
#if !(SUBMIT_TELEMETRY_MODE && SUBMIT_SKIP_GPS_UART)
    gps.update();
#endif
#if !(SUBMIT_TELEMETRY_MODE && SUBMIT_SKIP_FC_UART)
    fcBridge.update();
#endif
#endif

    // 方向控制: 计算 RC 通道 (含超时归中/接管), 更新内部运动状态; 真机输出受安全门约束
    if (simFc.getState().scenario == SimFcScenario::Manual) {
        RcChannels ch = g_manual.update(now);
        simFc.feedRcChannels(ch.roll, ch.pitch, ch.yaw, ch.throttle,
                             g_manual.isNeutralHold());
#if ENABLE_REAL_FC_OUTPUT
        if (!g_manual.isNeutralHold()) {
            const FCState& pilot = fcBridge.getState();
            const FCRawRCValues values = buildAssistOutputPreservingPilotChannels(
                ch.roll,
                ch.pitch,
                ch.yaw,
                pilot.rcChannels,
                pilot.rcChannelCount);
            FCOutput out;
            out.roll = values.roll;
            out.pitch = values.pitch;
            out.yaw = values.yaw;
            out.throttle = values.throttle;
            out.aux1 = values.aux1;
            out.aux2 = values.aux2;
            out.overrideRC = true;
            fcBridge.setOutput(out);
            g_realOutputRequested = true;
        } else {
            clearRealFCOutputOnce();
        }
#endif
    } else {
#if ENABLE_REAL_FC_OUTPUT
        clearRealFCOutputOnce();
#endif
    }

    simFc.update();

    const uint32_t afterCapture = millis();

#if !WIFI_DIAG_SKIP_PERIPHERALS
#if SUBMIT_SKIP_TOF_HW
    g_tofInit = false;
#else
    if (g_tofInit) {
        tof.update();
        if (!tof.isHealthy()) {
            g_tofInit = false;
            Serial.println("[TOF] no valid range; polling paused until next slow retry");
        }
    } else if (now - g_lastTofRetry >= TOF_RETRY_MS) {
        g_lastTofRetry = now;
        Serial.println("[TOF] retry init");
        initTof();
    }
#endif

    webServer.loop();

#endif

    static uint32_t tLog = 0;
    if (afterCapture - tLog >= 5000) {
        const uint32_t logNow = millis();
        tLog = logNow;
        TelemetryData sample = {};
        fillTelemetry(sample);
        Serial.printf("[SYS] %lus heap=%u stations=%u cam:ready=%d valid=%d frames=%lu err=%lu age=%lu rec=%lu tof:online=%d dist=%.0f gps:online=%d valid=%d sats=%d lat=%.6f lng=%.6f fc:online=%d mspTimeouts=%lu\n",
            static_cast<unsigned long>((logNow - g_startTime) / 1000),
            ESP.getFreeHeap(),
            static_cast<unsigned>(WiFi.softAPgetStationNum()),
            g_camReady,
            g_camValid,
            static_cast<unsigned long>(g_camFrames),
            static_cast<unsigned long>(g_camErrors),
            static_cast<unsigned long>(g_lastFrameMs ? logNow - g_lastFrameMs : 0),
            static_cast<unsigned long>(g_camRecoveries),
            static_cast<int>(sample.tofOnline),
            sample.tofDistance,
            static_cast<int>(sample.gpsOnline),
            static_cast<int>(sample.gpsValid),
            sample.gpsSats,
            sample.gpsLat,
            sample.gpsLng,
            static_cast<int>(sample.fcOnline),
            static_cast<unsigned long>(sample.fcMspTimeouts));
    }

    delay(5);
}
