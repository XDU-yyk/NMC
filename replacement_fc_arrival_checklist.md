# 新飞控到货现场验收清单

日期: 2026-07-05

用途: 新 F4V3S PLUS 或同级飞控到货后，按本清单从接线、Betaflight、MC6C 接收机、无桨电机验证，到低空手动方向验收逐项打勾。目标完成判定见 `replacement_fc_goal_completion_audit.md`，命令和日志保存见 `replacement_fc_command_record_card.md`，完整解释见 `replacement_fc_ready_workflow.md`，Betaflight 现场设置见 `replacement_fc_betaflight_setup_card.md`，MC6C 实物设置见 `mc6c_transmitter_setup_card.md`，低空异常排查见 `replacement_fc_manual_direction_troubleshooting_card.md`，证据记录用 `replacement_fc_acceptance_log_template.md`。

电机顺序、旋向和桨向记录见 `replacement_fc_motor_direction_card.md`。

核心目标:

```text
先证明 MC6C 能独立控制飞机按指定方向飞。
ESP32-S3 只做 Web/传感器/仿真和后续受限辅助，不参与第一次手动飞行。
```

## 0. 绝对停止条件

任意一条出现，就停止后续步骤:

- [ ] 飞控仍是已损坏那块旧飞控。
- [ ] 螺旋桨没有拆下，却准备做 Receiver / Motors / MSP / FC-ready 检查。
- [ ] 想把 ESP32-S3 接到电调白色信号线。
- [ ] Receiver 页面没有通道变化，却准备继续接电调或装桨。
- [ ] failsafe 未验证，却准备起飞。
- [ ] 手动低空方向验收未通过，却准备启用 ESP32 真机辅助输出。

## 1. 飞控和 Betaflight 上电

- [ ] 只接 USB，不接电池，不装桨。
- [ ] Betaflight Configurator 能连接新飞控。
- [ ] 飞控型号/固件目标和实际板子一致。
- [ ] 机架类型选择多旋翼对应布局。
- [ ] Board Alignment / 飞控朝向在模型预览里正确。
- [ ] 校准加速度计。

通过标准:

```text
Betaflight 能稳定连接，模型预览随手动转动机架方向一致。
```

未通过处理:

```text
先修飞控固件目标、驱动、USB、板载方向或校准；不要接电调和电池。
```

## 1.1 Betaflight 配置快照

到货当天先保存这些截图和 CLI 文本，再继续后面的电调和低空验收:

- [ ] `setup_model_preview.png`: Setup 页面模型预览方向正确。
- [ ] `ports_uart3_msp.png`: Ports 页面 UART3 打开 MSP。
- [ ] `ports_receiver_serial_rx.png`: 若接收机用串行口，该 UART 打开 Serial RX，且不和 UART3 MSP 混用。
- [ ] `receiver_protocol_map.png`: Receiver 协议/Provider/Channel Map 与真实接收机一致。
- [ ] `receiver_ch1_ch6.png`: Receiver 页面六路通道都有变化。
- [ ] `modes_arm_aux1.png`: Modes 页面 CH5/AUX1 控制 ARM 或基本模式。
- [ ] `modes_msp_override_aux2.png`: 仅 FC-ready 后期使用，MSP OVERRIDE 由 CH6/AUX2 高位触发。
- [ ] `motors_mixer_protocol.png`: Motors 页面机架/Mixer、电调协议、无桨状态记录清楚。
- [ ] `betaflight_cli_snapshot.txt`: 至少包含 `version`、`status`、`serial`、`resource`、`map`、`aux`、`diff all`。

通过标准:

```text
UART3 只做 MSP；接收机输入端口独立；CH1-CH6 映射清楚；ARM 不由 ESP32 控制；FC-ready 的 MSP OVERRIDE 只由 CH6/AUX2 许可。
```

## 2. MC6C 和接收机输入

MC6C 本体先这样设置:

- [ ] 底部模式: `UAV`。
- [ ] `V-TAIL`: 关闭。
- [ ] `ELEVON`: 关闭。
- [ ] 基础通道反向拨码先保持默认。
- [ ] CH5 规划为 ARM 或飞行模式。
- [ ] CH6 规划为 ESP32 辅助输出许可，默认保持低位。

先判定接收机输出类型:

- [ ] 如果接收机是 SBUS / PPM / iBUS / 串行输出，只接一根信号线到飞控对应输入。
- [ ] 如果接收机是 CH1-CH6 六路独立 PWM，先确认新飞控支持多路 PWM 输入；不支持就需要 PWM 转 PPM/SBUS 编码器或更换串行接收机。
- [ ] Betaflight Channel Map 先按 MC6C 候选 `AETR1234` 验证，若不符则改为 Receiver 页面证明正确的实际 map。
- [ ] Receiver 页面能看到 Roll / Pitch / Throttle / Yaw / AUX1 / AUX2 六路变化。

Receiver 页面验收:

| MC6C 操作 | Betaflight 里应看到 |
| --- | --- |
| AIL 左/右 | Roll 变化 |
| ELE 前/后 | Pitch 变化 |
| THR 上/下 | Throttle 变化 |
| RUD 左/右 | Yaw 变化 |
| CH5 开关 | AUX1 低/高变化 |
| CH6 开关 | AUX2 低/高变化 |

通过标准:

```text
六路都动，油门最低值接近 1000，横滚/俯仰/偏航中位接近 1500，开关低/高位清楚。
继续无桨台架检查前，CH5/AUX1 和 CH6/AUX2 先保持低位；高位只在方向自检步骤里单独确认。
```

未通过处理:

```text
先修接收机对频、5V/GND、Serial RX 端口、Receiver Provider、Channel Map 或 MC6C 反向拨码。
Receiver 不通过时，不接电调、不做 Motors。
不要用 ESP32 代码补偿错误的 Channel Map 或反向。
```

## 3. 电源和电调接线

若电调线束符合粗红/粗黑进电、三相粗线到电机、白/红/黑三针舵机头，就按普通三线 PWM/OneShot 电调处理:

- [ ] 当前照片中的电调已按普通 PWM/OneShot 三线舵机头电调处理，不默认 DShot。
- [ ] 电调粗红线接电池/PDB 正极。
- [ ] 电调粗黑线接电池/PDB 负极。
- [ ] 电调三根粗输出线接对应电机三相线。
- [ ] 电调白色细线接飞控 M1-M4 / S1-S4 信号。
- [ ] 电调黑色细线接飞控 GND。
- [ ] 万用表量过舵机头红线到黑线，确认是否为稳定 5V BEC 输出。
- [ ] 电调红色细线只保留必要的一根 BEC 供电，或全部退针/绝缘并使用独立 5V。
- [ ] 四个电调的红色 BEC 线没有直接并联到同一 5V 母线。
- [ ] ESP32-S3 不接任何电调信号线。
- [ ] 所有地线共地，电源极性用万用表确认。

通过标准:

```text
电池接入后飞控和接收机不重启，5V 稳定，电调不异常发热或报警。
```

## 4. UART3 MSP 只读验证

Betaflight CLI:

```text
serial 2 1 115200 57600 0 115200
save
```

同时把 `serial`、`resource` 和 `diff all` 保存进 `betaflight_cli_snapshot.txt`，用于确认 UART3 MSP 和接收机端口没有混用。

USB-TTL 优先验证:

```text
USB-TTL TX  -> 飞控 R3/RX3
USB-TTL RX  <- 飞控 T3/TX3
USB-TTL GND -> 飞控 GND
VCC 不接
```

验收:

- [ ] 发送 ASCII `#` 能进入 CLI。
- [ ] 发送 HEX `24 4D 3C 00 01 01` 有 `24 4D 3E ...` 回包。
- [ ] USB-TTL 通过后，再用 ESP32 GPIO16/GPIO17 接 UART3。
- [ ] `esp32-s3-fc-uart-probe` 只读 probe 能看到 MSP 回包。
- [ ] `esp32-s3-fc-diag` 能显示 FC online。

未通过处理:

```text
先查 R3/T3 是否接反、GND 是否共地、波特率、Betaflight serial 配置。
不要启用 MSP_SET_RAW_RC。
```

## 5. 无桨电机和 failsafe

- [ ] 仍然不装桨。
- [ ] Motors 页面一次只推一个电机。
- [ ] M1/M2/M3/M4 物理位置和 Betaflight 机架图一致。
- [ ] 每个电机旋转方向正确。
- [ ] 方向错时，只交换该电机任意两根三相线。
- [ ] `replacement_fc_motor_direction_card.md` 已填写 M1-M4 位置、旋向和桨向记录。
- [ ] 不用 ESP32 修正电机方向或顺序。
- [ ] 关闭 MC6C 后，飞控进入 failsafe，不继续加油门。

通过标准:

```text
电机顺序正确，旋转方向正确，failsafe 有效。
```

## 6. 默认 ESP32 Web 演示

优先使用默认安全固件:

```text
esp32-s3-unified-web
```

- [ ] Web 能打开。
- [ ] ToF / GPS / Camera / 模拟飞控状态显示正常。
- [ ] 页面显示真实输出未编译或锁定。
- [ ] 不写真飞控，不解锁，不发 MSP_SET_RAW_RC。

通过标准:

```text
ESP32 展示传感器和仿真，不影响 MC6C 手动控制。
```

## 7. 低空手动方向验收

前提:

- [ ] 前面 1-6 全部通过。
- [ ] 螺旋桨型号、安装方向正确。
- [ ] 开阔低风环境，无旁人。
- [ ] 飞手站在机尾后方，机头朝空旷方向。
- [ ] CH6 保持低位，不启用 ESP32 辅助输出。

低空动作验收:

| MC6C 操作 | 预期现象 | 不符合时 |
| --- | --- | --- |
| 油门轻推 | 飞机平稳增升力，能稳定收油 | 立刻收油，检查油门行程/电调/电池/重心 |
| AIL 右 | 以机头方向为准，向右轻微倾斜/移动 | 降落断电，查 Roll 反向和模型预览 |
| AIL 左 | 以机头方向为准，向左轻微倾斜/移动 | 降落断电，查 Roll 反向和模型预览 |
| ELE 前 | 机头下压，向前轻微移动 | 降落断电，查 Pitch 反向和模型预览 |
| ELE 后 | 机头上仰，向后轻微移动 | 降落断电，查 Pitch 反向和模型预览 |
| RUD 右 | 机头向右偏航，不突然翻滚 | 降落断电，查 Yaw、飞控朝向、桨向、电机顺序 |
| RUD 左 | 机头向左偏航，不突然翻滚 | 降落断电，查 Yaw、飞控朝向、桨向、电机顺序 |

通过标准:

```text
能稳定解锁和收油。
低空悬停可控。
前/后/左/右/左偏航/右偏航都和 MC6C 操作一致。
failsafe 后不继续加油门。
全程不需要 ESP32 真实输出。
```

## 8. FC-ready 真实辅助输出

只有第 7 步通过后，才考虑未来固件:

```text
esp32-s3-unified-web-fc-ready
```

继续前必须确认:

- [ ] Betaflight 已配置 MSP Override / MSP OVERRIDE。
- [ ] MSP Override 由 CH6/AUX2 高位触发。
- [ ] 不覆盖 ARM 通道。
- [ ] `modes_msp_override_aux2.png` 和 `betaflight_cli_snapshot.txt` 已保存。
- [ ] 页面显示真实 MSP 在线。
- [ ] 页面显示 Betaflight ARM位/解锁位，且该状态来自 Betaflight `MSP_STATUS` 的 ARM active-box 位，不是只看 CH5 开关高位。
- [ ] 页面显示 CH6 高位许可。
- [ ] Web 方向心跳停止或 CH6 拉低时，输出立刻锁定/归中。
- [ ] 第一次 FC-ready 验证仍然无桨。

通过标准:

```text
MC6C 仍然是主控制权。
ESP32 只在 CH6 高位、FC online、Betaflight ARM位/解锁位打开、Web 心跳新鲜时，受限改变 roll/pitch/yaw。
油门、AUX1/ARM、AUX2/CH6 许可不被 ESP32 覆盖，继续保留 MC6C/飞控实时值。
网页 `升仿真/降仿真` 只影响仿真显示，不作为真实油门输入。
```

## 9. 最终一句话判定

只有同时满足下面三条，才算达到本阶段目标:

```text
MC6C 手动低空方向验收通过。
ESP32 默认 Web/传感器/仿真不会影响手动飞行。
FC-ready 真实辅助输出仍然处于可控、可关闭、可诊断状态。
```

把最终判定、截图、日志和视频路径同步填进 `replacement_fc_acceptance_log_template.md`。没有记录和证据路径时，本清单只能算“执行过”，不能算“已验收通过”。

最终是否达到目标，按 `replacement_fc_goal_completion_audit.md` 再做一次逐项审计。
