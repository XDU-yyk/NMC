# OV5640 Web 图传任务计划

## Goal

开发 ESP32-S3 + OV5640 独立 Web 图传固件，用于比赛展示、工位调试和后续视觉辅助算法验证。

## Current Status

- [x] 确认 F4V3S R6/T6 不适合作为当前 MSP 外设口，飞控路线暂停。
- [x] 确认现有工程已有 OV2640 camera 骨架，但不适合作为 OV5640 第一版直接复用。
- [x] 确认本地 `esp_camera` 支持 `OV5640_PID`。
- [x] 确认当前 camera pin map 与 ToF/GPS 存在冲突。
- [x] 写出 `ov5640_web_workflow.md` 交接工作流。
- [ ] 阶段 1：实现并烧录 `esp32-s3-ov5640-diag` 串口诊断固件。
- [ ] 阶段 2：实现独立 AP Web 图传。
- [ ] 阶段 3：采集截图、串口日志和抓拍样张。

## Success Criteria

- 串口能打印 OV5640 PID/SCCB 地址或明确初始化错误。
- 能稳定抓 JPEG 帧并释放 framebuffer。
- 手机连接 `NMC-Camera` 后能访问 `http://192.168.4.1`。
- `/capture.jpg` 返回可打开的 JPEG。
- `/stream` 连续 60 秒不重启。
- 不启用 ToF/GPS/FC，不影响现有 Web MVP。

## Constraints

- 第一阶段必须 camera-only。
- 不做视觉跟随或飞控控制。
- 不宣传复杂环境自主飞行。
- 不复用会泄漏 framebuffer 的旧 `releaseFrame()` 逻辑。
- GPIO 冲突未解决前，不合入主 Web MVP。

## Errors Encountered

| Error | Evidence | Resolution |
|-------|----------|------------|
| F4V3S R6/T6 不响应 MSP/CLI | 用户 USB-TTL 测试无回应；说明书标注为 CRSF Receiver | 飞控 MSP 路线暂停，改做 OV5640 |
| Camera pin map 与 ToF/GPS 冲突 | `GPIO10/41/42/15` 已被 ToF/GPS 使用 | 先做独立 camera-only 固件 |

