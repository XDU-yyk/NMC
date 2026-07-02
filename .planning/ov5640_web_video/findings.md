# OV5640 Web 图传发现记录

## Hardware

- 用户现有硬件为 OV5640/5640 DVP 摄像头模块，图片标注 500 万像素、自带补光灯、支持自动对焦。
- 模块需要 DVP 并口信号：D0-D7、PCLK、VSYNC、HREF、XCLK、SDA、SCL、电源和地。
- 第一版按模块丝印接线，不按商品图物理顺序猜线。

## Project Facts

- `src/vision/camera.h/.cpp` 现有注释和参数偏 OV2640，且 framebuffer 释放设计不完整。
- `platformio.ini` 的 `esp32-s3-web-mvp` 当前不编译 `vision/camera.cpp`。
- `include/config.h` 已定义相机 pin map：
  - XCLK=15, SIOD=33, SIOC=34;
  - Y9=39, Y8=41, Y7=42, Y6=12, Y5=11, Y4=10, Y3=9, Y2=8;
  - VSYNC=6, HREF=7, PCLK=13。
- 当前冲突：
  - ToF XSHUT 使用 GPIO10；
  - ToF I2C 使用 GPIO41/42；
  - GPS TX 使用 GPIO15。

## Library Facts

- 本地 `sensor.h` 包含 `OV5640_PID = 0x5640` 和 `CAMERA_OV5640`。
- `OV5640_SCCB_ADDR = 0x3C`。
- Arduino ESP32 CameraWebServer 示例使用 JPEG、PSRAM 优先、`CAMERA_GRAB_LATEST`，可作为实现参考。

## Decision

第一阶段走独立 OV5640 诊断和图传固件，命名 `esp32-s3-ov5640-diag`。暂不合入 Web MVP，避免 ToF/GPS 引脚冲突。

