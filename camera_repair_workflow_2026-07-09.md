# 摄像头修复工作流 2026-07-09

## 当前边界

- 提交稳定版仍以 `esp32-s3-unified-web` 为主，WiFi 必须保持 `NMC-Umbrella`。
- 稳定版暂时跳过摄像头硬件启动，避免摄像头初始化导致 AP 重启或手机无法连接。
- 真实飞控输出继续关闭，不做电机、解锁、RC override 测试。
- 摄像头修复先走独立诊断环境 `esp32-s3-ov5640-diag`，确认不重启且能取帧后，再回灌到统一网页。

## 已知事实

- 摄像头 SCCB 控制链路能通，日志曾出现 `Camera SCCB ACK addr=0x30` 和 `Camera PID: 0x0026`。
- 问题集中在 `esp_camera_fb_get()`、warm-up 或 DMA 取帧阶段，不是简单的传感器完全无响应。
- 之前 `esp32-s3-ov5640-diag` 在 warm-up 附近触发 `loopTask` 栈保护。
- `esp32-s3-unified-web` 已经有 `-DARDUINO_LOOP_STACK_SIZE=16384`，但诊断环境之前没有。
- 当前已在 `platformio.ini` 的 `[env:esp32-s3-ov5640-diag]` 加入：
  - `-pipe`
  - `-DARDUINO_LOOP_STACK_SIZE=16384`
- 当前 `src/ov5640_diag_main.cpp` 已把 `esp_camera_init`、`esp_camera_fb_get` 和恢复逻辑移入独立的 `cam_worker` 任务（16KB）。HTTP 只读取受互斥锁保护的 JPEG 缓存，不再直接调用 camera driver。
- 本次 Codex 环境里 xtensa 工具链被 Windows 权限限制，assembler/linker 不能写子目录目标文件，所以未能完成本机编译和烧录验证。这是本地执行环境问题，不是源码语法错误。

## 硬件检查顺序

1. 确认摄像头 `PWDN` 接 GND。
2. 确认摄像头 `RST` 接 3.3V。
3. 确认摄像头供电 3.3V 稳定，GND 共地。
4. 确认 SCCB 当前接线为：
   - `SIOD/SDA -> GPIO2`
   - `SIOC/SCL -> GPIO1`
5. 确认 DVP 数据线：
   - `D0/Y2 -> GPIO8`
   - `D1/Y3 -> GPIO9`
   - `D2/Y4 -> GPIO14`
   - `D3/Y5 -> GPIO11`
   - `D4/Y6 -> GPIO12`
   - `D5/Y7 -> GPIO42`
   - `D6/Y8 -> GPIO41`
   - `D7/Y9 -> GPIO39`
   - `VSYNC -> GPIO6`
   - `HREF -> GPIO7`
   - `PCLK -> GPIO13`
6. 如果 PID 能读到但一直无法取帧，优先检查 `PCLK/VSYNC/HREF/D0-D7` 和模块是否自带 XCLK 晶振。

## 诊断固件验证

普通 Windows 终端里执行：

```powershell
$env:PLATFORMIO_CORE_DIR='D:\Code\NMC\.pio-core'
$env:PLATFORMIO_CACHE_DIR='D:\Code\NMC\.pio-core\.cache'
$env:TEMP='D:\Code\NMC\tmp'
$env:TMP='D:\Code\NMC\tmp'
$env:TMPDIR='D:\Code\NMC\tmp'
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' run -e esp32-s3-ov5640-diag -t upload --upload-port COM55 -j 1
```

然后看串口：

```powershell
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' device monitor --port COM55 --baud 115200 --echo
```

## 2026-07-15 实测更新

- `esp32-s3-ov5640-diag` 已在 COM55 实测稳定：`PID=0x0026`、首帧成功、连续 25 秒约 5 fps、`errors=0`、没有 `loopTask` 栈溢出或恢复循环。
- 已将相同 worker 模式回灌到 `esp32-s3-unified-web`：`SUBMIT_SKIP_CAMERA_HW=0`，最终 AP 保持 `NMC-Umbrella`，相机驱动只在 `cam_worker` 中运行，网页只发送锁保护的 JPEG 缓存副本。
- 统一固件已构建并烧录到 COM55。串口连续 30 秒显示 `cam:ready=1 valid=1`、帧率约 5 fps、`err=0`、内存稳定，且 GPS/FC 遥测保持在线。
- 未完成的唯一验收项是手机连接 `NMC-Umbrella` 后，在 `http://192.168.4.1/` 实际看到持续更新的画面，并确认 `/capture.jpg` 请求让网页端正常显示。

通过标准：

- 不再出现 `Stack canary watchpoint triggered (loopTask)`。
- 日志出现 `PID: 0x0026`。
- warm-up 能成功，或打开 `http://192.168.4.1/capture.jpg?manual=1` 后 `/status` 里的 `jpg` 和 `frames` 增加。
- `/status` 中 `camera=true`、`valid=true`、`timeouts=0` 或不持续增长。

失败分支：

- 如果当前 `cam_worker` 版本仍然栈溢出：先保存完整串口日志和复位前最后一行；不要再增加第二个取帧任务。
- 如果不重启但 `fb_get` 一直失败：先按硬件检查顺序查并口数据线、PCLK、VSYNC、HREF、XCLK/晶振和供电。
- 如果诊断环境成功，再把同一套 camera config 和启动顺序回灌到 `esp32-s3-unified-web`，并把 `SUBMIT_SKIP_CAMERA_HW` 改为 `0` 做统一网页验证。

## 回灌到统一网页的原则

- 不改变最终 AP 名称：必须仍是 `NMC-Umbrella`。
- 不让摄像头失败影响 WiFi、GPS、飞控数据显示。
- 摄像头启动失败时只标记离线，不循环重启硬件。
- 保留受控重试入口，不做恢复风暴。
- 统一网页验证路径：
  - `http://192.168.4.1/`
  - `http://192.168.4.1/api/telemetry`
  - `http://192.168.4.1/capture.jpg?manual=1`

## 给 DeepSeek 的 prompt

请直接复制下面整段：

```text
你现在接手 D:\Code\NMC 项目。先阅读 AGENTS.md、PROJECT_HANDOFF_2026-07-03.md、submit_stable_workflow_2026-07-09.md、camera_repair_workflow_2026-07-09.md、platformio.ini、include/config.h、src/ov5640_diag_main.cpp、src/unified_web_main.cpp。

当前目标：继续修复 OV2640/OV5640 摄像头取帧问题，但不能破坏稳定提交版网页。最终 WiFi 必须是 NMC-Umbrella，真实飞控输出保持关闭，不做电机、解锁或 RC override 测试。

当前已知：
1. 稳定版 esp32-s3-unified-web 为了保证 WiFi，暂时 SUBMIT_SKIP_CAMERA_HW=1。
2. 摄像头 SCCB 能读到 ACK/PID，曾出现 addr=0x30、PID=0x0026，所以控制链路大概率通。
3. 故障集中在 esp_camera_fb_get/warm-up/DMA 取帧阶段。
4. 之前 camera diag 在 loopTask 附近栈溢出，所以 platformio.ini 的 [env:esp32-s3-ov5640-diag] 已补 -pipe 和 -DARDUINO_LOOP_STACK_SIZE=16384。当前源码还把全部 camera driver 调用移入 16KB 的 cam_worker，HTTP 只发送缓存；不要重复添加第二个抓帧任务。
5. Codex 当前环境里 xtensa assembler/linker 不能写子目录目标文件，导致未能完成本地编译烧录；请在普通 Windows 终端或你可用的构建环境里先编译上传诊断固件。

请按这个顺序做：
1. 只编译上传 esp32-s3-ov5640-diag 到 COM55，不要先改统一网页。
2. 串口确认是否还出现 Stack canary watchpoint triggered (loopTask)。
3. 如果不重启，打开 http://192.168.4.1/capture.jpg?manual=1 和 /status，记录 frames、jpg、timeouts、valid、age。
4. 如果 diag 成功取帧，再把 proven camera config/启动顺序回灌到 src/unified_web_main.cpp，把 SUBMIT_SKIP_CAMERA_HW 改为 0，并保证 NMC-Umbrella WiFi 不变。
5. 如果 diag 不重启但 fb_get 失败，不要继续乱改网页，优先检查 PCLK/VSYNC/HREF/D0-D7、RST=3.3V、PWDN=GND、供电和模块是否自带 XCLK 晶振。
6. 每一步只改相关文件，报告目标文件、验证日志和未验证假设。不要提交 .pio-core、.pio、tmp、.reasonix、.codegraph、NMC_Source 等本地生成物。

推荐命令：
$env:PLATFORMIO_CORE_DIR='D:\Code\NMC\.pio-core'
$env:PLATFORMIO_CACHE_DIR='D:\Code\NMC\.pio-core\.cache'
$env:TEMP='D:\Code\NMC\tmp'
$env:TMP='D:\Code\NMC\tmp'
$env:TMPDIR='D:\Code\NMC\tmp'
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' run -e esp32-s3-ov5640-diag -t upload --upload-port COM55 -j 1
& 'C:\Users\yyk\.platformio\penv\Scripts\platformio.exe' device monitor --port COM55 --baud 115200 --echo
```
