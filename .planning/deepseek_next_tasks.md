# DeepSeek 下一阶段任务单

## 当前已确认状态

- `esp32-s3-web-mvp` 目标已经能在安卓手机上打开。
- 当前 Web 面板使用 HTTP 轮询，不使用 WebSocket。
- `http://192.168.4.1/ping` 可返回 `pong`。
- `http://192.168.4.1/` 可打开面板。
- `http://192.168.4.1/api/telemetry` 已有模拟 JSON 数据。
- 暂时不要恢复 WebSocket；先把 HTTP 轮询链路做稳。

## DeepSeek 开始前必须阅读

1. `AGENTS.md`
2. `.planning/web_mvp_http_debug/task_plan.md`
3. `platformio.ini`
4. `src/web_main.cpp`
5. `src/web/server.h`
6. `src/web/server.cpp`
7. `src/sensors/tof.*`
8. `src/sensors/gps.*`
9. `src/comm/msp.*`

## 总原则

- 保持 `esp32-s3-web-mvp` 为零硬件依赖的工位调试目标。
- 不要恢复 WebSocket，除非 HTTP 轮询版稳定并且用户明确要求。
- 不要实现 UWB、高精度人体跟随、复杂户外自主飞行。
- 不要让 ESP32-S3 直接控制电机 PWM。
- 所有新硬件数据必须支持“硬件未接入时自动退回模拟数据”。
- 每一步都必须能单独编译、烧录、用手机验证。

## 阶段 1：整理 HTTP Telemetry 接口

目标：让 `/api/telemetry` 成为后续真实数据接入的稳定合同。

要做：

- 固定 JSON 字段名，不要随意改前端字段。
- 增加字段：
  - `tofOnline`
  - `gpsOnline`
  - `fcOnline`
  - `dataSource`，取值如 `sim`、`tof`、`gps`、`fc`、`mixed`
  - `errorFlags`
- 在 `src/web/server.cpp` 中保持 HTTP/1.0、`Content-Length`、`Connection: close` 的响应方式。
- 面板上显示每个数据源的 online/offline 状态。

验收：

- `pio run -e esp32-s3-web-mvp` 编译通过。
- 手机打开 `/api/telemetry` 能看到 JSON。
- 手机首页每秒刷新，不再 loading 卡死。
- 不接任何硬件时仍显示模拟数据。

## 阶段 2：接入 VL53L1X ToF，但保持可降级

目标：让面板优先显示真实 ToF 距离；未接硬件时继续模拟。

要做：

- 检查 `src/sensors/tof.*` 当前实现，不要重写一套重复驱动。
- 在 `web_main.cpp` 或一个很薄的 telemetry provider 中初始化 ToF。
- 如果初始化失败，记录串口日志并继续使用模拟 ToF。
- 如果初始化成功，将 `tofDistance` 改为真实读数。
- 对异常值做最小保护：超时、0 值、超量程时设置 `tofOnline=false`，不要卡住 HTTP 服务。

验收：

- 不接 ToF：页面仍工作，`tofOnline=false`，显示模拟或降级值。
- 接 ToF：移动物体时页面距离值变化。
- I2C/ToF 失败不能影响 `/ping` 和首页打开。

## 阶段 3：接入 GPS NEO-M8N，只做状态显示

目标：在面板上显示 GPS fix、卫星数、经纬度，不参与控制。

要做：

- 检查 `src/sensors/gps.*` 当前实现。
- 串口读取 NMEA，解析坐标、fix、卫星数。
- GPS 无 fix 时 `gpsOnline` 可为 true，但 `gpsValid=false`。
- GPS 不得用于当前阶段的跟随控制。

验收：

- 不接 GPS：页面仍工作，GPS 显示 offline/no fix。
- 接 GPS：室外有卫星数和坐标变化。
- GPS 读取不能阻塞 HTTP 服务。

## 阶段 4：接入 F4V3S MSP 只读状态

目标：先只读飞控状态，不发送控制指令。

要做：

- 检查 `src/comm/msp.*` 和 `src/comm/fc_bridge.*`。
- 只读取姿态、电压、armed、flight mode 等状态。
- 不实现 RC override，不发送辅助控制。
- 飞控未连接或 MSP 超时时，`fcOnline=false`，HTTP 服务继续工作。

验收：

- 不接飞控：页面仍工作。
- 接飞控：姿态/模式/armed 状态能在面板上显示。
- MSP 超时不能卡住 `/api/telemetry`。

## 阶段 5：补充参数调节，只改安全参数

目标：用 HTTP POST 或简单 GET 接口调整显示/辅助参数，但不控制飞行。

建议接口：

- `GET /api/params`
- `POST /api/params`

只允许白名单参数：

- `web_poll_ms`
- `follow_distance_m`
- `max_pitch_deg`
- `max_roll_deg`

验收：

- 参数有范围限制。
- 串口记录每次修改。
- 刷新页面后参数能显示当前值。
- 不允许浏览器写任意变量。

## 阶段 6：最后再考虑 WebSocket

只有在 HTTP 轮询版稳定后才考虑。

恢复 WebSocket 前必须满足：

- 首页稳定打开。
- `/api/telemetry` 稳定返回。
- ToF/GPS/飞控任一硬件异常都不会卡死页面。
- 用户明确要求更低延迟或更像实时仪表盘。

如果恢复 WebSocket：

- 保留 HTTP 轮询作为 fallback。
- WebSocket 失败时页面自动降级到 HTTP polling。
- 不要删除 `esp32-s3-raw-http` 诊断目标。

## 建议 DeepSeek 第一条任务

先做阶段 1：整理 `/api/telemetry` 合同和页面 online/offline 状态，不接硬件。

原因：

- 这是所有真实硬件数据接入的基础。
- 不需要硬件即可验证。
- 风险最低。
- 能避免后续 ToF/GPS/MSP 接入时字段混乱。
