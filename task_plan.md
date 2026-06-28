# ToF I2C Error Diagnostic Plan

Goal: eliminate or clearly root-cause intermittent ESP32 Wire errors while reading VL53L1X:
`requestFrom(): i2cRead returned Error -1 / 263`.

## Current Status

- [x] Capture current source state and observed symptoms.
- [x] Analyze VL53L1X library read path and ESP32 Wire error trigger points.
- [x] Implement targeted ToF driver changes with stronger diagnostics.
- [x] Build and upload `esp32-s3-web-mvp`.
- [x] Verify by serial log after reset.
- [x] Build and upload ToF-only isolation firmware.
- [x] Compare ToF-only serial error rate with Web MVP error rate.
- [x] Add startup guard against phantom I2C addresses before Pololu init.
- [x] Restore and verify Web MVP firmware after scan guard.
- [x] Replace risky Pololu regular read path with explicit result-frame validation.
- [x] Add Web MVP background ToF init retry and preserve cached valid distance through short read failures.
- [x] Build and upload the final Web MVP ToF mitigation firmware.
- [x] Fix Web MVP ToF retry state after in-run driver reinitialization failure.
- [x] Add serial fault labels to distinguish probe, init, and read-frame failures.
- [x] Add pre-init VL53L1X model-ID check to isolate init/register-read failure.
- [x] Replace unsafe Pololu model read with checked two-byte register read.
- [x] Sync ToF-only diagnostic firmware with `fault=` output and verify it builds.
- [x] Build configured I2C scanner after rewriting it to use the real ToF bus.
- [x] Add guarded software reset and stricter stable-ACK probe before VL53L1X init.
- [x] Build and upload Web MVP with the latest ToF init hardening.
- [x] Capture live serial evidence after upload.
- [x] Enhance and upload standalone I2C scanner to capture idle levels, ACK stability, and model-ID distribution.
- [x] Capture standalone scanner evidence.
- [x] Wire VL53L1X XSHUT to GPIO10, set `TOF_XSHUT_PIN`, rebuild/upload Web MVP, and verify ToF recovers after sensor-side reset.

## Working Hypotheses

1. The sensor and wiring can work because logs show `tof:init=1 valid=1 dist=176`.
2. Errors are likely caused by the current continuous read sequence polling the full result frame when the sensor or I2C bus is not ready.
3. Recovery is currently too reactive for a bench diagnostic stage; it hides the exact failing operation by repeatedly restarting the sensor.
4. Current live evidence now points to sensor-side reset/electrical/module state rather than Web/GPS/measurement scheduling: `0x29` is not stably ACKing and model-ID reads return bad values.

## Success Criteria

- Startup scan finds only plausible I2C devices and includes `0x29`.
- ToF remains `tof:init=1`.
- `dist` stays within a realistic range and changes when an object moves.
- No frequent `Recovering VL53L1X` loop.
- `requestFrom()` errors are either absent in a 30-60s serial sample or tied to a clearly logged driver phase.
- After XSHUT wiring, remaining `requestFrom()` errors are tolerated read-level transients: ToF stays initialized/valid and does not enter recovery.

## Errors Encountered

| Error | Evidence | Next Action |
|-------|----------|-------------|
| `requestFrom(): i2cRead returned Error -1 / 263` | User serial logs after reset | Add phase diagnostics and reduce risky I2C reads |
| `dist=64347` after I2C read failure | Prior serial logs | Keep rejecting invalid distances and preserve last valid value |
| Data-ready gate caused many misses | 60s serial sample showed `miss=207`, `ok=16` | Remove `dataReady()` from regular path |
| Pololu init crash after polluted scan | Web MVP short sample showed phantom addresses then `IntegerDivideByZero` | Require clean scan before init |
| `0x29` sometimes stops ACKing after repeated resets/uploads | Final local serial sample showed retry init plus model-ID read failure | Power-cycle ToF or wire XSHUT to an ESP32 GPIO for real sensor reset |
| `g_tofInit` could stay stale after failed in-run recovery | Code audit showed `web_main.cpp` only refreshed it in the retry branch | Synchronize `g_tofInit = tof.isInitialized()` every loop |
| Model/register read returns all ones | Live sample after pre-init check showed `id=0xFFFF i2c=2` | Stop before full init; require XSHUT or power-cycle retest |
| Checked model read still fails | Live sample showed `id=0x0000 i2c=2/5` with `fault=init_model` | Reduce avoidable reads; still require XSHUT or power-cycle retest |
| Stable ACK probe fails | 2026-06-27 sample after latest upload showed repeated `fault=probe_ack` and bad model IDs `0x001F/0x1FFF/0x3FFF` | Use XSHUT/power-cycle/module replacement; software reset cannot fix this state |
| Standalone scanner sees polluted bus | Scanner sample showed idle before `SDA=1 SCL=1`, after recovery `SDA=0 SCL=1`, dozens of phantom addresses, `0x29 ACK=10/30`, model-ID `ok=0/20`, soft reset write failed `i2c=2` | Stop software-only diagnosis; validate pull-ups/power/module/XSHUT |
| XSHUT hardware reset restores ToF init | After wiring XSHUT to GPIO10 and uploading Web MVP, 70s serial sample showed `tof:init=1 valid=1 status=0 dist=61-64 fault=none` for most samples | Keep XSHUT enabled; note GPIO10 conflicts with future OV2640 D2 pin |
| Stability recheck passes with transient read errors | Later 90s sample kept `tof:init=1 valid=1 status=0`, `rec=0`, and distance `61-64mm`; visible `Error 263/-1` appeared but recovered as `fault=none` | Accept for Web MVP; keep fault counters for future hardware cleanup |

## Implemented Driver Changes

- ToF timing budget increased to 100ms.
- Continuous inter-measurement period set to 200ms.
- ToF sample rate macro reduced to 5Hz for bench diagnostics.
- Added I2C bus recovery before sensor initialization.
- Added `dataReady()` gate before full result-frame reads.
- Added counters: total reads, valid reads, failed reads, ready misses, recoveries.
- Expanded `[SYS]` serial line with ToF status, errors, data age, and counters.
- Switched ToF to `Wire1` and hard-isolation settings: 10kHz I2C, 1Hz sample rate.
- Added scan gate: only initialize ToF after a clean scan containing exactly `0x29`.
- Added direct result-frame read with byte-count validation, cached last-good distance behavior, and Web background init retry.
- Added `fault=` serial diagnostics and fixed stale Web ToF init state.
- Added optional `TOF_XSHUT_PIN` config and model-ID precheck before Pololu init.
- Added checked model-register read to avoid unnecessary `requestFrom()` after write-phase failure.
- Updated ToF-only diagnostic output for isolated XSHUT/power-cycle retesting.
- I2C scanner now includes `<Wire.h>` and builds as an isolated configured-bus diagnostic.
- ToF init now requires stable repeated ACKs, retries model-ID reads, and attempts a guarded `SOFT_RESET` before giving up.
- Standalone scanner now prints idle SDA/SCL levels, repeated `0x29` ACK statistics, repeated model-ID reads, and soft-reset write result.
- `TOF_XSHUT_PIN` is now GPIO10 for real sensor-side reset. This is acceptable for Web MVP, but it conflicts with `CAM_PIN_Y4` if the OV2640 parallel camera is enabled later.
