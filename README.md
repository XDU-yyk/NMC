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
默认安全固件是 `esp32-s3-unified-web`。它只做 Web、传感器、相机和模拟飞控显示，不向真实飞控写控制输出。

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-unified-web -t upload --upload-port COM55
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe device monitor --port COM55 --baud 115200
```

不要使用未指定环境的通用上传命令；本项目有多个诊断/历史环境，现场烧录必须显式带 `-e esp32-s3-unified-web`。

`esp32-s3-unified-web-fc-ready` 是未来新飞控通过 Receiver、无桨电机、failsafe、UART3 MSP 和低空 MC6C 手动方向验收后的专用固件，不作为第一次手动飞行固件。

## 软件验证
当前飞控损坏时不要上传 FC-ready 固件、不要解锁、不要接桨测试。可先跑 host 侧安全逻辑测试:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_host_tests.ps1
```

该脚本会验证方向输入映射、Betaflight RAW_RC 顺序、CH6 辅助门槛、MC6C 油门/AUX 保留，以及网页油门仅用于仿真。

替换飞控到货前或到货当天，可跑完整软件检查:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1
```

该脚本会先跑 host 安全测试，再顺序构建默认 Web、FC-ready、UART3 probe 和 FC diag 固件；它不上传固件、不打开串口、不写 MSP、不碰电机。

如果只想快速检查 replacement-FC 静态安全边界，可运行:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_static_checks.ps1
```

该脚本检查默认安全环境、关键安全宏、MSP 写入口是否收窄、电调照片是否已按当前证据记录、BEC 红线是否仍要求万用表确认，以及低空方向验收记录字段是否完整。

新飞控到货并保存现场证据后，可运行只读证据包检查:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check_replacement_fc_evidence.ps1
```

该脚本只检查 `docs/evidence/replacement_fc/` 下截图、日志、视频和 JSON 是否齐全，不烧录、不打开串口、不碰电机；通过也只代表证据包完整，不代表飞行目标已经完成。

## 新飞控到货
现场先看 `replacement_fc_field_one_page.md`，目标是否完成看 `replacement_fc_goal_completion_audit.md`，命令和日志保存看 `replacement_fc_command_record_card.md`，Betaflight 逐项配置看 `replacement_fc_betaflight_setup_card.md`，再按 `replacement_fc_arrival_checklist.md` 打勾执行。低空方向异常时看 `replacement_fc_manual_direction_troubleshooting_card.md`。完整解释在 `replacement_fc_ready_workflow.md`，证据记录填 `replacement_fc_acceptance_log_template.md`。

## 版本
**v1.0** — 嵌赛1.0版本

### 架构说明
- ESP32-S3 不直接控制电机 PWM，仅通过 MSP/UART 向飞控发送指令
- F4V3S 负责姿态稳定、电机输出、遥控器接管与失控保护
- 当前飞控损坏，MSP 通信开发已暂停
- UWB 非当前硬件，仅作为未来扩展方向
