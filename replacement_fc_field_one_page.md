# 新飞控到货现场一页表

日期: 2026-07-05

用途: 新飞控到货后，先按这页完成接线和第一轮验收。详细说明见 `replacement_fc_ready_workflow.md`，打勾清单见 `replacement_fc_arrival_checklist.md`，证据记录见 `replacement_fc_acceptance_log_template.md`。

目标只有一个:

```text
先证明 MC6C 遥控器能独立控制飞机按指定方向飞。
ESP32-S3 默认只做 Web、传感器、相机和仿真显示。
第一次手动飞行不启用 ESP32 真机辅助输出。
```

## 0. 先停手的情况

出现任意一条，先不要继续:

- 还是那块已损坏旧飞控。
- 螺旋桨没拆，却准备做 Receiver、Motors、MSP 或 FC-ready 检查。
- 想把 ESP32 GPIO 接到电调白色信号线。
- Receiver 页面看不到 MC6C 通道变化。
- 无桨电机顺序、旋向或 failsafe 没验过。
- CH6 没保持低位，却准备第一次手动飞行。

## 1. 电调按照片确认的普通三线航模电调处理

你提供的电调照片已经能看出: 透明热缩外壳、粗红/粗黑进电、三根粗线到电机、白/红/黑三针舵机头。现场先按 PWM/OneShot 电调处理，不默认 DShot。

```text
粗红线 -> 电池/PDB 正极
粗黑线 -> 电池/PDB 负极
三根粗相线 -> 电机三根线

白色细线 -> 飞控 M1/M2/M3/M4 或 S1/S2/S3/S4 信号
黑色细线 -> 飞控 GND
红色细线 -> BEC/5V 待确认；只保留一根，或全部退针/绝缘并使用独立 5V
```

不要把多个电调 BEC 红线直接并在一起。接电池前先用万用表量舵机头红线到黑线；只有稳定约 5V 时，才允许把其中一根红线作为 BEC 供电。

## 2. MC6C 和接收机

MC6C 本体先这样设:

```text
底部模式: UAV
V-TAIL: 关闭
ELEVON: 关闭
CH5: ARM 或模式
CH6: ESP32 辅助许可，第一次手动飞行保持低位
```

接收机接飞控，不接 ESP32。Receiver 页面优先试:

```text
map AETR1234
save
```

保存后必须实测:

```text
AIL -> Roll
ELE -> Pitch
THR -> Throttle
RUD -> Yaw
CH5 -> AUX1
CH6 -> AUX2
```

如果不对，改 Betaflight Channel Map 或 MC6C 反向拨码。不要用 ESP32 代码补。

## 3. ESP32 到飞控 UART3

只有飞控和 Receiver 基本正常后再接 ESP32 MSP:

```text
ESP32 GPIO16 TX -> 飞控 R3/RX3
ESP32 GPIO17 RX <- 飞控 T3/TX3
ESP32 GND       -> 飞控 GND
```

Betaflight UART3 MSP:

```text
serial 2 1 115200 57600 0 115200
save
```

UART3 只做 MSP。接收机端口不要和 UART3 混用。

## 4. 到货日顺序

按这个顺序走，不跳步:

1. 只接 USB，Betaflight 能稳定连接新飞控。
2. Setup 页面模型预览方向正确，校准加速度计。
3. Receiver 页面证明 MC6C 六路都动，最终 Channel Map 记录下来。
4. 继续无桨台架检查前，CH5/CH6 先保持低位；做网页/Receiver 方向自检时保持无桨，建议只 USB、不接动力电池；CH5 若是 ARM，只用于确认 AUX1 数值变化。
5. 全程无桨，接电调和电池，Motors 页面一次只推一个电机。
6. 记录 M1-M4 实际位置，确认电机旋向；错了改输出口或交换该电机任意两根相线。
7. 关 MC6C，确认 failsafe 后不继续加油门。
8. 烧或使用默认 `esp32-s3-unified-web`，确认 Web 只显示传感器/相机/仿真，真实输出未编译或锁定。
9. 装桨前复查桨型和桨向。
10. 低空手动方向验收，CH6 保持低位，ESP32 不参与。

低空方向验收通过标准:

```text
油门能稳定增升力并能收油。
AIL 左/右与机头方向下的左/右一致。
ELE 前/后与前后方向一致。
RUD 左/右与偏航方向一致。
没有起飞瞬间翻滚、猛烈侧倾或原地打转。
```

任一方向不对，立刻降落/断解锁，回到 Receiver、模型预览、飞控朝向、电机顺序、电机旋向和桨叶方向检查。

## 5. FC-ready 后续才用

`esp32-s3-unified-web-fc-ready` 不是第一次手动飞行固件。只有低空 MC6C 手动方向验收通过后，才做无桨 FC-ready 台架验证。

FC-ready 真输出必须同时满足:

```text
ENABLE_REAL_FC_OUTPUT=1 的专用构建
FC online
Betaflight MSP_STATUS ARM active-box 为解锁
CH6/AUX2 高位许可
Web 方向心跳新鲜
Betaflight MSP Override 由 CH6/AUX2 触发
```

即使启用，ESP32 也只允许改 roll/pitch/yaw。真实油门、AUX1/ARM、AUX2/CH6 仍由 MC6C 保留。
