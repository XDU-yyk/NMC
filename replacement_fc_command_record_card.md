# 新飞控到货命令记录卡

日期: 2026-07-05

用途: 到货当天把需要输入的 PowerShell、PlatformIO、Betaflight CLI、USB-TTL 串口命令集中在一页，并把输出保存到证据目录。

证据目录:

```text
docs/evidence/replacement_fc/
```

硬边界:

```text
当前损坏旧飞控上不使用本卡做 MSP、解锁、电机或飞行验证。
所有 Receiver、Motors、MSP、FC-ready 检查前先拆桨。
ESP32 不接电调信号线。
FC-ready 上传和验证必须排在低空 MC6C 手动方向验收之后。
```

## 1. 本机软件检查

这些命令不上传固件、不打开串口、不写 MSP、不碰电机。

```powershell
powershell -ExecutionPolicy Bypass -File scripts\run_host_tests.ps1
powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1
```

记录到:

```text
docs/evidence/replacement_fc/software_checks.txt
```

通过标准:

```text
manual-control: 23 passed
fc-ready logic: 79 passed
web-output-path: 20 passed
esp32-s3-unified-web build SUCCESS
esp32-s3-unified-web-fc-ready build SUCCESS
esp32-s3-fc-uart-probe build SUCCESS
esp32-s3-fc-diag build SUCCESS
```

## 1.1 证据包完整性检查

到货日证据保存后，在仓库根目录运行:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check_replacement_fc_evidence.ps1
```

若已经完成低空手动方向验收之后的 FC-ready 无桨台架验证，再运行:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check_replacement_fc_evidence.ps1 -RequireFcReady
```

通过标准:

```text
必需截图、日志、视频、JSON 都存在且非空。
betaflight_cli_snapshot.txt 包含 version/status/serial/resource/map/aux/diff all。
software_checks.txt 包含完整软件检查最终通过行、host 测试通过行和四个构建环境名。
manual_flight_notes.txt 记录最终 Channel Map、CH6/AUX2 低位、ESP32 未参与、油门/AIL/ELE/RUD 结果和通过/不通过结论。
unified_web_status.json 能被解析为 JSON。
若启用 -RequireFcReady，FC-ready 三项证据和 modes_msp_override_aux2.png 也必须存在。
```

注意: 这个检查只证明证据包可审查，不证明飞机已经按 MC6C 方向飞行成功。

低空手动方向验收完成后，可先复制模板再填写:

```text
docs/evidence/replacement_fc/manual_flight_notes_template.txt
-> docs/evidence/replacement_fc/manual_flight_notes.txt
```

## 2. Betaflight CLI 快照

先连接新飞控，只接 USB，不接动力电池，不装桨。

先保存基础快照:

```text
version
status
serial
resource
map
aux
diff all
```

保存为:

```text
docs/evidence/replacement_fc/betaflight_cli_snapshot.txt
```

如果需要把 UART3 设为 MSP:

```text
serial 2 1 115200 57600 0 115200
save
```

重连后再次保存:

```text
serial
resource
diff all
```

通过标准:

```text
UART3 / serial index 2 只做 MSP。
接收机输入端口和 UART3 MSP 不混用。
```

## 3. MC6C Channel Map 候选

MC6C 物理顺序按:

```text
CH1 AIL, CH2 ELE, CH3 THR, CH4 RUD
```

Betaflight Receiver 页第一候选:

```text
map AETR1234
save
```

保存后必须回 Receiver 页面确认:

```text
AIL 左/右 -> Roll
ELE 前/后 -> Pitch
THR 上/下 -> Throttle
RUD 左/右 -> Yaw
CH5 -> AUX1
CH6 -> AUX2
```

若 `AETR1234` 不符合真实接收机输出，按 Receiver 页面改最终实际 map，并写入:

```text
replacement_fc_acceptance_log_template.md
docs/evidence/replacement_fc/betaflight_cli_snapshot.txt
```

不要用 ESP32 代码补偿错误 Channel Map。

## 4. USB-TTL UART3 MSP 只读证明

前提:

```text
新飞控
无桨
GND 共地
VCC 不接
Betaflight UART3 MSP 已启用
```

接线:

```text
USB-TTL TX  -> 飞控 R3/RX3
USB-TTL RX  <- 飞控 T3/TX3
USB-TTL GND -> 飞控 GND
USB-TTL VCC 不接
```

串口参数:

```text
115200
8N1
无流控
```

先发 ASCII:

```text
#
```

能进入 CLI 后，说明 TX/RX/GND/波特率基本正确。

再发 HEX:

```text
24 4D 3C 00 01 01
```

期望回包:

```text
24 4D 3E ...
```

保存为:

```text
docs/evidence/replacement_fc/uart3_usb_ttl.log
```

未通过时只查:

```text
R3/T3 是否接反
GND 是否共地
Betaflight serial 2 MSP 是否启用
波特率是否 115200
```

不要启用 `MSP_SET_RAW_RC`。

## 5. ESP32 UART3 只读 Probe

只有 USB-TTL UART3 MSP 已通过后，才接 ESP32:

```text
ESP32 GPIO16 TX -> 飞控 R3/RX3
ESP32 GPIO17 RX <- 飞控 T3/TX3
ESP32 GND       -> 飞控 GND
```

上传只读 probe:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-fc-uart-probe -t upload --upload-port COM55
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe device monitor --port COM55 --baud 115200
```

保存为:

```text
docs/evidence/replacement_fc/fc_uart_probe.log
```

通过标准:

```text
UART3 MSP LINK ALIVE
或能看到以 24 4D 3E 开头的 MSP 回包
```

## 6. ESP32 FC Diag

只有 probe 通过后使用。

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-fc-diag -t upload --upload-port COM55
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe device monitor --port COM55 --baud 115200
```

保存为:

```text
docs/evidence/replacement_fc/fc_diag.log
```

通过标准:

```text
FC online
MSP response ok 增加
Receiver/Status/Attitude 等只读数据能更新
```

## 7. 默认 Web 固件

默认演示固件:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-unified-web -t upload --upload-port COM55
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe device monitor --port COM55 --baud 115200
```

保存:

```text
docs/evidence/replacement_fc/unified_web_page.png
docs/evidence/replacement_fc/unified_web_status.json
```

状态 JSON 判读:

```text
默认 esp32-s3-unified-web 不编译真实 FC 输出。
因此默认固件里的 fcOut* 不能当作电机/飞控真实输出证据。
rcRoll/rcPitch/rcYaw/rcThrottle/rcAux1/rcAux2 只有在真实 MSP 在线时才代表 Betaflight 映射后的功能通道。
```

通过标准:

```text
Web 能打开。
ToF/GPS/Camera/模拟飞控状态显示正常。
真实输出未编译或锁定。
不发 MSP_SET_RAW_RC。
```

## 8. FC-ready 后期命令

只有下面全部完成后才允许:

```text
Receiver 六路和方向正确
无桨 Motors 顺序和旋向正确
Failsafe 有效
UART3 MSP 只读证明通过
默认 Web 不影响手动飞行
低空 MC6C 手动方向验收通过
Betaflight MSP Override 由 CH6/AUX2 高位触发
```

仅到这个阶段，才考虑上传:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-unified-web-fc-ready -t upload --upload-port COM55
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe device monitor --port COM55 --baud 115200
```

保存:

```text
docs/evidence/replacement_fc/fc_ready_gate_page.png
docs/evidence/replacement_fc/fc_ready_output_diag.json
docs/evidence/replacement_fc/fc_ready_no_prop_test_video.mp4
```

FC-ready 状态 JSON 判读:

```text
rcRoll/rcPitch/rcYaw/rcThrottle/rcAux1/rcAux2:
  来自 Betaflight MSP_RC, 是经过 Betaflight Channel Map 后的功能顺序。
  顺序是 roll, pitch, yaw, throttle, AUX1, AUX2, 不是 MC6C 原始 CH1-CH6 的物理插针顺序。

fcOutRoll/fcOutPitch/fcOutYaw/fcOutThrottle/fcOutAux1/fcOutAux2:
  是 ESP32 最近一次打包并限幅后的 Betaflight RAW_RC 输出诊断。
  英文判读短语: packed/clamped Betaflight RAW_RC output diagnostics.
  只有 FC-ready 固件、真实 MSP 在线、Betaflight ARM active-box 已解锁、CH6/AUX2 高位、
  Web 方向心跳新鲜、且 Betaflight MSP Override 已配置时，才可作为真实输出验证证据。

正确现象:
  fcOutRoll/fcOutPitch/fcOutYaw 可以跟随 Web 辅助方向变化。
  fcOutThrottle 必须保持 MC6C 实时油门值。
  英文判读短语: fcOutThrottle preserves live MC6C throttle.
  fcOutAux1 必须保持 MC6C 的 ARM/模式通道值。
  fcOutAux2 必须保持 MC6C 的 CH6 许可通道值。
```

通过标准:

```text
仍然无桨。
真实 MSP 在线。
Betaflight ARM active-box 状态显示正确。
CH6 高位才允许输出门槛打开。
Web 心跳停止或 CH6 拉低时输出锁定/归中。
ESP32 只改变 roll/pitch/yaw。
MC6C throttle、AUX1/ARM、AUX2/CH6 保持实时值。
```
