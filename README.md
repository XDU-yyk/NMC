# NMC 智能无人机伞 — 嵌赛1.0

**双处理器架构：ESP32-S3（协处理器）+ F4V3S PLUS（飞控）**

## 功能特性
- Web 控制界面（WiFi AP，浏览器操控）
- GPS (NEO-M8N) 室外定位与跟随
- ToF (VL53L1X) 激光测距
- OV2640 摄像头视觉追踪
- UWB (DW3000) 室内定位跟随
- MSP 协议与飞控通信
- 卡尔曼滤波传感器融合
- PID 自主跟随控制

## 硬件平台
- **主控**：ESP32-S3 (240MHz Xtensa LX7 双核)
- **飞控**：F4V3S PLUS
- **传感器**：NEO-M8N / VL53L1X / DW3000 / OV2640

## 烧录
```bash
pio run --target upload --upload-port COM55
pio device monitor --port COM55 --baud 115200
```

## 版本
**v1.0** — 嵌赛1.0版本
