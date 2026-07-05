# MC6C 指定方向飞行目标完成审计

日期: 2026-07-05

用途: 判断本阶段目标是否真正完成。目标不是“代码能编译”，而是新飞控到货后，飞机能在 Microzone MC6C 遥控器控制下按指定方向飞，并且 ESP32-S3 不破坏人工控制权。

当前结论:

```text
软件/流程准备: 已完成到可到货验证的状态。
真实飞行目标: 未完成，必须等待新飞控实物证据。
```

## 1. 目标拆解

只有下面全部为“已证明”，才算达到“飞机可以在 MC6C 遥控器控制下往指定方向飞”。

| 要求 | 当前状态 | 需要的权威证据 |
| --- | --- | --- |
| 使用新飞控，不再使用损坏旧飞控 | 待硬件证明 | 新飞控照片、Betaflight 连接截图、型号/固件目标记录 |
| MC6C 设置适合多旋翼 | 待硬件证明 | MC6C 为 UAV；V-TAIL/ELEVON 关闭；CH5/CH6 用途记录 |
| 接收机输出类型正确接入飞控 | 待硬件证明 | Receiver 页面截图；接收机输出类型记录；端口未与 UART3 MSP 混用 |
| CH1-CH6 映射正确 | 待硬件证明 | Receiver 页面或视频证明 Roll/Pitch/Throttle/Yaw/AUX1/AUX2 六路都动 |
| AIL/ELE/THR/RUD 方向正确 | 待硬件证明 | Receiver 方向自检、Betaflight 模型预览、最终 Channel Map 记录 |
| CH5/ARM 或模式开关清楚 | 待硬件证明 | Modes 页面截图；ARM 不由 ESP32 控制 |
| CH6 保持为 ESP32 辅助许可 | 待硬件证明 | AUX2 低/高变化截图；第一次手动飞行时 CH6 低位 |
| 电机 M1-M4 顺序正确 | 待硬件证明 | 无桨 Motors 页面逐电机视频和 `replacement_fc_motor_direction_card.md` |
| 电机旋向和桨向正确 | 待硬件证明 | 无桨旋向记录、桨叶安装记录、装桨前复核 |
| Failsafe 有效 | 待硬件证明 | `failsafe_video.mp4`，关闭 MC6C 后不继续加油门 |
| 默认 ESP32 Web 不影响手动飞行 | 待硬件证明 | `esp32-s3-unified-web` 页面截图/状态 JSON；真实输出未编译或锁定 |
| 低空 MC6C 手动方向验收通过 | 待硬件证明 | `manual_hover_direction_video.mp4`，油门、AIL 左右、ELE 前后、RUD 左右均符合 |
| FC-ready 后期辅助仍可关闭和诊断 | 软件已准备，硬件待证明 | 仅在低空手动方向验收通过后，无桨验证 FC-ready 门槛、CH6 门控、输出诊断 |

## 2. 软件已经证明的部分

这些项目由当前源码和本地测试证明，但不能替代真实飞行证据。

| 软件项 | 当前证据 |
| --- | --- |
| 默认固件安全 | `platformio.ini` 默认环境为 `esp32-s3-unified-web`；默认 `ENABLE_REAL_FC_OUTPUT=0` |
| FC-ready 只作为未来专用构建 | `esp32-s3-unified-web-fc-ready` 单独环境才启用 `ENABLE_REAL_FC_OUTPUT=1` |
| ESP32 不直接控制电调 | 文档要求电调信号进飞控 M1-M4/S1-S4；ESP32 不接电调白色信号线 |
| ESP32 不负责 ARM/DISARM | `ENABLE_ESP32_ARM_DISARM=0`；MC6C/Betaflight 是解锁来源 |
| RAW_RC 顺序按 Betaflight 功能顺序 | host 测试覆盖 roll, pitch, yaw, throttle, AUX1, AUX2 |
| FC-ready 保留 MC6C 油门和 AUX | host 测试覆盖 throttle、AUX1/ARM、AUX2/CH6 保留实时值 |
| Web 油门只用于仿真 | host 测试覆盖网页升降/油门不转发到真实 FC throttle |
| 输出有多重门槛 | 编译期开关 + FC online + Betaflight ARM active-box + CH6 高位 + Web 心跳 |
| MC6C 接收机基准保守 | host 测试覆盖六路、三轴中位、油门低、CH5/CH6 低位基准 |
| MC6C 方向自检为双向 | host 测试覆盖 AIL 左/右、ELE 前/后动作、THR 上、RUD 左/右、CH5/CH6 高位 |

最近一次软件检查命令:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1
```

最近一次已记录结果:

```text
manual-control: 23 passed, 0 failed
fc-ready logic: 79 passed, 0 failed
web-output-path: 20 passed, 0 failed
build passed: esp32-s3-unified-web
build passed: esp32-s3-unified-web-fc-ready
build passed: esp32-s3-fc-uart-probe
build passed: esp32-s3-fc-diag
```

## 3. 不能用来判定完成的证据

下面这些只能说明“准备工作有进展”，不能说明飞机已经能按方向飞。

- 代码编译通过。
- Host 测试通过。
- Web 页面能打开。
- ESP32 能显示模拟飞控状态。
- Receiver 页面只有部分通道变化。
- 只证明 UART3 MSP 有回包。
- 只做无桨电机测试，未做低空手动方向验收。
- 没有视频或日志路径的口头描述。

## 4. 到货当天最终判定

把下面每项填成“是”，并在 `replacement_fc_acceptance_log_template.md` 填入证据路径后，才允许把目标判为完成。

| 判定项 | 是/否 | 证据路径 |
| --- | --- | --- |
| 新飞控已替换损坏旧飞控 | | |
| Betaflight Setup 模型预览方向正确 | | |
| MC6C 为 UAV，V-TAIL/ELEVON 关闭 | | |
| Receiver 页面 CH1-CH6 映射和方向正确 | | |
| 最终 Channel Map 已记录 | | |
| CH5/ARM 或模式开关含义清楚 | | |
| CH6/AUX2 低位为默认，第一次手动飞行不启用 ESP32 辅助 | | |
| M1-M4 电机顺序正确 | | |
| 电机旋向正确 | | |
| 桨叶型号和安装方向正确 | | |
| Failsafe 有视频证明 | | |
| 默认 ESP32 Web 不影响手动飞行 | | |
| 低空油门轻推能稳定增升力并能稳定收油 | | |
| AIL 左/右与飞机左右反应一致 | | |
| ELE 前/后与飞机前后反应一致 | | |
| RUD 左/右与偏航方向一致 | | |
| 全程没有起飞瞬间翻滚、猛烈侧倾或原地打转 | | |

最终结论:

```text
本阶段目标是否完成: 是 / 否
若否，缺失的最小下一步证据:
```

## 5. 若目标未完成

按缺失项回退:

- Receiver 或方向不对: 回 `mc6c_transmitter_setup_card.md` 和 Betaflight Receiver/模型预览。
- 电机或桨不对: 回 `replacement_fc_motor_direction_card.md`。
- 起飞异常: 回 `replacement_fc_manual_direction_troubleshooting_card.md`。
- 证据不全: 回 `docs/evidence/replacement_fc/README.md`。
- 不要用 ESP32 代码补偿 MC6C 手动方向错误。
