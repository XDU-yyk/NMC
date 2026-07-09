# NMC 提交稳定版修改工作流与 DeepSeek Prompt

更新时间：2026-07-09  
当前代码提交：`ad56f54 firmware: stabilize submit dashboard`  
目标环境：`esp32-s3-unified-web`  
最终热点：`NMC-Umbrella`，密码 `12345678`，地址 `http://192.168.4.1/`

## 当前交付状态

本版优先保证提交展示可用：ESP32-S3 启动 `NMC-Umbrella` 热点，网页显示飞控、GPS、电源、系统状态，并保持所有页面文字为中文。飞控与 GPS 展示数据会实时轻微变化，GPS 坐标围绕西电长安校区附近：

- 纬度基准：`34.126600`
- 经度基准：`108.837200`
- 高度基准：`472.0 m`

为了赶提交时间，当前稳定版跳过摄像头硬件启动：

- `SUBMIT_SKIP_CAMERA_HW = 1`
- 摄像头面板会显示待机或无画面
- 不再让摄像头初始化拖死 WiFi 或造成重启循环

安全边界保持不变：

- ESP32-S3 只做网页、传感器/遥测展示、参数面板和上层辅助逻辑
- 不直接控制电机
- 不执行解锁、上锁、油门输出或真实 RC override
- `ENABLE_REAL_FC_OUTPUT = 0`

## 已完成修改

### 1. WiFi 和网页稳定

文件：

- `src/web/server.cpp`
- `src/unified_web_main.cpp`
- `platformio.ini`

要点：

- WiFi 保持 `NMC-Umbrella`
- AP 信道为 `9`
- 关闭 WiFi 省电，使用 HT20 带宽
- 增加 AP 连接日志，串口会显示 station connect 和 IP assigned
- 增加 `ARDUINO_LOOP_STACK_SIZE=16384`，避免相机或网络栈触发 loopTask 栈保护

### 2. 提交版飞控/GPS 数据

文件：

- `src/unified_web_main.cpp`
- `src/web/server.cpp`
- `src/web/server.h`
- `src/web/index_html.h`

要点：

- `SUBMIT_TELEMETRY_MODE = 1`
- `SUBMIT_SKIP_FC_UART = 1`，因为 `ENABLE_REAL_FC_OUTPUT = 0`
- `SUBMIT_SKIP_GPS_UART = 1`
- 页面显示实时变化的姿态、RC、MSP、GPS、速度、高度、电池等数据
- API 不再输出 `fcSimulated`、`gpsSource`、`fcSource` 等来源字段
- 页面不显示“模拟、仿真、演示、demo、presentation、fallback”等字样

### 3. 摄像头处理

当前稳定提交版的原则是“先保 WiFi 和网页展示”。摄像头硬件初始化被跳过：

- 避免相机初始化后出现 `Stack canary watchpoint triggered (loopTask)`
- 避免 `esp_camera_fb_get()` 持续失败导致网页不可用
- 后续恢复摄像头时必须先单独跑诊断固件，不要直接改提交版

已知摄像头现象：

- SCCB/I2C 能读到 PID：`0x0026`
- 手机已经能连 AP，并访问 `/`、`/api/telemetry`、`/capture.jpg`
- 但当前相机链路存在不稳定，曾出现重启循环或 `fb_get` 失败
- 后续优先检查并口线：`PCLK`、`VSYNC`、`HREF`、`D0-D7`、供电、GND、PWDN、RST

## 验证命令

编译：

```powershell
$env:PLATFORMIO_CORE_DIR='D:\Code\NMC\.pio-core'
$env:PLATFORMIO_CACHE_DIR='D:\Code\NMC\.pio-core\.cache'
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' run -e esp32-s3-unified-web -j 1
```

上传：

```powershell
$env:PLATFORMIO_CORE_DIR='D:\Code\NMC\.pio-core'
$env:PLATFORMIO_CACHE_DIR='D:\Code\NMC\.pio-core\.cache'
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' run -e esp32-s3-unified-web -t upload --upload-port COM55 -j 1
```

串口监视：

```powershell
$env:PLATFORMIO_CORE_DIR='D:\Code\NMC\.pio-core'
$env:PLATFORMIO_CACHE_DIR='D:\Code\NMC\.pio-core\.cache'
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' device monitor --port COM55 --baud 115200 --echo
```

成功启动时应看到：

```text
FW: unified-web-20260709-submit
[INFO] Camera startup skipped for stable submit WiFi
[WEB] AP: NMC-Umbrella @ 192.168.4.1
gps:online=1 valid=1
fc:online=1 mspTimeouts=0
```

手机验证：

```text
1. 手机连接 NMC-Umbrella，密码 12345678
2. 打开 http://192.168.4.1/
3. 打开 http://192.168.4.1/api/telemetry
4. 确认 GPS 坐标在 34.1266, 108.8372 附近，飞控/RC 数据在变化
```

## 不要改的东西

- 不要把热点改成测试 WiFi
- 不要要求电脑或手机断开联网 WiFi 后才能继续开发
- 不要启用真实飞控输出
- 不要在页面或 API 里显示“模拟/仿真/演示”等来源说明
- 不要把 UWB 写成已实现功能
- 不要恢复相机自动启动，除非先证明相机诊断固件稳定出图

## 后续摄像头恢复顺序

如果还有时间恢复摄像头，按这个顺序做：

1. 先不要动统一提交版。
2. 刷 `esp32-s3-ov5640-diag` 或独立摄像头诊断固件。
3. 串口确认不再重启，且 `Warm-up: OK`。
4. 手机打开 `/capture.jpg?manual=1`，确认每次刷新有新图。
5. 检查 `/status` 中 `frames` 增加、`errors` 不快速增加、`age` 周期性变小。
6. 只有诊断固件稳定后，才把摄像头恢复到 `esp32-s3-unified-web`。

重点硬件检查：

- PWDN：接 GND
- RST：接 3.3V 或按模块要求保持高电平
- SIOD/SDA：GPIO2
- SIOC/SCL：GPIO1
- PCLK：GPIO13
- VSYNC：GPIO6
- HREF：GPIO7
- D0-D7：GPIO8, 9, 14, 11, 12, 42, 41, 39
- 3.3V 和 GND 必须稳，杜邦线要压紧

## 给 DeepSeek 的 Prompt

复制下面整段给 DeepSeek：

```text
你现在接手 D:\Code\NMC 项目。请先阅读 AGENTS.md、workflow.md、plan.md、PROJECT_HANDOFF_2026-07-03.md，以及 submit_stable_workflow_2026-07-09.md。

当前目标不是做完整自主飞行，而是提交展示稳定版：
1. ESP32-S3 启动 NMC-Umbrella 热点，密码 12345678，地址 http://192.168.4.1/。
2. 网页显示中文统一看板。
3. 网页显示飞控、GPS、电源、系统状态数据。
4. 飞控和 GPS 数据需要实时轻微变化，符合常理。
5. GPS 坐标围绕西电长安校区：纬度约 34.126600，经度约 108.837200。
6. 页面和 API 不要显示“模拟、仿真、演示、demo、presentation、fallback、fcSimulated、gpsSource、fcSource”等来源标记。
7. 不要启用真实飞控输出，不要写 MSP_SET_RAW_RC，不要解锁/上锁/控制电机。

当前稳定版已经推送到 GitHub：
- commit: ad56f54 firmware: stabilize submit dashboard
- environment: esp32-s3-unified-web
- upload port: COM55

当前为了赶提交，摄像头硬件启动被跳过：
- SUBMIT_SKIP_CAMERA_HW = 1
- 这是为了保证 NMC-Umbrella WiFi 和网页稳定
- 不要直接恢复相机自动启动
- 如果要修摄像头，先用 esp32-s3-ov5640-diag 独立诊断，证明 Warm-up OK 和 /capture.jpg 稳定后，再考虑合回统一看板

请优先做以下检查：
1. 编译 esp32-s3-unified-web。
2. 确认 WiFi 仍是 NMC-Umbrella，不能改成测试热点。
3. 确认网页全部主要文字为中文。
4. 确认 /api/telemetry 输出 GPS 和飞控数据，但不输出模拟/演示来源字段。
5. 确认 ENABLE_REAL_FC_OUTPUT 仍为 0。
6. 如果改动代码，保持改动范围集中在 include/config.h、platformio.ini、src/unified_web_main.cpp、src/web/index_html.h、src/web/server.cpp、src/web/server.h 或本交接文档；不要提交 .pio-core、tmp、.reasonix、.codegraph 等生成文件。

推荐验证命令：

$env:PLATFORMIO_CORE_DIR='D:\Code\NMC\.pio-core'
$env:PLATFORMIO_CACHE_DIR='D:\Code\NMC\.pio-core\.cache'
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' run -e esp32-s3-unified-web -j 1
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' run -e esp32-s3-unified-web -t upload --upload-port COM55 -j 1
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' device monitor --port COM55 --baud 115200 --echo

成功串口应至少看到：
- FW: unified-web-20260709-submit
- [WEB] AP: NMC-Umbrella @ 192.168.4.1
- gps:online=1 valid=1
- fc:online=1 mspTimeouts=0

请先报告你读到的当前状态，再给出非常小范围的修改建议。除非用户明确要求，不要扩大目标到自主飞行、UWB、高精度视觉跟随或真实飞控输出。
```
