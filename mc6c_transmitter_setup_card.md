# MC6C 遥控器现场设置卡

日期: 2026-07-05

用途: 新飞控到货后，先把 Microzone MC6C 和接收机调到多旋翼可控状态，再进入 Betaflight、无桨电机和低空方向验收。完整流程见 `replacement_fc_ready_workflow.md`，现场打勾见 `replacement_fc_arrival_checklist.md`。

核心原则:

```text
MC6C 是人工主控制权。
接收机接飞控，不接 ESP32。
ESP32 不修正 MC6C 手动方向。
第一次手动飞行时 CH6 保持低位，不启用 ESP32 辅助输出。
```

## 1. 开机前拨码和模式

| 项目 | 目标设置 | 说明 |
| --- | --- | --- |
| 底部模型模式 | `UAV` | 多旋翼优先使用 UAV 模式 |
| `V-TAIL` | 关闭 | 不要打开固定翼 V 尾混控 |
| `ELEVON` | 关闭 | 不要打开三角翼混控 |
| AIL/ELE/THR/RUD 反向拨码 | 先保持默认 | 只在 Betaflight Receiver 和模型预览证明方向反了以后再改 |
| CH5 | 规划为 ARM 或飞行模式 | 先确认低位/高位含义，再配置 Modes |
| CH6 | 规划为 ESP32 辅助许可 | 默认低位；手动低空方向验收时必须低位 |

不要用 `V-TAIL`、`ELEVON` 或 ESP32 代码去补偿多旋翼方向。方向问题优先按这个顺序查:

```text
Betaflight Receiver 页面
Channel Map
MC6C 基础通道反向拨码
Betaflight 模型预览 / Board Alignment
电机顺序 / 电机方向 / 桨叶方向
```

## 2. 接收机输出类型先判定

| 接收机外观 | 处理方式 |
| --- | --- |
| 只有一组信号/5V/GND，标注 SBUS / PPM / iBUS / 串行 | 一根信号线接飞控对应接收机输入 |
| 有 CH1-CH6 六组独立 S/+/- 排针 | 先确认新飞控支持多路 PWM 输入；不支持就用 PWM 转 PPM/SBUS 编码器或换串行接收机 |
| 不确定 | 只接飞控 USB 和接收机 5V/GND/信号，先看 Betaflight Receiver 页面，不接电调 |

Receiver 页面没有通道变化时，停止后续电调和电机步骤。

## 3. 目标通道表

| MC6C 通道 | 遥控器功能 | Betaflight 功能 | 到货日用途 |
| --- | --- | --- | --- |
| CH1 AIL | 副翼 / 横滚 | Roll | 左右平移/横滚方向 |
| CH2 ELE | 升降 / 俯仰 | Pitch | 前后方向 |
| CH3 THR | 油门 | Throttle | 升力/高度 |
| CH4 RUD | 方向 / 偏航 | Yaw | 机头左转/右转 |
| CH5 | 辅助开关 | AUX1 | ARM 或飞行模式 |
| CH6 | 辅助开关 | AUX2 | ESP32 辅助输出许可 |

注意: MC6C 物理通道是 `CH1 AIL, CH2 ELE, CH3 THR, CH4 RUD`；Betaflight MSP 内部功能顺序是 `roll, pitch, yaw, throttle, AUX1, AUX2`。本项目网页按功能名显示，不按接收机物理编号猜 yaw/throttle。

## 3.1 Betaflight Channel Map 候选

MC6C 给出的基础通道顺序是:

```text
CH1 AIL, CH2 ELE, CH3 THR, CH4 RUD
```

到货当天在 Betaflight Receiver 页优先先试这个 Channel Map:

```text
AETR1234
```

也可以在 CLI 里设置后保存:

```text
map AETR1234
save
```

这只是首选候选，不是免检结论。保存后必须回 Receiver 页面逐项确认:

```text
AIL 只动 Roll
ELE 只动 Pitch
THR 只动 Throttle
RUD 只动 Yaw
CH5 只动 AUX1
CH6 只动 AUX2
```

如果 `AETR1234` 不符合真实接收机输出，就按 Receiver 页面改 Channel Map，并把最终实际 map 记录到 `replacement_fc_acceptance_log_template.md`。不要用 ESP32 代码修正错误的 Channel Map。

## 4. Receiver 页面验收

| 操作 | 应看到 | 通过标准 |
| --- | --- | --- |
| 松开 AIL/ELE/RUD，油门最低 | Roll/Pitch/Yaw 接近 1500，Throttle 接近 1000 | 中位和低油门稳定 |
| AIL 右 | Roll 明显变化，模型预览向右横滚 | 若方向反，先调 Receiver/反向拨码 |
| AIL 左 | Roll 反向明显变化 | 不得无变化或跳动 |
| ELE 前 | Pitch 明显变化，模型预览机头下压 | 数值增减不固定，以模型预览为准 |
| ELE 后 | Pitch 反向明显变化，模型预览机头上仰 | 不得用网页代码补偿 |
| THR 上 | Throttle 明显升高 | 低位约 1000，高位约 1800-2000 |
| RUD 右 | Yaw 明显变化，模型预览机头右转 | 若方向反，先调 Receiver/反向拨码 |
| RUD 左 | Yaw 反向明显变化 | 不得无变化 |
| CH5 高/低 | AUX1 在低位和高位之间切换 | 低/高位要清楚，不要停在中间 |
| CH6 高/低 | AUX2 在低位和高位之间切换 | 低位默认；高位只给后续 ESP32 辅助许可 |

建议阈值:

```text
横滚/俯仰/偏航中位: 1400-1600
油门低位: <= 1200
开关低位: <= 1200
开关高位: >= 1700
```

## 5. 首次低空方向验收前确认

低空手动方向验收前必须全部满足:

- [ ] Betaflight 模型预览方向正确。
- [ ] Receiver 页面六路通道变化正确。
- [ ] CH5/ARM 或模式开关的低位/高位已经清楚。
- [ ] CH6 保持低位。
- [ ] 无桨电机顺序和方向已经通过。
- [ ] failsafe 已验证，关闭 MC6C 后不会继续加油门。
- [ ] 螺旋桨型号和安装方向确认正确。
- [ ] 飞手站在机尾后方，机头朝空旷方向。

低空验收只证明 MC6C 手动控制方向。通过以后，才允许继续考虑 FC-ready 的无桨 MSP Override 验证。
