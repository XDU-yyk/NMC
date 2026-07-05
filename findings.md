# ToF I2C Findings

## Observed Behavior

- The ToF can initialize and produce valid data:
  - `tof:init=1 valid=1 dist=176`
  - Later samples showed `dist=174`.
- ESP32 Wire intermittently logs:
  - `requestFrom(): i2cRead returned Error -1`
  - `requestFrom(): i2cRead returned Error 263`
- After earlier mitigation, invalid `64347` no longer appears in verified logs, and the driver can keep `valid=1` with stable `dist=118/119`.

## Current Driver State

- I2C bus: `TOF_I2C_PORT = Wire1`.
- I2C speed: `TOF_I2C_FREQ = 10000`.
- Measurement timing budget: `TOF_TIMING_BUDGET_MS = 100`.
- Driver samples every 1000ms.
- Driver rejects `0`, `>=4000`, I2C-status errors, and timeout failures.
- Driver preserves last valid data briefly before marking invalid.

## Library Read Path Notes

- `VL53L1X::read(false)` performs:
  - `readResults()` with a 17-byte `requestFrom()`.
  - calibration/DSS helper operations.
  - `getRangingData()`.
  - `writeReg(SYSTEM__INTERRUPT_CLEAR, 0x01)`.
- `VL53L1X::dataReady()` is inline and reads `GPIO__TIO_HV_STATUS`.
- Calling `dataReady()` before each full read would add another I2C register read; it may help avoid full-frame reads before readiness, but on this module it can also add more opportunities for `requestFrom()` errors.

## Current Analysis

- `esp32-s3-web-mvp` only builds `web_main.cpp`, `web/`, `sensors/tof.cpp`, and `sensors/gps.cpp`.
- Therefore the current Web MVP firmware does not run `TaskManager::taskToF`; there is no second ToF task racing `web_main.cpp`.
- ESP32 Arduino `Wire.cpp` logs `i2cRead returned Error %d` inside `TwoWire::requestFrom()` after `i2cRead(...)` returns an error.
- Pololu `last_status` only tracks `endTransmission()` results. It does not capture `requestFrom()` read errors, so user-visible Wire `-1/263` can happen while `m_vl53l1x.last_status` remains `0`.
- The current driver cannot tell whether a Wire log came from:
  - `dataReady()` / `readReg()`
  - `readResults()` 17-byte read
  - calibration reads
  - other Pololu helper reads

## Next Diagnostic Direction

- Add driver-level counters and explicit phase labels around the public `read(false)` call.
- Avoid immediate recovery on sporadic Wire read errors because Wire errors are not fully reflected in `last_status`.
- Prefer longer sample interval and stable last-good value for Web telemetry.
- Consider reducing the Pololu low-power-auto overhead by allowing initial calibration to complete before regular reads.

## 60s Verification After Data-Ready Gate

- Firmware: `budget=100ms period=200ms i2c=50000Hz`.
- Startup found `0x29` and initialized.
- Result: not acceptable.
- Evidence:
  - `ok` only reached 16 in 60s.
  - `miss` reached 207.
  - Wire `requestFrom()` errors still appeared frequently.
  - ToF occasionally became stale: `valid=0 status=253`.
- Interpretation:
  - `VL53L1X::dataReady()` is not reliable for this module/configuration.
  - Because `dataReady()` itself performs a 1-byte I2C `requestFrom()`, it adds many extra I2C transactions and extra error opportunities.
  - Next attempt should remove `dataReady()` from the regular path and reduce total transaction rate another way.

## 60s Verification After Low-Frequency Full Read

- Firmware: `Wire`, `budget=100ms`, `period=500ms`, `i2c=50000Hz`.
- Result: improved but not solved.
- Evidence:
  - `miss=0`.
  - `ok=81`, `fail=20` in 60s.
  - Wire `requestFrom()` errors still appeared.
  - ToF generally stayed usable.

## 60s Verification After Wire1 / 10kHz / 1Hz

- Firmware: `Wire1`, `budget=100ms`, `period=1000ms`, `i2c=10000Hz`.
- Result: significantly improved but not fully eliminated.
- Evidence:
  - Startup found `0x29`.
  - ToF remained `tof:init=1 valid=1`.
  - `ok=44`, `fail=7` near 51s.
  - Only four visible `requestFrom()` errors appeared in 60s.
- Interpretation:
  - Application-level handling is robust enough to keep ToF online.
  - The remaining error is likely below the Pololu API boundary or caused by system-level interference/load/power.
  - A ToF-only firmware is needed to separate sensor/driver behavior from WiFi/GPS/Web runtime effects.

## ToF-only Isolation Result

- Firmware: `esp32-s3-tof-only`, no WiFi, no GPS, no Web.
- Result: `requestFrom()` errors still occur.
- Evidence:
  - Startup scan saw `0x29`, but also occasional phantom addresses (`0x08`, `0x4B`) during one scan.
  - First init attempt failed after a Wire `Error 263`.
  - Second scan found `0x29` and initialization succeeded.
  - 60s sample still showed `Error -1` and `Error 263`.
  - ToF remained mostly usable: around `ok=36 fail=13` near 53s.
- Conclusion:
  - WiFi/AP/Web/GPS are not the primary cause.
  - The remaining issue is in the VL53L1X module, I2C electrical layer, ESP32-S3 Wire behavior, or Pololu library transaction pattern.
  - Software can make telemetry robust, but cannot prove the bus is clean with this hardware/module combination.

## Hardware-Oriented Interpretation

- Phantom I2C addresses during scan strongly suggest bus-level instability, even if wires are visually correct.
- Common causes still worth checking:
  - Pull-up strength on SDA/SCL.
  - Sensor module voltage regulator / level shifting quality.
  - Breadboard or jumper contact resistance.
  - Long parallel wires near USB/power/noisy lines.
  - Clone VL53L1X module timing compatibility.

## Startup Crash Finding

- After restoring Web MVP, one boot scan found phantom addresses `0x13`, `0x14`, `0x17`, and `0x29`.
- Immediately after a Wire `Error 263`, Pololu initialization crashed with `IntegerDivideByZero`.
- The next reboot scanned only `0x29` and initialized successfully.
- Interpretation:
  - A noisy scan can still include `0x29`, but the bus/register data is not trustworthy.
  - Calling Pololu `init()` after a polluted scan can feed bad calibration values into the library and crash.
- Fix:
  - Treat `0x29` plus any other unexpected scan address as unstable.
  - Retry scan before calling `tof.begin()`.

## Final Web MVP Verification

- Firmware: Web MVP restored after scan guard.
- Result:
  - First scan found `0x29`, `0x61`, `0x64`.
  - Driver treated this as unstable and retried.
  - Second scan found only `0x29`.
  - ToF initialized successfully.
  - No startup crash occurred.
- Sample:
  - `tof:init=1 valid=1 status=0 dist=103`
  - `ok` increased from 2 to 17 in the short sample.
  - `fail` increased slowly from 0 to 4.
- Conclusion:
  - The firmware now survives polluted I2C scans and keeps ToF telemetry usable.
  - Remaining `requestFrom()` messages are not caused by Web/GPS/WiFi because ToF-only firmware reproduced them.
  - Fully eliminating the underlying Wire errors likely requires hardware/module/pull-up changes or a different VL53L1X library/driver, not only application-level scheduling.

## Follow-up After Reset Logs

- User-provided reset logs showed:
  - `tof:init=1 valid=1 dist=176` at first;
  - repeated `Wire.cpp requestFrom(): i2cRead returned Error -1 / 263`;
  - repeated recovery and re-initialization.
- A direct result-frame reader was added to avoid the Pololu `read(false)` path doing extra calibration/DSS reads on every sample.
- In local serial samples, direct 17-byte reads could keep ToF usable with stable distances near `84-96 mm`, even when occasional Wire errors still appeared.
- A single-shot measurement experiment was rejected:
  - it sometimes reduced visible Wire logs;
  - but it produced more invalid/stale periods and recovery on this module.
- Final software stance:
  - keep continuous ranging in the sensor;
  - read only once per second from ESP32;
  - validate byte count and distance before updating telemetry;
  - preserve last good distance during short failures;
  - retry initialization in the Web MVP if ToF was absent during boot.
- Current unresolved hardware/state issue:
  - after repeated flashing/resets, the module sometimes stops ACKing `0x29`;
  - when `0x29` is absent or model-ID read fails, firmware cannot recover the sensor without a true sensor-side power/XSHUT reset.
  - Recommended hardware fix: wire VL53L1X XSHUT to an ESP32 GPIO instead of tying it permanently high, or power-cycle the ToF module when this state occurs.

## Completion Audit Finding

- A software retry-state bug was found after the first mitigation:
  - `ToFSensor::begin()` correctly sets `m_initialized=false` when re-init fails.
  - `web_main.cpp` previously only updated `g_tofInit` inside the retry branch, so an in-run recovery failure could leave `g_tofInit=true` while the driver was no longer initialized.
  - Fix: synchronize `g_tofInit = tof.isInitialized()` at the start of each loop.
- Added `lastFault` to ToF telemetry so the serial line can distinguish failure phases.
- After rebuild/upload, the current serial evidence is:
  - repeated background retry attempts are running;
  - `[SYS]` includes `fault=init_model`;
  - Arduino Wire logs `requestFrom(): i2cRead returned Error -1` during VL53L1X `init()`.
- Interpretation:
  - The active problem is no longer just the normal measurement read path.
  - The VL53L1X is failing model/register reads during initialization, even after software bus recovery and repeated ACK probes.
  - If the sensor is in this state, ESP32-side I2C recovery can retry and report it, but cannot guarantee a true sensor reset unless XSHUT or sensor power is controllable.
- Next meaningful gate:
  - Connect VL53L1X `XSHUT` to a safe ESP32 GPIO and define `TOF_XSHUT_PIN`, or power-cycle the ToF module before retesting.
  - Once XSHUT is wired, verify whether a firmware-controlled low/high reset clears `fault=init_model` without unplugging power.

## Pre-init Model Check Result

- A pre-init model-ID check was added before Pololu `init()`:
  - expected model ID: `0xEACC`;
  - live sample after upload: `id=0xFFFF i2c=2`.
- Meaning:
  - `0xFFFF` is an all-ones readback and is consistent with a failed I2C register read or a device that is not actively driving SDA during the read phase.
  - `i2c=2` comes from the write/address phase status recorded by the library.
  - This confirms the current fault occurs before normal ranging starts.
- Consequence:
  - Changing the 1Hz result-frame read loop cannot solve this current state.
  - The next useful experiment is to make VL53L1X reset controllable with XSHUT, then verify whether the model-ID read returns `0xEACC` after firmware-controlled reset.

## Checked Model Read Result

- The pre-init model check was changed from Pololu `readReg16Bit()` to a local checked two-byte register read.
- Reason:
  - Pololu read helpers continue into `requestFrom()` even when the preceding address/write phase has already failed.
  - The local helper returns immediately on write-phase failure and only reads when it has a reasonable chance to succeed.
- Verification:
  - short sample after upload showed only one visible Wire `requestFrom()` error in about 30s;
  - model check consistently failed with `id=0x0000 i2c=2` or `i2c=5`;
  - serial state remained `fault=init_model`.
- Interpretation:
  - avoidable `requestFrom()` noise was reduced;
  - the underlying sensor/register-read failure remains;
  - expected next proof point is still `id=0xEACC` after a real sensor reset through XSHUT or power cycling.

## ToF-only Retest Workflow

- The ToF-only diagnostic firmware now prints the same fault labels as Web MVP.
- Use it after wiring XSHUT or after a full ToF power cycle to isolate sensor/I2C behavior from WiFi, GPS, and Web.
- Build command:
  - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-tof-only`
- Upload command when needed:
  - `C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-tof-only -t upload --upload-port COM55`
- Passing evidence should include:
  - `ToF: initialized`;
  - `[TOF_ONLY] ... init=1 valid=1 status=0 dist=<realistic> fault=none`;
  - no repeated `fault=init_model` after reset.

## 2026-06-27 Final Software Hardening Result

- The configured I2C scanner was fixed to include `<Wire.h>` so `TOF_I2C_PORT = Wire1` is visible in the isolated scanner target.
- Added a stricter init gate:
  - `probeCleanBus()` now requires repeated stable ACKs from `0x29`;
  - `configureSensor()` retries the VL53L1X model-ID read several times;
  - after early model-read failures it attempts a guarded `SOFT_RESET` write before retrying.
- Build verification passed for:
  - `esp32-s3-web-mvp`;
  - `esp32-s3-tof-only`;
  - `esp32-s3-i2c-scan`.
- Web MVP with the latest hardening was uploaded to `COM55`.
- Live serial sample after upload:
  - repeated `VL53L1X address 0x29 did not ACK stably before init`;
  - `[SYS] ... tof:init=0 ... fault=probe_ack`;
  - occasional model reads reached the read phase but returned bad IDs such as `0x001F`, `0x1FFF`, and `0x3FFF`, not the expected `0xEACC`.
- Interpretation:
  - this is no longer a Web refresh, telemetry sampling, or normal ranging-frame problem;
  - the ESP32 is not seeing a stable VL53L1X register interface at startup;
  - guarded software reset did not recover this state because the sensor must first respond reliably enough to accept the reset write.
- Practical conclusion:
  - firmware now fails safely and reports the phase accurately;
  - the next required fix is sensor-side reset/power/electrical validation: wire `XSHUT` to an ESP32 GPIO and set `TOF_XSHUT_PIN`, power-cycle the ToF module, replace the module, or verify pull-ups/power with instruments.

## 2026-06-27 Standalone I2C Scanner Result

- The standalone scanner was enhanced to print:
  - SDA/SCL idle levels before and after bus recovery;
  - all detected I2C addresses;
  - repeated `0x29` ACK success/failure counts;
  - 20 repeated VL53L1X model-ID reads before and after a guarded soft-reset attempt.
- Firmware `esp32-s3-i2c-scan` was uploaded to `COM55` and sampled for about 75 seconds.
- Pure scanner evidence:
  - first cycle: `idle[before]: SDA=1 SCL=1`, then `idle[after-recover]: SDA=0 SCL=1`;
  - scanner found dozens of phantom addresses such as `0x08`, `0x0E`, `0x1F`, `0x52`, `0x73`;
  - `0x29 ACK: 10/30 err2=18 err5=2`;
  - second cycle again found dozens of phantom addresses, including `0x29`;
  - model-ID reads before reset: `ok=0/20 expected=0 write_fail=18 short_read=2`;
  - guarded soft reset could not be issued: `write 0x0000 failed: i2c=2`;
  - model-ID reads after reset attempt remained `ok=0/20 expected=0 write_fail=18 short_read=2`.
- Interpretation:
  - this proves the issue exists even in a firmware with no WiFi, GPS, Web server, WebSocket, or VL53L1X library init sequence;
  - `SDA=0` after recovery plus many phantom addresses are strong evidence of bus/module electrical trouble or a sensor holding SDA low;
  - `0x29` ACKing only one third of the time means a software driver cannot reliably reset or configure the device;
  - because the soft-reset register write itself fails, ESP32 firmware cannot recover this state without controlling sensor-side power/XSHUT.
- Required next hardware gate:
  - power-cycle the ToF module independently or disconnect/reconnect its VCC;
  - wire VL53L1X `XSHUT` to a safe ESP32 GPIO and set `TOF_XSHUT_PIN`;
  - verify SDA/SCL pull-ups and power with a meter/logic analyzer if available;
  - if the same scanner output persists after a real sensor reset, replace the VL53L1X module.

## 2026-06-28 GPIO10 XSHUT Verification

- User wired VL53L1X `XSHUT` to ESP32-S3 GPIO10.
- `include/config.h` was updated:
  - `TOF_XSHUT_PIN` changed from `-1` to `10`.
- Important pin note:
  - GPIO10 is also configured as `CAM_PIN_Y4` for the future OV2640 parallel camera path;
  - this does not affect the current `esp32-s3-web-mvp` target because the camera module is not compiled there;
  - if OV2640 is enabled later, move XSHUT to another safe GPIO or revise the camera pin map.
- Build verification passed for:
  - `esp32-s3-web-mvp`;
  - `esp32-s3-tof-only`.
- Web MVP was uploaded to `COM55`.
- 70s Web MVP serial sample after XSHUT wiring:
  - repeated healthy lines: `tof:init=1 valid=1 status=0 dist=61-64 ... fault=none`;
  - valid reads increased from `ok=5` to `ok=58`;
  - recoveries stayed `rec=0`;
  - one visible Wire `requestFrom(): i2cRead returned Error 263` still appeared;
  - occasional transient `fault=read_reg` and `fail` increments occurred, but cached valid data recovered and ToF remained initialized.
- Interpretation:
  - the prior `probe_ack/init_model` startup failure was caused by a sensor-side state that required real XSHUT reset;
  - XSHUT restored stable initialization and real distance reporting;
  - the remaining occasional Wire read error is now handled as a transient read failure rather than a module-killing init failure.
- Current acceptance:
  - Web MVP ToF is usable for bench telemetry;
  - keep the conservative 1Hz read rate, 10kHz I2C, cached-valid behavior, and fault counters for debugging.

## 2026-06-28 Stability Recheck

- A later 90s Web MVP serial sample was captured without reflashing.
- Evidence:
  - every `[SYS]` line kept `tof:init=1 valid=1 status=0`;
  - distance stayed realistic and stable around `61-64 mm`;
  - recoveries stayed `rec=0`;
  - valid reads increased from `ok=125` to `ok=193`;
  - visible Wire errors still appeared occasionally: one `Error 263` and one `Error -1`;
  - transient `fault=read_reg` appeared but cleared back to `fault=none`.
- Conclusion:
  - the original failure mode is solved: ToF no longer gets stuck at `probe_ack/init_model` after ESP32 reset;
  - remaining Wire errors are tolerated read-level transients, not fatal initialization failures;
  - current firmware behavior is acceptable for the Web MVP bench/debug stage.

## 2026-06-28 Serial Monitor Split-Line Finding

- User screenshot showed one `[SYS]` log displayed as two serial monitor entries:
  - first entry was exactly `64 Bytes` and ended mid-field at `... status=0 dis`;
  - second entry continued with `t=26 err=0 age=847 ...`.
- Code audit:
  - `src/web_main.cpp` prints the whole `[SYS]` line with one `Serial.printf(...)` call;
  - there is no second `[SYS]` print for the ToF distance suffix.
- Interpretation:
  - this is a serial monitor / USB CDC packet display split, not the ESP32 firmware sending two separate logical messages;
  - many serial tools display received chunks as they arrive, and a long line can be shown in two entries even when the firmware used one print call;
  - the same explanation can make Arduino Wire's `requestFrom(): i2cRead returned Error ` appear without the trailing numeric code if the UI split/hid the next chunk.
- Action:
  - no firmware logic change is needed for the split-line display;
  - if cleaner viewing is needed, use a line-buffering serial monitor such as PlatformIO `device monitor`, or copy/export raw serial text instead of relying on chunk cards.

## 2026-06-28 Split-Line And Residual Wire Error Explanation

- Why most serial output is normal but a small part is split:
  - USB CDC / serial delivery is stream-based, not message-based;
  - the host can receive one long `Serial.printf()` line in several chunks;
  - the observed split at exactly `64 Bytes` matches a common USB full-speed packet boundary;
  - the monitor sometimes merges chunks before display, and sometimes shows chunks separately depending on timing.
- Can split-line display be fully solved in firmware:
  - not reliably, because the host monitor decides how to display received chunks;
  - it can be made less visible by shortening `[SYS]` lines to under about 64 bytes, or by using a line-buffered monitor.
- Why occasional `Error -1` / `Error 263` remains:
  - after GPIO10 XSHUT, the fatal `probe_ack/init_model` startup failure is fixed;
  - remaining errors happen during normal result-frame reads and are handled as transient read failures;
  - current evidence shows they do not force recovery or invalidate the cached telemetry.
- Can `Error -1` / `Error 263` be fully eliminated:
  - software can reduce impact and frequency, but cannot guarantee zero on a marginal I2C physical layer;
  - true elimination requires hardware cleanup: shorter SDA/SCL wires, stronger/known pull-ups, stable 3.3V with local decoupling, common ground, less noise, or a better VL53L1X module.

## 2026-06-30 F4V3S UART6 MSP Probe

- Historical note: this was the early UART6/R6/T6 probe path before the manual check. It is superseded by the 2026-07-03 UART3 migration finding below.
- Historical assumption at that time: try F4V3S `R6/T6` (UART6), mapped to ESP32-S3 `GPIO16/17`.
- Important protocol correction:
  - MSP is request/response; the flight controller normally does not stream MSP frames from TX unless something sends a valid `$M<` request into RX.
  - Therefore, "USB-TTL only listens to T6 and sees nothing" does not prove UART6 is dead.
- Code audit found an actual workspace inconsistency:
  - `src/fc_diag_main.cpp` printed `diag.rxBytes`, but `src/comm/msp.h` had the `rxBytes` declaration stuck onto a mojibake comment line, effectively commented out.
  - Rewrote `src/comm/msp.h` and `src/comm/msp.cpp` as clean ASCII while preserving the existing MSP public interface.
- MSP diagnostic improvements now include:
  - `txFrames`, `txBytes`;
  - `rxBytes`, `rxDollarCount`, `rxHeaderCount`, `lastRxByte`;
  - per-request error classification using RX counters before/after the request rather than stale cumulative booleans.
- Added `src/fc_uart_probe_main.cpp` and PlatformIO env `esp32-s3-fc-uart-probe`.
- Probe firmware sends read-only MSP v1 request frames and dumps raw RX bytes. Current request direction is `$M<`, so `FC_VERSION` is `24 4D 3C 00 01 01`; valid replies should start with `$M>` / `24 4D 3E`.
- Build verification passed:
  - `esp32-s3-fc-diag`;
  - `esp32-s3-fc-uart-probe`.
- Uploaded `esp32-s3-fc-uart-probe` to ESP32 on `COM55`.
- Live COM55 sample after upload:
  - ESP32 repeatedly printed MSP request bytes on the selected Serial2 pins;
  - RX was still `<none>` in the short local sample.
- Interpretation:
  - ESP32 firmware was transmitting on Serial2 `TX=GPIO16`;
  - the later manual and bench evidence superseded this UART6 path;
  - do not continue R6/T6 MSP work unless new documentation proves that pad pair is usable for MSP.

## 2026-07-03 UART3 MSP Migration Finding

- The F4V3S PLUS scanned manual identifies the upper R6/T6 pads as a CRSF receiver interface.
- The same manual says to turn on UART6 as Serial RX, not as the preferred MSP peripheral.
- The bottom connector area identifies GPS as UART1 and indicates UART3 for MSP/compass-related setup.
- User verified USB-TTL loopback: `24 4D 3C 00 01 01` is transmitted and received correctly when TX/RX are shorted.
- User verified R6/T6 does not reply to ASCII `#` and does not reply to MSP request bytes.
- Current conclusion: move MSP telemetry to UART3 R3/T3. Leave UART6 R6/T6 for receiver Serial RX/CRSF or unused bench state.
- Current project correction supersedes earlier notes that showed `$M>` as the request direction. Correct MSP v1 bring-up expectation is:
  - request to FC: `$M<`, example `24 4D 3C 00 01 01`;
  - reply from FC: `$M>`, expected prefix `24 4D 3E`.
- Handoff workflow for DeepSeek is now in `fc_uart3_msp_workflow.md`.

## 2026-07-03 Camera No-Image Regression

- User reported that after the self-healing stream change, the camera page no longer shows an image and only shows the `capture.jpg` link.
- Current code inspection found the root page renders `<img id='f'>` with no `src`; the stream only starts if the inline JavaScript runs and assigns `/stream?...`.
- This creates a fragile startup path: if the browser delays, blocks, or fails the inline script, the image remains blank while the `capture.jpg` link still appears.
- Fix direction: make `/stream` the direct `<img>` `src` fallback, keep JavaScript only for periodic reconnect, and close non-stream HTTP responses so the browser receives a complete page.
- Implemented fix: root page now renders `<img id='f' src='/stream'>`; reconnect JavaScript is ES5-compatible and only refreshes the stream periodically; non-stream responses call `c.stop()` after sending.
- Follow-up fix: root HTML and `capture.jpg` now send no-store/no-cache headers so phones should stop reusing the previous cached page that had no image source.
- Serial verification after upload showed the camera firmware boots, reports PID `0x0026`, completes warm-up with a JPEG frame, and logs `err=0`.
- Final robustness fix: make `/capture.jpg` polling the primary visible image path. The page now renders `<img id='f' src='/capture.jpg?boot=1'>` and refreshes it with cache-busting timestamps; `/stream` remains only as an optional link. This avoids depending on browser MJPEG support for the first visible image.
- Serial verification after the final upload showed background capture continued after boot (`f=10` then `f=24`) with `err=0` and a valid cached JPEG length.

## 2026-07-03 Camera 30s Freeze Follow-up

- User reported that the camera page became smooth for about 30 seconds and then completely froze.
- Code audit found three plausible freeze amplifiers in `src/ov5640_diag_main.cpp`:
  - the page forced `img.src` every 250ms regardless of whether the previous `/capture.jpg` request had finished;
  - each HTTP request used Arduino `String` objects for the request line and headers, creating avoidable heap churn at image-refresh rates;
  - JPEG cache length was also used as allocation capacity, so normal JPEG size fluctuation could trigger repeated `realloc()` calls.
- Implemented mitigation:
  - browser refresh is now one-in-flight: next `/capture.jpg` request is scheduled only after `onload`, `onerror`, or a 1500ms guard timeout;
  - status polling is limited to one in-flight XHR and runs every 2 seconds;
  - HTTP request parsing now uses fixed `char` buffers;
  - JPEG cache now has separate `g_cacheCap`, so the buffer expands only when a larger frame appears;
  - JPEG write timeout is shorter for disconnected clients, and timeout counters are exposed;
  - `/stream` diagnostic max lifetime was raised from 20s to 60s so manually opening it no longer dies around the reported time window.
- Added `/status` and serial fields:
  - `http`, `jpg`, `stat`, `timeouts`, `bad`, `age`, and cache `cap`.
- Build and upload of `esp32-s3-ov5640-diag` succeeded on `COM55`.
- A 70s no-browser serial sample after upload showed stable runtime:
  - `heap=243620`;
  - `err=0`;
  - `cache=2521 cap=2521`;
  - `http=0 jpg=0 stat=0 to=0 bad=0`;
  - frame counter advanced from `f=22` to `f=78`.
- After the final `/status` field addition, a second upload succeeded and a 35s serial confirmation showed:
  - frame counter advanced from `f=6` to `f=34`;
  - `heap=243372`;
  - `cache=2588 cap=2588`;
  - `age=0`;
  - early `err=2` did not increase during the sample.
- Completion audit evidence on the PC side:
  - current Windows network profile is `stu-xdwlan`, not `NMC-Camera`;
  - `Test-NetConnection 192.168.4.1 -Port 80` timed out;
  - `ping -n 1 -w 1000 192.168.4.1` timed out;
  - a non-invasive `netsh wlan show networks mode=bssid` scan only listed `stu-xdwlan`, not `NMC-Camera`.
- Added AP observability to distinguish AP failure from browser/image failure:
  - `/status` now includes `ap`, `stations`, `ip`, and `channel`;
  - periodic serial logs now include `ap`, `sta`, `ip`, and `ch`.
- AP diagnostic firmware on channel 6 built and uploaded successfully.
- 45s serial evidence on channel 6:
  - `ap=ok sta=0 ip=192.168.4.1 ch=6`;
  - `err=0`;
  - frame counter advanced from `f=21` to `f=53`.
- A simultaneous Windows WLAN scan still did not list `NMC-Camera`, only 5GHz campus networks.
- To test a common 2.4GHz compatibility path, the AP channel was changed from 6 to 1, rebuilt, and uploaded.
- 45s serial evidence on channel 1:
  - `ap=ok sta=0 ip=192.168.4.1 ch=1`;
  - `err=0`;
  - frame counter advanced from `f=16` to `f=35`.
- A second Windows WLAN scan after the channel-1 upload still did not list `NMC-Camera`.
- Interpretation:
  - current firmware reports the AP is started and the camera loop is alive;
  - this PC cannot be used as the AP/browser verifier in its current WiFi state;
  - the remaining requirement needs a phone or other client that can see/connect to `NMC-Camera`, or a change in local WiFi hardware/state.
- User-provided phone `/status` after connecting to the AP:
  - `valid=true`, `camera=true`, `ap=true`, `stations=1`;
  - `cache=2571`, `cap=2571`;
  - `http=16`, `jpg=3`, `stat=4`;
  - `timeouts=3`, `bad=6`;
  - `age=14`.
- Interpretation of that phone evidence:
  - AP, HTTP `/status`, camera init, and frame cache are all alive;
  - the failure is now narrowed to `/capture.jpg` response delivery, because every observed JPEG request coincided with a write timeout;
  - `bad=6` also suggests the firmware's 120ms request-line timeout was too aggressive for some phone/browser connections.
- Mitigation after the phone evidence:
  - increased request-line timeout from 120ms to 400ms;
  - increased cached JPEG write timeout from 180ms to 1200ms;
  - reduced write chunks from 1024 bytes to 512 bytes;
  - added `flush()` after cached JPEG writes.
- Rebuilt and uploaded the JPEG-timeout mitigation firmware to `COM55`.
- 40s serial evidence after upload:
  - `ap=ok sta=0 ip=192.168.4.1 ch=1`;
  - `err=0`;
  - `cache=2566 cap=2573`;
  - frame counter advanced from `f=7` to `f=47`.
- Follow-up audit found the key writer bug was still present:
  - `writeClient()` still gated every write on `WiFiClient::availableForWrite()`;
  - if that API reports `0` on this stack/client state, the function never attempts `c.write()` and every JPEG request becomes a timeout;
  - this matches the phone evidence where `jpg` and `timeouts` increased together.
- Final direct-write fix:
  - removed `availableForWrite()` gating from `writeClient()`;
  - the writer now attempts `c.write()` directly in 512-byte chunks and only times out when writes return `0` without progress.
- Rebuilt and uploaded the direct-write firmware to `COM55`.
- 35s serial evidence after upload:
  - `ap=ok sta=0 ip=192.168.4.1 ch=1`;
  - `err=0`;
  - `cache=2589 cap=2589`;
  - frame counter advanced from `f=12` to `f=51`.
- Remaining proof needed is phone-side or AP-connected-client testing, because the PC is not currently able to reach the `NMC-Camera` AP for direct HTTP verification.
- User later reported repeated refreshes showing an identical `/status` body where `jpg=26` and `timeouts=9` did not change.
- Interpretation of the unchanged body:
  - after the direct-write fix, `jpg` no longer equaled `timeouts`, so JPEG delivery likely improved compared with the previous all-timeout evidence;
  - if every field stays identical across repeated refreshes, the immediate suspect is a stale cached `/status` response or a client not actually reaching the ESP32;
  - current source now includes a monotonic `/status` field `seq`, `Content-Length`, and strong no-cache headers, so the next phone test can prove whether each refresh is hitting live firmware.
- Current source audit before the next patch:
  - the root page still has a direct `<img id='f' src='/capture.jpg?boot=1'>`;
  - the remaining root-page response is emitted as one large `c.printf(...)` without `Content-Length`;
  - for phone debugging, a stronger root response should include a firmware tag and explicit length so the user can distinguish stale cached HTML from the current firmware.
- Tagged HTML patch:
  - added `FW_TAG = camdiag-20260704-html1`;
  - `/status` now includes `"fw":"camdiag-20260704-html1"` in addition to monotonic `seq`;
  - the root page visibly prints `fw: camdiag-20260704-html1`;
  - the root page is now sent with explicit `Content-Length` through `writeClient()` instead of one large `printf`.
- Verification after the tagged HTML patch:
  - `esp32-s3-ov5640-diag` built successfully;
  - upload to `COM55` succeeded;
  - 45s serial sample after upload showed `ap=ok`, `ip=192.168.4.1`, `ch=1`, `err=0`, valid cache around `2400-2430` bytes, `heap=243712`, and frame `age=0-1`.
- Follow-up local network audit after the tagged HTML upload:
  - Windows WLAN is still connected to `stu-xdwlan` on 5 GHz channel 40;
  - `netsh wlan show networks mode=bssid` still lists only `stu-xdwlan`, not `NMC-Camera`;
  - `ping 192.168.4.1` timed out;
  - a fresh serial sample still showed `ap=ok`, `sta=0`, `http=0`, `jpg=0`, `stat=0`, `to=0`, and `bad=0`.
- Interpretation:
  - the ESP32 camera/AP loop is alive, but this PC still cannot provide the final browser proof;
  - the next authoritative evidence must come from a phone or another client connected to `NMC-Camera`;
  - if the phone page/status does not show `camdiag-20260704-html1`, it is not testing the currently uploaded firmware.
- User-provided live phone evidence after the tagged HTML patch:
  - root page `http://192.168.4.1/?v=html1` can display a camera image;
  - the image is frozen rather than absent;
  - `/status` includes `fw=camdiag-20260704-html1`, proving the phone is testing the current firmware;
  - status shows `valid=true`, `cache=2454`, `jpg=324`, `timeouts=0`, and `bad=0`, so HTML/JPEG delivery is working;
  - status also shows `camera=false`, `errors=12230`, and `age=36716754`, proving the cached frame is stale and camera capture has been offline for a long time.
- Current root cause direction:
  - the bug is no longer the root HTML or `/capture.jpg` transport path;
  - `captureFrame()` currently marks `g_cameraReady=false` on a failed `esp_camera_fb_get()` but does not immediately perform a clean camera-driver restart;
  - the repeated errors over many hours match the retry loop failing to bring the camera driver back after the first capture-side stall.
- Camera recovery implementation result:
  - `camdiag-20260704-recover1` added consecutive capture-failure handling, stale-frame restart handling, and `/status` fields `cfail`, `creq`, and `crec`;
  - the first recover1 serial sample exposed a firmware bug in the stale check: `now` was captured before `captureFrame()`, while `g_lastFrameMs` was updated inside `captureFrame()`, so unsigned subtraction could underflow and trigger repeated false stale restarts;
  - this was fixed by refreshing `now = millis()` after `captureFrame()` before checking stale age.
- Current camera hardware/state evidence after the fixed recovery firmware:
  - after upload/reset, the camera no longer reaches PID detection;
  - repeated init attempts fail with `Camera probe failed with error 0x105(ESP_ERR_NOT_FOUND)`;
  - AP remains alive at `192.168.4.1`, but `f=0`, `cache=0`, and no JPEG frame is available;
  - `recoverSccbBus()` was added before `esp_camera_init()` and uploaded as `camdiag-20260704-recover2`, but a 90s sample still showed repeated `ESP_ERR_NOT_FOUND`.
- Interpretation:
  - ESP32-side HTTP, AP, and recovery code are now observable, but the camera module is currently not responding on SCCB at all;
  - because `CAM_PIN_PWDN` and `CAM_PIN_RESET` are both `-1`, firmware cannot perform a true sensor-side hardware reset;
  - the next gate is a real camera/board power-cycle or wiring/power check, then retest `recover2`.

## 2026-07-04 Camera Recovery Recheck

- After resuming the camera recovery task, a fresh 75s COM55 serial sample still showed repeated:
  - `Camera probe failed with error 0x105(ESP_ERR_NOT_FOUND)`;
  - `CAM INIT FAILED - will keep AP up and retry`;
  - `[CAM] f=0 ... cache=0 ... ap=ok ... ip=192.168.4.1 ch=1`.
- This confirms the `camdiag-20260704-recover2` firmware is alive and retrying, but the camera sensor is not detected at the SCCB probe stage.
- Since `CAM_PIN_PWDN` and `CAM_PIN_RESET` are both `-1`, the ESP32 cannot hard-reset the camera sensor from firmware.
- Do not keep changing the browser/JPEG path for this failure mode. The next meaningful test is a real camera-side power-cycle, reset-pin wiring if available, reseating the module/ribbon/jumpers, or checking camera 3.3V/GND/SIOD/SIOC continuity and stability.
- A subsequent strict continuation audit rebuilt `esp32-s3-ov5640-diag` successfully and captured another 60s serial sample with the same result:
  - `Camera probe failed with error 0x105(ESP_ERR_NOT_FOUND)`;
  - `[CAM] f=0 ... cache=0 ... ap=ok ... ip=192.168.4.1 ch=1`;
  - `err` increased from 66 to 70.
- The source-side page bug is not the active failure anymore: root HTML has a direct `/capture.jpg?boot=1` image, the cached JPEG path was previously proven by phone evidence, and the current failure occurs before any frame is available.
- User hardware checks then showed `SDA/SCL` idle at a little over 3V and no reported short/open on the checked VCC/GND/SDA/SCL lines.
- After that hardware check/reconnection, the camera recovered in serial:
  - `[CAM] f=873 -> 1073`;
  - `fps=4.9`;
  - `err=0`;
  - `cache=2741-3007 cap=3072`;
  - `age=165`;
  - `ap=ok ip=192.168.4.1 ch=1`.
- Interpretation:
  - `ESP_ERR_NOT_FOUND` was consistent with a temporary camera-side connection, reset, power, or latch-up state, not the root HTML or JPEG writer;
  - the firmware recovery path is now useful for surfacing the fault, but a real hardware reconnection/power-state correction was needed to restore SCCB detection;
  - next proof must come from an AP-connected client: `/status` should show `camera=true`, `frames` increasing, `cache>0`, and low `age`, while the root page image visibly updates.
- A later 90s serial sample confirmed the backend stayed healthy without a connected client:
  - `[CAM] f=1773 -> 2173`;
  - `fps=4.9`;
  - `err=0`;
  - `cache=3007 cap=3072`;
  - `age=165`;
  - `sta=0 http=0 jpg=0 stat=0`.
- This proves the camera is currently producing frames, but it does not prove the phone page because no browser request reached the ESP32 during the sample.
- A repeated 75s serial sample showed the same split:
  - backend healthy: `[CAM] f=2573 -> 2873`, `fps=5.0`, `err=0`, `cache=2754-3007 cap=3072`, `age=165`;
  - no client: `sta=0 http=0 jpg=0 stat=0`.
- Therefore the remaining evidence gap is specifically AP-client/browser verification, not camera capture or firmware-side frame caching.
- Windows can now see the ESP32 AP:
  - SSID `NMC-Camera`;
  - signal 99%;
  - WPA2-Personal;
  - 2.4GHz channel 1.
- However, the PC is still connected to `stu-xdwlan`, and a simultaneous serial sample still showed `sta=0 http=0 jpg=0 stat=0`.
- This means the ESP32 AP is visible and the camera backend is healthy, but no client has actually loaded the page/status endpoint yet.
- Switching the PC WiFi to `NMC-Camera` would be a viable local verification path, but it would temporarily disconnect the PC from `stu-xdwlan` and should be done only with user approval.
- User provided phone-side `/status` after connecting to `NMC-Camera`:
  - `fw=camdiag-20260704-recover2`;
  - `stations=1`;
  - `camera=true`;
  - `valid=true`;
  - `frames=169`;
  - `fps=5`;
  - `errors=0`;
  - `cache=3022 cap=3022`;
  - `age=505`;
  - `timeouts=0`.
- This proves AP association, status HTTP delivery, camera detection, and live frame caching.
- It does not yet prove the root-page image path because `jpg=0`; the browser has not successfully requested `/capture.jpg`.
- Next test should directly open `/capture.jpg?manual=1` once, then open the root page and re-check whether `jpg` increases.
- A follow-up 75s serial sample while waiting for `/capture.jpg` still showed no client traffic:
  - backend healthy: `[CAM] f=395 -> 745`, `fps=5.0`, `err=0`, `cache=2980-3040 cap=3050`, `age=175`;
  - no connected AP client or requests: `sta=0 http=0 jpg=0 stat=0`.
- The image-path verification remains pending until a phone/client actually requests `/capture.jpg`.
- A repeated 60s sample again showed the same boundary:
  - source has direct root image source `/capture.jpg?boot=1`;
  - backend healthy: `[CAM] f=1195 -> 1445`, `fps=5.0`, `err=0`, `cache=2565-3055 cap=3056`, `age=175`;
  - no client traffic: `sta=0 http=0 jpg=0 stat=0`.
- At this point the only missing evidence is external: an AP-connected phone/client must request `/capture.jpg` and report `/status` with `jpg>0`.
- Final phone evidence satisfies that missing gate:
  - direct `/capture.jpg?manual=1` displays the camera image;
  - each refresh shows the live current frame;
  - `/status?fresh=jpg1` reports `camera=true`, `valid=true`, `frames=947`, `fps=5`, `errors=0`, `cache=3051`, `stations=1`, `http=29`, `jpg=12`, `stat=3`, `timeouts=0`, `bad=0`, `age=179`.
- Conclusion: the original "only capture.jpg link / no image" failure and the later frozen-frame failure are resolved for the diagnostic camera firmware. Remaining risk is hardware-contact sensitivity; if `ESP_ERR_NOT_FOUND` recurs, repeat the SDA/SCL/VCC/GND and camera module seating checks before changing web code.

## 2026-07-04 Replacement FC Ready Prep

- User requirement: prepare the project so a replacement flight controller can be wired and brought up quickly when it arrives, while the currently damaged FC remains off-limits for motor, arming, RC override, MSP write, or flight work.
- ESC photo / harness status:
  - user later provided `D:\Vitual C\virtual desktop\微信图片_20260704225613_218_125.jpg`;
  - visual inspection confirms the ESC has a transparent heatshrink board, a white/red/black servo-style control plug, independent thick red/black power input, and three thick motor phase leads;
  - treat this ESC as an ordinary PWM/OneShot three-wire servo-plug ESC unless a real label/manual proves a different protocol.
- Wiring conclusion:
  - ESC signal and ground must go to the flight controller motor outputs `M1-M4` / `S1-S4`;
  - ESP32-S3 must not connect to ESC signal wires;
  - if the FC already has a regulated 5V supply, ESC servo-plug red wires should be removed/insulated, or only one BEC red wire should be used after confirming stable 5V with a meter.
- Added FC-ready safety hardening:
  - default `esp32-s3-unified-web` remains simulation/Web only and does not compile in `fc_bridge`;
  - new `esp32-s3-unified-web-fc-ready` environment compiles `fc_bridge` and `msp` only for future replacement-FC use;
  - real MSP RC output now requires `ENABLE_REAL_FC_OUTPUT=1`, FC online, FC armed, MC6C CH6/AUX2 above threshold, and continuous output refresh;
  - `FCBridge` now also checks the assist gate internally before any `MSP_SET_RAW_RC`, so stale output cannot be repeated solely because an old `overrideRC=true` was cached.
- Documentation:
  - created `replacement_fc_ready_workflow.md` with ESC wiring, MC6C receiver/channel plan, UART3 MSP wiring, Betaflight setup, no-prop motor validation order, and default-vs-FC-ready firmware usage.
- Verification:
  - host manual-control test passed: `20 通过, 0 失败`;
  - sequential `esp32-s3-unified-web` build passed;
  - sequential `esp32-s3-unified-web-fc-ready` build passed.

## 2026-07-04 FC-ready RC Gate Visibility

- Gap found after the first FC-ready pass:
  - `esp32-s3-unified-web-fc-ready` polled `fcBridge`, but the Web telemetry still always displayed simulated FC values;
  - this would make replacement-FC bring-up harder because the page could not directly prove MC6C CH1-CH6, real MSP online state, or CH6 assist permission.
- Implemented Web/JSON visibility:
  - telemetry now includes `rcAux1`, `rcAux2`, `rcChannelCount`, `fcAssistSwitch`, `fcAssistGateOpen`, `fcRealOutputCompiled`, and `fcRealOnline`;
  - the Web page now displays CH1-CH6 and a FC-ready gate line: real MSP online/offline, armed yes/no, CH6 permission high/low, output locked/releasable;
  - default `esp32-s3-unified-web` still reports that real output is not compiled.
- Implemented real FC telemetry overlay:
  - in `ENABLE_REAL_FC_OUTPUT` builds, if `fcBridge.isOnline()` is true, the Web telemetry uses real MSP attitude, altitude, battery, armed state, cycle time, and RC channels;
  - if the real FC is offline, the page still has simulation available, while `fcRealOnline=false` and `fcAssistGateOpen=false` make the real-link status explicit.
- Safety hardening:
  - `FCBridge::isAssistGateOpen()` is now a public read-only status used by both output gating and Web telemetry;
  - `FCBridge::arm()` and `FCBridge::disarm()` are default no-ops unless `ENABLE_ESP32_ARM_DISARM=1`, preserving legacy compile compatibility while keeping MC6C/Betaflight as the arming authority.
- MSP status parsing correction:
  - `MSP::readStatus()` now derives armed state from `MSP_STATUS` mode flags bit 0 instead of the old payload index 12 shortcut, which was likely wrong for Betaflight MSP_STATUS.
- Verification:
  - host manual-control test passed again: `20 通过, 0 失败`;
  - sequential `esp32-s3-unified-web` build passed;
  - sequential `esp32-s3-unified-web-fc-ready` build passed;
  - sequential `esp32-s3-fc-diag` build passed;
  - sequential `esp32-s3-fc-uart-probe` build passed.

## 2026-07-04 MC6C Receiver Readiness Check

- Gap found:
  - the Web page could show CH1-CH6 values, but it still required the user to interpret raw numbers;
  - replacement-FC bring-up needs an explicit bench-readiness result before continuing to no-prop motor checks.
- Added conservative receiver sanity thresholds in `include/config.h`:
  - center range: `MC6C_RC_CENTER_MIN=1400`, `MC6C_RC_CENTER_MAX=1600`;
  - low threshold: `MC6C_RC_LOW_MAX=1200`;
  - high threshold: `MC6C_RC_HIGH_MIN=1700`.
- Added Web/JSON receiver checks:
  - `rxHasSixChannels`;
  - `rxCenterOk` for roll/pitch/yaw center;
  - `rxThrottleLow`;
  - `rxAux1Valid`;
  - `rxAux2Valid`;
  - `rxBenchReady`.
- Behavior:
  - after the later 2026-07-05 tightening, `rxBenchReady=true` only if real MSP is online and the receiver has 6 channels, roll/pitch/yaw are centered, throttle is low, and CH5/CH6 are both low;
  - this is only a permission to continue no-prop bench checks, not a permission to mount props or fly.
- Verification:
  - host manual-control test passed: `20 通过, 0 失败`;
  - `esp32-s3-unified-web` build passed;
  - `esp32-s3-unified-web-fc-ready` build passed;
  - `esp32-s3-fc-diag` build passed;
  - `esp32-s3-fc-uart-probe` build passed.

## 2026-07-04 MC6C Direction Self-Test Wizard

- Gap found:
  - receiver readiness can prove center/throttle/AUX sanity, but it cannot prove stick direction;
  - the replacement-FC workflow needs an explicit way to catch reversed AIL/ELE/RUD or non-moving AUX channels before motor checks.
- Added Web-only MC6C direction self-test:
  - requires `fcRealOnline=true`;
  - records a baseline with roll/pitch/yaw centered, throttle low, CH5/CH6 low;
  - then records AIL right, ELE forward, THR high, RUD right, CH5 high, and CH6 high;
  - compares each step against the baseline and reports pass/fail with measured channel values and deltas.
- Safety behavior:
  - the wizard does not write to the FC and does not unlock output;
  - failed direction checks instruct the user to fix Betaflight Receiver/Channel Map or MC6C servo reverse switches, not ESP32 code.

## 2026-07-04 Betaflight MSP RC Order Correction

- Safety gap found during completion audit:
  - the FC-ready bridge and real-FC telemetry treated the first four MSP RC channels as roll, pitch, throttle, yaw;
  - Betaflight's internal RC order after Channel Map is roll, pitch, yaw, throttle, then AUX channels;
  - if left unfixed, a future `MSP_SET_RAW_RC` assist output could put throttle in the yaw slot and yaw in the throttle slot.
- Primary-source check:
  - Betaflight `rc_controls.h` defines `ROLL=0`, `PITCH=1`, `YAW=2`, `THROTTLE=3`, `AUX1=4`, `AUX2=5`;
  - Betaflight `msp.c` handles `MSP_SET_RAW_RC` by reading the payload into a channel frame and passing it to the MSP receiver path.
- Implemented correction:
  - `FCBridge::sendRawRC()` now builds payload order as roll, pitch, yaw, throttle, AUX1, AUX2;
  - unused AUX slots sent in the 8-channel MSP payload are initialized to neutral 1500 instead of zero;
  - `fillTelemetry()` now reads real MSP RC data using the same Betaflight order;
  - the Web page now labels real RC values by function name instead of saying `CH3 throttle / CH4 yaw`, avoiding confusion with MC6C physical channel labels;
  - `replacement_fc_ready_workflow.md` now notes the difference between MC6C physical channel naming and Betaflight MSP internal order.
- Verification:
  - host manual-control test passed: `20 通过, 0 失败`;
  - `esp32-s3-unified-web` build passed;
  - `esp32-s3-unified-web-fc-ready` build passed;
  - `esp32-s3-fc-diag` build passed;
  - `esp32-s3-fc-uart-probe` build passed.

## 2026-07-04 Betaflight MSP Override Gate

- Gap found:
  - the FC-ready workflow said UART3 MSP plus CH6 high was required, but did not explicitly state that Betaflight must be configured to use MSP Override;
  - Betaflight can receive `MSP_SET_RAW_RC` frames without necessarily using them as an override source for an existing receiver.
- Primary-source check:
  - Betaflight `rx/msp.c` records incoming MSP RC frames and tracks whether each channel is fresh; the source comments note a 300 ms freshness window;
  - Betaflight `rx/msp_override.c` uses the MSP value only when `BOXMSPOVERRIDE` is active, the channel is included in `msp_override_channels_mask`, and the MSP channel is fresh.
- Implemented correction:
  - `replacement_fc_ready_workflow.md` now requires MC6C UAV mode, V-TAIL/ELEVON mixing off, and Receiver/model-preview based direction checks;
  - the workflow now adds a dedicated MSP Override section: configure Betaflight MSP Override on AUX2/CH6 high, do not cover ARM, and verify it only with props removed;
  - `include/config.h` now documents MSP Override as a required precondition before enabling `ENABLE_REAL_FC_OUTPUT`;
  - the Web button text was changed from `手柄接管` to `暂停网页控制`, because it only stops Web-originated direction output and is not the physical MC6C takeover switch.

## 2026-07-04 FC-ready Output Diagnostics

- Gap found:
  - the FC-ready build could show MC6C receiver readiness and the CH6/AUX2 permission gate, but it still could not explain why a real output test did or did not reach `MSP_SET_RAW_RC`;
  - replacement-FC bench bring-up needs to distinguish "the browser never requested output", "the bridge blocked output", "the command went stale", "MSP write failed", and "the FC/MSP link is not replying".
- Implemented Web/JSON observability:
  - `FCBridge` output diagnostics are now copied into unified telemetry: set/override/clear counts, raw RC attempts, send OK/fail counts, gate blocks, stale blocks, last output age, last RC values, and last reason;
  - MSP diagnostics are now copied into unified telemetry: TX frames, TX bytes, RX bytes, timeout count, and last MSP error string;
  - the Web FC-ready gate card now shows compact Chinese output/MSP diagnostic lines under the existing gate status.
  - `replacement_fc_ready_workflow.md` now explains how to interpret gate blocks, stale blocks, send failures, and MSP timeouts during no-prop replacement-FC bring-up.
- Safety behavior:
  - this change does not enable real output in the default firmware;
  - FC-ready output remains compiled only in `esp32-s3-unified-web-fc-ready` and remains gated by FC online, armed state, MC6C CH6/AUX2 high, fresh direction heartbeat, and Betaflight MSP Override configuration.

## 2026-07-04 FCBridge-Owned Output Gate

- Follow-up gap:
  - `unified_web_main.cpp` still checked the real-output gate before calling `FCBridge::setOutput()`;
  - when the gate was closed, the bridge never saw the requested direction output, so `gateBlocks` could stay flat even though the Web page was actively requesting motion.
- Correction:
  - the Web loop now queues non-neutral direction requests into `FCBridge` in FC-ready builds;
  - `FCBridge::update()` remains the only 50Hz path that can send or block `MSP_SET_RAW_RC`;
  - if FC online/armed/CH6 is not satisfied, the output is counted as a gate block instead of being invisible.
- Safety and RC authority:
  - this does not loosen the real-output gate;
  - FC-ready output still requires compile-time `ENABLE_REAL_FC_OUTPUT=1`, FC online, FC armed, MC6C CH6/AUX2 high, fresh Web command, and Betaflight MSP Override;
  - the queued real output preserves the current MC6C throttle, AUX1, and AUX2 values, so ESP32 assist only modifies roll/pitch/yaw intent and does not stomp throttle, ARM, or mode channels.

## 2026-07-05 FC-ready MSP Poll Responsiveness

- Bring-up risk found:
  - `FCBridge::update()` could attempt several MSP reads in one loop iteration;
  - if the replacement FC UART is unplugged, reversed, unconfigured, or unpowered, each read can wait for `FC_MSP_TIMEOUT_MS`;
  - during that common bench-fault state, the unified Web/camera/ToF loop could become sluggish even though the fault is simply "FC not replying".
- Correction:
  - when `fcBridge.isOnline()` is false, the bridge now probes only `MSP_STATUS` at a low rate;
  - when the FC is online, each `update()` reads at most one due data block, with status and RC input prioritized over attitude/battery reads;
  - output gating still runs after the poll step, so closed-gate Web direction attempts remain visible as `gateBlocks`.
- Expected replacement-FC behavior:
  - with UART3 wrong or FC offline, the Web page should continue responding and MSP diagnostics should show timeouts/silence instead of making the whole dashboard look frozen;
  - once MSP replies, normal status/RC/attitude polling resumes and the MC6C receiver checks become meaningful.

## 2026-07-05 FC-ready Host-Tested RC Helpers

- Test gap found:
  - the Betaflight RAW_RC slot order and CH6 assist gate are safety-critical, but the implementation was mostly protected by comments and target builds;
  - a future edit could silently swap yaw/throttle again without a host-side test catching it.
- Correction:
  - added `src/comm/fc_rc_mapping.h`, an Arduino-free helper for Betaflight RAW_RC packing and assist-gate evaluation;
  - `FCBridge::sendRawRC()` now uses the shared helper to pack roll, pitch, yaw, throttle, AUX1, and AUX2;
  - `FCBridge::isAssistGateOpen()` now uses the same helper for online + armed + channel-count + CH6-threshold checks.
- Verification intent:
  - `test/test_fc_ready_logic.cpp` locks the RAW_RC slot order and CH6 gate behavior on the host;
  - this gives a fast software proof for the most dangerous part of FC-ready output while the real replacement FC is still unavailable.

## 2026-07-05 MC6C ELE Direction Self-Test Adjustment

- Bring-up risk found:
  - the Web direction wizard originally treated every axis as having one expected PWM sign;
  - on MC6C plus Betaflight Channel Map, ELE forward can legitimately appear as either a numeric increase or decrease depending on receiver mapping and reverse settings;
  - hard-failing ELE only because the number moved in the opposite direction could push the user to reverse a correct pitch channel.
- Correction:
  - ELE forward now checks for significant CH2 movement only;
  - the Web result still reports the measured delta and whether the value increased, decreased, or did not move;
  - final pitch direction must be confirmed in Betaflight model preview: pushing ELE forward should make the model nose-down / forward-flight direction.
- Safety behavior:
  - AIL right, THR high, RUD right, CH5 high, and CH6 high still check expected movement direction;
  - failed or suspicious direction checks should be fixed in Betaflight Receiver/Channel Map or MC6C channel reverse, not by compensating in ESP32 code.

## 2026-07-05 MC6C Receiver Type Triage

- Bring-up risk found:
  - the workflow already said the MC6C receiver must connect to the FC, but it did not force a clear split between serial receiver output and 6-channel PWM output;
  - if the user's receiver exposes six independent `CH1-CH6` PWM outputs and the replacement FC does not support multi-PWM RX input, Betaflight Receiver will show no movement even though the transmitter is bound.
- Documentation correction:
  - `replacement_fc_ready_workflow.md` now adds a quick receiver-output type check before any ESC/motor step;
  - serial-style receivers are treated as one signal line into the correct FC receiver input;
  - six independent PWM outputs require confirmed FC PWM RX support, a PWM-to-PPM/SBUS encoder, or a different serial receiver.
- Failure split:
  - if Receiver has no movement, check bind state, receiver 5V/GND, Betaflight Serial RX port, receiver provider, and PWM RX support before continuing;
  - the workflow now explicitly stops ESC/motor steps until receiver input works.

## 2026-07-05 FCOutput Functional Order Cleanup

- Code audit finding:
  - `RcChannels` and Betaflight RAW_RC use functional order `roll, pitch, yaw, throttle`;
  - `FCOutput` had declared `throttle` before `yaw`, while all real uses assigned by field name and `buildBetaflightRawRC()` already packed yaw/throttle correctly.
- Correction:
  - reordered `FCOutput` declaration to `roll, pitch, yaw, throttle, AUX1, AUX2`;
  - changed the `RcChannels` comment to say it follows Betaflight function names rather than implying C++ struct-layout compatibility.
- Safety intent:
  - this does not change the runtime gate or output range;
  - it reduces the chance a future edit or aggregate initialization silently swaps yaw and throttle.

## 2026-07-05 FCBridge Compile-Time Output Gate

- Safety gap found:
  - `include/config.h` promises `ENABLE_REAL_FC_OUTPUT=0` means direction control only drives simulation and never writes the real FC;
  - most safe builds avoid compiling FC output callers, but `FCBridge` itself still had the `sendRawRC()` call guarded only by runtime online/armed/CH6/freshness conditions;
  - legacy modules such as `followCtrl` can still call `fcBridge.setOutput()` in broader builds, so the bridge layer should enforce the compile-time gate itself.
- Correction:
  - `FCBridge::update()` now wraps the only `sendRawRC()` call with `#if ENABLE_REAL_FC_OUTPUT`;
  - when the macro is off, queued override output increments `gateBlocks`, records `lastReason="compile_gate"`, and does not call `MSP::setRawRC()`;
  - FC-ready builds still require the existing runtime gate: FC online, FC armed, MC6C CH6/AUX2 high, and fresh output.
- Safety intent:
  - read-only diagnostic builds can still use `FCBridge` for MSP polling;
  - non-FC-ready firmware cannot accidentally write `MSP_SET_RAW_RC` through the bridge even if a legacy controller queues an output.

## 2026-07-05 Direction Input And Receiver Readiness Hardening

- Gap found:
  - Web/WS direction input is supposed to be normalized `-1..+1`, but malformed values can still reach the pure RC mapper during bench/debug traffic;
  - finite out-of-range values were already clamped, but NaN needed an explicit neutral fallback before deadzone/span math;
  - MC6C receiver-readiness logic lived inside `unified_web_main.cpp`, so it was visible in the Web page but not directly locked by host tests.
- Correction:
  - `manual_control.cpp` now sanitizes NaN direction axes to `0.0f` before applying deadzone and RC limiting;
  - infinities and large finite values still use the existing clamp-to-range behavior;
  - `src/comm/fc_rc_mapping.h` now also contains a pure `evaluateMC6CReceiverReadiness()` helper for six-channel presence, centered roll/pitch/yaw, low throttle, and clear AUX1/AUX2 switch extremes;
  - `unified_web_main.cpp` now uses that helper for Web telemetry fields instead of duplicating threshold checks locally.
- Verification:
  - `test/test_manual_control.cpp` now covers NaN axis, infinity axis, and NaN throttle behavior;
  - `test/test_fc_ready_logic.cpp` now covers receiver-ready success plus missing CH6, off-center roll, non-low throttle, and mid AUX failure modes;
  - host tests pass: manual-control `23 passed, 0 failed`; FC-ready logic `23 passed, 0 failed`;
  - firmware builds pass for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-diag`, and `esp32-s3-fc-uart-probe`.
- Safety intent:
  - this does not enable real output or loosen the CH6/armed/online gates;
  - it only makes the preflight Web diagnostics and direction-mapping path more deterministic before the replacement FC arrives.

## 2026-07-05 MC6C Direction Self-Test Host Coverage

- Test gap found:
  - the Web wizard could guide AIL/ELE/THR/RUD/CH5/CH6 checks, but the matching baseline and step rules needed a host-side regression test;
  - direction mistakes here are bring-up blockers because a reversed or non-moving stick should stop the user before no-prop motor checks.
- Correction:
  - `test/test_fc_ready_logic.cpp` now covers `evaluateMC6CDirectionBaseline()` for six-channel presence, centered roll/pitch/yaw, low throttle, and low AUX baseline;
  - the same test covers `evaluateMC6CDirectionStep()` for AIL right, THR high, RUD right, ELE movement in either numeric direction, CH5 high, CH6 high, missing channels, and invalid channel indices.
- Safety intent:
  - this does not enable real output or change FC-ready gating;
  - it locks the diagnostic rules that decide whether MC6C direction mapping is sane enough to continue replacement-FC no-prop bench checks.

## 2026-07-05 MC6C Manual Direction Acceptance Gate

- Gap found:
  - the replacement-FC workflow covered Receiver, Motors, failsafe, and Web direction self-test, but the final "does the aircraft actually move in the commanded MC6C direction" gate was still implicit;
  - without an explicit low-altitude manual acceptance gate, a bench-passing setup could still reach first hover with roll/pitch/yaw expectations unclear.
- Documentation correction:
  - `replacement_fc_ready_workflow.md` now adds `4.1 低空手动方向验收`;
  - the section requires prop direction, motor order, Betaflight model preview, failsafe, CH5/ARM clarity, and CH6 low before flight;
  - it lists expected responses for throttle, AIL left/right, ELE forward/back, and RUD left/right, plus immediate stop-and-debug actions when any direction is wrong.
- Safety intent:
  - this keeps MC6C manual control as the first real flight authority;
  - ESP32 real assist remains out of scope until manual low-altitude direction acceptance is proven.

## 2026-07-05 Replacement FC Arrival Checklist

- Usability gap found:
  - `replacement_fc_ready_workflow.md` is complete, but too long for fast bench/field execution when the new FC arrives;
  - the user needs a short checklist that preserves the same safety gates without re-reading the entire workflow under time pressure.
- Documentation correction:
  - added `replacement_fc_arrival_checklist.md`;
  - linked it from the top of `replacement_fc_ready_workflow.md`;
  - the checklist covers stop conditions, Betaflight bring-up, MC6C receiver input, ESC wiring, UART3 MSP read-only proof, no-prop motor/failsafe checks, default ESP32 Web demo, low-altitude manual direction acceptance, and the later FC-ready gate.
- Safety intent:
  - the short checklist does not loosen any gate;
  - it keeps ESP32 true assist disabled until MC6C manual direction flight is proven.

## 2026-07-05 Replacement FC Acceptance Log Template

- Evidence gap found:
  - the workflow and short checklist told the user what to do, but the arrival-day proof still needed a single place to record hardware versions, Betaflight screenshots, Receiver values, UART3/MSP logs, no-prop motor/failsafe evidence, Web status, and manual direction-flight result;
  - without a fillable record, it would be too easy to say a gate "passed" without preserving proof.
- Documentation correction:
  - added `replacement_fc_acceptance_log_template.md` as the fillable evidence record;
  - linked it from `replacement_fc_ready_workflow.md`, `replacement_fc_arrival_checklist.md`, and `task_plan.md`;
  - the template uses `docs/evidence/replacement_fc/` as the expected folder for screenshots, logs, and videos.
- Safety intent:
  - a low-altitude MC6C direction flight is not considered accepted until the record includes pass/fail notes and evidence paths;
  - FC-ready real assist remains blocked until the manual direction acceptance record supports continuing.

## 2026-07-05 MC6C Transmitter Setup Card

- Usability gap found:
  - MC6C setup guidance existed in the full workflow and short checklist, but the transmitter-specific steps were still mixed with receiver, ESC, MSP, and flight-test steps;
  - the user's MC6C has UAV/CAR/BOAT/AIRPLANE modes, V-TAIL/ELEVON mixing, CH5/CH6 switches, and servo reverse toggles, so a wrong physical setting could make the aircraft fail the first direction test even if the ESP32 code is correct.
- Documentation correction:
  - added `mc6c_transmitter_setup_card.md`;
  - linked it from the replacement-FC workflow, arrival checklist, and acceptance log template;
  - the card captures UAV mode, V-TAIL/ELEVON off, reverse toggles default-first, receiver output type triage, target CH1-CH6 mapping, Receiver-page expected behavior, and first low-altitude prerequisites.
- Safety intent:
  - MC6C manual control remains the first real flight authority;
  - CH6 stays low during first low-altitude manual direction acceptance;
  - ESP32 code must not be used to hide a bad transmitter mode, mixed channel, reversed channel, motor order, motor direction, or prop direction.

## 2026-07-05 FC-ready Pilot Channel Preservation Test

- Safety gap found:
  - the FC-ready path was designed to preserve live MC6C throttle, AUX1/ARM, and AUX2 permission while ESP32 assist only changes roll/pitch/yaw;
  - before this pass, that rule existed in the target code and documentation but was not locked by a focused host test.
- Correction:
  - added `buildAssistOutputPreservingPilotChannels()` to `src/comm/fc_rc_mapping.h`;
  - `unified_web_main.cpp` now uses that helper when composing FC-ready `FCOutput`;
  - the Web page and serial startup text now describe the gate as Betaflight ARM/解锁位 instead of a vague `armed` label.
- Verification:
  - `test/test_fc_ready_logic.cpp` now proves assist output replaces only roll/pitch/yaw and preserves MC6C throttle, AUX1/ARM, and AUX2 permission;
  - missing pilot channels fall back to safe defaults: throttle low, AUX1 neutral, AUX2 low;
  - host FC-ready logic test passed: `61 passed, 0 failed`;
  - host manual-control test passed: `23 passed, 0 failed`;
  - builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-diag`, and `esp32-s3-fc-uart-probe`.

## 2026-07-05 FC-ready Acceptance Wording Alignment

- Documentation gap found:
  - `replacement_fc_ready_workflow.md` still described MSP Override as allowed to cover attitude/throttle channels, while the current FC-ready safety rule is roll/pitch/yaw only;
  - `replacement_fc_acceptance_log_template.md` still used vague `FC armed` wording and did not explicitly require preserving MC6C AUX1/AUX2 values.
- Correction:
  - aligned the workflow, arrival checklist, and acceptance log template with Betaflight `MSP_STATUS` ARM active-box wording;
  - made the evidence row require ESP32 to preserve MC6C throttle, AUX1/ARM, and AUX2/CH6 permission while only changing roll/pitch/yaw.
- Safety intent:
  - this is a documentation-only alignment with the already host-tested code path;
  - no firmware upload or hardware action was performed because the current FC remains damaged.

## 2026-07-05 FC-ready Six-Channel Output Diagnostics

- Evidence gap found:
  - the FC-ready Web page showed the last output roll/pitch/yaw/throttle values, but did not show AUX1/ARM or AUX2/CH6;
  - that made it harder to preserve proof that ESP32 assist leaves MC6C ARM/mode and assist-permission channels untouched.
- Correction:
  - added `lastAux1` and `lastAux2` to `FCOutputDiag`;
  - exposed `fcOutAux1` and `fcOutAux2` in unified telemetry JSON;
  - changed the Web diagnostic line to show `最后RC R/P/Y/T/A1/A2`;
  - updated the replacement-FC workflow and acceptance log template to use the six-value diagnostic as bench evidence.
- Safety intent:
  - this is observability only and does not loosen any output gate;
  - FC-ready real output remains gated by compile-time enable, FC online, Betaflight ARM active-box, MC6C CH6/AUX2 high, fresh Web command, and Betaflight MSP Override.

## 2026-07-05 FC-ready Web Throttle Clarification

- Usability gap found:
  - the Web direction panel had `升/降` and a throttle axis for simulation;
  - FC-ready real output intentionally preserves MC6C throttle and does not use webpage throttle, so the UI could mislead bench testing if not labeled.
- Correction:
  - changed the direction panel wording to label webpage throttle/up/down as simulation-only;
  - changed the FC-ready gate text to say real output is roll/pitch/yaw only and real throttle remains MC6C-controlled;
  - updated the workflow, arrival checklist, and acceptance log template with the same rule.
- Safety intent:
  - this is a UI/documentation clarification only;
  - it reinforces that MC6C remains the real throttle authority for first flight and FC-ready assist.

## 2026-07-05 FC-ready Web Output Path Host Test

- Test gap found:
  - the pure FC-ready helper test proved assist composition preserves MC6C throttle/AUX values;
  - the remaining risk was the full Web direction path: future edits could accidentally forward webpage throttle/up/down into real `MSP_SET_RAW_RC` while keeping the lower-level helper correct.
- Correction:
  - added `test/test_fc_ready_web_output_path.cpp`;
  - the test runs Web-style direction input through `mapCommandToChannels()`, then `buildAssistOutputPreservingPilotChannels()`, then `buildBetaflightRawRC()`;
  - it asserts real RAW_RC roll/pitch/yaw follow assist, but real RAW_RC throttle, AUX1/ARM, and AUX2/CH6 remain the live MC6C values.
- Verification:
  - Web-output-path host test passed: `9 passed, 0 failed`;
  - FC-ready logic host test passed: `61 passed, 0 failed`;
  - manual-control host test passed: `23 passed, 0 failed`;
  - `esp32-s3-unified-web` and `esp32-s3-unified-web-fc-ready` both built successfully.
- Safety intent:
  - this does not enable real output or loosen gates;
  - it locks the rule that webpage throttle/up/down is simulation-only, while MC6C remains the real throttle and AUX authority.

## 2026-07-05 Host Test Runner

- Usability gap found:
  - the host tests now cover the most safety-critical software behavior, but running them required three separate `g++` commands;
  - a future verification pass could easily forget the Web-output-path test and miss a regression in webpage throttle isolation.
- Correction:
  - added `scripts/run_host_tests.ps1`;
  - documented the command in `README.md` and `task_plan.md`.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_host_tests.ps1` passed;
  - the runner built and ran manual-control, FC-ready logic, and FC-ready Web-output-path tests, ending with `All host tests passed`.
- Safety intent:
  - this is a test-runner addition only;
  - it does not upload firmware, arm/disarm, write MSP, or touch motors.

## 2026-07-05 Replacement FC Evidence Package Index

- Evidence gap found:
  - the workflow and acceptance template named many evidence files, but the target directory itself did not explain what each file proves;
  - on arrival day, screenshots and videos need to be collected in the right order so "MC6C can fly in commanded directions" is not treated as a verbal claim.
- Correction:
  - added `docs/evidence/replacement_fc/README.md`;
  - linked it from `replacement_fc_ready_workflow.md` and `replacement_fc_acceptance_log_template.md`;
  - the README groups evidence by stage: Betaflight configuration, MC6C Receiver, UART3 MSP, no-prop Motors/failsafe, default ESP32 Web, low-altitude MC6C manual direction acceptance, and later FC-ready no-prop validation.
- Safety intent:
  - this is documentation and evidence organization only;
  - no missing evidence stage is allowed to be silently skipped before flight or FC-ready real output.

## 2026-07-05 Replacement FC Software Check Runner

- Usability gap found:
  - the host tests had one command, but the replacement-FC software readiness check still required separately running four PlatformIO builds;
  - forgetting `fc-uart-probe` or `fc-diag` would weaken the arrival-day UART3 recovery path.
- Correction:
  - added `scripts/run_replacement_fc_software_checks.ps1`;
  - documented it in `README.md` and `task_plan.md`.
- Safety behavior:
  - the script runs host tests, then builds `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`;
  - it does not upload firmware, open serial ports, arm/disarm, write MSP, or run motors.
- Remaining proof:
  - passing the script is software readiness only;
  - the objective still requires replacement-FC hardware evidence: Receiver, no-prop Motors, failsafe, and low-altitude MC6C direction acceptance.

## 2026-07-05 MC6C Channel Map First Candidate

- Bring-up gap found:
  - the MC6C physical channel order is known as `CH1 AIL, CH2 ELE, CH3 THR, CH4 RUD`;
  - the workflow said to check Betaflight Channel Map, but did not give the likely first candidate, which could slow down arrival-day Receiver setup.
- Documentation correction:
  - added `AETR1234` as the first Betaflight Channel Map candidate in `mc6c_transmitter_setup_card.md` and `replacement_fc_ready_workflow.md`;
  - updated the arrival checklist, acceptance log template, and evidence README so the final actual map must be recorded and proven by the Receiver page.
- Safety intent:
  - `AETR1234` is only a first candidate, not a substitute for Receiver-page and Betaflight model-preview verification;
  - if the real receiver output needs another map, fix Betaflight Channel Map or MC6C reverse settings, not ESP32 code.

## 2026-07-05 Motor Order And Prop Direction Gate

- Bring-up gap found:
  - MC6C/Receiver direction can be correct while the aircraft still flips because M1-M4 physical outputs, motor rotation, or prop direction are wrong;
  - the workflow had yes/no motor checks, but not a fillable motor-position/rotation/prop record.
- Safety correction:
  - added `replacement_fc_motor_direction_card.md` for no-prop M1-M4 position checks, rotation direction checks, prop direction checks, and first-flight failure recovery;
  - linked the card from the replacement-FC workflow, arrival checklist, acceptance log, and evidence README;
  - expanded the acceptance log so M1-M4 actual position, rotation direction, and prop direction are recorded before low-altitude manual direction acceptance.
- Code hardening:
  - changed the legacy `MotorController` stub so ESP32-side `arm()`, `disarm()`, `emergencyStop()`, and `setThrottle()` are no-ops and `isArmed()` always returns false;
  - this keeps old includes build-compatible without implying ESP32 can arm or drive motors.
- Safety intent:
  - motor order, motor direction, prop direction, and failsafe evidence are required before proving that MC6C can fly the aircraft in commanded directions;
  - ESP32 code must not compensate for any motor, prop, or FC orientation error.

## 2026-07-05 Legacy Follow Output Disabled By Default

- Code audit finding:
  - the current FC-ready Web path preserves MC6C throttle, AUX1/ARM, and AUX2/CH6 while only changing roll/pitch/yaw;
  - the older `FollowController` PID path still constructed `FCOutput` with its own throttle values and called `fcBridge.setOutput(out)`;
  - even though default builds still have `ENABLE_REAL_FC_OUTPUT=0`, keeping that legacy queue path active would be confusing and risky for replacement-FC bring-up.
- Correction:
  - added `ENABLE_LEGACY_FOLLOW_FC_OUTPUT`, default `0`, in `include/config.h`;
  - changed `FollowController::begin()`, `emergencyStop()`, and `update()` so legacy follow modes do not queue FC output unless a future bench-only build explicitly opts in;
  - updated `follow.h` comments to say real FC output is disabled by default.
- Safety intent:
  - MC6C manual control remains the first real flight authority;
  - FC-ready assist remains the only intended ESP32 real-output path, and it is still gated by compile-time enable, FC online, Betaflight ARM active-box, MC6C CH6/AUX2 high, fresh Web command, and Betaflight MSP Override;
  - old autonomous follow PID output must not be used to prove that the aircraft can fly in commanded MC6C directions.

## 2026-07-05 Replacement FC Field One-Page Sheet

- Usability gap found:
  - the full workflow, checklist, MC6C card, motor card, and evidence template are thorough, but arrival-day wiring still benefits from a first page that says exactly what to connect and what not to skip;
  - at that time the referenced ESC photo path was still unavailable locally, so the practical assumption remained ordinary three-wire PWM/OneShot-style ESC wiring until hardware confirmation.
- Later evidence update:
  - user later provided the ESC photo; the active workflow now treats it as a verified ordinary PWM/OneShot three-wire servo-plug ESC, with the red wire/BEC still requiring meter confirmation.
- Documentation correction:
  - added `replacement_fc_field_one_page.md`;
  - linked it from `README.md`;
  - the sheet captures stop conditions, ESC white/red/black wiring, MC6C UAV/no-mixing setup, `AETR1234` first Channel Map candidate, UART3 MSP wiring, no-prop motor/failsafe order, default Web usage, and the low-altitude manual direction pass criteria.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed: manual-control `23 passed, 0 failed`, FC-ready logic `61 passed, 0 failed`, Web-output-path `9 passed, 0 failed`;
  - builds passed: `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Remaining proof:
  - this is still software and workflow readiness only;
  - the objective is not complete until the replacement FC produces Receiver-page proof, no-prop motor order/direction proof, failsafe proof, default Web evidence, and low-altitude MC6C manual direction acceptance evidence.

## 2026-07-05 MC6C Direction Self-Test No-Prop Warning

- Safety gap found:
  - the Web receiver self-test asks the user to move throttle high and CH5 high;
  - CH5 is planned as ARM or mode, so on a configured FC that action can be safety-sensitive if props or动力电池 are present.
- Correction:
  - updated the Web self-test text in `src/web/index_html.h` to require no props, recommend USB-only/no动力电池, and clarify that CH5 high is only a Receiver/AUX1 movement check;
  - updated `replacement_fc_field_one_page.md` and `replacement_fc_acceptance_log_template.md` with the same rule.
- Verification:
  - `git diff --check` reported no real whitespace errors, only existing LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed: manual-control `23 passed, 0 failed`, FC-ready logic `61 passed, 0 failed`, Web-output-path `9 passed, 0 failed`;
  - builds passed: `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - Receiver direction proof is still required before motor checks and flight;
  - this change prevents the receiver wizard from being mistaken for an arming, motor, or flight permission step.

## 2026-07-05 MC6C Bench Baseline Requires CH5/CH6 Low

- Safety gap found:
  - `rxBenchReady` previously accepted CH5/AUX1 and CH6/AUX2 at either clear low or clear high;
  - because CH5 is planned as ARM/mode and CH6 is ESP32 assist permission, a high switch position is not a good default baseline for continuing bench bring-up.
- Correction:
  - `MC6CReceiverReadiness` now reports `aux1Low` and `aux2Low`;
  - `rxBenchReady` now requires roll/pitch/yaw centered, throttle low, CH5 low, and CH6 low;
  - telemetry JSON and the Web receiver-check line now show `CH5低位` and `CH6低位`;
  - host tests prove AUX1 high and AUX2 high are still valid switch positions but each blocks bench-baseline readiness.
- Follow-up verification:
  - added explicit host coverage for CH5/AUX1 high blocking `rxBenchReady`;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_host_tests.ps1` passed with manual-control `23 passed`, FC-ready logic `66 passed`, and Web-output-path `9 passed`.
- Follow-up documentation alignment:
  - updated `replacement_fc_ready_workflow.md` and `docs/evidence/replacement_fc/README.md` so the evidence package and full workflow match the low-baseline rule.
- Documentation:
  - updated `replacement_fc_arrival_checklist.md`, `replacement_fc_field_one_page.md`, and `replacement_fc_acceptance_log_template.md` so the default no-prop bench baseline is CH5/CH6 low.
- Verification:
  - `git diff --check` reported no real whitespace errors, only existing LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed: manual-control `23 passed, 0 failed`, FC-ready logic `64 passed, 0 failed`, Web-output-path `9 passed, 0 failed`;
  - builds passed: `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - high CH5/CH6 positions are still checked during direction self-test;
  - they are not treated as the safe baseline state before Motors, failsafe, or first manual direction acceptance.

## 2026-07-05 Safe Default Upload/Build Alignment

- Risk found:
  - README still showed a generic upload command without `-e`;
  - with multiple PlatformIO environments, that could upload the older first environment instead of the current safe `esp32-s3-unified-web` firmware.
- Correction:
  - added `[platformio] default_envs = esp32-s3-unified-web` in `platformio.ini`;
  - updated README to use the explicit safe upload command for `esp32-s3-unified-web`;
  - kept `esp32-s3-unified-web-fc-ready` documented as future-only after replacement-FC Receiver, no-prop motor, failsafe, UART3 MSP, and low-altitude MC6C direction acceptance evidence.
- ESC image note:
  - at that time the referenced image path was not present in the current filesystem;
  - this note is superseded by the later user-provided ESC photo, now verified as an ordinary white/red/black three-wire PWM/OneShot harness with BEC red-wire meter confirmation still required.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - direct `platformio.exe run` built `esp32-s3-unified-web`;
  - `git diff --check` reported no real whitespace errors, only existing LF/CRLF warnings.
- Safety intent:
  - this change does not enable real FC output;
  - default firmware remains Web, sensors, camera, and simulated FC display only.

## 2026-07-05 Betaflight Setup Card And RAW_RC Clamp

- Gap found:
  - the workflow covered replacement-FC bring-up, but the Betaflight setup steps were still spread across several files;
  - FC-ready RAW_RC packing trusted caller-provided channel values even though the final MSP send boundary is the best place for a last clamp.
- Correction:
  - added `replacement_fc_betaflight_setup_card.md` for Setup, Ports, Receiver, Modes, Motors, failsafe, and required evidence;
  - linked the new card from the README, full workflow, arrival checklist, acceptance log template, and evidence README;
  - `buildBetaflightRawRC()` now clamps every outgoing channel and unused neutral fill to 1000-2000;
  - host tests now prove low and high RAW_RC values are clamped before MSP packing.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed: manual-control `23 passed, 0 failed`, FC-ready logic `72 passed, 0 failed`, Web-output-path `9 passed, 0 failed`;
  - builds passed: `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`;
  - `git diff --check` reported no real whitespace errors, only existing LF/CRLF warnings.
- Safety intent:
  - MC6C manual direction remains a Betaflight/receiver/motor-order problem to prove on hardware;
  - ESP32 still must not compensate for a wrong Channel Map, reversed stick direction, motor order, motor direction, or prop direction.

## 2026-07-05 Low-Altitude Direction Troubleshooting Card

- Gap found:
  - the acceptance workflow defined the low-altitude MC6C direction test, but a failed first hop still required the user to infer the right rollback path;
  - that is exactly when wrong fixes happen, such as changing ESP32 direction code or toggling multiple Betaflight/MC6C settings at once.
- Correction:
  - added `replacement_fc_manual_direction_troubleshooting_card.md`;
  - linked it from the README, full workflow, arrival checklist, acceptance log template, evidence README, and task plan;
  - added a failure/fix table to the acceptance log so symptoms and corrective actions are recorded before retesting.
- Safety intent:
  - every failed manual direction symptom routes back to Receiver, model preview, Board Alignment, Motor Order, Motor Direction, prop direction, or failsafe;
  - ESP32 remains out of the first manual direction correction loop.

## 2026-07-05 Bidirectional MC6C Self-Test Completion

- Final readiness gap checked:
  - the user wants the replacement FC to work after wiring, so the MC6C Receiver proof must catch one-sided or reversed channels before motor tests or low-altitude flight;
  - AIL and RUD need explicit left/right checks, while ELE should prove forward/back movement but still use Betaflight model preview for final direction because numeric pitch polarity can vary by receiver map and reverse settings.
- Current implementation/documentation state:
  - the Web receiver wizard records baseline, AIL right, AIL left, ELE forward, ELE back, THR high, RUD right, RUD left, CH5 high, and CH6 high;
  - the workflow, acceptance log template, and evidence README require the same bidirectional proof before treating Receiver setup as complete;
  - `task_plan.md` now names the bidirectional coverage explicitly so future agents do not reduce it back to a generic movement check.
- Verification:
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - this is still software/workflow readiness only;
  - the project is not flight-ready until the replacement FC produces real Receiver-page evidence, no-prop motor order/rotation evidence, failsafe evidence, UART3 MSP evidence, default Web evidence, and low-altitude MC6C manual direction acceptance evidence;
  - this older ESC-photo note is superseded by the later verified photo; current docs treat the ESC as ordinary PWM/OneShot, while keeping BEC red-wire use conditional on meter-confirmed stable 5V.

## 2026-07-05 MC6C Goal Completion Audit Page

- Final evidence gap found:
  - the workflow, checklist, evidence README, and acceptance template already contain the necessary stages, but the active goal is easy to overclaim because software tests and builds are complete while real replacement-FC evidence is still missing.
- Correction:
  - added `replacement_fc_goal_completion_audit.md`;
  - the page states that software/flow preparation is ready, but the true MC6C commanded-direction flight goal remains incomplete until new-FC hardware evidence exists;
  - it lists every requirement that must be proven: new FC, MC6C UAV/no-mixing setup, receiver mapping, CH5/CH6 roles, M1-M4 order, motor direction, prop direction, failsafe, default Web noninterference, and low-altitude manual direction video;
  - linked the audit from the README, arrival checklist, acceptance log template, and evidence README.
- Verification:
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - prevent treating host tests, PlatformIO builds, Web simulation, or UART3 MSP replies as proof that the aircraft can fly in commanded directions;
  - keep FC-ready assist behind the manual low-altitude acceptance gate.

## 2026-07-05 Replacement-FC Command Record Card

- Usability gap found:
  - the replacement-FC flow had the right stages, but arrival-day commands were spread across several documents;
  - that makes it too easy to forget a log file, rerun the wrong PlatformIO environment, or reconstruct UART3 MSP commands from memory.
- Correction:
  - added `replacement_fc_command_record_card.md`;
  - the card groups software checks, Betaflight CLI snapshot, MC6C Channel Map candidate, USB-TTL UART3 MSP proof, ESP32 UART3 probe, FC diag, default Web upload, and later FC-ready upload commands;
  - linked the card from the README, arrival checklist, acceptance log template, evidence README, and task plan.
- ESC image note:
  - at the time of this command-card pass, the referenced image path was still absent locally, so no photo-based ESC line identification was made;
  - this is superseded by the later provided ESC photo, now verified as the ordinary PWM/OneShot pattern with thick battery leads, three motor phase wires, and a white/red/black servo-style control plug.
- Verification:
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - this is a command/logging aid only;
  - it does not make the damaged FC usable and does not replace Receiver, no-prop motor, failsafe, UART3 MSP, default Web, or low-altitude MC6C manual direction evidence.

## 2026-07-05 MSP Write-Interface Narrowing

- Risk found:
  - `FCBridge` exposed a mutable `MSP&` even though current callers only needed read-only diagnostic counters;
  - `MSP::sendCommand()` was public, leaving an easy future path to send arbitrary MSP writes outside the `FCBridge::update()` gate.
- Correction:
  - changed `FCBridge` to expose `getMSPDiag()` only;
  - moved `MSP::sendCommand()` to the private API;
  - added compile-time guards inside `MSP::setRawRC()` and `MSP::sendArmCommand()`, so direct calls fail closed unless the relevant output or arm/disarm macro is explicitly enabled;
  - updated FC diag and unified Web telemetry to use the read-only diagnostic accessor.
- Verification:
  - `rg "getMSP\(|sendCommand\(" -n src test` now shows no external mutable `getMSP()` use and no external `sendCommand()` calls;
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_host_tests.ps1` passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed after the interface narrowing;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - real output remains concentrated in the explicit FC-ready path: compile-time `ENABLE_REAL_FC_OUTPUT`, FC online, Betaflight ARM active-box, MC6C CH6/AUX2 high, and fresh Web command;
  - diagnostics can still report MSP TX/RX/timeout state without giving the Web or diag layer an arbitrary MSP write handle.

## 2026-07-05 Replacement-FC Evidence Wording Correction

- Evidence contradiction found:
  - during that pass, `task_plan.md` said the user's ESC photo had been inspected while then-current records said the referenced image path was absent locally.
- Correction:
  - at that time, changed `task_plan.md` to say the ESC photo inspection was attempted but the image was unavailable in the current workspace;
  - later user-provided photo evidence superseded that correction, and current docs now record the ESC as verified ordinary PWM/OneShot with BEC red-wire use still meter-confirmed;
  - tightened the low-altitude manual direction evidence template to record pilot position, nose direction toward open space, short/light stick inputs, and immediate throttle cut/disarm on abnormal behavior.
- Verification:
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - avoid treating unavailable evidence as proof of ESC pinout, and after the later photo arrived, avoid treating BEC red-wire power as safe without a meter check;
  - make the final manual-direction video and notes easier to judge against the real goal: MC6C commands produce the expected aircraft direction without ESP32 compensation.

## 2026-07-05 Replacement-FC Static Check Repair

- Static-check failure found:
  - `scripts/run_replacement_fc_static_checks.ps1` failed in Windows PowerShell with `The string is missing the terminator: "` at the final `Write-Host`;
  - the trigger was the script's Chinese source-string assertions for low-altitude evidence wording, which are fragile under Windows PowerShell source decoding;
  - after that was fixed, the recursive source scan tried to read `src\comm` as a file because the `Get-ChildItem -Include` result also included directories.
- Correction:
  - added an ASCII-only UTF-8 hex assertion helper so the script can still verify Chinese document phrases without embedding those phrases as PowerShell source literals;
  - filtered the recursive `src` scan to non-directory items before checking for external `sendCommand(` calls.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_static_checks.ps1` passed;
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - static checks, host tests, and all four replacement-FC builds passed in the full run;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - this check remains read-only and performs no firmware upload, serial open, MSP write, arming, motor action, or flight action;
  - it now catches drift in the replacement-FC safety boundary before arrival-day hardware work.

## 2026-07-05 Replacement-FC Goal-Overclaim Static Guard

- Remaining risk found:
  - the goal audit correctly says the software/flow is ready but the real MC6C direction-flight goal is not complete until replacement-FC hardware evidence exists;
  - that statement is important enough to protect with the same static check as the compile-time output guards;
  - otherwise a future documentation cleanup could accidentally turn "ready for arrival-day validation" into "goal complete".
- Correction:
  - extended `scripts/run_replacement_fc_static_checks.ps1` to assert that `replacement_fc_goal_completion_audit.md` still says the real flight goal is incomplete and requires replacement-FC physical evidence;
  - extended the same check to ensure `replacement_fc_arrival_checklist.md` still forbids ESP32-S3 ESC signal wiring and keeps CH6 low for the first manual flight;
  - extended the same check to ensure `replacement_fc_field_one_page.md` still says the first manual flight does not use ESP32 real assist output.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_static_checks.ps1` passed after the new assertions.
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed after the new assertions;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - keep the active objective anchored to real evidence: MC6C Receiver proof, no-prop Motors proof, failsafe proof, default Web noninterference, and low-altitude manual direction video;
  - prevent code/build success from being mistaken for actual aircraft direction acceptance.

## 2026-07-05 Replacement-FC Script Hardware-Action Guard

- Remaining risk found:
  - `scripts/run_replacement_fc_software_checks.ps1` is meant to be safe before the replacement FC arrives: static checks, host tests, and PlatformIO builds only;
  - the script printed that it does not upload or open serial ports, but the static check did not enforce that promise;
  - a future edit adding `-t upload`, `--upload-port`, `device monitor`, or `monitor --port` would be dangerous because it could touch current hardware during a routine verification command.
- Correction:
  - extended `scripts/run_replacement_fc_static_checks.ps1` with a no-hardware-action command guard for `run_host_tests.ps1` and `run_replacement_fc_software_checks.ps1`;
  - asserted that the replacement-FC software check script still invokes `run_replacement_fc_static_checks.ps1`.
- Repair notes:
  - the first implementation scanned the static script itself and matched the guard's own regex literal, so the scan was narrowed to the executable host/software check scripts;
  - a PowerShell array syntax attempt caused `Get-Item` to see `LiteralPath` twice, so the script now stores paths first and calls `Get-Item` inside the loop.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_static_checks.ps1` passed after both repairs.
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - keep software verification as a no-upload, no-serial, no-MSP-write, no-motor action;
  - make the command safe to run while the old FC is damaged and before replacement-FC hardware evidence exists.

## 2026-07-05 Real-FC Output PlatformIO Environment Guard

- Remaining risk found:
  - `ENABLE_REAL_FC_OUTPUT=1` is intentionally present only in `esp32-s3-unified-web-fc-ready`;
  - the default `esp32-s3-unified-web` firmware must stay Web/sensor/camera/simulation only, because the first real manual direction flight must be under MC6C and Betaflight control;
  - before this guard, a future edit could accidentally add the real-output macro to the default or diagnostic environment without the static check catching the environment-level drift.
- Correction:
  - extended `scripts/run_replacement_fc_static_checks.ps1` with an INI-section scan over `platformio.ini`;
  - the scan requires `-DENABLE_REAL_FC_OUTPUT=1` to appear exactly once in `esp32-s3-unified-web-fc-ready`;
  - the scan fails if that macro appears in any other environment or a global PlatformIO section.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_static_checks.ps1` passed after adding the environment guard.
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `9`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - default and diagnostic builds remain safe to compile before replacement-FC proof;
  - FC-ready remains a deliberate future-only build, still requiring hardware gates before any real assist output.

## 2026-07-05 FC-ready Web Takeover Timeout Host Test

- Remaining test gap found:
  - the FC-ready Web output-path test already proved that Web throttle is simulation-only and real FC output preserves live MC6C throttle/AUX values;
  - it did not explicitly prove that Web pause/takeover or a stale Web command stops queuing real override output.
- Correction:
  - extended `test/test_fc_ready_web_output_path.cpp` with a small helper mirroring `unified_web_main.cpp`: real output can be queued only when `ManualController` is not in neutral-hold;
  - added host assertions that a fresh Web direction leaves neutral-hold and may queue output;
  - added host assertions that `takeover=true` forces neutral-hold, neutralizes roll, lowers the Web throttle channel, and prevents real FC override queueing;
  - added host assertions that a stale Web direction after `MC_INPUT_TIMEOUT_MS` also forces neutral-hold and prevents real FC override queueing.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_host_tests.ps1` passed;
  - Web-output-path host test now reports `20 passed, 0 failed`.
  - `git diff --check` reported no real whitespace errors, only LF/CRLF warnings;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - full replacement-FC check passed host tests with manual-control `23`, FC-ready logic `79`, and Web-output-path `20`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`.
- Safety intent:
  - preserve MC6C/flight-controller takeover semantics;
  - ensure browser pause or lost Web heartbeat cannot continue driving a real FC-ready output stream.

## 2026-07-05 ESP32 Arming And Legacy Output Build-Flag Guard

- Final legacy-path audit:
  - old `MissionPlanner::arm()` / `disarm()` still route through `FCBridge::arm()` / `disarm()`, which compile closed unless `ENABLE_ESP32_ARM_DISARM=1`;
  - old `FollowController` PID output still contains `fcBridge.setOutput()` calls, but they compile closed unless `ENABLE_LEGACY_FOLLOW_FC_OUTPUT=1`;
  - old `main.cpp` is not the default PlatformIO environment and leaves `fcBridge.begin()` commented, while the safe default remains `esp32-s3-unified-web`.
- Correction:
  - extended `scripts/run_replacement_fc_static_checks.ps1` with `Assert-BuildFlagNotEnabled`;
  - the static check now fails if any PlatformIO environment enables `ENABLE_ESP32_ARM_DISARM=1`;
  - the static check now fails if any PlatformIO environment enables `ENABLE_LEGACY_FOLLOW_FC_OUTPUT=1`.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_static_checks.ps1` passed;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_software_checks.ps1` passed;
  - host tests passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `20`;
  - PlatformIO builds passed for `esp32-s3-unified-web`, `esp32-s3-unified-web-fc-ready`, `esp32-s3-fc-uart-probe`, and `esp32-s3-fc-diag`;
  - `git diff --check` reported only LF/CRLF warnings, with no real whitespace errors.
- Safety intent:
  - keep MC6C and Betaflight as the only arming/motor authority during replacement-FC arrival checks;
  - prevent a future PlatformIO build flag from quietly reviving legacy ESP32 arming or old autonomous follow output.

## 2026-07-05 Unified Web Environment Separation Guard

- Remaining configuration gap found:
  - the safe default environment is `esp32-s3-unified-web`, and the future-only environment is `esp32-s3-unified-web-fc-ready`;
  - the default Web environment currently does not compile `comm/fc_bridge.cpp` or `comm/msp.cpp`, but the static checks did not explicitly protect that source-level separation.
- Correction:
  - added `Get-PlatformioEnvSection` and `Assert-UnifiedWebEnvSeparation` to `scripts/run_replacement_fc_static_checks.ps1`;
  - the static check now fails if default `esp32-s3-unified-web` compiles FC bridge/MSP sources;
  - the static check also fails if `esp32-s3-unified-web-fc-ready` does not explicitly compile both `comm/fc_bridge.cpp` and `comm/msp.cpp`.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_static_checks.ps1` passed.
- Safety intent:
  - keep the default Web/camera/ToF/GPS/simulation firmware physically separated from FC UART/MSP code;
  - make real-FC capability remain a deliberate FC-ready build choice, not an accidental source-filter drift.

## 2026-07-05 FC-ready Output Diagnostic Accuracy

- Evidence gap found:
  - FC-ready Web/JSON exposes `fcOutRoll`, `fcOutPitch`, `fcOutYaw`, `fcOutThrottle`, `fcOutAux1`, and `fcOutAux2` for arrival-day screenshots;
  - `MSP_SET_RAW_RC` packing already clamps all outgoing channels to 1000-2000, but `FCBridge` diagnostics recorded the pre-packed `FCOutput` request values;
  - if a future malformed caller queued out-of-range values, the page could show a value different from the actual Betaflight-bound RAW_RC packet.
- Correction:
  - changed `FCBridge::sendRawRC()` so output diagnostics copy from the packed `ch[FC_RAW_RC_*]` array after `buildBetaflightRawRC()`;
  - extended `scripts/run_replacement_fc_static_checks.ps1` so the diagnostic fields must keep reporting the packed/clamped Betaflight slots.
- Verification:
  - `powershell -ExecutionPolicy Bypass -File scripts\run_replacement_fc_static_checks.ps1` passed;
  - `powershell -ExecutionPolicy Bypass -File scripts\run_host_tests.ps1` passed with manual-control `23`, FC-ready logic `79`, and Web-output-path `20`.
- Safety intent:
  - make FC-ready bench screenshots and JSON evidence reflect the actual RAW_RC values being sent to Betaflight;
  - keep evidence trustworthy when proving that throttle, AUX1/ARM, and AUX2/CH6 remain MC6C-controlled.
