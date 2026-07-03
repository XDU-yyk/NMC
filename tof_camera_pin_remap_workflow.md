# ToF And Camera Pin Remap Workflow For DeepSeek

Date: 2026-07-03

## Goal

Resolve the current ESP32-S3 pin conflict between the VL53L1X ToF sensor and the parallel camera.

Current conflict:

```text
GPIO41 = ToF SDA and camera D6 / Y8
GPIO42 = ToF SCL and camera D5 / Y7
```

These signals cannot share the same physical pins. The recommended fix is:

```text
Keep camera GPIO41/42 unchanged.
Move ToF I2C from GPIO41/42 to GPIO4/5.
Keep ToF XSHUT on GPIO10.
```

This workflow is for the current no-flight-controller fallback route. The flight controller is damaged, so do not do motor, arming, RC override, MSP write, or flight tests while doing this pin remap.

## Final Pin Plan

### ToF VL53L1X

```text
VL53L1X SDA   -> ESP32 GPIO4
VL53L1X SCL   -> ESP32 GPIO5
VL53L1X XSHUT -> ESP32 GPIO10
VL53L1X VCC   -> ESP32 3.3V
VL53L1X GND   -> ESP32 GND
```

### Camera

Keep the current camera pin map:

```text
SIOD / SCCB SDA -> GPIO1
SIOC / SCCB SCL -> GPIO2
VSYNC           -> GPIO6
HREF            -> GPIO7
D0 / Y2         -> GPIO8
D1 / Y3         -> GPIO9
D2 / Y4         -> GPIO14
D3 / Y5         -> GPIO11
D4 / Y6         -> GPIO12
PCLK            -> GPIO13
D5 / Y7         -> GPIO42
D6 / Y8         -> GPIO41
D7 / Y9         -> GPIO39
```

### GPS

Keep unchanged:

```text
ESP32 GPIO15 TX -> GPS RX
ESP32 GPIO18 RX <- GPS TX
```

### Flight Controller

Do not use during this workflow because the FC is damaged.

Keep the planned pins documented for future replacement-FC recovery only:

```text
ESP32 GPIO16 TX -> future FC R3/RX3
ESP32 GPIO17 RX <- future FC T3/TX3
```

## Why This Route

- The camera parallel bus uses many pins and is already working as an independent preview firmware, so avoid moving camera data pins.
- VL53L1X uses I2C, and ESP32-S3 can route I2C to other available GPIOs.
- GPIO4/GPIO5 are not used in the current project pin table and are suitable for low-speed I2C.
- Keeping ToF XSHUT on GPIO10 preserves the proven sensor-side reset fix.
- Do not move ToF onto camera SCCB pins GPIO1/GPIO2; that would couple two unrelated buses and make camera/ToF init order harder to debug.

## Files To Update

Primary file:

```text
include/config.h
```

Required macro changes:

```cpp
#define TOF_SDA             4
#define TOF_SCL             5
#define TOF_XSHUT_PIN       10
```

Also update nearby comments so they state:

```text
ToF I2C uses GPIO4/GPIO5.
Camera keeps GPIO41/GPIO42 for D6/D5.
GPIO10 is ToF XSHUT.
```

If a comment still says GPIO10 conflicts with camera D2, fix it. In the current camera map, camera D2/Y4 is GPIO14, not GPIO10.

Do not change:

```text
GPS_RX_PIN = 18
GPS_TX_PIN = 15
FC_RX_PIN  = 17
FC_TX_PIN  = 16
camera D5/D6 on GPIO42/GPIO41
```

## Phase 0 - Safety Freeze

1. Power off the ESP32-S3 before rewiring.
2. Keep propellers removed.
3. Do not power motor/ESC outputs.
4. Do not use the damaged flight controller for live tests.
5. Do not enable MSP control, RC override, arm/disarm, or assist output.

Acceptance:

- Work is limited to ESP32-S3, ToF, GPS, and camera diagnostics.
- No motor or flight path is touched.

## Phase 1 - Physical Rewire

Move only the ToF I2C wires:

```text
Old: ToF SDA -> GPIO41
Old: ToF SCL -> GPIO42

New: ToF SDA -> GPIO4
New: ToF SCL -> GPIO5
```

Keep:

```text
ToF XSHUT -> GPIO10
ToF VCC   -> 3.3V
ToF GND   -> GND
```

Check:

- SDA/SCL are not swapped.
- GND is common.
- ToF is powered from 3.3V, not 5V, unless the module board explicitly supports 5V input.
- Wires are short and mechanically fixed.
- If I2C remains unstable, add or verify pull-ups to 3.3V, typically 4.7k to 10k.

Acceptance:

- ToF no longer touches GPIO41 or GPIO42.
- GPIO41/GPIO42 are free for camera D6/D5.

## Phase 2 - Code Update

Edit `include/config.h`:

```cpp
#define TOF_SDA             4
#define TOF_SCL             5
#define TOF_XSHUT_PIN       10
```

Keep initial I2C settings conservative:

```cpp
#define TOF_I2C_FREQ        10000
#define TOF_TIMING_BUDGET_MS 100
```

Do not optimize speed until the remapped bus is proven stable.

Acceptance:

- `include/config.h` no longer assigns ToF SDA/SCL to GPIO41/GPIO42.
- Camera macros still assign GPIO41/GPIO42 to Y8/Y7.

## Phase 3 - I2C Scan Verification

Build:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-i2c-scan
```

Upload if ESP32-S3 is on COM55:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-i2c-scan -t upload --upload-port COM55
```

Expected evidence:

```text
I2C bus uses SDA=4 SCL=5
0x29 appears
No large set of phantom addresses
Model ID can be read or scanner reports stable ACK
```

If `0x29` does not appear:

- check SDA/SCL swap;
- check XSHUT wiring to GPIO10;
- check 3.3V/GND;
- check pull-ups;
- power-cycle the VL53L1X;
- do not change camera pins to "fix" this.

## Phase 4 - ToF-Only Verification

Build:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-tof-only
```

Upload if ESP32-S3 is on COM55:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-tof-only -t upload --upload-port COM55
```

Expected evidence:

```text
tof:init=1
valid=1
status=0
distance changes when an object moves
recoveries stay low
no continuous requestFrom() error loop
```

Short transient I2C read errors are acceptable only if ToF remains initialized and valid.

## Phase 5 - Web MVP Verification

Build:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp
```

Upload if ESP32-S3 is on COM55:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp -t upload --upload-port COM55
```

Expected evidence:

```text
Web page loads
ToF status is visible and honest
GPS status is visible and honest
FC status remains damaged/offline/integration paused
No motor or FC control path is enabled
```

## Phase 6 - Camera Verification

Build:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag
```

Upload if ESP32-S3 is on COM55:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag -t upload --upload-port COM55
```

Expected evidence:

```text
SSID: NMC-Camera
IP:   192.168.4.1
/ping works
/capture.jpg returns an image
/stream shows MJPEG preview
```

Camera and ToF do not have to run in the same firmware yet. The acceptance target for this remap is that their physical pins no longer conflict.

## Acceptance Checklist

```text
[ ] ToF SDA moved from GPIO41 to GPIO4.
[ ] ToF SCL moved from GPIO42 to GPIO5.
[ ] ToF XSHUT remains GPIO10.
[ ] Camera keeps GPIO41/GPIO42 as D6/D5.
[ ] include/config.h has TOF_SDA=4 and TOF_SCL=5.
[ ] Any stale GPIO10 camera-conflict comment is fixed.
[ ] esp32-s3-i2c-scan builds.
[ ] esp32-s3-i2c-scan sees VL53L1X at 0x29 or the hardware blocker is documented.
[ ] esp32-s3-tof-only builds and ToF produces valid readings, or blocker is documented.
[ ] esp32-s3-web-mvp builds after the pin change.
[ ] esp32-s3-ov5640-diag builds and camera still works, or blocker is documented.
[ ] No flight-controller, motor, arming, RC override, or assist-output work is done.
```

## DeepSeek Prompt

```text
请先阅读 D:\Code\NMC\AGENTS.md、D:\Code\NMC\PROJECT_HANDOFF_2026-07-03.md、D:\Code\NMC\no_fc_fallback_workflow.md 和 D:\Code\NMC\tof_camera_pin_remap_workflow.md。

当前任务：
解决 ESP32-S3 引脚冲突。现在 `include/config.h` 里 ToF 使用 GPIO41/42：
- `TOF_SDA = 41`
- `TOF_SCL = 42`

但摄像头并口也使用：
- `CAM_PIN_Y8 = 41`，也就是 D6
- `CAM_PIN_Y7 = 42`，也就是 D5

GPIO41/42 不能同时给 ToF I2C 和摄像头数据线使用。

最终方案：
- 摄像头引脚保持不动。
- ToF I2C 从 GPIO41/42 改到 GPIO4/5。
- ToF XSHUT 继续使用 GPIO10。
- GPS 继续使用 GPIO15/18。
- 飞控 GPIO16/17 只保留为未来换飞控后的 UART3 MSP 规划；当前飞控已损坏，不做飞控测试。

请执行以下工作流：
1. 安全冻结：不要装桨、不要跑电机、不要解锁、不要 RC override、不要 MSP 写入、不要做飞控联调。
2. 硬件改线：
   - VL53L1X SDA -> ESP32 GPIO4
   - VL53L1X SCL -> ESP32 GPIO5
   - VL53L1X XSHUT -> ESP32 GPIO10
   - VL53L1X VCC -> 3.3V
   - VL53L1X GND -> GND
3. 修改 `include/config.h`：
   - `#define TOF_SDA 4`
   - `#define TOF_SCL 5`
   - `#define TOF_XSHUT_PIN 10`
   - 保留 `CAM_PIN_Y8 41` 和 `CAM_PIN_Y7 42`
   - 如果注释还说 GPIO10 和摄像头 D2 冲突，请修正；当前摄像头 D2/Y4 是 GPIO14。
4. 先编译并上传 I2C scanner：
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-i2c-scan`
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-i2c-scan -t upload --upload-port COM55`
   - 期望看到 VL53L1X 地址 `0x29`，且没有大量 phantom addresses。
5. 再验证 ToF-only：
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-tof-only`
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-tof-only -t upload --upload-port COM55`
   - 期望 `tof:init=1`、`valid=1`、`status=0`，距离随物体移动变化。
6. 再验证 Web MVP：
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp`
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp -t upload --upload-port COM55`
   - Web 页面应能显示 ToF/GPS 状态，FC 状态必须是 damaged/offline/integration paused，不能显示成在线。
7. 最后验证摄像头独立固件：
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag`
   - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag -t upload --upload-port COM55`
   - 期望 `NMC-Camera` AP、`192.168.4.1`、`/ping`、`/capture.jpg`、`/stream` 可用。

验收时请报告：
- 实际改了哪些文件；
- 最终引脚表；
- 硬件怎么接；
- 每个 PlatformIO 命令的结果；
- I2C scanner 是否看到 `0x29`；
- ToF-only 串口证据；
- Web MVP 证据；
- Camera preview 证据；
- 哪些通过，哪些未验证；
- 是否仍需要替换飞控。

禁止项：
- 不要把 ToF 继续留在 GPIO41/42。
- 不要改摄像头 D5/D6 到别的脚，除非这条方案失败且用户重新批准。
- 不要把 ToF 接到摄像头 SCCB 的 GPIO1/2 上。
- 不要启用飞控、马达、解锁、RC override、MSP_SET_RAW_RC 或任何自主辅助输出。
- 不要把当前无飞控保底版本说成完整飞行 MVP。
```
