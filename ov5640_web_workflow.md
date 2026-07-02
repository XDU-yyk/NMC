# OV5640 Web 图传开发工作流

## 目标

在 F4V3S UART6/MSP 当前不可用的前提下，转向现有 OV5640 摄像头模块，先做一个稳定、可展示、可验证的 ESP32-S3 本地图传功能。

第一阶段目标不是视觉跟随，也不是复杂识别，而是：

- ESP32-S3 启动独立 WiFi AP；
- 手机或电脑连接后打开 `http://192.168.4.1`；
- 页面显示 OV5640 摄像头状态、实时低帧率画面、抓拍按钮和基础日志；
- 串口能打印传感器 PID、分辨率、JPEG 帧大小、FPS、剩余 heap/PSRAM。

最终展示口径：

> ESP32-S3 接入 OV5640，实现本地 WiFi Web 图传与抓拍，用于工位调试、比赛展示和后续视觉辅助算法验证。

## 当前工程事实

- 当前 Web MVP 只编译 `web_main.cpp`、`web/`、`tof.cpp`、`gps.cpp`，没有编译 `vision/camera.cpp`。
- `src/vision/camera.*` 是早期 OV2640 骨架，不适合作为 OV5640 第一版直接复用。
- 本地 `esp_camera` 头文件支持 `OV5640_PID` 和 `CAMERA_OV5640`。
- 当前 `include/config.h` 的相机引脚和已有模块冲突：
  - `CAM_PIN_Y4 = GPIO10` 与 `TOF_XSHUT_PIN = GPIO10` 冲突；
  - `CAM_PIN_Y8/Y7 = GPIO41/42` 与 ToF I2C 冲突；
  - `CAM_PIN_XCLK = GPIO15` 与 GPS TX 冲突。
- 因此第一阶段必须做独立摄像头固件，不要和 ToF/GPS/飞控混编。

## 硬件连接原则

按模块丝印信号连接，不按图片里的物理顺序猜。

OV5640 DVP 必需信号：

```text
3V3/VCC -> ESP32-S3 3.3V  （除非模块资料明确写可接 5V）
GND     -> ESP32-S3 GND
SDA     -> CAM_PIN_SIOD
SCL     -> CAM_PIN_SIOC
XCLK    -> CAM_PIN_XCLK
PCLK    -> CAM_PIN_PCLK
VSYNC   -> CAM_PIN_VSYNC
HREF    -> CAM_PIN_HREF
D0-D7   -> CAM_PIN_Y2-Y9
RST     -> CAM_PIN_RESET 或 3.3V/模块默认
PWDN    -> CAM_PIN_PWDN 或 GND/模块默认
LED1/LED2 -> 第一版不接，除非确认需要补光灯控制
```

第一版使用工程现有相机 pin map 作为 camera-only 诊断映射：

```text
XCLK  GPIO15
SDA   GPIO33
SCL   GPIO34
D7    GPIO39
D6    GPIO41
D5    GPIO42
D4    GPIO12
D3    GPIO11
D2    GPIO10
D1    GPIO9
D0    GPIO8
VSYNC GPIO6
HREF  GPIO7
PCLK  GPIO13
PWDN  -1
RESET -1
```

测试 OV5640 时先断开或忽略 ToF/GPS/飞控，避免 GPIO 冲突。

## 阶段 1：OV5640 独立诊断固件

新增目标：

- `src/ov5640_diag_main.cpp`
- `platformio.ini` 新增 `[env:esp32-s3-ov5640-diag]`

固件行为：

- 不启动 ToF/GPS/FC/Web MVP。
- 初始化 Serial。
- 打印 pin map、`ESP.getPsramSize()`、`psramFound()`。
- 使用 `esp_camera_init()` 初始化 OV5640：
  - `pixel_format = PIXFORMAT_JPEG`
  - `frame_size = FRAMESIZE_QVGA`
  - `jpeg_quality = 12`
  - `fb_count = psramFound() ? 2 : 1`
  - `fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM`
  - `grab_mode = CAMERA_GRAB_LATEST`
  - `xclk_freq_hz = 20000000`
- 初始化成功后打印：
  - sensor PID；
  - SCCB 地址；
  - 当前 frame size；
  - JPEG 支持状态。
- 每 3 秒抓一帧：
  - 成功：打印 `frame len`、宽高、heap、PSRAM；
  - 失败：打印错误计数；
  - 必须调用 `esp_camera_fb_return(fb)` 释放帧。

通过条件：

- 串口出现 `PID=0x5640` 或明确识别为 OV5640；
- 连续 30 秒抓帧不重启；
- JPEG 长度非 0；
- heap/PSRAM 不持续下降。

失败处理：

- `Camera init failed 0x105/0x20001`：优先查供电、SCCB SDA/SCL、PWDN/RST。
- PID 不对或读不到：先用万用表确认 3.3V/GND，再核对 SDA/SCL。
- 抓帧失败但 PID 正常：检查 D0-D7、PCLK、VSYNC、HREF、XCLK。
- 若 20MHz XCLK 不稳定，再改 10MHz 复测，并记录结果。

## 阶段 2：独立 Web 图传固件

在阶段 1 通过后扩展同一个独立固件，不要先合入 Web MVP。

新增 HTTP 功能：

```text
GET /              图传页面
GET /status        JSON 状态
GET /capture.jpg   单张 JPEG 抓拍
GET /stream        MJPEG 流
GET /ping          pong
```

AP 配置：

```text
SSID: NMC-Camera
PASS: 12345678
IP:   192.168.4.1
```

页面要求：

- 首页直接显示可用图传，不做营销页；
- 显示摄像头在线/离线、PID、分辨率、最近帧大小、估算 FPS、heap/PSRAM；
- 提供抓拍按钮；
- 默认 QVGA 低帧率，优先稳定；
- 不要在页面里宣传“视觉跟随已完成”。

性能目标：

```text
QVGA 320x240: 8-15 FPS 作为理想目标，实际以稳定为先
VGA 640x480: 3-8 FPS，可作为第二档
SVGA/UXGA: 只用于抓拍，不作为默认实时流
```

通过条件：

- 手机能连接 `NMC-Camera`；
- `http://192.168.4.1/ping` 返回 `pong`；
- `http://192.168.4.1/capture.jpg` 能打开 JPEG；
- 首页能看到图像；
- `/stream` 连续 60 秒不卡死、不重启；
- 串口每 5 秒打印帧计数、FPS、heap、PSRAM。

## 阶段 3：整理可交付证据

DeepSeek 完成阶段 1 和阶段 2 后，需要交付：

- 串口启动日志；
- `PID/SCCB/frame len/FPS/heap` 样例；
- 手机浏览器页面截图；
- `/capture.jpg` 实拍样张；
- 当前实际接线表；
- 若失败，给出失败阶段、错误码和已排查项。

## 暂不做的事情

- 不接入飞控控制；
- 不做自动跟随；
- 不做高精度人体识别；
- 不把 OV5640 和 ToF/GPS 同时启用；
- 不使用 UWB 叙事；
- 不改动 F4V3S 安全链路。

## 后续合入 Web MVP 的条件

只有在独立图传稳定后，才考虑合入主 Web MVP。合入前必须重新规划 GPIO：

- ToF XSHUT 不能继续占 GPIO10；
- ToF I2C 不能继续占 GPIO41/42，除非重新换 camera pin map；
- GPS 不能继续占用 camera XCLK 需要的 GPIO15；
- 如果引脚不够，优先保留 ToF/GPS Web MVP，摄像头继续作为独立演示固件。

## 给 DeepSeek 的 Prompt

```text
你现在接手 D:\Code\NMC 项目的 OV5640 图传模块开发。请先阅读 AGENTS.md、ov5640_web_workflow.md、include/config.h、platformio.ini、src/vision/camera.h、src/vision/camera.cpp、src/web/server.cpp、src/web_main.cpp。

背景：
1. F4V3S 的上方 R6/T6 说明书标为 CRSF Receiver，不适合作为当前 MSP 外设口；飞控通信先暂停。
2. 现在有 OV5640/5640 DVP 摄像头模块，目标转向“ESP32-S3 本地 WiFi Web 图传与抓拍”。
3. 第一阶段必须做独立 camera-only 固件，不要混入 ToF/GPS/FC。
4. 当前 camera pin map 与 ToF/GPS 冲突，所以测试时不要启用 ToF/GPS。
5. 本地 esp_camera 支持 OV5640_PID。现有 vision/camera.* 是 OV2640 旧骨架，不能直接当成已完成模块。

任务：
1. 新增 src/ov5640_diag_main.cpp。
2. 在 platformio.ini 新增 env:esp32-s3-ov5640-diag，只编译 ov5640_diag_main.cpp。
3. 使用 include/config.h 里 CAM_PIN_* 作为第一版 camera-only pin map。
4. 初始化 esp_camera，默认 QVGA JPEG，xclk 20MHz，psram 有则用 PSRAM。
5. 串口打印 pin map、PSRAM、sensor PID、SCCB 地址、抓帧长度、宽高、heap/PSRAM、失败计数。
6. 每 3 秒抓一帧并且必须 esp_camera_fb_return(fb)，避免内存泄漏。
7. 阶段 1 通过后，再扩展独立 AP Web 图传：SSID=NMC-Camera，密码=12345678，IP=192.168.4.1。
8. 实现 /ping、/status、/capture.jpg、/stream、/ 首页。
9. 首页显示实时图像、摄像头状态、PID、分辨率、帧大小、FPS、heap/PSRAM、抓拍按钮。
10. 不要做视觉跟随、不要控制飞控、不要写高精度人体识别宣传。

验收：
- esp32-s3-ov5640-diag 编译通过；
- 烧录后串口能识别 OV5640 或给出明确错误码；
- /ping 返回 pong；
- /capture.jpg 能打开 JPEG；
- /stream 连续 60 秒不重启；
- 串口日志显示 heap/PSRAM 不持续下降；
- 最后汇报实际接线、编译/烧录命令、串口关键输出、浏览器测试结果和剩余问题。
```
