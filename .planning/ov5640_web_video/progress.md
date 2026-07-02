# OV5640 Web 图传进度

## 2026-07-02

- 用户确认 F4V3S 当前硬件路线不支持 MSP 开发，转向 OV5640 摄像头模块。
- 已检查项目结构、`include/config.h`、`platformio.ini`、`src/vision/camera.*`、`src/web/server.*`、`src/web_main.cpp`。
- 已确认本地 `esp_camera` 支持 OV5640。
- 已确认现有 camera pin map 与 ToF/GPS 冲突，因此第一版必须独立 camera-only。
- 已新增 `ov5640_web_workflow.md`，用于交给 DeepSeek 执行。
- 已创建 scoped planning 目录 `.planning/ov5640_web_video/`。
