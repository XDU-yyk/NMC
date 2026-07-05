# 新飞控 Betaflight 现场设置卡

日期: 2026-07-05

用途: 新飞控到货后，把 Betaflight 配到“MC6C 能独立手动控制飞机按指定方向飞”的状态。完整流程见 `replacement_fc_ready_workflow.md`，现场打勾见 `replacement_fc_arrival_checklist.md`。

核心原则:

```text
先拆桨。
先让 MC6C 接收机和 Betaflight Receiver 页面正确。
ESP32 不参与第一次手动飞行方向修正。
不要盲目粘贴整套 diff all；每一项配置都要能在页面或 CLI 快照里证明。
```

## 1. Setup 页面

- 只接 USB，先不要接动力电池。
- 确认飞控固件目标和实际板子一致。
- 选择实际机架类型，多旋翼按当前 Betaflight 机架图为准。
- 校准加速度计。
- 手拿机架前后、左右、偏航转动，模型预览方向必须一致。

不通过时先修 `Board Alignment`、飞控安装朝向或固件目标，不要继续接电调。

## 2. Ports 页面

UART3 只给 ESP32 MSP:

```text
UART3 / serial index 2: MSP 打开，波特率 115200
不要在 UART3 同时打开 Serial RX。
```

CLI 可用:

```text
serial 2 1 115200 57600 0 115200
save
```

接收机端口单独配置:

```text
串行/SBUS/iBUS/CRSF 接收机: 只在接收机所在 UART 打开 Serial RX，不开 MSP。
PPM 接收机: 按飞控支持的 PPM 输入配置。
6 路独立 PWM 接收机: 先确认新飞控支持多路 PWM 输入；不支持就不要硬接到 UART。
```

## 3. Receiver 页面

MC6C 本体先确认:

```text
底部模式: UAV
V-TAIL: 关闭
ELEVON: 关闭
CH5: ARM 或基本模式
CH6: ESP32 辅助许可，第一次手动飞行保持低位
```

Channel Map 首选候选:

```text
map AETR1234
save
```

这只是第一候选。保存后必须在 Receiver 页面逐项证明:

| MC6C 操作 | Betaflight 应看到 |
| --- | --- |
| AIL 左/右 | Roll 变化 |
| ELE 前/后 | Pitch 变化 |
| THR 上/下 | Throttle 变化 |
| RUD 左/右 | Yaw 变化 |
| CH5 开关 | AUX1 低/高变化 |
| CH6 开关 | AUX2 低/高变化 |

基准值建议:

```text
Roll/Pitch/Yaw 中位: 1400-1600
Throttle 最低: <= 1200
CH5/CH6 低位: <= 1200
CH5/CH6 高位: >= 1700
```

如果方向或通道错，优先改 Betaflight Channel Map、Receiver Provider 或 MC6C 反向拨码。不要用 ESP32 代码补偿。

## 4. Modes 页面

第一次手动飞行前只做最少模式:

```text
CH5/AUX1: ARM 或基本飞行模式，低/高位含义必须清楚。
CH6/AUX2: 保留给 ESP32 辅助许可，第一次手动低空方向验收保持低位。
```

建议:

- 新手低空验收优先使用稳定/自稳类模式，但是否可用取决于当前 Betaflight 固件和传感器状态。
- 若把 ARM 和自稳模式都绑到 CH5，同一高位范围必须在 Modes 页面截图中清楚显示。
- 不要让 CH6 参与第一次手动飞行的 ARM 或飞行模式切换。

FC-ready 后期才配置:

```text
MSP OVERRIDE / MSP Override: 由 CH6/AUX2 高位触发。
覆盖通道: 只允许 roll/pitch/yaw 辅助。
不要覆盖 throttle、AUX1/ARM、AUX2/CH6。
```

如果 Betaflight 版本或目标没有 MSP Override 功能，FC-ready 真实辅助输出保持不用，只用默认 Web/传感器/仿真。

## 5. Motors 页面

进入本页前必须:

```text
螺旋桨已拆下。
Receiver 页面六路已通过。
CH5/CH6 当前低位。
```

电调协议:

```text
普通白/红/黑三针舵机头电调: 先按 PWM 或 OneShot 处理。
DShot: 只有确认电调支持时才选。
```

无桨测试顺序:

1. 一次只推一个电机。
2. 记录 Betaflight 图中的 M1-M4 位置。
3. 确认实际转动电机和图中位置一致。
4. 确认旋转方向。
5. 方向错时交换该电机任意两根三相线，或按电调工具改向后再复测。
6. 填 `replacement_fc_motor_direction_card.md`。

任何电机顺序、旋向或桨向没确认前，不装桨。

## 6. Failsafe

必须做且必须录像:

```text
无桨。
接收机在线，油门低位。
关闭 MC6C 或触发失控条件。
飞控必须进入 failsafe，不继续加油门。
```

Failsafe 没通过，不做低空手动方向验收。

## 7. 必存证据

保存到:

```text
docs/evidence/replacement_fc/
```

至少包括:

```text
setup_model_preview.png
ports_uart3_msp.png
ports_receiver_serial_rx.png
receiver_protocol_map.png
receiver_ch1_ch6.png
modes_arm_aux1.png
motors_mixer_protocol.png
betaflight_cli_snapshot.txt
motor_order_video.mp4
failsafe_video.mp4
```

CLI 快照至少包含:

```text
version
status
serial
resource
map
aux
diff all
```

通过标准:

```text
MC6C 六路通道正确。
模型预览方向正确。
电机 M1-M4 位置和旋向正确。
Failsafe 有效。
ESP32 未参与第一次手动飞行方向修正。
```
