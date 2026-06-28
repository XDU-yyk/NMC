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
