# 新飞控到货日证据包

用途: 保存新飞控接线、Betaflight 配置、MC6C 接收机、无桨电机、failsafe、低空手动方向验收和后期 FC-ready 台架验证的证据。

本目录只放证据，不代表已经通过验收。最终结论必须填写在仓库根目录的 `replacement_fc_acceptance_log_template.md` 里。

最终目标是否真正完成，按仓库根目录的 `replacement_fc_goal_completion_audit.md` 逐项复核。
现场命令和串口日志保存，按仓库根目录的 `replacement_fc_command_record_card.md` 执行。

现场 Betaflight 配置逐项看 `replacement_fc_betaflight_setup_card.md`，不要盲目粘贴整套 `diff all`。

低空手动方向异常时看 `replacement_fc_manual_direction_troubleshooting_card.md`，并把症状、排查项和修正动作写回验收记录。

证据保存后，可在仓库根目录运行只读检查:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check_replacement_fc_evidence.ps1
```

这个脚本只检查本目录文件是否齐全、CLI 快照是否包含关键段落、`software_checks.txt` 是否像完整软件检查日志、`manual_flight_notes.txt` 是否记录最终 Channel Map/CH6 低位/ESP32 未参与/各方向结论、JSON 是否能解析。它不代表飞行验收通过；最终仍看验收记录和 `replacement_fc_goal_completion_audit.md`。

## 0. 硬停止条件

出现任一情况时，不继续后续步骤:

- 使用的是已损坏旧飞控。
- 未拆桨就准备做 Receiver、Motors、MSP 或 FC-ready 检查。
- ESP32 接到任何电调信号线。
- Receiver 页面没有通道变化，却准备继续接电调或装桨。
- Failsafe 没验证，却准备起飞。
- MC6C 低空手动方向验收未通过，却准备启用 ESP32 真机辅助输出。

## 1. Betaflight 配置快照

必须保存:

```text
setup_model_preview.png
ports_uart3_msp.png
ports_receiver_serial_rx.png
receiver_protocol_map.png
receiver_ch1_ch6.png
modes_arm_aux1.png
motors_mixer_protocol.png
betaflight_cli_snapshot.txt
```

后期只在 FC-ready 无桨台架验证前保存:

```text
modes_msp_override_aux2.png
```

`betaflight_cli_snapshot.txt` 至少包含:

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
UART3 只做 MSP。
接收机端口和 UART3 MSP 未混用。
CH1-CH6 映射清楚。
CH5/AUX1 控制 ARM 或基本模式。
CH6/AUX2 只作为 ESP32 辅助许可。
ARM 不由 ESP32 控制。
MC6C 首选 Channel Map 候选为 AETR1234；如果最终实际 map 不同，必须在验收记录里说明并由 Receiver 页面证明。
```

## 2. MC6C 接收机证据

必须保存:

```text
receiver_page.png
receiver_channel_video.mp4
```

视频里应能看到:

```text
AIL 左/右 -> Roll 变化
ELE 前/后 -> Pitch 变化
THR 上/下 -> Throttle 变化
RUD 左/右 -> Yaw 变化
CH5 开关 -> AUX1 低/高变化
CH6 开关 -> AUX2 低/高变化
```

若使用 Web 的 MC6C 方向自检，结果应覆盖:

```text
基准
AIL 右 / AIL 左
ELE 前 / ELE 后
THR 上
RUD 右 / RUD 左
CH5 高位
CH6 高位
```

继续无桨台架检查前的基准状态必须是:

```text
Roll/Pitch/Yaw 中位。
Throttle 低位。
CH5/AUX1 低位。
CH6/AUX2 低位。
CH5/CH6 高位只作为方向自检里的单独通道变化证据。
```

Receiver 不通过时，不接电调、不做 Motors。

## 3. UART3 MSP 只读证据

必须保存:

```text
uart3_usb_ttl.log
fc_uart_probe.log
fc_diag.log
```

通过标准:

```text
USB-TTL TX -> 飞控 R3/RX3
USB-TTL RX <- 飞控 T3/TX3
USB-TTL GND -> 飞控 GND
VCC 不接

发送 ASCII # 能进入 CLI。
发送 HEX 24 4D 3C 00 01 01 有 24 4D 3E ... 回包。
ESP32 只读 probe 能看到 MSP 回包。
FC diag 能显示 online。
```

## 4. 无桨电机和 Failsafe 证据

必须保存:

```text
motor_order_video.mp4
failsafe_video.mp4
replacement_fc_motor_direction_card_filled.md 或照片/扫描件
```

通过标准:

```text
全程无桨。
Motors 页面一次只推一个电机。
M1/M2/M3/M4 物理位置和 Betaflight 机架图一致。
每个电机旋转方向正确。
桨叶型号/方向与对应电机旋向匹配。
关闭 MC6C 后飞控进入 failsafe，不继续加油门。
```

## 5. 默认 ESP32 Web 证据

必须保存:

```text
unified_web_page.png
unified_web_status.json
```

通过标准:

```text
使用默认 esp32-s3-unified-web。
页面显示传感器、相机、GPS/ToF 和仿真状态。
页面显示真实输出未编译或锁定。
不写真飞控，不解锁，不发 MSP_SET_RAW_RC。
```

`unified_web_status.json` 判读:

```text
默认固件不编译真实 FC 输出，fcOut* 不能作为真实飞控输出证据。
真实 MSP 未在线时，rcRoll/rcPitch/rcYaw/rcThrottle/rcAux1/rcAux2 也不能作为 MC6C 接收机通过证据。
```

## 6. 低空 MC6C 手动方向验收证据

必须保存:

```text
manual_hover_direction_video.mp4
manual_flight_notes.txt
```

可先复制本目录的模板再填写:

```text
manual_flight_notes_template.txt -> manual_flight_notes.txt
```

`manual_flight_notes.txt` 至少写清:

```text
最终 Channel Map。
第一次手动飞行时 CH6/AUX2 保持低位。
ESP32 不参与第一次手动方向飞行。
油门、AIL、ELE、RUD 各方向实际现象和通过/不通过结论。
```

验收前提:

```text
螺旋桨型号和安装方向确认正确。
电机顺序和旋转方向确认正确。
Betaflight 模型预览方向正确。
Failsafe 已验证。
CH5/ARM 或模式开关位置清楚。
CH6 保持低位。
飞手站在机尾后方，机头朝空旷方向。
每个方向只做短促轻微动作，异常立刻收油/断解锁。
ESP32 不参与第一次手动方向飞行。
```

通过标准:

```text
油门轻推能稳定增升力并能稳定收油。
AIL 左/右与机头方向下的左/右反应一致。
ELE 前/后与前/后反应一致。
RUD 左/右与偏航方向一致。
全程可控，没有起飞瞬间翻滚、猛烈侧倾或原地打转。
```

如果任一方向不符合，停止飞行，回到 Receiver、模型预览、Board Alignment、Motor Order、Motor Direction 和桨叶安装方向检查。不要用 ESP32 代码补偿 MC6C 手动方向错误。

建议同步保存:

```text
manual_direction_failure_notes.txt
```

## 7. 后期 FC-ready 无桨台架证据

只有低空 MC6C 手动方向验收通过后，才填写本节。

必须保存:

```text
fc_ready_gate_page.png
fc_ready_output_diag.json
fc_ready_no_prop_test_video.mp4
```

通过标准:

```text
第一次 FC-ready 验证仍然无桨。
Betaflight 已配置 MSP OVERRIDE。
MSP OVERRIDE 由 CH6/AUX2 高位触发。
页面显示真实 MSP 在线。
页面显示 Betaflight ARM 位来自 MSP_STATUS ARM active-box。
页面显示 CH6 高位许可。
Web 心跳停止或 CH6 拉低时输出锁定/归中。
页面最后 RC 显示 R/P/Y/T/A1/A2 六路值。
JSON 里的 rc* 是 Betaflight MSP_RC 读回的功能顺序值。
JSON 里的 fcOut* 是最近一次打包限幅后的 Betaflight RAW_RC 输出诊断。
fcOutThrottle、fcOutAux1、fcOutAux2 必须保持 MC6C 实时值。
ESP32 只改变 roll/pitch/yaw。
MC6C throttle、AUX1/ARM、AUX2/CH6 保持实时值。
网页升降/油门仅作为仿真显示。
```

## 8. 最终判定

只有同时满足以下条件，才算本阶段目标通过:

```text
MC6C 手动低空方向验收通过。
证据文件和验收记录齐全。
ESP32 默认 Web/传感器/仿真不会影响手动飞行。
FC-ready 真机辅助仍可关闭、可诊断、可受 CH6 控制。
```
