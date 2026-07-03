# No-FC 保底交付 — 证据包模板

## 一、验收清单

每个勾选需要对应证据（截图/日志/照片）存放在 `docs/evidence/` 目录下。

### D1 — ESP32-S3 Web/Sensor Demo

| # | 验收项 | 通过标准 | 证据 | ✅ |
|---|--------|---------|------|----|
| 1 | Web MVP 编译通过 | `pio run -e esp32-s3-web-mvp` 输出 SUCCESS | 编译日志 | ☐ |
| 2 | Web MVP 上传成功 | `pio run -e esp32-s3-web-mvp -t upload` 输出 SUCCESS | 上传日志 | ☐ |
| 3 | Web 面板加载 | 浏览器打开 `192.168.4.1` 正常显示 | 截图 | ☐ |
| 4 | `/ping` 响应 | 返回 `pong` | 截图/curl 结果 | ☐ |
| 5 | ToF 状态可见 | 面板 ToF 卡片显示 init/valid/distance | 截图 | ☐ |
| 6 | ToF 距离变化 | 伸手在传感器前移动，数值跟踪变化 | 视频/截图序列 | ☐ |
| 7 | GPS 状态可见 | 面板 GPS 卡片显示 online/sats/位置 | 截图 | ☐ |
| 8 | GPS 室外 fix | 室外可见卫星数≥5，HDOP≤2.0 | 串口日志 | ☐ |
| 9 | 飞控状态为离线 | 面板显示 ⛔ 离线（已损坏） | 截图 | ☐ |
| 10 | 参数接口可用 | `GET /api/params` 返回 JSON | 截图 | ☐ |
| 11 | 参数写入校验 | 白名单外参数被拒绝 | 串口日志 | ☐ |

### D2 — 摄像头预览

| # | 验收项 | 通过标准 | 证据 | ✅ |
|---|--------|---------|------|----|
| 1 | Camera 固件编译通过 | `pio run -e esp32-s3-ov5640-diag` SUCCESS | 编译日志 | ☐ |
| 2 | Camera 固件上传成功 | 上传到 ESP32-S3 正常启动 | 串口日志 | ☐ |
| 3 | AP 可见 | WiFi 列表中看到 `NMC-Camera` | 截图 | ☐ |
| 4 | `/ping` 响应 | 返回 `pong` | 截图 | ☐ |
| 5 | `/capture.jpg` 可用 | 浏览器打开返回 JPEG 图片 | 截图 | ☐ |
| 6 | `/stream` 可用 | 浏览器看到 MJPEG 实时画面 | 截图/视频 | ☐ |

### D3 — 证据包

| # | 证据项 | 说明 | ✅ |
|---|--------|------|----|
| 1 | Web 面板截图 | 显示 ToF/GPS/FC 状态完整页 | ☐ |
| 2 | ToF 串口日志 | `[SYS]` 行含 tof:init valid dist status | ☐ |
| 3 | GPS 串口日志 | `[SYS]` 行含 gps:online valid sats hdop | ☐ |
| 4 | 摄像头截图 | `/capture.jpg` 返回的图片 | ☐ |
| 5 | 接线照片 | 全部模块接好线的实物照片 | ☐ |
| 6 | 接线表 | 引脚对应关系表 | ☐ |
| 7 | 风险降级表 | 见下表 | ☐ |
| 8 | 替换飞控恢复计划 | 见 `fc_uart3_msp_workflow.md` | ☐ |

---

## 二、风险降级表

| 原计划 | 风险 | 降级后 | 影响 |
|--------|------|--------|------|
| F4V3S 飞控姿态稳定 + 手动飞行 | 飞控硬件损坏 | ESP32-S3 Web/Sensor 桌面演示 | 无法展示飞行能力 |
| MSP 通信 + 飞控数据集成 | 同上 | 删除 FC 数据展示，标注为离线 | FC 部分显示"损坏" |
| 近距离辅助跟随 | 同上 | 仅展示 ToF 前向测距能力 | 无法演示闭环控制 |
| 遥控器接管 + RC override | 同上 | 无遥控器演示 | 架构说明中解释 |
| 室外自主跟随 | 同上 | 仅展示 GPS 定位数据 | 无法演示跟随 |
| 摄像头飞行闭环视觉 | 同上 | 摄像头仅用于 Web 预览 | 降低为展示/调试用途 |
| UWB 定位 | 无此硬件 | 从功能列表中删除 | 仅作为未来方向提及 |

### 正面因素

- ESP32-S3 所有传感器驱动已验证（ToF 初始化、GPS NMEA 解析）
- Web 服务器 + JSON API + WebSocket 框架完整
- 参数白名单/范围校验已实现
- 固件编译通过，板级支持包完整
- 接线方案已确定，引脚冲突已解（ToF→GPIO4/5, 摄像头→GPIO41/42）
- 替换飞控后有明确的 UART3 MSP 恢复路线

---

## 三、证据存放结构

```
docs/evidence/
├── web_panel.png          # Web 面板完整截图
├── tof_serial.log         # 包含 ToF 数据的串口日志
├── gps_serial.log         # 包含 GPS 数据的串口日志
├── camera_capture.jpg     # 摄像头抓拍
├── wiring_photo.jpg       # 接线实物照片
├── build_web.log          # Web MVP 编译日志
├── build_camera.log       # 摄像头固件编译日志
└── risk_downgrade.md      # 本文件用作风险降级说明
```

---

## 四、上传与调试快速参考

```powershell
# Web MVP（ToF + GPS + Web）
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-web-mvp -t upload --upload-port COM55

# 串口监视
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe device monitor --port COM55 --baud 115200

# 摄像头固件
C:\Users\yyk\.platformio\penv\Scripts\platformio.exe run -e esp32-s3-ov5640-diag -t upload --upload-port COM55
```
