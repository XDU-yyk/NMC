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

- Hardware constraint: final FC link must use F4V3S `R6/T6` (UART6), mapped to ESP32-S3 `GPIO16/17`.
- Important protocol correction:
  - MSP is request/response; the flight controller normally does not stream MSP frames from `T6` unless something sends a valid `$M>` request into `R6`.
  - Therefore, "USB-TTL only listens to T6 and sees nothing" does not prove UART6 is dead.
- Code audit found an actual workspace inconsistency:
  - `src/fc_diag_main.cpp` printed `diag.rxBytes`, but `src/comm/msp.h` had the `rxBytes` declaration stuck onto a mojibake comment line, effectively commented out.
  - Rewrote `src/comm/msp.h` and `src/comm/msp.cpp` as clean ASCII while preserving the existing MSP public interface.
- MSP diagnostic improvements now include:
  - `txFrames`, `txBytes`;
  - `rxBytes`, `rxDollarCount`, `rxHeaderCount`, `lastRxByte`;
  - per-request error classification using RX counters before/after the request rather than stale cumulative booleans.
- Added `src/fc_uart_probe_main.cpp` and PlatformIO env `esp32-s3-fc-uart-probe`.
- Probe firmware sends read-only MSP v1 request frames and dumps raw RX bytes:
  - `FC_VERSION`: `24 4D 3E 00 01 01`;
  - `STATUS`: `24 4D 3E 00 65 65`;
  - `ATTITUDE`: `24 4D 3E 00 6C 6C`;
  - `ALTITUDE`: `24 4D 3E 00 6D 6D`;
  - `ANALOG`: `24 4D 3E 00 6E 6E`;
  - `RC`: `24 4D 3E 00 69 69`.
- Build verification passed:
  - `esp32-s3-fc-diag`;
  - `esp32-s3-fc-uart-probe`.
- Uploaded `esp32-s3-fc-uart-probe` to ESP32 on `COM55`.
- Live COM55 sample after upload:
  - ESP32 repeatedly prints valid MSP request bytes, for example `tx=24 4D 3E 00 65 65`;
  - RX was still `<none>` in the short local sample.
- Interpretation:
  - ESP32 firmware is now definitely transmitting valid MSP v1 requests on Serial2 `TX=GPIO16`;
  - if the wiring is `GPIO16 -> F4 R6`, then the remaining failure is between ESP32 TX and FC UART6 MSP handling, or FC `T6` response path;
  - next physical test should verify that `R6` sees the exact request bytes and that `T6` responds with `$M<...`, not just passive listening on `T6`.

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
