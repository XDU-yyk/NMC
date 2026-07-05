# 新飞控到货接线即验证工作流

日期: 2026-07-04

现场短版清单: `replacement_fc_arrival_checklist.md`

现场验收记录模板: `replacement_fc_acceptance_log_template.md`

MC6C 遥控器现场设置卡: `mc6c_transmitter_setup_card.md`

Betaflight 现场设置卡: `replacement_fc_betaflight_setup_card.md`

电机顺序/旋向/桨向现场卡: `replacement_fc_motor_direction_card.md`

低空手动方向故障排查卡: `replacement_fc_manual_direction_troubleshooting_card.md`

目标: 新 F4V3S PLUS 或同级飞控到货后，按本文件接线、配置和验证，先实现 MC6C 遥控器手动安全飞行，再启用 ESP32-S3 的 Web/传感器/仿真和后续受限辅助方向控制。

当前硬约束:

- 当前飞控已损坏，不在损坏飞控上做电机、解锁、飞行、RC override 或 MSP 写入。
- ESP32-S3 不是飞控，不直接驱动电机或电调。
- 电机输出、解锁、失控保护、遥控器接管必须由 F4V3S/Betaflight 完成。
- 所有电机测试先拆桨。没有例外。

## 1. 你的电调识别

若你手上的电调线束与下面一致，就按常见三线舵机头 PWM/OneShot 电调处理:

```text
粗红线      -> 电池正极 / 分电板正极
粗黑线      -> 电池负极 / 分电板负极
底部三根粗线 -> 无刷电机三相线
白色细线    -> 电调控制信号
红色细线    -> 电调 BEC/5V 输出或伺服头中间电源脚
黑色细线    -> 信号地
```

重要判断:

- 用户 2026-07-05 提供的电调照片符合这个类型: 透明热缩外壳、白/红/黑三针舵机头、独立粗红/粗黑动力输入、三根粗相线到无刷电机。
- 因此现场先按普通 PWM/OneShot 电调处理，不默认它支持 DShot。
- 白线接飞控电机输出的 `S` 信号脚。
- 黑线接飞控电机输出旁边的 `GND/-`。
- 红线不能随便全插。若飞控已有独立 5V 供电，就把每个电调舵机头的红线退针或绝缘，只保留白线和黑线。若确实要用电调 BEC 给飞控供电，只允许先接一个电调的红线，并用万用表确认是稳定 5V。
- 多个电调 BEC 的红线不要直接并联到同一 5V 母线上，除非电调说明书明确允许。

红线/BEC 确认方法:

```text
拆桨。
电调白线暂时不接飞控。
电调粗红/粗黑按正确极性接电池或限流电源。
万用表 DC 档量舵机头红线到黑线。
若约 5.0V 且稳定: 红线是 BEC 5V 输出。
若不是稳定 5V: 不要用它给飞控供电。
```

到货默认建议: 如果飞控或电源模块已经有稳定 5V，四个电调舵机头的红线全部退针或单独绝缘，只插白线和黑线。只有确认必须用电调 BEC 供电时，才保留其中一个电调的红线。

## 2. 推荐总体接线

### 2.1 电池和电调

```text
电池正极 -> 分电板/电源分配正极 -> 4 个电调粗红线
电池负极 -> 分电板/电源分配负极 -> 4 个电调粗黑线
电调三相线 -> 对应电机三根线
```

电机方向不靠一开始猜线序，后面在 Betaflight 电机测试里确认。方向反了就交换任意两根电机三相线。

### 2.2 电调到飞控

按飞控板上的 `M1/M2/M3/M4` 或 `S1/S2/S3/S4` 标号接:

```text
电调 1 白线 -> 飞控 M1/S1 信号
电调 1 黑线 -> 飞控 GND

电调 2 白线 -> 飞控 M2/S2 信号
电调 2 黑线 -> 飞控 GND

电调 3 白线 -> 飞控 M3/S3 信号
电调 3 黑线 -> 飞控 GND

电调 4 白线 -> 飞控 M4/S4 信号
电调 4 黑线 -> 飞控 GND
```

如果飞控电机输出是三针排针:

```text
S -> 白线
+ -> 红线, 仅在确认需要 BEC 供电时接一个
- -> 黑线
```

### 2.3 MC6C 接收机到飞控

到货当天先按 `mc6c_transmitter_setup_card.md` 检查 MC6C 底部模式、混控开关、通道反向拨码、CH5/CH6 用途和 Receiver 页面方向，再继续下面的飞控/电调步骤。

MC6C 遥控器本身不接 ESP32。接收机必须接飞控，让飞控直接获得人工接管能力。

遥控器本体先这样设置:

```text
底部模型模式: UAV / 普通无人机模式
V-TAIL 混控: 关闭
ELEVON 混控: 关闭
基础通道反向: 先保持默认, 后面按 Receiver 页面和模型预览逐项调整
```

不要在 MC6C 上打开固定翼 V 尾/三角翼混控来“修”多旋翼方向。多旋翼通道顺序和反向优先在 Betaflight Receiver/Channel Map、飞控模型预览、以及 MC6C 对应通道反向拨码里逐项确认。

必须先确认你的接收机输出类型:

```text
如果接收机支持 SBUS/PPM/iBUS/串行输出:
  接收机信号 -> 飞控对应 RX/PPM/SBUS 输入
  接收机 5V -> 飞控 5V
  接收机 GND -> 飞控 GND

如果接收机只有 6 路独立 PWM:
  先确认新飞控是否支持多路 PWM 接收机输入。
  如果不支持, 需要 PWM 转 PPM/SBUS 编码器或更换支持串行输出的接收机。
```

快速判定方法:

```text
接收机只有 3 根线整体输出, 常见标注 S.BUS / PPM / iBUS:
  优先按串行接收机处理, 只接一个信号口到飞控对应输入。

接收机有 CH1/CH2/CH3/CH4/CH5/CH6 六组排针, 每组都有 S/+/-:
  这是 6 路独立 PWM 的可能性很高。
  这时不要把六路 PWM 乱插到飞控 UART。
  先查新飞控是否支持 PWM RX, 或准备 PWM 转 PPM/SBUS 编码器。

不确定输出类型:
  先不要接电调和电池。
  只用 USB 给飞控供电, 按接收机说明接 5V/GND/信号, 到 Betaflight Receiver 页看通道是否动。
```

Receiver 页没有通道变化时, 按这个顺序分流:

```text
1. 接收机是否已和 MC6C 对频, 接收机指示灯是否进入已连接状态。
2. 接收机是否有 5V 和 GND, 极性是否正确。
3. Betaflight Ports 页是否给接收机所在 UART 打开 Serial RX, 不是 MSP。
4. Configuration 页 Receiver Mode / Serial Receiver Provider 是否匹配 SBUS/IBUS/PPM/CRSF 等真实输出。
5. 如果是 6 路 PWM 接收机, 新飞控是否真的支持多路 PWM 输入。
6. Receiver 页仍无变化时, 暂停后续电调/电机步骤, 先解决接收机输入。
```

MC6C 通道计划:

```text
CH1 AIL -> Roll 横滚
CH2 ELE -> Pitch 俯仰
CH3 THR -> Throttle 油门
CH4 RUD -> Yaw 偏航
CH5/AUX1 -> 飞控解锁或飞行模式
CH6/AUX2 -> ESP32 辅助输出许可开关
```

建议:

- CH5 用作 Betaflight ARM 开关或模式开关。
- CH6 保留给 ESP32 辅助方向控制许可。CH6 低位时 ESP32 即使接上飞控也不能发真实输出。

Betaflight Channel Map 首选候选:

```text
map AETR1234
save
```

含义是先按 MC6C 的物理顺序 `CH1 AIL, CH2 ELE, CH3 THR, CH4 RUD` 映射到 Betaflight 的 `Roll, Pitch, Throttle, Yaw`。这只是到货当天的第一候选，不是跳过 Receiver 检查的理由。保存后必须在 Receiver 页确认 `AIL -> Roll`、`ELE -> Pitch`、`THR -> Throttle`、`RUD -> Yaw`、`CH5 -> AUX1`、`CH6 -> AUX2`，并把最终实际 map 写进验收记录。

### 2.4 ESP32-S3 到飞控

ESP32 只做上层 Web/传感器/遥测和未来受限辅助。使用 UART3 MSP:

```text
ESP32 GPIO16 TX -> 飞控 R3/RX3
ESP32 GPIO17 RX <- 飞控 T3/TX3
ESP32 GND       -> 飞控 GND
```

不要把 ESP32 接到电调信号线。

## 3. Betaflight 初始配置

### 3.1 端口

UART3 打开 MSP:

```text
serial 2 1 115200 57600 0 115200
save
```

UART6 若用于接收机，配置为 Serial RX。若不用，保持空闲。

### 3.2 接收机

在 Receiver 页面确认:

```text
AIL 摇杆 -> Roll 动
ELE 摇杆 -> Pitch 动
THR 摇杆 -> Throttle 动
RUD 摇杆 -> Yaw 动
CH5 开关 -> AUX1 动
CH6 开关 -> AUX2 动
```

如果通道顺序错，优先改 Betaflight Channel Map 或遥控器模式，不要靠猜飞控代码修。

注意一个容易混淆的点:

- MC6C/接收机物理通道通常按 `CH1 AIL, CH2 ELE, CH3 THR, CH4 RUD` 理解。
- Betaflight 经过 Channel Map 后，MSP `MSP_RC`/`MSP_SET_RAW_RC` 使用内部功能顺序 `roll, pitch, yaw, throttle, AUX1, AUX2...`。
- 本项目网页显示按功能名显示 `横滚/俯仰/油门/偏航/AUX1/AUX2`，不要把网页里的功能值直接当成原始接收机 CH3/CH4 排序。

### 3.3 MSP Override 只作为后期辅助输出

MC6C 手动飞行不依赖 ESP32。先完成 Receiver、Modes、Motors、failsafe 和手动悬停，确认遥控器能独立接管。

未来要让 ESP32 的网页方向控制进入真实飞控时，Betaflight 端还必须支持并配置 MSP Override:

```text
模式页: 添加 MSP Override / MSP OVERRIDE 模式
触发通道: AUX2 / CH6
触发范围: 只在 CH6 高位时启用
覆盖通道: 只允许 roll/pitch/yaw 姿态辅助, 不覆盖油门、ARM/AUX1 或 CH6/AUX2
```

注意:

- 仅开启 UART3 MSP 不等于飞控会采用 `MSP_SET_RAW_RC`。
- `CH6 高位` 在本项目里只是 ESP32 许可开关；还要确认 Betaflight 里 MSP Override 模式真的随 CH6 高位激活。
- 第一次验证必须无桨: 页面显示门槛打开后，轻推网页方向控制，Receiver/状态页应能看到对应功能值变化；松手或关 CH6 后必须立刻回到遥控器/飞控本身的安全输入。
- 如果 Betaflight 版本/固件目标没有 MSP Override 功能，FC-ready 真实辅助输出保持禁用，只使用默认 `esp32-s3-unified-web` 做传感器/相机/仿真演示。

### 3.4 Betaflight 配置快照

到货当天必须保存一组配置证据。目的不是写报告好看，而是防止“线接对了但 Betaflight 页面没配对”这种现场最难查的问题。

建议放到:

```text
docs/evidence/replacement_fc/
```

证据目录说明见 `docs/evidence/replacement_fc/README.md`。

必须截图或录像:

```text
setup_model_preview.png        Setup 页面模型预览，手动转动机架方向一致
ports_uart3_msp.png            Ports 页面，UART3 打开 MSP
ports_receiver_serial_rx.png   若接收机走串行口，该 UART 打开 Serial RX，不能和 UART3 MSP 混在一起
receiver_protocol_map.png      Receiver 配置，协议/Provider/Channel Map 与真实接收机一致
receiver_ch1_ch6.png           Receiver 页面，Roll/Pitch/Throttle/Yaw/AUX1/AUX2 六路都动
modes_arm_aux1.png             Modes 页面，ARM 或基本模式由 CH5/AUX1 控制
modes_msp_override_aux2.png    仅后期 FC-ready 使用，MSP OVERRIDE 由 CH6/AUX2 高位触发
motors_mixer_protocol.png      Motors 页面，机架/Mixer、电调协议、无桨电机测试状态
```

必须保存 CLI 文本:

```text
version
status
serial
resource
map
aux
diff all
```

可用时再补充:

```text
get serialrx_provider
get msp_override_channels_mask
get msp_override_failsafe
```

保存为:

```text
betaflight_cli_snapshot.txt
```

快照通过标准:

```text
UART3 只承担 MSP。
接收机所在端口只承担真实接收机输入，如 Serial RX/SBUS/iBUS/CRSF/PPM/PWM。
CH1-CH4 与 AIL/ELE/THR/RUD 对应，CH5 映射 AUX1，CH6 映射 AUX2。
ARM 不由 ESP32 控制。
FC-ready 阶段若启用 MSP OVERRIDE，只由 CH6/AUX2 高位允许，并且只允许 roll/pitch/yaw 辅助，不覆盖油门、ARM/AUX1、CH6/AUX2。
```

如果截图或 CLI 快照缺失，不把“Betaflight 已配置好”当作通过。

### 3.5 电调协议

此类三线舵机头电调优先按普通 PWM/OneShot 类电调处理。

初次配置建议:

```text
Motor protocol: PWM 或 Oneshot125
Idle/min throttle: 按 Betaflight 和电调校准结果设置
```

如果确认电调支持 DShot，才改 DShot。不能只因为飞控支持 DShot 就默认电调也支持。

PWM 电调通常需要油门行程校准。校准必须拆桨。

## 4. 无桨验证顺序

1. 不接电池，只接 USB，确认飞控能进 Betaflight。
2. Receiver 页面确认 MC6C 六个通道方向正确。
3. Modes 页面设置 ARM、Angle/Horizon 或需要的基本模式。
4. 保存 Betaflight 配置快照，确认 UART3 MSP、接收机输入、Receiver、Modes、Motors 页面证据齐全。
5. 不装桨，接电池，检查飞控和接收机不断电、不重启。
6. Motors 页面勾选安全确认，一次只推一个电机。
7. 确认 M1/M2/M3/M4 顺序和机架方向一致。
8. 确认每个电机旋转方向。方向错则交换该电机任意两根三相线。
9. 检查 failsafe: 关闭遥控器后飞控必须进入失控保护，不得继续加油门。
10. 只在以上全部通过后，才考虑装桨低油门约束测试。

电机顺序、旋向和桨向建议同步填写 `replacement_fc_motor_direction_card.md`。M1-M4 位置或旋向未记录通过时，不把无桨检查当作已完成。

## 4.1 低空手动方向验收

这一步用于证明“MC6C 能让飞机按指定方向飞”。它必须排在 Receiver、Modes、Motors、failsafe、无桨检查之后，并且只允许在开阔、低风、无旁人、飞手能立刻收油/切断解锁的环境里做。

到货当天建议同步填写 `replacement_fc_acceptance_log_template.md`，把 Betaflight 截图、Receiver 页面、无桨电机视频、failsafe 视频和低空方向验收视频放到 `docs/evidence/replacement_fc/` 下。没有证据记录时，不要把本步骤口头当作已通过。

验收前提:

```text
螺旋桨型号和安装方向确认正确
电机 M1-M4 顺序和旋转方向确认正确
Betaflight 模型预览方向正确
failsafe 已验证
CH5/ARM 或模式开关位置清楚
CH6 保持低位, 不启用 ESP32 辅助输出
默认使用 MC6C 手动遥控, 不依赖 ESP32 起飞或接管
```

建议先让机头朝向空旷方向，飞手站在机尾后方判断方向。不要在机头朝向飞手或侧向飞手时做第一次方向验收，容易把前后左右看反。

| MC6C 操作 | 预期现象 | 不符合时怎么处理 |
| --- | --- | --- |
| 油门轻推 | 飞机平稳增升力，低空离地或接近离地 | 立刻收油并检查油门行程、电调校准、电池/重心 |
| AIL 右 | 以机头方向为准，飞机向右侧轻微倾斜/漂移 | 降落断电，回 Receiver/通道反向和模型预览检查 |
| AIL 左 | 以机头方向为准，飞机向左侧轻微倾斜/漂移 | 降落断电，回 Receiver/通道反向和模型预览检查 |
| ELE 前 | 机头下压，飞机向前轻微移动 | 降落断电，先看 Betaflight 模型预览，再调 ELE 反向 |
| ELE 后 | 机头上仰，飞机向后轻微移动 | 降落断电，先看 Betaflight 模型预览，再调 ELE 反向 |
| RUD 右 | 机头向右偏航，机体不应突然翻滚 | 降落断电，检查 yaw 通道、飞控朝向、桨向/电机顺序 |
| RUD 左 | 机头向左偏航，机体不应突然翻滚 | 降落断电，检查 yaw 通道、飞控朝向、桨向/电机顺序 |

通过标准:

```text
能稳定解锁和收油
低空悬停不明显漂移到不可控
前/后/左/右/左偏航/右偏航都和 MC6C 操作一致
关闭遥控器或触发 failsafe 后不继续加油门
整个过程不需要 ESP32 真实输出
```

失败处理原则:

- 只要飞机起飞瞬间有翻滚、猛烈侧倾、原地打转、方向与摇杆相反，立刻收油/断解锁，停止飞行。
- 低空方向异常的症状排查按 `replacement_fc_manual_direction_troubleshooting_card.md` 执行。
- 优先回到 Betaflight Receiver、模型预览、Board Alignment、Motor Order、Motor Direction、桨叶安装方向检查。
- 不要用 ESP32 代码修正 MC6C 手动飞行方向。手动方向必须由飞控、接收机和遥控器本身先正确。
- 手动低空方向验收通过以后，才允许继续考虑 `esp32-s3-unified-web-fc-ready` 的无桨 MSP Override 验证。

## 5. ESP32 固件使用方式

当前默认固件:

```text
esp32-s3-unified-web
```

用途:

- 相机、ToF、GPS、Web 仪表盘
- 模拟飞控
- Web 方向控制仿真
- 不写真飞控

旧 `FollowController` 自动跟随 PID 路径默认不向真实飞控排队输出，受 `ENABLE_LEGACY_FOLLOW_FC_OUTPUT=0` 保护。MC6C 低空手动方向验收前，不启用任何旧自动跟随真机输出。

未来新飞控通过 UART3 MSP、接收机、电调和无桨验证后，才可编译:

```text
esp32-s3-unified-web-fc-ready
```

这个环境会编入 `comm/fc_bridge.cpp` 和 `comm/msp.cpp`，但真实输出仍被运行时安全门锁住。必须同时满足:

```text
ENABLE_REAL_FC_OUTPUT=1
飞控 MSP 在线
Betaflight ARM位/解锁位=1
MC6C CH6/AUX2 > 1700
Web 方向控制正在发送有效心跳
```

任一条件不满足，ESP32 不发送 `MSP_SET_RAW_RC`。

这里的 `Betaflight ARM位/解锁位=1` 来自 Betaflight `MSP_STATUS` 的 ARM active-box 位。它表示飞控运行时真实解锁状态，不是简单的 CH5/ARM 开关高位，也不是 `MSP_STATUS_EX` 里的 arming-disable-flags。若未来换成非 Betaflight 固件，必须重新确认该字段含义。

### 5.1 FC-ready 页面检查

上传 `esp32-s3-unified-web-fc-ready` 前仍然要先完成无桨、Receiver、Motors、failsafe 和 MSP 只读验证。上传后，在 `http://192.168.4.1/` 重点看:

```text
RC 输入:
  CH1 横滚、CH2 俯仰、CH3 油门、CH4 偏航
  CH5/AUX1 ARM 或模式开关
  CH6/AUX2 ESP32 辅助输出许可

FC-ready 门槛:
  真实MSP 在线/离线
  Betaflight ARM位/解锁位 是/否
  CH6许可 高位/低位
  输出 可放行/锁定
  输出诊断: 请求、发送OK、发送失败、门槛拦截、超时归中、最后原因
  MSP诊断: TX帧、TX字节、RX字节、超时、最后错误、最后RC R/P/Y/T/A1/A2

MC6C 接收机检查:
  6 路是否读到
  横滚/俯仰/偏航是否在中位
  油门是否低位
  CH5/CH6 是否处于低位台架基准
  CH5/CH6 高位只在方向自检步骤里单独确认

MC6C 方向自检:
  按页面提示依次记录基准、AIL 右、AIL 左、ELE 前、ELE 后、THR 上、RUD 右、RUD 左、CH5 高位、CH6 高位
  页面会根据记录值判断通道是否有动作；AIL/RUD/THR/CH5/CH6 会判断预期方向
```

正确现象:

- MC6C 摇杆动时，CH1-CH4 应跟随变化。
- CH5 开关动时，AUX1 应变化，并且 Betaflight 的 ARM/模式配置要能对应。
- CH6 开关动时，AUX2 应从低位跳到高位。只有高位才允许 ESP32 辅助输出通过门槛。
- 真实 MSP 未在线、Betaflight ARM位/解锁位未打开、CH6 未高位、Web 方向心跳停止时，页面都必须显示输出锁定。
- FC-ready 真实输出只能改 roll/pitch/yaw；油门、AUX1/ARM、AUX2/CH6 必须继续使用 MC6C/飞控通过 MSP 读到的实时值。页面 `最后RC R/P/Y/T/A1/A2` 用来核对输出六路值。
- 页面方向面板里的 `升仿真/降仿真` 和油门摇杆只用于仿真显示；即使 FC-ready 门槛打开，真实飞控油门也不接受网页输入，必须由 MC6C 油门杆控制。
- 调试真实辅助输出时，先看 `输出诊断` 和 `MSP诊断`: `门槛拦截` 增加说明安全门没开；`超时归中` 增加说明 Web 心跳断了；`发送失败` 或 MSP `超时/最后错误` 增加说明 UART3/MSP 链路还不可靠。
- `MC6C 接收机检查` 显示 `可继续无桨台架检查` 只表示接收机链路、油门低位、CH5/AUX1 低位和 CH6/AUX2 低位适合作为台架基准，不表示可以装桨或飞行。
- `开始方向自检` 需要真实 MSP 在线。每一步都要按页面提示保持摇杆/开关，再点 `记录当前步骤`。
- 方向自检的基准、双向摇杆变化和 CH5/CH6 高位判断在源码里有 host 测试覆盖；如果页面提示异常，优先排查真实 MSP、Receiver 映射和 MC6C 反向拨码，不要先改 ESP32 代码补偿。
- `ELE 前/后` 只要求 CH2 有明显双向变化，正反方向最终必须看 Betaflight 模型预览：前推俯仰杆时模型应表现为机头下压/向前飞的方向。不同接收机和 Channel Map 下数值增减可能不同，不要只按数值大小改反向。
- 如果方向自检失败，优先调整 Betaflight Receiver/Channel Map 或 MC6C 舵机反向拨码；不要用 ESP32 代码把反向通道硬补成“看起来正确”。

## 6. 到货当天最短执行清单

```text
1. 拆桨。
2. 飞控接 USB, 刷/配置 Betaflight，并保存配置快照。
3. 判定接收机输出类型: 串行/SBUS/PPM/iBUS 或 6 路独立 PWM。
4. MC6C 接收机接飞控, Receiver 页面确认 CH1-CH6。
5. 电调粗线接分电, 电调白/黑接 M1-M4 信号/GND。
6. 只保留必要的一个 BEC 红线, 或全部红线绝缘并用独立 5V。
7. 配置 UART3 MSP。
8. USB-TTL 或 ESP32 只读 MSP 验证通过。
9. Motors 页面逐个电机验证顺序和方向。
10. Failsafe 验证。
11. 上传默认 `esp32-s3-unified-web` 做 Web/传感器/仿真演示。
12. 按 `replacement_fc_acceptance_log_template.md` 填写验收记录和证据路径。
13. 真机辅助方向控制只在手动飞行稳定后, 再考虑 `esp32-s3-unified-web-fc-ready`。
```

## 7. 明确禁止

- 不用 ESP32 直接接电调信号线。
- 不在损坏飞控上做电机测试。
- 不带桨做电机方向/顺序测试。
- 不跳过 Receiver 页面通道检查。
- 不跳过 failsafe 检查。
- 不在 MC6C 接管未验证前启用 ESP32 辅助输出。
