# No-FC Fallback Delivery Workflow For DeepSeek

Date: 2026-07-03

## Goal

The F4V3S PLUS flight controller is currently damaged. Stop all flight, motor, arming, RC override, and MSP-control work.

This workflow turns the project into a safe, defensible fallback deliverable:

> ESP32-S3 companion sensing and Web demonstration platform, with ToF/GPS diagnostics, camera preview, parameter/debug UI, logs, wiring evidence, and an honest statement that flight-controller integration is paused until replacement hardware is available.

This is the fallback for option 1: a no-flight-controller competition/demo package. It must not pretend the aircraft can fly without a working flight controller.

## Current Project Truth

- The project remains an embedded competition drone umbrella prototype.
- The original safe architecture is ESP32-S3 companion computer plus F4V3S PLUS flight controller.
- The F4V3S PLUS normally owns attitude stabilization, motor/ESC output, arming, failsafe, RC receiver takeover, and basic flight modes.
- The ESP32-S3 owns sensors, Web UI, WebSocket/HTTP telemetry, parameter/debug panel, camera preview, logging, and later limited assist commands only after flight safety gates.
- Because the flight controller is damaged now, the current deliverable is no longer a flying MVP.
- Do not describe ESP32-S3 as directly driving motors, replacing the flight controller safety loop, or enabling autonomous flight.

## Hard Safety Limits

- Remove propellers for any bench work.
- Do not run motor tests.
- Do not power ESC/motor outputs for demonstration.
- Do not send MSP write/control commands.
- Do not enable `MSP_SET_RAW_RC`, arm/disarm, RC override, assist output, or motor control from ESP32.
- Do not claim stable hover, outdoor flight, autonomous following, UWB capability, or high-precision human tracking.
- If the damaged flight controller is powered for diagnosis, it must be separate from all motor/prop tests and treated as untrusted hardware.

## Fallback Deliverables

### D1 - ESP32-S3 Web/Sensor Demo

Show an ESP32-S3 access point or local Web page that can demonstrate:

- Web UI loads reliably.
- `/ping` returns `pong` if the firmware exposes it.
- ToF status is visible:
  - initialized/valid when the VL53L1X is working;
  - clear fault/offline text when not working.
- GPS status is visible:
  - online/NMEA/fix/sats/HDOP when available;
  - no-fix status is acceptable indoors if it is clearly labeled.
- Parameter/debug panel remains whitelist/range based.
- Flight-controller section is explicitly marked `offline`, `not installed`, `damaged`, or `integration paused`.

### D2 - Independent Camera Preview Demo

Use the existing independent camera firmware path instead of merging it into Web MVP:

- AP: `NMC-Camera`
- IP: `192.168.4.1`
- endpoints: `/ping`, `/capture.jpg`, `/stream`
- expected demo quality: about 5 FPS and about 0.5-1 s latency is acceptable for display/debug.

Camera must be described as preview/capture only, not flight-control feedback.

### D3 - Evidence Package

Collect artifacts that can be used in report/PPT/defense:

- screenshots or phone photos of the Web page;
- serial logs showing ToF/GPS/camera startup and status;
- wiring photos or a wiring table for ESP32-S3, ToF, GPS, and camera;
- a risk downgrade table explaining that flight-controller integration is paused because the FC is damaged;
- a replacement plan for restoring the original ESP32-S3 plus F4V3S architecture;
- an acceptance checklist for the no-FC fallback demo.

### D4 - Documentation Cleanup

Before using docs or Web pages in a report, remove or downgrade overclaims:

- replace "high-precision autonomous human following" with "near-range assist follow and situational awareness prototype";
- replace "complex outdoor autonomy" with "constrained demo after flight-controller replacement and safety validation";
- replace any wording that suggests ESP32-S3 directly controls motors;
- mark current FC telemetry/control as unavailable until replacement hardware is tested;
- keep UWB as a future extension only unless real UWB hardware is purchased, wired, tested, and documented.

## Phase 0 - Freeze Flight Work

1. Physically remove propellers.
2. Disconnect or avoid powering motor/ESC output during this fallback work.
3. Label the F4V3S PLUS as damaged/untrusted in the project notes.
4. Stop the UART3 MSP migration workflow until replacement FC hardware exists.
5. Keep `fc_uart3_msp_workflow.md` as historical/replacement-hardware guidance, not the active route.

Acceptance:

- No propellers installed.
- No motor output tests are part of this workflow.
- The active project narrative says "flight-controller integration paused".

## Phase 1 - Audit Claims And Active Files

Target files to inspect first:

```text
AGENTS.md
PROJECT_HANDOFF_2026-07-03.md
workflow.md
plan.md
Summary.md
README.md
index.html
data/index.html
```

Required audit:

- Find claims that imply current flying capability.
- Find claims that imply autonomous following is already proven.
- Find claims that imply ESP32-S3 is the flight controller or directly controls motors.
- Find pages that still present F4V3S telemetry/control as currently working.
- Prepare edits or notes that make the no-FC fallback truthful.

Acceptance:

- A short file-by-file list exists for what must be revised before report/defense use.
- Any changed public-facing text is conservative and test-backed.

## Phase 2 - ESP32-S3 Web/Sensor Bring-Up

Use the Web MVP first.

Build:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp
```

Upload if the ESP32-S3 is on `COM55`:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp -t upload --upload-port COM55
```

Verify:

```text
Web page loads from ESP32-S3 AP.
/ping returns pong if available.
ToF status is shown.
GPS status is shown.
FC status is not falsely shown as online.
No control-write commands are enabled.
```

If ToF needs isolated proof:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-tof-only
```

If GPS needs isolated proof:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-gps-diag
```

Acceptance:

- Web/sensor demo can be shown without a flight controller.
- Sensor failures are displayed as diagnostic state, not hidden.
- Logs are captured for report evidence.

## Phase 3 - Camera Preview Bring-Up

Build:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag
```

Upload if the ESP32-S3 is on `COM55`:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag -t upload --upload-port COM55
```

Verify:

```text
SSID: NMC-Camera
IP:   192.168.4.1
/ping works
/capture.jpg returns an image
/stream shows live MJPEG preview
```

Acceptance:

- Camera preview is usable for demo/display.
- It is not described as flight-control feedback.
- If ToF and camera pin maps conflict, do not claim they run in one combined firmware.

## Phase 4 - No-FC Demo Script

Recommended live demonstration order:

1. Show the hardware and explain the safety downgrade:
   - ESP32-S3, ToF, GPS, camera are active;
   - F4V3S flight controller is damaged, so no flight demo is attempted.
2. Power the ESP32-S3 only.
3. Open the Web MVP and show ToF/GPS/status/debug cards.
4. Move an object in front of ToF and show distance changes.
5. If outdoors or near a window, show GPS online/fix evidence; otherwise explain no-fix indoors.
6. Flash or switch to the independent camera firmware and show `/stream`.
7. End with the replacement-FC recovery plan and the original safety architecture.

Acceptance:

- The demo proves useful subsystem progress without unsafe flight.
- The limitation is stated clearly before judges ask about it.
- No presenter claims that the damaged FC path is currently working.

## Phase 5 - Replacement-FC Recovery Plan

Once a replacement F4V3S PLUS or compatible FC is available:

1. Restore the original architecture: F4V3S owns flight control, ESP32-S3 remains companion.
2. Repeat no-prop desktop checks.
3. Use `fc_uart3_msp_workflow.md` for UART3 MSP proof.
4. Prove UART3 with USB-TTL before ESP32.
5. Build/upload `esp32-s3-fc-uart-probe`.
6. Build/upload `esp32-s3-fc-diag`.
7. Only then integrate read-only FC telemetry into Web MVP.
8. Do not enable assist output until manual hover and RC takeover are verified.

## Acceptance Checklist

The no-FC fallback is complete when all checked items below are true:

```text
[ ] Props removed and no motor tests included.
[ ] Web MVP builds successfully.
[ ] Web MVP uploads to ESP32-S3 or a reason is documented.
[ ] Web page/status panel can be shown.
[ ] ToF status is visible and honest.
[ ] GPS status is visible and honest.
[ ] FC status is offline/damaged/integration-paused, not falsely online.
[ ] Camera diagnostic firmware builds successfully.
[ ] Camera preview can be shown or the blocker is documented.
[ ] Summary/report wording no longer claims current flight capability.
[ ] DeepSeek/Codex notes list changed files, commands, evidence, and assumptions.
```

## DeepSeek Prompt

```text
请先阅读 D:\Code\NMC\AGENTS.md、D:\Code\NMC\PROJECT_HANDOFF_2026-07-03.md、D:\Code\NMC\workflow.md、D:\Code\NMC\plan.md 和 D:\Code\NMC\no_fc_fallback_workflow.md。

当前新事实：
- F4V3S PLUS 飞控已经损坏，当前阶段暂停所有飞行、解锁、电机、RC override、MSP 控制写入和自主辅助输出。
- 不要继续执行 UART3 MSP 迁移，除非后续有替换飞控。`fc_uart3_msp_workflow.md` 只作为换飞控后的恢复路线保留。
- 现在先做“无飞控保底交付”版本：ESP32-S3 Web/Sensor Demo + 独立 Camera Preview + 证据包 + 文档降级说明。
- Codex 正在和 DeepSeek 协作；请把每次工作都写清楚目标文件、命令、证据、假设和未验证事实。

架构边界：
- 原始正确架构仍是 ESP32-S3 伴随计算机 + F4V3S PLUS 飞控。
- F4V3S 正常情况下负责姿态稳定、电机/电调输出、解锁、失控保护、遥控器接管和基础飞行模式。
- ESP32-S3 只负责传感器读取、Web UI、WebSocket/HTTP telemetry、参数/调试面板、摄像头预览、日志，以及未来在安全门槛通过后的有限辅助命令。
- 当前飞控已坏，所以不能说项目现在可飞，也不能说 ESP32-S3 替代飞控直接控制电机。

你的任务：
1. 按 `no_fc_fallback_workflow.md` 执行无飞控保底路线。
2. 先审查这些文件是否有过度宣传或当前飞行能力误导：
   - Summary.md
   - README.md
   - index.html
   - data/index.html
   - workflow.md
   - plan.md
3. 把描述改成保守、可证明的版本：
   - “ESP32-S3 伴随计算机与传感器/Web 展示平台”
   - “近距离辅助跟随与态势感知原型”
   - “飞控集成因硬件损坏暂停，待替换飞控后恢复”
4. 不要写高精度自主人体跟随、复杂户外自主飞行、UWB 已接入、ESP32 直控电机等未验证能力。
5. 优先验证 ESP32-S3 可展示能力：
   - `esp32-s3-web-mvp`
   - `esp32-s3-tof-only`（如需隔离 ToF）
   - `esp32-s3-gps-diag`（如需隔离 GPS）
   - `esp32-s3-ov5640-diag`（独立摄像头预览）
6. 常用命令：
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp`
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp -t upload --upload-port COM55`
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag`
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag -t upload --upload-port COM55`
7. 验收时给出：
   - 修改了哪些文件；
   - 使用了哪些命令；
   - Web/ToF/GPS/Camera 的串口或浏览器证据；
   - 哪些内容是已验证，哪些只是计划；
   - 是否仍需要替换飞控。

禁止项：
- 不要运行电机或装桨测试。
- 不要启用 `MSP_SET_RAW_RC`、arm/disarm、RC override、assist output。
- 不要把 damaged FC 标成 online。
- 不要把相机预览说成飞行闭环视觉控制。
- 不要把无飞控保底版本包装成完整飞行 MVP。
```
