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

## Flight Controller UART6/MSP Diagnostic Status

- Superseded on 2026-07-03: do not continue using UART6 R6/T6 for MSP. The F4V3S PLUS manual identifies R6/T6 as the receiver/Serial RX interface. Use the UART3 MSP migration plan below instead.
- Rewrote MSP diagnostic layer in `src/comm/msp.h` and `src/comm/msp.cpp`.
- Added `src/fc_uart_probe_main.cpp`.
- Added PlatformIO env `esp32-s3-fc-uart-probe`.
- Verified builds:
  - `esp32-s3-fc-diag`: pass;
  - `esp32-s3-fc-uart-probe`: pass.
- Uploaded `esp32-s3-fc-uart-probe` to ESP32 on `COM55`.
- Historical evidence:
  - early UART6 probe samples did not receive valid replies;
  - USB-TTL and ESP32-side tests later showed R6/T6 is not a viable MSP path on this board.
- Correct MSP direction for all future tests:
  - request to FC: `$M<`, example `24 4D 3C 00 01 01`;
  - reply from FC: `$M>`, expected prefix `24 4D 3E`.
- Next acceptance check:
  - run the UART3 plan below, starting with USB-TTL proof on R3/T3.

## UART3 MSP Migration Plan

Goal: move the flight-controller MSP telemetry link away from UART6 R6/T6 and onto UART3 R3/T3, because the F4V3S PLUS manual identifies the upper R6/T6 pads as the receiver/Serial RX interface.

Status:

- [x] Determine from manual and bench evidence that R6/T6 should not be used for MSP.
- [x] Define UART3 wiring and Betaflight CLI workflow.
- [x] Create DeepSeek handoff workflow in `fc_uart3_msp_workflow.md`.
- [ ] USB-TTL proof on UART3: `#` enters CLI and `24 4D 3C 00 01 01` receives `24 4D 3E ...`.
- [ ] Update project comments/diagnostic banners from UART6/R6/T6 to UART3/R3/T3.
- [ ] Build/upload `esp32-s3-fc-uart-probe` and verify `rx=24 4D 3E ...`.
- [ ] Build/upload `esp32-s3-fc-diag` and verify `[FC] online=1`.
- [ ] Only after the diagnostic link is stable, integrate read-only FC telemetry into Web MVP.

Success criteria:

- UART3 MSP is proven first with USB-TTL, then with ESP32.
- MSP request direction remains `$M<`; FC reply direction remains `$M>`.
- No ESP32 motor/arming/control-write functions are enabled during bring-up.
- UART6 is reserved for receiver use or left unused during bench testing.

## OV5640/OV2640 Camera No-Image Bug Plan

Goal: restore visible camera image on the `esp32-s3-ov5640-diag` page after the page regressed to showing only the `capture.jpg` link.

Status:

- [x] Inspect current camera diagnostic HTML and stream handler.
- [x] Identify likely regression: the `<img>` element has no static `src` and depends entirely on JavaScript to assign `/stream`.
- [x] Try direct `/stream` image source; superseded by `/capture.jpg` polling because browser MJPEG display remained fragile.
- [x] Ensure non-stream HTTP responses close cleanly after sending.
- [x] Add no-store/no-cache headers so the browser does not reuse the broken old page.
- [x] Switch the root page to use `/capture.jpg` polling as the primary visible image path.
- [x] Keep `/stream` as an optional link rather than the only visible image path.
- [x] Build `esp32-s3-ov5640-diag`.
- [x] Upload to `COM55`.
- [x] Verify from current source that the page should show an image without relying on JS-only initialization.
- [x] Verify from COM55 serial that camera firmware boots and warm-up captures a JPEG frame.
- [x] Verify from COM55 serial that background capture continues and `err=0`.
- [x] Fix follow-up freeze where the page was smooth for about 30s and then stopped responding.
- [x] Replace fixed-interval image polling with one-in-flight `/capture.jpg` refresh.
- [x] Replace per-request `String` parsing in the diagnostic HTTP handler with fixed buffers.
- [x] Track JPEG cache capacity separately from current JPEG length to avoid repeated heap reallocations.
- [x] Add serial/status counters for HTTP requests, JPEG requests, status requests, write timeouts, bad requests, and frame age.
- [x] Build and upload the freeze-mitigation firmware to `COM55`.
- [x] Capture 70s serial evidence after upload showing stable heap/cache capacity and no camera errors without a connected browser.
- [x] Audit current PC-side network reachability without changing WiFi settings.
- [x] Add AP startup/status diagnostics to the camera diagnostic firmware.
- [x] Build and upload the AP-diagnostic camera firmware to `COM55`.
- [x] Capture serial evidence that the ESP32 reports AP started at `192.168.4.1`.
- [x] Move the camera AP from channel 6 to channel 1 for a compatibility retest.
- [x] Rebuild/upload the channel-1 firmware and capture serial evidence that `ap=ok`.
- [x] Repeat non-invasive Windows WLAN scan after the channel-1 upload.
- [x] Analyze phone `/status` evidence showing AP/camera/cache are valid but `/capture.jpg` writes timed out.
- [x] Relax the diagnostic HTTP request-line timeout and `/capture.jpg` write timeout.
- [x] Rebuild/upload the JPEG-timeout mitigation firmware to `COM55`.
- [x] Capture serial evidence that AP and camera still run after the JPEG-timeout mitigation.
- [x] Remove `availableForWrite()` gating from the cached JPEG writer after phone evidence showed every JPEG request still timed out.
- [x] Rebuild/upload the direct-write JPEG firmware to `COM55`.
- [x] Capture serial evidence that AP and camera still run after the direct-write fix.
- [x] Add a firmware tag to `/status` and the root HTML so stale phone/browser responses are obvious.
- [x] Send root HTML with explicit `Content-Length` through `writeClient()` instead of one large `printf`.
- [x] Rebuild/upload the tagged HTML firmware to `COM55`.
- [x] Capture serial evidence that AP and camera still run after the tagged HTML firmware.
- [x] Verify from an AP-connected client that the root page displays the camera image, not just the `capture.jpg` link.
- [x] Analyze tagged phone `/status` evidence showing the visible image is frozen because camera capture is offline.
- [x] Fix camera capture recovery so a failed `esp_camera_fb_get()` does not leave the page frozen on a stale cached frame.
- [x] Rebuild/upload the camera recovery firmware to `COM55`.
- [x] Add SCCB bus recovery before camera init for a stuck SIOD/SIOC state.
- [x] Recheck hardware side after repeated `ESP_ERR_NOT_FOUND`; SDA/SCL idle high and subsequent serial sample shows camera frames alive again.
- [ ] Verify from `/status` that `camera=true`, `age` stays low, and `frames` continues increasing while the phone page is open.
- [x] Clear the SCCB detection blocker: post-hardware-check serial shows `f=873 -> 1073`, `fps=4.9`, `err=0`, `cache>0`, and low `age`.
- [ ] AP-client/browser verification is pending: Windows can see `NMC-Camera`, but serial samples still show `sta=0 http=0 jpg=0 stat=0`.
- [x] Phone `/status` now proves AP client and live camera backend: `stations=1`, `camera=true`, `frames=169`, `fps=5`, `errors=0`, `cache=3022`, `age=505`.
- [x] Verify image path specifically: user confirmed `/capture.jpg?manual=1` shows a camera image and each refresh shows the live current frame.
- [x] Final `/status?fresh=jpg1` proves JPEG delivery: `jpg=12`, `timeouts=0`, `bad=0`, `camera=true`, `frames=947`, `fps=5`, `age=179`.

Success criteria:

- Root page HTML includes a direct `/capture.jpg` image source.
- JavaScript refreshes `/capture.jpg` with cache-busting timestamps; without JavaScript the first image should still load.
- `/stream` remains available as an optional diagnostic link.
- Build and upload both succeed.
- Serial log shows camera PID and warm-up JPEG capture.
- During a 60s+ no-client serial sample, `heap` and cache `cap` stay stable, `err=0`, and frame `age` does not grow.
- During phone testing, `/status` should keep responding and `timeouts` should stay low; if the page freezes again, these fields identify whether the browser link or camera capture path stalled.
- No flight-controller, motor, arming, RC override, or assist-output work is done.
- Hardware/browser verification still requires opening `http://192.168.4.1/` on a device connected to `NMC-Camera`; the current PC is on `stu-xdwlan`, did not see `NMC-Camera` in repeated non-invasive WLAN scans, while ESP32 serial reports `ap=ok`.
- Latest phone verification should first prove the current firmware by checking `fw=camdiag-20260704-html1` on the page or `"fw":"camdiag-20260704-html1"` plus increasing `seq` in `/status`.
- Once tagged firmware is proven, frozen image means the recovery target is `camera=true` and low `age`, not HTML delivery.
