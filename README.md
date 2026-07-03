# NMC 智能无人机伞 — 嵌赛1.0

**双处理器架构：ESP32-S3（协处理器）+ F4V3S PLUS（飞控）**

> ⚠️ **当前状态：飞控已损坏**。暂停所有飞行、电机测试和自主控制开发。
> 当前仅验证 ESP32-S3 伴随计算机的传感器读取与 Web 交互能力。

## 功能特性
- Web 控制界面（WiFi AP，浏览器操控）
- GPS (NEO-M8N) 定位诊断
- ToF (VL53L1X) 激光测距
- OV2640 摄像头 Web 预览
- MSP 协议与飞控通信（待替换飞控后恢复）

## 硬件平台
- **协处理器**：ESP32-S3 (240MHz Xtensa LX7 双核)
- **飞控**：F4V3S PLUS（当前损坏）
- **传感器**：NEO-M8N / VL53L1X / OV2640

## 烧录
```bash
pio run --target upload --upload-port COM55
pio device monitor --port COM55 --baud 115200
```

## 版本
**v1.0** — 嵌赛1.0版本

### 架构说明
- ESP32-S3 不直接控制电机 PWM，仅通过 MSP/UART 向飞控发送指令
- F4V3S 负责姿态稳定、电机输出、遥控器接管与失控保护
- 当前飞控损坏，MSP 通信开发已暂停
- UWB 非当前硬件，仅作为未来扩展方向
