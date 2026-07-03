# NMC Project Handoff Summary

Date: 2026-07-03

Use this file as the first-context handoff for a new Codex or DeepSeek conversation.

## 1. Project Identity

This repository is `D:\Code\NMC`.

The project is an embedded competition prototype: an ESP32-S3 companion computer plus an F4V3S PLUS flight controller for a drone/umbrella demonstration platform.

The realistic claim is:

> Near-range assist follow and situational-awareness prototype, with Web telemetry, camera preview, GPS/ToF diagnostics, and RC-safe flight-controller integration.

Do not claim high-precision autonomous human following, complex outdoor autonomy, or UWB capability unless new hardware and tests prove it.

## 2. Architecture Rules

Read `AGENTS.md` before changing plans, reports, or code.

Core split:

- F4V3S PLUS owns low-level flight control:
  - attitude stabilization;
  - motor/ESC output;
  - arming/disarming;
  - failsafe;
  - RC receiver takeover;
  - basic flight modes.
- ESP32-S3 owns upper-layer work:
  - sensors;
  - Web UI;
  - WebSocket/HTTP telemetry;
  - parameter/debug panel;
  - camera preview;
  - logging;
  - later limited assist commands only after safety gates.

Do not describe ESP32-S3 as directly driving motors or replacing the flight controller safety loop.

Safety gate order:

```text
no-prop desktop checks
motor direction/order checks
tethered or constrained tests
low-altitude manual flight
assisted follow
outdoor demo
```

## 3. Important Current Hardware Facts

### ESP32-S3

- Board target: `esp32-s3-devkitc-1`.
- Main serial port observed earlier: `COM55`, CP210x.
- PlatformIO executable commonly used:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe
```

### F4V3S PLUS

- Board/manual: B-CUBE F4V3S PLUS.
- Firmware target shown in manual: `betaflight_4.1.1_OMNIBUSF4SD.hex` or INAV equivalent.
- Current update: user reports the flight controller is damaged. Treat all flight, motor, arming, RC override, and MSP-control work as paused until replacement hardware is available.
- Current safety conclusion from manual:
  - upper `R6/T6` pads are receiver/Serial RX interface, especially CRSF;
  - do not continue using `R6/T6` as the MSP telemetry path;
  - use `UART3` pads `R3/T3` for MSP telemetry only after a replacement/working FC exists.

### VL53L1X ToF

Remap completed (2026-07-03): ToF I2C moved from GPIO41/42 to GPIO4/5 to avoid camera conflict.

```text
SDA   -> ESP32 GPIO4   (was GPIO41, camera D6 conflict)
SCL   -> ESP32 GPIO5   (was GPIO42, camera D5 conflict)
XSHUT -> ESP32 GPIO10
GND   -> GND
VCC   -> 3.3V
```

Config (updated in `include/config.h`):

```cpp
#define TOF_I2C_PORT         Wire1
#define TOF_SDA              4
#define TOF_SCL              5
#define TOF_I2C_FREQ         10000
#define TOF_TIMING_BUDGET_MS 100
#define TOF_XSHUT_PIN        10
```

The remap frees GPIO41/42 for camera exclusive use. Both firmwares can now share the same wiring without pin conflicts.

### GPS

Current intended wiring/config:

```text
GPS TX -> ESP32 GPIO18 RX
GPS RX -> ESP32 GPIO15 TX
GND    -> GND
VCC    -> module-appropriate power
```

Config:

```cpp
#define GPS_RX_PIN 18
#define GPS_TX_PIN 15
#define GPS_BAUD   9600
```

### Camera

There is an independent camera/Web stream firmware in `src/ov5640_diag_main.cpp`.

User-reported current camera result:

- about 5 FPS;
- about 0.5-1 s latency;
- acceptable for project demo/Web preview;
- not acceptable for high-speed visual closed-loop control.

Current role:

> Local WiFi camera preview and capture for display/debug, not flight-control feedback.

## 4. Important Repository Files

Core project memory and planning:

```text
AGENTS.md
workflow.md
plan.md
Summary.md
task_plan.md
findings.md
progress.md
no_fc_fallback_workflow.md
tof_camera_pin_remap_workflow.md
fc_uart3_msp_workflow.md
ov5640_web_workflow.md
```

Main config/build:

```text
include/config.h
platformio.ini
```

Main standalone firmware entries:

```text
src/web_main.cpp
src/raw_http_main.cpp
src/i2c_scan_main.cpp
src/tof_only_main.cpp
src/gps_diag_main.cpp
src/fc_uart_probe_main.cpp
src/fc_diag_main.cpp
src/idle_main.cpp
src/ov5640_diag_main.cpp
```

Main modules:

```text
src/sensors/tof.cpp / tof.h
src/sensors/gps.cpp / gps.h
src/comm/msp.cpp / msp.h
src/comm/fc_bridge.cpp / fc_bridge.h
src/web/server.cpp / server.h
src/web/index_html.h
src/vision/camera.cpp / camera.h
```

Frontend/static files:

```text
data/index.html
index.html
```

Note: some Chinese comments in source files are mojibake in the terminal, but the code still compiles in previous runs.

## 5. PlatformIO Environments

Known environments in `platformio.ini`:

```text
esp32-s3-devkitc-1        full-ish original target
esp32-s3-web-mvp          Web MVP + ToF + GPS
esp32-s3-raw-http         minimal WiFi/HTTP diagnostic
esp32-s3-i2c-scan         I2C scanner diagnostic
esp32-s3-tof-only         ToF-only diagnostic
esp32-s3-gps-diag         GPS-only diagnostic
esp32-s3-fc-diag          flight-controller MSP diagnostic
esp32-s3-fc-uart-probe    raw MSP byte probe
esp32-s3-idle             quiet ESP32 firmware for external USB-TTL tests
esp32-s3-ov5640-diag      independent camera Web stream/capture firmware
```

Typical build command:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e <env>
```

Typical upload command:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e <env> -t upload --upload-port COM55
```

## 6. Completed Work

### 6.1 Planning And Scope Grounding

Completed:

- Project architecture was grounded as ESP32-S3 companion plus F4V3S flight controller.
- Documentation guidance now avoids overclaiming high-precision autonomous following.
- `AGENTS.md` contains durable project-level memory and safety gates.
- `workflow.md` and `plan.md` are the grounded planning docs.

Important constraints:

- UWB is not current hardware.
- Complex autonomous human following is not a current acceptance item.
- The competition story should stay conservative and test-backed.

### 6.2 Web MVP

Completed:

- ESP32-S3 AP/Web MVP was brought up.
- Browser access to `192.168.4.1` was debugged.
- `/ping` returned `pong`.
- A minimal HTTP diagnostic page worked.
- Web MVP shows diagnostic cards/status/log-style telemetry.
- Captive-portal helper paths such as `/generate_204` and `/hotspot-detect.html` were added earlier.
- `src/web/server.cpp` can prefer `LittleFS` `/index.html` when available, allowing teammate-edited frontend pages.

Known limitation:

- Web MVP integration of flight-controller telemetry is not complete yet.
- Camera is currently better treated as an independent firmware/demo, not merged into Web MVP.

### 6.3 ToF VL53L1X

Completed:

- ToF driver exists in `src/sensors/tof.cpp/.h`.
- Uses Pololu VL53L1X base support plus custom safer direct result-frame reads.
- I2C was moved to stable pins:

```text
SDA=41
SCL=42
```

- XSHUT was wired to GPIO10 and configured in `include/config.h`.
- Firmware-controlled XSHUT reset solved the startup `probe_ack/init_model` stuck state.
- Web MVP with ToF was built and uploaded successfully in previous testing.
- Serial evidence after XSHUT showed:
  - `tof:init=1`;
  - `valid=1`;
  - `status=0`;
  - realistic changing distance;
  - recoveries stayed low/zero.

Remaining ToF caveat:

- Occasional Arduino `Wire.cpp requestFrom(): i2cRead returned Error -1 / 263` still appears.
- Current interpretation: tolerated transient I2C read failures, not fatal initialization failure.
- Full elimination likely requires hardware cleanup:
  - shorter wires;
  - stable pull-ups;
  - better power/decoupling;
  - cleaner module;
  - less noise.

### 6.4 GPS NEO-M8N

Completed enough for MVP diagnostics:

- GPS driver exists in `src/sensors/gps.cpp/.h`.
- GPS diagnostic target exists: `esp32-s3-gps-diag`.
- User previously moved GPS outdoors and saw NMEA data.
- A later sample showed a valid fix:

```text
gps:online=1 valid=1 sats=5 hdop=2.0
```

Current status:

- GPS is acceptable for telemetry/status.
- GPS alone is not enough for human following or lateral person tracking.

### 6.5 Camera Web Preview

Completed enough for display MVP:

- `src/ov5640_diag_main.cpp` exists as independent AP camera firmware.
- It starts AP:

```text
SSID: NMC-Camera
IP:   192.168.4.1
```

- It implements:
  - `/ping`;
  - `/capture.jpg`;
  - `/stream`;
  - simple HTML page.
- User reports current camera result:
  - about 5 FPS;
  - 0.5-1 s latency;
  - acceptable for this project stage.

Important limitation:

- Use for visual display/debug only.
- Do not use current camera stream for real-time flight closed-loop control.
- Source header says OV2640 in places while project discussion says OV5640. Clean naming later.

### 6.6 Flight Controller MSP Layer

Completed in code:

- `src/comm/msp.h` and `src/comm/msp.cpp` were rewritten/cleaned earlier.
- Diagnostic counters were added:
  - `txFrames`;
  - `txBytes`;
  - `rxBytes`;
  - `rxDollarCount`;
  - `rxHeaderCount`;
  - `lastRxByte`;
  - `lastError`.
- `src/fc_uart_probe_main.cpp` was added for raw read-only MSP request/response probing.
- `src/fc_diag_main.cpp` exists for read-only FC diagnostic polling.
- `esp32-s3-fc-uart-probe` and `esp32-s3-fc-diag` previously compiled.
- `esp32-s3-fc-uart-probe` was uploaded to `COM55` earlier.

Critical MSP correction:

MSP v1 direction must be:

```text
Request to FC:  $M<  -> hex prefix 24 4D 3C
Reply from FC:  $M>  -> hex prefix 24 4D 3E
```

Current `src/comm/msp.h` has:

```cpp
#define MSP_HEADER_DIR_TO_FC    '<'
#define MSP_HEADER_DIR_FROM_FC  '>'
```

Do not revert this.

## 7. Flight Controller Current State And Next Path

Current update:

- The user reports the F4V3S PLUS flight controller is damaged.
- Do not continue live FC bring-up, motor work, arming, RC override, MSP writes, or UART3 proof on the damaged board.
- The active route is now `no_fc_fallback_workflow.md`: ESP32-S3 Web/sensor demo, independent camera preview, evidence package, and honest documentation downgrade.
- Keep the UART3 notes below only as the recovery path for a replacement/working flight controller.

### 7.1 UART6 R6/T6 Is Superseded

Previous attempt:

- Tried to use F4V3S UART6 `R6/T6` for MSP.
- USB-TTL loopback worked.
- R6/T6 did not respond to:
  - ASCII `#`;
  - MSP request `24 4D 3C 00 01 01`.
- Betaflight `serial/resource` had shown UART6 resources, but the hardware still did not behave as a usable MSP port.

Manual finding:

- The B-CUBE F4V3S PLUS manual labels the upper R6/T6 pads as CRSF receiver interface.
- Manual says to enable UART6 as Serial RX.

Conclusion:

```text
Do not continue UART6/R6/T6 MSP debugging.
Reserve UART6 R6/T6 for receiver/Serial RX/CRSF or leave unused during bench tests.
```

### 7.2 Replacement-FC Route: UART3 R3/T3 MSP

Use `fc_uart3_msp_workflow.md` as the detailed workflow only after replacement/working FC hardware is available.

Target wiring:

```text
ESP32 GPIO16 TX  ->  F4V3S R3 / RX3
ESP32 GPIO17 RX  <-  F4V3S T3 / TX3
ESP32 GND        ->  F4V3S GND
```

Betaflight CLI for UART3 MSP:

```text
serial 2 1 115200 57600 0 115200
save
```

If UART6 is unused during bench tests:

```text
serial 5 0 115200 57600 0 115200
save
```

If UART6 is used by CRSF receiver:

```text
serial 5 64 115200 57600 0 115200
set serialrx_provider = CRSF
save
```

First proof must be USB-TTL, before ESP32:

```text
USB-TTL TX  -> F4V3S R3 / RX3
USB-TTL RX  <- F4V3S T3 / TX3
USB-TTL GND -> F4V3S GND
VCC not connected
```

Test 1, ASCII:

```text
#
```

Expected:

```text
Entering CLI Mode
#
```

Then send:

```text
exit
```

Test 2, HEX MSP:

```text
24 4D 3C 00 01 01
```

Expected:

```text
24 4D 3E ...
```

Only after USB-TTL UART3 passes should ESP32 probe/diag be rebuilt and uploaded.

## 8. Current Code State Notes

`git status --short` currently shows modified/untracked files, including:

```text
findings.md
include/config.h
platformio.ini
progress.md
src/fc_diag_main.cpp
src/fc_uart_probe_main.cpp
src/web/server.cpp
task_plan.md
fc_uart3_msp_workflow.md
data/
index.html
```

Also modified local metadata:

```text
.codegraph/daemon.pid
.reasonix/desktop-topic-title-sources.json
.reasonix/desktop-topic-titles.json
```

Treat user/DeepSeek changes as intentional. Do not revert unrelated changes.

`include/config.h` already contains updated comments saying FC MSP uses UART3 R3/T3 while ESP32 pins remain GPIO16/17.

Important: changing the comments is not the same as proving hardware. UART3 still needs USB-TTL proof and ESP32 proof.

## 9. Remaining Work

### Immediate

1. Follow `no_fc_fallback_workflow.md`.
2. Freeze all flight-controller work:
   - no propellers;
   - no motor tests;
   - no arming/disarming;
   - no MSP writes;
   - no RC override or assist output.
3. Build/verify the ESP32-S3 Web/sensor fallback:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp
```

4. If hardware is connected, upload Web MVP to `COM55` and collect browser/serial evidence:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp -t upload --upload-port COM55
```

5. Build/verify the independent camera preview:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag
```

6. Audit and downgrade public-facing claims in:
   - `Summary.md`;
   - `README.md`;
   - `index.html`;
   - `data/index.html`;
   - any report/defense text.
7. Make FC status explicit as damaged/offline/integration paused.
8. Collect the fallback evidence package:
   - Web screenshots/photos;
   - ToF/GPS serial logs;
   - camera preview evidence;
   - wiring table/photos;
   - risk downgrade and replacement-FC recovery plan.

### Next

9. Keep Web parameter writes whitelisted and range-checked.
10. Improve the no-FC demo page/report so it presents the ESP32-S3 companion platform honestly.
11. Keep camera preview independent unless pin-map and memory conflicts are redesigned.

### After Replacement/Working FC Exists

12. Do UART3 USB-TTL proof on R3/T3 using `fc_uart3_msp_workflow.md`.
13. Build/upload `esp32-s3-fc-uart-probe` and verify `rx=24 4D 3E ...`.
14. Build/upload `esp32-s3-fc-diag` and verify `[FC] online=1`.
15. Integrate read-only FC telemetry into Web MVP.
16. Manual flight bring-up with props off first, then RC receiver setup, failsafe, motor direction/order, tethered hover, and only then limited assist.

## 10. Do Not Do Yet

Do not enable these during current bring-up:

```text
MSP_SET_RAW_RC
arm/disarm from ESP32
RC override
autonomous assist output
motor control from ESP32
motor tests
propeller tests
flight demo
complex visual following
high-precision human tracking claims
UWB claims
```

Do not spend more time on UART6 MSP unless new board documentation proves R6/T6 can be used differently.

Do not continue UART3 MSP proof on the damaged flight controller. Current active route is `no_fc_fallback_workflow.md`; UART3 is reserved for replacement/working FC recovery.

Do not present the damaged flight controller as online or flight-ready in Web UI, reports, PPT, or defense materials.

## 11. Known Conflicts And Caveats

- ToF uses GPIO41/42.
- Camera current pin map also uses GPIO41/42 for data lines, so ToF and camera cannot be naively merged on the current pin map.
- Recommended pin-conflict fix: keep camera GPIO41/42 as D6/D5, move ToF I2C to GPIO4/GPIO5, and keep ToF XSHUT on GPIO10. See `tof_camera_pin_remap_workflow.md`.
- ToF XSHUT is GPIO10.
- Camera pin map and comments have changed over time; verify `include/config.h` before wiring.
- Current camera firmware is independent. Do not assume it can run simultaneously with ToF/GPS/Web MVP.
- The `data/index.html` and root `index.html` appear to be frontend/static assets; validate what is actually uploaded to LittleFS before relying on them.
- Serial monitor chunking can split long `[SYS]` lines; this is display behavior, not necessarily firmware printing duplicate logs.

## 12. Recommended New-Conversation Prompt

Paste this to a new assistant:

```text
You are working in D:\Code\NMC. Read AGENTS.md, PROJECT_HANDOFF_2026-07-03.md, and no_fc_fallback_workflow.md first.

Architecture: ESP32-S3 is a companion computer; F4V3S PLUS owns stabilization, motors, arming, failsafe, and RC takeover. Do not make ESP32 directly control motors.

Current state:
- The F4V3S PLUS flight controller is damaged. Stop all flight, motor, arming, RC override, MSP write/control, and autonomous assist work.
- The active route is now the no-flight-controller fallback in no_fc_fallback_workflow.md.
- Web MVP exists and ToF/GPS diagnostics are mostly working.
- ToF VL53L1X was previously configured on ESP32 GPIO41/42 with XSHUT GPIO10; GPIO41/42 conflicts with camera D6/D5, so follow `tof_camera_pin_remap_workflow.md` and move ToF I2C to GPIO4/GPIO5.
- GPS NEO-M8N has produced valid outdoor fix evidence.
- Camera Web preview exists as an independent AP firmware, about 5 FPS and 0.5-1s latency, acceptable for display/debug only.
- Flight-controller MSP over UART6 R6/T6 is abandoned because the F4V3S manual marks R6/T6 as receiver/Serial RX interface.
- UART3 R3/T3 MSP is reserved for replacement/working FC recovery only.
- MSP direction is request `$M<` / reply `$M>`; do not revert this.
- Codex is collaborating with DeepSeek on this project. Keep changes scoped, report target files, commands, evidence, assumptions, and unverified hardware facts.

Next task:
Execute the no-FC fallback workflow:
1. Freeze flight work: no props, no motor tests, no arming, no MSP writes, no RC override, no assist output.
2. Build/verify esp32-s3-web-mvp and collect Web/ToF/GPS evidence.
3. Build/verify esp32-s3-ov5640-diag and collect camera preview evidence.
4. Audit Summary.md, README.md, index.html, data/index.html, workflow.md, and plan.md for overclaims.
5. Downgrade wording to "ESP32-S3 companion sensing and Web demonstration platform" and "near-range assist follow and situational awareness prototype".
6. Mark FC telemetry/control as damaged/offline/integration paused until replacement hardware is tested.
7. Keep fc_uart3_msp_workflow.md only as the replacement-FC recovery route.

Do not claim current flight capability, high-precision autonomous human following, UWB capability, or ESP32 direct motor control.
```
