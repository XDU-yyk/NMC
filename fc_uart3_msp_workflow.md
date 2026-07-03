# F4V3S UART3 MSP Migration Workflow For DeepSeek

Date: 2026-07-03

## Goal

Move the ESP32-S3 to F4V3S flight-controller telemetry link from UART6 R6/T6 to UART3 R3/T3, using read-only MSP first.

Do not use ESP32-S3 to directly control motors. The F4V3S remains responsible for stabilization, motor output, arming, failsafe, and RC takeover. ESP32 only reads telemetry first, then may later send strictly limited assist commands after safety gates pass.

## Current Evidence

- The B-CUBE F4V3S PLUS manual marks the upper R6/T6 pads as a CRSF receiver interface.
- The manual says to enable UART6 as Serial RX, not as a general MSP peripheral.
- The same manual indicates GPS uses UART1 and compass/MSP-related setup uses UART3.
- USB-TTL loopback works: sending `24 4D 3C 00 01 01` returns the same bytes when TX/RX are shorted.
- USB-TTL connected to R6/T6 gets no CLI response to `#` and no valid MSP reply, so the failure is on the UART6/R6/T6 side, not the PC, USB-TTL, ESP32, or MSP request bytes.
- Project MSP direction is now corrected: request to FC is `$M<` (`24 4D 3C ...`), FC reply is `$M>` (`24 4D 3E ...`).

## Hardware Wiring

Use the same ESP32 pins if possible. Only move the flight-controller pads from UART6 to UART3.

Final bench wiring:

```text
ESP32 GPIO16 TX  ->  F4V3S R3 / RX3
ESP32 GPIO17 RX  <-  F4V3S T3 / TX3
ESP32 GND        ->  F4V3S GND
```

USB-TTL verification wiring:

```text
USB-TTL TX  ->  F4V3S R3 / RX3
USB-TTL RX  <-  F4V3S T3 / TX3
USB-TTL GND ->  F4V3S GND
USB-TTL VCC not connected
```

Keep R6/T6 for receiver use only:

```text
CRSF receiver TX -> F4V3S R6
CRSF receiver RX -> F4V3S T6
5V/GND as required by the receiver
```

If no receiver is currently installed, leave UART6 unused during MSP bench tests.

## Betaflight Configuration

In Betaflight CLI, first inspect:

```text
serial
resource
```

Required UART3 MSP configuration:

```text
serial 2 1 115200 57600 0 115200
save
```

If UART6 is not used by a receiver during bench tests, remove MSP/other accidental use from UART6:

```text
serial 5 0 115200 57600 0 115200
save
```

If UART6 is used by a CRSF receiver, configure it as Serial RX instead of MSP:

```text
serial 5 64 115200 57600 0 115200
set serialrx_provider = CRSF
save
```

Expected resource lines:

```text
resource SERIAL_TX 3 B10
resource SERIAL_RX 3 B11
```

Do not remap motor resources or arming-related settings for this task.

## Phase 1 - USB-TTL UART3 Proof

1. Disconnect ESP32 from the flight controller UART pads.
2. Connect USB-TTL to R3/T3/GND.
3. Close or disconnect Betaflight Configurator after saving settings.
4. In serial assistant, use:

```text
115200
8N1
no flow control
```

5. ASCII test:

```text
#
```

Expected result:

```text
Entering CLI Mode
#
```

Then send:

```text
exit
```

Wait for reboot.

6. HEX MSP test:

```text
24 4D 3C 00 01 01
```

Expected response:

```text
24 4D 3E ...
```

If UART3 fails here, stop code work and fix flight-controller configuration, wiring, or board-pad identification first.

## Phase 2 - ESP32 Raw Probe

Target files:

```text
include/config.h
src/fc_uart_probe_main.cpp
src/fc_diag_main.cpp
platformio.ini
```

Required code/doc updates:

- Keep `FC_RX_PIN = 17` and `FC_TX_PIN = 16` if ESP32 physical pins are unchanged.
- Update comments from UART6/R6/T6 to UART3/R3/T3.
- Update probe banner and wiring print:

```text
F4 T3/TX3 -> ESP32 GPIO17 RX
F4 R3/RX3 -> ESP32 GPIO16 TX
```

- Keep MSP direction:

```cpp
#define MSP_HEADER_DIR_TO_FC    '<'
#define MSP_HEADER_DIR_FROM_FC  '>'
```

Build and upload:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-fc-uart-probe
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-fc-uart-probe -t upload --upload-port COM55
```

Monitor `COM55` at `115200`.

Expected probe output:

```text
tx=24 4D 3C 00 01 01 rx=24 4D 3E ...
```

Any received frame beginning with `24 4D 3E` proves the UART3 MSP link is alive.

## Phase 3 - ESP32 FC Diagnostic

Build and upload:

```powershell
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-fc-diag
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-fc-diag -t upload --upload-port COM55
```

Expected serial evidence:

```text
[FC] online=1 ...
[MSP] ... rxB>0 ... ok>0 ... err=ok
```

Minimum acceptance:

- `rxB` increases.
- `ok` increases.
- `timeout` does not increase continuously.
- Attitude/status values are plausible.
- No write-control commands are sent.

## Phase 4 - Web MVP Integration

Only after UART3 probe and fc-diag pass:

1. Add FC read-only telemetry to Web MVP.
2. Compile `comm/fc_bridge.cpp` and `comm/msp.cpp` into the Web MVP target if not already included.
3. Poll read-only MSP values only:

```text
MSP_FC_VERSION
MSP_STATUS
MSP_ATTITUDE
MSP_ALTITUDE
MSP_ANALOG
MSP_RC
```

4. Display FC online/offline, attitude, battery, armed flag, and MSP diagnostic counters on the Web panel.
5. Do not enable `MSP_SET_RAW_RC`, arm/disarm, RC override, or assist output in this phase.

## Safety Gates

- No propellers during all bench tests.
- Betaflight must remain responsible for motor output, arming, failsafe, and manual RC takeover.
- ESP32 must not send motor or arming commands in UART3 bring-up.
- Do not start assisted follow until manual hover and RC takeover are verified.
- If R6/T6 is used for CRSF receiver, verify RC receiver still works after moving MSP to UART3.

## DeepSeek Prompt

```text
请先阅读 AGENTS.md、workflow.md、plan.md 和 fc_uart3_msp_workflow.md，然后执行 F4V3S UART3 MSP 迁移任务。

背景：
- 这块 B-CUBE F4V3S PLUS 的上方 R6/T6 根据说明书是接收机口，应该作为 UART6 Serial RX/CRSF 使用，不再强行做 MSP。
- MSP 链路改走 UART3，也就是飞控 R3/T3。
- ESP32 仍使用 GPIO16/GPIO17 作为 Serial2：GPIO16 TX 接飞控 R3/RX3，GPIO17 RX 接飞控 T3/TX3，GND 共地。
- MSP v1 方向已经修正：ESP32/USB-TTL 发给飞控是 `$M<`，HEX 例如 `24 4D 3C 00 01 01`；飞控回包应以 `$M>`，HEX `24 4D 3E ...` 开头。

任务目标：
1. 不改飞控安全架构，不让 ESP32 直接控制电机。
2. 在 Betaflight 中把 UART3 配为 MSP：`serial 2 1 115200 57600 0 115200`。
3. 如果 UART6 没接接收机，取消 UART6 的 MSP/误配置；如果 UART6 接 CRSF 接收机，则将 UART6 配为 Serial RX，并设置 `serialrx_provider = CRSF`。
4. 先用 USB-TTL 在 R3/T3 上验证：
   - ASCII 发 `#` 能进入 CLI；
   - 退出 CLI 重启后，HEX 发 `24 4D 3C 00 01 01` 能收到 `24 4D 3E ...`。
5. 修改项目文件中的飞控通信注释和诊断输出，把 UART6/R6/T6 改成 UART3/R3/T3。若 ESP32 仍用 GPIO16/17，不要改 `FC_RX_PIN=17`、`FC_TX_PIN=16`。
6. 编译并烧录 `esp32-s3-fc-uart-probe` 到 COM55，确认串口输出里 `tx=24 4D 3C ...` 后能收到 `rx=24 4D 3E ...`。
7. 再编译并烧录 `esp32-s3-fc-diag`，确认 `[FC] online=1`，MSP `ok` 增加、`rxB` 增加、timeout 不连续增加。
8. 只有上述通过后，才考虑 Web MVP 只读集成 FC telemetry。不要启用 `MSP_SET_RAW_RC`、arm/disarm、RC override 或任何辅助控制输出。

请严格按阶段执行，每阶段给出：
- 修改了哪些文件；
- 使用了哪些 Betaflight CLI 命令；
- 硬件接线；
- 编译/烧录命令；
- 串口输出证据；
- 是否通过验收。

如果 USB-TTL 在 UART3 上仍无法进入 CLI 或无法收到 MSP 回包，立即停止代码修改，优先排查飞控 UART3 配置、R3/T3 焊盘识别、GND、飞控供电和 Betaflight 资源映射。
```

