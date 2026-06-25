# NMC Project Memory

This repository is for an embedded competition drone umbrella prototype. Future agents should treat this file as project-level memory before editing plans, reports, or implementation files.

## Core Architecture

- The realistic architecture is ESP32-S3 plus F4V3S PLUS.
- F4V3S PLUS owns low-level flight control: attitude stabilization, motor/ESC output, RC receiver takeover, arming, failsafe, and basic altitude/flight modes.
- ESP32-S3 owns upper-layer work only: sensor reading, Web UI, WebSocket telemetry, parameter tuning, camera preview, logging, and limited assist commands through MSP/UART or RC override.
- ESP32-S3 must not be described as directly driving motor PWM or replacing the flight controller safety loop.

## Feasible Delivery Scope

- MVP: stable manual flight through RC, Web status panel, telemetry display, parameter tuning, and desktop or tethered validation of assist logic.
- Standard target: near-range fixed-distance assist using ToF under constrained or low-altitude conditions.
- Challenge target: short outdoor low-speed following only in open space, low wind, and with immediate RC takeover.
- Do not make high-precision autonomous human following a required acceptance item unless new tracking hardware or reliable vision code exists.

## Sensor And Hardware Limits

- GPS plus one forward VL53L1X ToF cannot reliably identify a person or infer lateral position in complex environments.
- UWB is not part of the listed hardware. Mention it only as a future extension unless a real UWB module is purchased, wired, tested, and documented.
- OV2640 streaming should be low resolution and low frame rate, preferably MJPEG over a separate HTTP path, because it may compete with WebSocket telemetry for ESP32-S3 memory and bandwidth.
- Validate power integrity before flight: separate regulation for logic power, common ground, filtering, wiring fixation, and no ESP32-S3 reset when motors start.
- Any Web parameter write must use a whitelist, range checks, and logs. Never let the browser write arbitrary control variables.

## Safety Gates

- Keep the validation order: no-prop desktop checks, motor direction/order checks, tethered or constrained tests, low-altitude manual flight, assisted follow, then outdoor demo.
- Do not enable autonomous assist until manual hover is stable and RC takeover is verified.
- Assist commands must be limited. Default roll/pitch assist should stay within about 10 degrees, yaw rate within about 30 deg/s, and output should stop on lost sensor data or communication timeout.
- Any outdoor test needs open space, low wind, no nearby bystanders, logs/video, and a pilot ready to switch back to manual or disarm.

## Documentation Guidance

- `workflow.md` and `plan.md` are the grounded planning documents; preserve their conservative MVP/standard/challenge framing.
- `Summary.md` is more promotional and currently overstates "complex environment", "high precision", "human following", and ESP32-S3 AI acceleration. Revise those claims before using it in a report or defense.
- In reports and defense materials, say "near-range assist follow and situational awareness prototype" unless the hardware and tests prove stronger claims.
- Important deliverables for the competition are wiring/power diagrams, test logs, demo video, risk downgrade table, budget, and an acceptance checklist.

## Multi-Agent Collaboration

- The user may collaborate with DeepSeek on this project. Ask DeepSeek to read `AGENTS.md`, `workflow.md`, and `plan.md` before editing anything.
- DeepSeek should begin from the grounded MVP: RC-safe manual flight, Web telemetry/status, parameter tuning, and desktop or tethered assist validation.
- DeepSeek should not implement or document high-precision autonomous human following, UWB capability, or complex outdoor autonomy unless the hardware and tests actually support it.
- For each task, DeepSeek should state the target file(s), keep changes scoped, preserve the safety gates, and report any assumptions or unverified hardware facts.
