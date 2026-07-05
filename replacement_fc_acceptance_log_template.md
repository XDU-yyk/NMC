# 新飞控 MC6C 方向验收记录模板

日期: ________

记录人/飞手: ________

使用文件:

- `mc6c_transmitter_setup_card.md`
- `replacement_fc_betaflight_setup_card.md`
- `replacement_fc_manual_direction_troubleshooting_card.md`
- `replacement_fc_arrival_checklist.md`
- `replacement_fc_ready_workflow.md`
- `replacement_fc_goal_completion_audit.md`
- `replacement_fc_command_record_card.md`

结论先留空，最后再填:

```text
本次是否证明 MC6C 能控制飞机按指定方向飞: 是 / 否
是否允许继续考虑 ESP32 FC-ready 真实辅助输出: 是 / 否
```

## 1. 硬件信息

| 项目 | 记录 |
| --- | --- |
| 飞控型号 | |
| Betaflight / INAV 版本 | |
| 机架类型 | |
| MC6C 接收机型号/输出类型 | 串行 / SBUS / PPM / iBUS / 6 路 PWM / 其他 |
| 电调类型 | PWM / OneShot / DShot / 未确认 |
| 电调线束确认 | 粗红/粗黑进电，三相粗线到电机，白/红/黑三针舵机头: 是 / 否 |
| 电池规格 | |
| 螺旋桨规格 | |
| ESP32 固件 | 默认 `esp32-s3-unified-web` / FC-ready / 未接入 |

硬件照片/视频路径:

```text
docs/evidence/replacement_fc/
```

证据目录说明: `docs/evidence/replacement_fc/README.md`

## 2. 停止条件检查

| 停止条件 | 是否触发 | 证据/备注 |
| --- | --- | --- |
| 使用的是损坏旧飞控 | 否 / 是 | |
| 未拆桨就做 Receiver/Motors/MSP 检查 | 否 / 是 | |
| ESP32 接到电调信号线 | 否 / 是 | |
| Receiver 无通道变化还继续接电调/装桨 | 否 / 是 | |
| failsafe 未验证就准备起飞 | 否 / 是 | |
| 手动方向验收未过就启用 ESP32 真输出 | 否 / 是 | |

必须全部为“否”才继续。

## 3. Betaflight 上电和模型预览

| 检查项 | 通过 | 证据/备注 |
| --- | --- | --- |
| USB 能稳定连接飞控 | 是 / 否 | |
| 固件目标与飞控型号一致 | 是 / 否 | |
| 机架类型正确 | 是 / 否 | |
| Board Alignment 正确 | 是 / 否 | |
| 模型预览随手动转动方向一致 | 是 / 否 | |
| 加速度计已校准 | 是 / 否 | |

证据文件:

```text
docs/evidence/replacement_fc/betaflight_model_preview.png
docs/evidence/replacement_fc/betaflight_config_notes.txt
```

## 3.1 Betaflight 配置快照

| 证据文件 | 必须证明的内容 | 通过 | 备注 |
| --- | --- | --- | --- |
| `setup_model_preview.png` | Setup 页面模型预览随机架转动方向一致 | 是 / 否 | |
| `ports_uart3_msp.png` | UART3 打开 MSP | 是 / 否 | |
| `ports_receiver_serial_rx.png` | 接收机端口独立，不和 UART3 MSP 混用 | 是 / 否 | |
| `receiver_protocol_map.png` | Receiver 协议/Provider/Channel Map 与真实接收机一致 | 是 / 否 | |
| `receiver_ch1_ch6.png` | Roll/Pitch/Throttle/Yaw/AUX1/AUX2 六路都动 | 是 / 否 | |
| `modes_arm_aux1.png` | ARM 或基本模式由 CH5/AUX1 控制 | 是 / 否 | |
| `modes_msp_override_aux2.png` | 仅 FC-ready 后期使用，MSP OVERRIDE 由 CH6/AUX2 高位触发 | 是 / 否 / 暂不启用 | |
| `motors_mixer_protocol.png` | 机架/Mixer、电调协议、无桨电机测试状态清楚 | 是 / 否 | |
| `betaflight_cli_snapshot.txt` | 包含 `version`、`status`、`serial`、`resource`、`map`、`aux`、`diff all` | 是 / 否 | |

快照结论:

```text
UART3 只做 MSP: 是 / 否
接收机端口和 UART3 MSP 未混用: 是 / 否
CH1-CH6 映射清楚: 是 / 否
ARM 不由 ESP32 控制: 是 / 否
```

## 4. MC6C Receiver 页面记录

本节只确认接收机通道和方向。全程无桨；建议只接 USB、不接动力电池。若 CH5/AUX1 已配置为 ARM，本节的 CH5 高位只用于确认 Receiver 数值变化，不作为解锁或电机测试许可。

MC6C 设置:

| 项目 | 结果 |
| --- | --- |
| 底部模式为 UAV | 是 / 否 |
| V-TAIL 关闭 | 是 / 否 |
| ELEVON 关闭 | 是 / 否 |
| Channel Map 首选候选 | `AETR1234` / 其他 |
| 最终实际 Channel Map | |
| CH5 用途 | ARM / 模式 / 其他 |
| CH6 用途 | ESP32 辅助许可 / 其他 |
| 无桨台架基准时 CH5/AUX1 低位 | 是 / 否 |
| 无桨台架基准时 CH6/AUX2 低位 | 是 / 否 |

通道数值记录:

| 通道/动作 | 低/左/后 | 中位 | 高/右/前 | 通过 |
| --- | --- | --- | --- | --- |
| Roll / AIL | | | | 是 / 否 |
| Pitch / ELE | | | | 是 / 否 |
| Throttle / THR | | | | 是 / 否 |
| Yaw / RUD | | | | 是 / 否 |
| AUX1 / CH5 | | | | 是 / 否 |
| AUX2 / CH6 | | | | 是 / 否 |

证据文件:

```text
docs/evidence/replacement_fc/receiver_page.png
docs/evidence/replacement_fc/receiver_channel_video.mp4
```

Web 方向自检结果:

| 步骤 | 通过 | 备注 |
| --- | --- | --- |
| 基准: 中位/低油门/CH5低/CH6低 | 是 / 否 | |
| AIL 右 | 是 / 否 | |
| AIL 左 | 是 / 否 | |
| ELE 前 | 是 / 否 | |
| ELE 后 | 是 / 否 | |
| THR 上 | 是 / 否 | |
| RUD 右 | 是 / 否 | |
| RUD 左 | 是 / 否 | |
| CH5 高位 | 是 / 否 | |
| CH6 高位 | 是 / 否 | |

## 5. UART3 MSP 只读证明

Betaflight CLI 设置:

```text
serial 2 1 115200 57600 0 115200
save
```

| 检查项 | 通过 | 证据/备注 |
| --- | --- | --- |
| `betaflight_cli_snapshot.txt` 里保存了 `serial`、`resource`、`diff all` | 是 / 否 | |
| USB-TTL TX -> R3/RX3 | 是 / 否 | |
| USB-TTL RX <- T3/TX3 | 是 / 否 | |
| USB-TTL GND -> FC GND | 是 / 否 | |
| ASCII `#` 能进 CLI | 是 / 否 | |
| HEX `24 4D 3C 00 01 01` 有 `24 4D 3E ...` 回包 | 是 / 否 | |
| ESP32 `esp32-s3-fc-uart-probe` 看到回包 | 是 / 否 | |
| ESP32 `esp32-s3-fc-diag` 显示 online | 是 / 否 | |

证据文件:

```text
docs/evidence/replacement_fc/uart3_usb_ttl.log
docs/evidence/replacement_fc/fc_uart_probe.log
docs/evidence/replacement_fc/fc_diag.log
```

## 6. 无桨电机和 failsafe

详细记录卡:

```text
replacement_fc_motor_direction_card.md
```

| 检查项 | 通过 | 证据/备注 |
| --- | --- | --- |
| 全程无桨 | 是 / 否 | |
| M1 位置正确 | 是 / 否 | |
| M2 位置正确 | 是 / 否 | |
| M3 位置正确 | 是 / 否 | |
| M4 位置正确 | 是 / 否 | |
| M1 旋转方向正确 | 是 / 否 | |
| M2 旋转方向正确 | 是 / 否 | |
| M3 旋转方向正确 | 是 / 否 | |
| M4 旋转方向正确 | 是 / 否 | |
| 关闭 MC6C 后进入 failsafe | 是 / 否 | |
| failsafe 后不继续加油门 | 是 / 否 | |

电机顺序/旋向/桨向摘要:

| 电机 | 实际机架位置 | 实际旋向 | 桨型/安装方向 | 通过 |
| --- | --- | --- | --- | --- |
| M1 | | CW / CCW | | 是 / 否 |
| M2 | | CW / CCW | | 是 / 否 |
| M3 | | CW / CCW | | 是 / 否 |
| M4 | | CW / CCW | | 是 / 否 |

证据文件:

```text
docs/evidence/replacement_fc/motor_order_video.mp4
docs/evidence/replacement_fc/failsafe_video.mp4
```

## 7. 默认 ESP32 Web 演示

| 检查项 | 通过 | 证据/备注 |
| --- | --- | --- |
| 使用默认 `esp32-s3-unified-web` | 是 / 否 | |
| Web 页面可打开 | 是 / 否 | |
| ToF/GPS/Camera/模拟飞控正常显示 | 是 / 否 | |
| 页面显示真实输出未编译或锁定 | 是 / 否 | |
| 没有发 `MSP_SET_RAW_RC` | 是 / 否 | |

证据文件:

```text
docs/evidence/replacement_fc/unified_web_page.png
docs/evidence/replacement_fc/unified_web_status.json
```

## 8. 低空手动方向验收

前提:

| 前提 | 通过 |
| --- | --- |
| 前面 1-7 全部通过 | 是 / 否 |
| 螺旋桨型号和安装方向正确 | 是 / 否 |
| 开阔低风环境，无旁人 | 是 / 否 |
| 飞手站在机尾后方 | 是 / 否 |
| 机头朝空旷方向 | 是 / 否 |
| CH6 保持低位 | 是 / 否 |
| ESP32 不参与第一次手动方向飞行 | 是 / 否 |
| 每个方向只做短促轻微动作，异常立刻收油/断解锁 | 是 / 否 |

动作记录:

| MC6C 操作 | 预期现象 | 实际现象 | 通过 |
| --- | --- | --- | --- |
| 油门轻推 | 平稳增升力，能稳定收油 | | 是 / 否 |
| AIL 右 | 向右轻微倾斜/移动 | | 是 / 否 |
| AIL 左 | 向左轻微倾斜/移动 | | 是 / 否 |
| ELE 前 | 机头下压，向前轻微移动 | | 是 / 否 |
| ELE 后 | 机头上仰，向后轻微移动 | | 是 / 否 |
| RUD 右 | 机头向右偏航，不突然翻滚 | | 是 / 否 |
| RUD 左 | 机头向左偏航，不突然翻滚 | | 是 / 否 |

证据文件:

```text
docs/evidence/replacement_fc/manual_hover_direction_video.mp4
docs/evidence/replacement_fc/manual_flight_notes.txt
```

如果任意方向不通过:

```text
停止飞行 -> 降落/断解锁 -> 回到 Receiver / 模型预览 / Board Alignment / Motor Order / Motor Direction / 桨叶安装方向检查。
不要用 ESP32 代码补偿手动飞行方向。
按 replacement_fc_manual_direction_troubleshooting_card.md 记录症状、排查项和修正动作。
```

失败/修正记录:

| 时间 | 现象 | 排查项 | 修正动作 | 是否复测通过 |
| --- | --- | --- | --- | --- |
| | | | | 是 / 否 |

## 9. FC-ready 真实辅助输出记录

只有第 8 步通过后才填写本节。

| 检查项 | 通过 | 证据/备注 |
| --- | --- | --- |
| Betaflight 已配置 MSP Override | 是 / 否 | |
| MSP Override 由 CH6/AUX2 高位触发 | 是 / 否 | |
| 不覆盖 ARM 通道 | 是 / 否 | |
| `modes_msp_override_aux2.png` 已保存 | 是 / 否 | |
| `betaflight_cli_snapshot.txt` 可复查 MSP Override / AUX 配置 | 是 / 否 | |
| 第一次 FC-ready 验证仍然无桨 | 是 / 否 | |
| 页面显示真实 MSP 在线 | 是 / 否 | |
| 页面显示 Betaflight ARM位/解锁位，且来自 `MSP_STATUS` ARM active-box 位 | 是 / 否 | |
| 页面显示 CH6 高位许可 | 是 / 否 | |
| Web 心跳停止时输出锁定/归中 | 是 / 否 | |
| CH6 拉低时输出锁定/归中 | 是 / 否 | |
| 页面 `最后RC R/P/Y/T/A1/A2` 显示六路输出值 | 是 / 否 | |
| 网页升降/油门仅作为仿真显示，真实油门仍由 MC6C 控制 | 是 / 否 | |
| ESP32 只改变 roll/pitch/yaw，保留 MC6C 油门、AUX1/ARM、AUX2/CH6 许可实时值 | 是 / 否 | |

证据文件:

```text
docs/evidence/replacement_fc/fc_ready_gate_page.png
docs/evidence/replacement_fc/fc_ready_output_diag.json
docs/evidence/replacement_fc/fc_ready_no_prop_test_video.mp4
```

## 10. 最终结论

| 判定项 | 是/否 | 证据 |
| --- | --- | --- |
| MC6C 手动低空方向验收通过 | | |
| ESP32 默认 Web/传感器/仿真不影响手动飞行 | | |
| FC-ready 真实辅助输出仍可关闭、可诊断、可受 CH6 控制 | | |

最终结论必须同步复核 `replacement_fc_goal_completion_audit.md`。若审计表还有任何硬件证据缺失，本记录不得写“已达到目标”。

最终结论:

```text
本次是否达到“飞机可以在 MC6C 遥控器控制下往指定方向飞”: 是 / 否
下一步动作:
```
