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
#define UNIFIED_FW_TAG      "unified-web-20260708-real-fc-ui"

#define CAMERA_CAPTURE_PERIOD_MS 200
#define CAMERA_CAPTURE_FAIL_LIMIT 3
#define CAMERA_RESTART_DELAY_MS 500
#define CAMERA_RETRY_MS 3000
#define CAMERA_BOOT_RETRY_MS 30000
#define CAMERA_STALE_RESTART_MS 5000

static uint32_t g_startTime = 0;
static bool g_tofInit = false;
static uint32_t g_lastTofRetry = 0;

static uint8_t* g_camBuf = nullptr;
static size_t g_camLen = 0;
static size_t g_camCap = 0;
static bool g_camValid = false;
static bool g_camReady = false;
static bool g_camDriverActive = false;
static bool g_camRestartPending = false;
static uint32_t g_camFrames = 0;
static uint32_t g_camErrors = 0;
static uint32_t g_camFailStreak = 0;
static uint32_t g_camRecoveries = 0;
static uint32_t g_camRestartRequests = 0;
static uint32_t g_lastFrameMs = 0;
static uint32_t g_nextCameraStartMs = 0;
static bool g_cameraBootFailed = false;
static bool g_manualCameraRetry = false;

static bool captureCameraFrame();

uint8_t* getCamBuf() { return g_camBuf; }
size_t getCamLen() { return g_camLen; }
bool getCamValid() { return g_camValid; }
uint32_t getCamFrames() { return g_camFrames; }
uint32_t getCamErrors() { return g_camErrors; }

extern "C" bool requestCameraRetryFromWeb()
{
    if (g_camRestartPending || g_manualCameraRetry) return false;
    g_manualCameraRetry = true;
    return true;
}

extern "C" bool setSimFcScenarioFromWeb(const char* scenario)
{
    return simFc.setScenarioByName(scenario);
}

/* ================================================================
 * 方向控制 (Web 方向输入 → RC 通道 → 仿真; 真机输出受安全门约束)
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
    // 收到方向指令即切入 Manual 仿真场景 (若尚未)
    if (simFc.getState().scenario != SimFcScenario::Manual) {
        simFc.setScenarioByName("manual");
        simFc.resetKinematics();
    }
    return true;
}

static void recoverSccbBus()
{
    if (CAM_PIN_SIOD < 0 || CAM_PIN_SIOC < 0) return;

    pinMode(CAM_PIN_SIOD, INPUT_PULLUP);
    pinMode(CAM_PIN_SIOC, OUTPUT_OPEN_DRAIN);
    digitalWrite(CAM_PIN_SIOC, HIGH);
    delayMicroseconds(10);

    for (uint8_t i = 0; i < 16; i++) {
        digitalWrite(CAM_PIN_SIOC, LOW);
        delayMicroseconds(10);
        digitalWrite(CAM_PIN_SIOC, HIGH);
        delayMicroseconds(10);
    }

    pinMode(CAM_PIN_SIOD, OUTPUT_OPEN_DRAIN);
    digitalWrite(CAM_PIN_SIOD, LOW);
    delayMicroseconds(10);
    digitalWrite(CAM_PIN_SIOC, HIGH);
    delayMicroseconds(10);
    digitalWrite(CAM_PIN_SIOD, HIGH);
    delayMicroseconds(10);

    pinMode(CAM_PIN_SIOD, INPUT_PULLUP);
    pinMode(CAM_PIN_SIOC, INPUT_PULLUP);
    delay(20);
}

static bool initCameraDriver()
{
    recoverSccbBus();

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
    cfg.pin_sccb_sda = CAM_PIN_SIOD;
    cfg.pin_sccb_scl = CAM_PIN_SIOC;
    cfg.pin_pwdn = CAM_PIN_PWDN;
    cfg.pin_reset = CAM_PIN_RESET;
    cfg.xclk_freq_hz = 24000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size = FRAMESIZE_QQVGA;
    cfg.jpeg_quality = 20;
    cfg.fb_count = 2;
    cfg.fb_location = CAMERA_FB_IN_DRAM;
    cfg.grab_mode = CAMERA_GRAB_LATEST;

    return esp_camera_init(&cfg) == ESP_OK;
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
        if (initCameraDriver()) {
            g_camDriverActive = true;
            g_camReady = true;
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

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        g_camErrors++;
        g_camFailStreak++;
        if (g_camFailStreak >= CAMERA_CAPTURE_FAIL_LIMIT) {
            requestCameraRestart("fb_get");
        }
        return false;
    }

    g_camFrames++;
    g_camFailStreak = 0;

    if (fb->len > g_camCap) {
        uint8_t* next = static_cast<uint8_t*>(realloc(g_camBuf, fb->len));
        if (!next) {
            g_camErrors++;
            esp_camera_fb_return(fb);
            return false;
        }
        g_camBuf = next;
        g_camCap = fb->len;
    }

    memcpy(g_camBuf, fb->buf, fb->len);
    g_camLen = fb->len;
    g_camValid = true;
    g_lastFrameMs = millis();

    esp_camera_fb_return(fb);
    return true;
}

static bool scanForTof()
{
    TOF_I2C_PORT.begin(TOF_SDA, TOF_SCL);
    TOF_I2C_PORT.setTimeOut(150);
    TOF_I2C_PORT.setClock(TOF_I2C_FREQ);

    Serial.printf("  Scanning ToF I2C on SDA=%d SCL=%d\n", TOF_SDA, TOF_SCL);
    uint8_t foundCount = 0;
    bool foundTof = false;

    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        TOF_I2C_PORT.beginTransmission(addr);
        uint8_t err = TOF_I2C_PORT.endTransmission();
        if (err == 0) {
            foundCount++;
            Serial.printf("  I2C device: 0x%02X%s\n",
                addr,
                addr == 0x29 ? " (VL53L1X)" : "");
            foundTof = foundTof || (addr == 0x29);
        }
        yield();
    }

    if (foundTof && foundCount == 1) return true;
    if (foundTof) {
        Serial.printf("  ToF scan unstable: %u devices found\n", foundCount);
    }
    return false;
}

static void initTof()
{
    Serial.println("--- ToF ---");

    bool found = false;
    for (uint8_t attempt = 0; attempt < 5 && !found; attempt++) {
        found = scanForTof();
        if (!found) delay(250);
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

    g_tofInit = tof.isInitialized();
    const auto& td = tof.getData();
    Serial.printf("  ToF: init=%d valid=%d status=%u dist=%u errors=%lu\n",
        g_tofInit,
        td.valid,
        td.rangeStatus,
        td.distance,
        static_cast<unsigned long>(td.errorCount));
}

static void fillTelemetry(TelemetryData& data)
{
    const uint32_t now = millis();

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

    const auto& fc = simFc.getState();
    data.fcOnline = false;
    data.fcSimulated = false;
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
        data.fcSimulated = false;
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
        data.errorFlags |= 4;
    }

    // Manual 方向控制场景: 用仿真运动学位置覆盖显示 (m → mm)
    if (fc.scenario == SimFcScenario::Manual) {
        data.posX = fc.posX * 1000.0f;
        data.posY = fc.posY * 1000.0f;
        data.posZ = fc.altitudeCm * 10.0f;
    }

    data.camOnline = g_camReady;
    data.camValid = g_camValid;
    data.camFrames = g_camFrames;
    data.camErrors = g_camErrors;
    data.camAgeMs = g_lastFrameMs ? (now - g_lastFrameMs) : 0;
    data.camBytes = static_cast<uint32_t>(g_camLen);
    data.camRecoveries = g_camRecoveries;
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
    g_manual.begin();
    fcBridge.begin();

    Serial.println("--- Camera ---");
    g_cameraBootFailed = !startCamera();

    Serial.println("--- WiFi AP ---");
    webServer.setTelemetrySource(fillTelemetry);
    if (!webServer.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD, true)) {
        Serial.println("[FATAL] Web server init failed");
        while (true) delay(1000);
    }

    delay(200);
    if (captureCameraFrame()) {
        Serial.printf("  Camera warm-up: OK (%u bytes)\n", static_cast<unsigned>(g_camLen));
    } else {
        Serial.println("  Camera warm-up: not ready");
    }

    initTof();

    Serial.println("--- GPS ---");
    gps.begin();
    Serial.printf("  GPS: UART%d @ %d RX=%d TX=%d\n",
        GPS_UART_NUM,
        GPS_BAUD,
        GPS_RX_PIN,
        GPS_TX_PIN);

    g_startTime = millis();

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

    gps.update();
    fcBridge.update();

    // 方向控制: 计算 RC 通道 (含超时归中/接管), 喂给仿真; 真机输出受安全门约束
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

    g_tofInit = tof.isInitialized();
    if (g_tofInit) {
        tof.update();
    } else if (now - g_lastTofRetry >= 5000) {
        g_lastTofRetry = now;
        Serial.println("[TOF] retry init");
        initTof();
    }

    webServer.loop();

    static uint32_t tRetry = 0;
    if (g_camRestartPending && millis() >= g_nextCameraStartMs) {
        g_camRestartPending = false;
        tRetry = millis();
        startCamera(2, true);
        restoreTofBus();
    } else if (!g_cameraBootFailed && !g_camReady && !g_camRestartPending && millis() - tRetry >= CAMERA_RETRY_MS) {
        tRetry = millis();
        stopCameraDriver();
        startCamera(1, true);
        restoreTofBus();
    }

    static uint32_t tBootRetry = 0;
    if (g_cameraBootFailed && !g_camReady && !g_camRestartPending &&
        WiFi.softAPgetStationNum() > 0 &&
        (tBootRetry == 0 || millis() - tBootRetry >= CAMERA_BOOT_RETRY_MS)) {
        tBootRetry = millis();
        Serial.println("[CAM] low-rate boot retry because a client is connected");
        runCameraRetry("connected-client");
    }

    if (g_manualCameraRetry) {
        g_manualCameraRetry = false;
        runCameraRetry("web");
    }

    static uint32_t tCapture = 0;
    if (millis() - tCapture >= CAMERA_CAPTURE_PERIOD_MS) {
        tCapture = millis();
        captureCameraFrame();
    }

    const uint32_t afterCapture = millis();
    if (g_camReady && g_camValid && g_lastFrameMs &&
        (afterCapture - g_lastFrameMs > CAMERA_STALE_RESTART_MS)) {
        requestCameraRestart("stale");
    }

    static uint32_t tLog = 0;
    if (afterCapture - tLog >= 5000) {
        tLog = afterCapture;
        const auto& td = tof.getData();
        const auto& gd = gps.getData();
        Serial.printf("[SYS] %lus heap=%u stations=%u cam:ready=%d valid=%d frames=%lu err=%lu age=%lu rec=%lu tof:init=%d valid=%d dist=%u err=%lu gps:online=%d valid=%d sats=%u chars=%lu\n",
            static_cast<unsigned long>((afterCapture - g_startTime) / 1000),
            ESP.getFreeHeap(),
            static_cast<unsigned>(WiFi.softAPgetStationNum()),
            g_camReady,
            g_camValid,
            static_cast<unsigned long>(g_camFrames),
            static_cast<unsigned long>(g_camErrors),
            static_cast<unsigned long>(g_lastFrameMs ? afterCapture - g_lastFrameMs : 0),
            static_cast<unsigned long>(g_camRecoveries),
            g_tofInit,
            td.valid,
            td.distance,
            static_cast<unsigned long>(td.errorCount),
            gd.online,
            gd.valid,
            gd.satellites,
            static_cast<unsigned long>(gd.charsProcessed));
    }

    delay(5);
}
