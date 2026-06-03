# 智能无人机伞模块并行开发工作流

> 用途：嵌赛项目开发路线图与模块协作约定。
> 核心策略：F4V3S PLUS 负责底层飞控闭环，ESP32-S3 负责传感器、Web 交互、上层跟随逻辑和演示数据。
> 安全原则：任何自主逻辑都不能绕过遥控器接管、失控保护和无桨/绑绳测试流程。

---

## 一、总体架构

本作品定位为“智能无人机伞原型”，优先完成可安全演示的系统闭环：飞控稳定飞行、手机 Web 态势感知、参数调节、近距离辅助跟随。高精度全自主人体跟随作为挑战目标，不作为基础验收前置。

```text
手机浏览器
  │  WiFi / HTTP / WebSocket
  ▼
ESP32-S3
  ├─ Web 服务与前端页面
  ├─ GPS / ToF / 摄像头数据读取
  ├─ 传感器预处理与上层跟随逻辑
  └─ MSP / UART 与飞控通信
       │
       ▼
F4V3S PLUS 飞控
  ├─ 姿态闭环
  ├─ 电机/电调输出
  ├─ 气压计/IMU 数据
  ├─ 遥控器接管
  └─ 失控保护
```

### 模块划分

| 模块 | 名称 | 核心任务 | 依赖 | 验收优先级 |
| --- | --- | --- | --- | --- |
| M1 | 硬件平台搭建 | 组装、供电、接线、基础通电验证 | 无 | 必做 |
| M2 | 飞控基础系统 | F4V3S 配置、遥控器、PID、基础悬停、安全保护 | M1 | 必做 |
| M3 | 感知与跟随系统 | GPS/ToF/气压/IMU 数据融合、跟随辅助、限幅控制 | M1+M2 通信 | 必做+挑战 |
| M4 | Web 交互系统 | HTTP/WebSocket、数据面板、参数调节、低帧率图传 | M1 | 必做 |
| M5 | 系统集成与测试 | 桌面联调、绑绳测试、户外演示、性能评估 | M2+M3+M4 | 必做 |

### 关键设计决策

- ESP32-S3 不直接控制电机 PWM，只通过 MSP/UART 向飞控发送模式、RC 辅助或速度类指令。
- F4V3S 始终保留遥控器通道优先级，遥控器可一键切回手动/自稳模式。
- GPS 和单个 VL53L1X 不能保证复杂环境下稳定识别人，因此基础演示采用“定高 + 前向定距 + Web 态势感知”；若补充 UWB 或视觉识别，再升级为更可靠的人体跟随。
- 摄像头图传采用低分辨率、低帧率方案，避免影响 WebSocket 状态数据和控制通信。

---

## 二、模块接口约定

### 通信物理层

```text
ESP32-S3 ──UART/MSP── F4V3S PLUS 飞控
ESP32-S3 ──I2C────── VL53L1X ToF
ESP32-S3 ──UART───── NEO-M8N GPS
ESP32-S3 ──DVP────── OV2640 摄像头
ESP32-S3 ──WiFi───── 手机浏览器
遥控器接收机 ──SBUS/IBUS── F4V3S PLUS 飞控
```

### M3 -> M2：上层控制指令

发送频率建议 20-50Hz。实际落地时优先映射为飞控支持的 MSP RC override 或模式通道，避免自定义协议无法被飞控接收。

```c
typedef struct {
    float roll;      // 横滚辅助量，限幅建议 [-10, +10] deg
    float pitch;     // 俯仰辅助量，限幅建议 [-10, +10] deg
    float yaw_rate;  // 偏航速率辅助量，限幅建议 [-30, +30] deg/s
    float throttle;  // 油门/高度辅助量，必须受飞控模式限制
    uint8_t mode;    // bit0=enable_assist, bit1=follow, bit2=rtl_request
} assist_cmd_t;
```

### M2 -> M3/M4：飞控状态

```c
typedef struct {
    float roll;
    float pitch;
    float yaw;
    float altitude_baro;
    float battery_voltage;
    uint8_t armed;
    uint8_t failsafe;
    uint8_t flight_mode;
} flight_state_t;
```

### M3 -> M4：传感器快照

```c
typedef struct {
    double lat;
    double lon;
    uint8_t gps_fix;
    uint16_t tof_distance_mm;
    float altitude_baro;
    float heading_deg;
    float follow_error_m;
    uint8_t lock_state;   // 0=idle, 1=locked, 2=lost, 3=searching
    uint8_t safety_state; // 0=safe, 1=warn, 2=emergency
} sensor_snapshot_t;
```

### M4 -> M3：参数调节指令

```json
{
  "cmd": "set_param",
  "key": "follow_distance_m",
  "value": 1.5
}
```

参数必须做白名单和范围限制，不能让 Web 前端直接写入任意变量。

| 参数 | 建议范围 | 说明 |
| --- | --- | --- |
| follow_distance_m | 1.0 - 3.0 | 跟随/定距目标距离 |
| max_pitch_deg | 3 - 10 | 自主辅助最大俯仰角 |
| max_roll_deg | 3 - 10 | 自主辅助最大横滚角 |
| max_yaw_rate_dps | 10 - 30 | 最大偏航速率 |
| web_push_hz | 2 - 10 | Web 状态推送频率 |

---

## 三、M1 硬件平台搭建

### 目标

完成物理装配、供电隔离、接线检查和基础通电验证。所有电机测试先不装桨。

### 工作流程

```text
Step 1: 机架与动力系统
  - 安装机架、电机、电调、桨叶保护
  - 检查电机线、电源线、香蕉头焊点和绝缘

Step 2: 飞控安装
  - F4V3S 固定在重心附近，方向标识与机头一致
  - 电调信号线接入飞控输出口

Step 3: ESP32-S3 与传感器安装
  - ESP32-S3 远离电调和电机线
  - GPS 放到相对开阔位置
  - ToF 朝向跟随目标方向
  - OV2640 固定后避免排线受力

Step 4: 供电系统
  - 动力电给电调/飞控
  - 降压后给 ESP32-S3 和传感器
  - 共地连接，但避免大电流经过逻辑地线

Step 5: 遥控器与失控保护
  - 接收机连接飞控
  - 设置 ARM、飞行模式、手动接管和 failsafe

Step 6: 基础通电验证
  - ESP32-S3 blink/串口日志
  - I2C 扫描 VL53L1X
  - GPS 输出 NMEA
  - 摄像头初始化成功
```

### 验收清单

| 编号 | 验证项 | 通过标准 |
| --- | --- | --- |
| V1.1 | 电源稳定 | 电机启动时 ESP32 不复位 |
| V1.2 | 飞控方向 | 调参软件中姿态方向正确 |
| V1.3 | 遥控器通道 | 摇杆、开关映射正确 |
| V1.4 | ToF | 0.5-3m 距离读数变化合理 |
| V1.5 | GPS | 室外可获取有效定位 |
| V1.6 | 摄像头 | QVGA 抓帧成功 |

---

## 四、M2 飞控基础系统

### 目标

让飞行器在遥控器操控下稳定、安全地起飞、悬停和降落，并提供 ESP32-S3 可读取的飞控状态。

### 工作流程

```text
Step 1: 固件与机型配置
  - 确认使用 Betaflight 或 INAV
  - 配置机架类型、电机协议、端口功能

Step 2: 传感器与遥控器校准
  - 加速度计/陀螺仪校准
  - 遥控器通道、端点、中位值校准

Step 3: 电机验证
  - 无桨检查电机顺序与转向
  - 确认解锁、锁定和急停流程

Step 4: 手动悬停
  - 从低高度、短时间开始
  - 记录 PID、滤波、Rate 参数

Step 5: MSP 通信
  - ESP32-S3 读取姿态、高度、电压、模式
  - 桌面无桨验证辅助控制指令
```

### 注意事项

- 先完成手动可飞，再接入任何自主控制。
- 自主辅助指令必须限幅，默认最大俯仰/横滚不超过 10 度。
- Web 或 ESP32-S3 异常时，飞控应保持手动/自稳模式，不依赖 ESP32-S3 保命。

---

## 五、M3 感知与跟随系统

### 目标

完成传感器数据读取、预处理和近距离辅助跟随逻辑。当前器材条件下，不承诺复杂环境中的高精度人体识别；基础演示以“前向定距 + 定高 + 状态感知”为主。

### 工作流程

```text
Step 1: 传感器驱动
  - NEO-M8N: NMEA 解析，获取经纬度/fix
  - VL53L1X: 连续测距，检测异常跳变
  - F4V3S: 通过 MSP 获取姿态、气压高度、电压

Step 2: 数据预处理
  - GPS 转本地 ENU 坐标，仅用于慢速位置参考和围栏
  - ToF 做中值滤波和超量程判定
  - 高度做滑动平均
  - 姿态用于补偿 ToF 安装角度

Step 3: 状态机
  - IDLE: 手动/待机
  - ASSIST: 定距辅助
  - LOST: 测距丢失或 GPS 异常
  - EMERGENCY: 低电量、通信中断、超距

Step 4: 控制输出
  - 距离误差 -> pitch 辅助
  - 横向目标不明确时不输出 roll，避免误跟随
  - 高度控制优先交给飞控定高模式，ESP32 只做小幅辅助

Step 5: 参数热调
  - Web 修改参数
  - 参数白名单校验
  - 串口日志记录每次修改
```

### 验收清单

| 编号 | 验证项 | 通过标准 |
| --- | --- | --- |
| V3.1 | GPS 数据 | 室外坐标稳定输出 |
| V3.2 | ToF 数据 | 固定距离波动在可接受范围内 |
| V3.3 | 飞控状态 | 姿态/高度/电压可读 |
| V3.4 | 桌面闭环 | 移动目标板时 pitch 辅助方向正确 |
| V3.5 | 限幅 | 任意参数下控制输出不越界 |
| V3.6 | 失锁 | ToF 遮挡后进入 LOST，不继续盲目加速 |

---

## 六、M4 Web 交互系统

### 目标

在 ESP32-S3 上提供无需 App 的手机 Web 控制台，完成状态显示、参数调节和低帧率图传。

### 工作流程

```text
Step 1: WiFi AP
  - SSID: DroneUmbrella_XXXX
  - 手机连接后访问 192.168.4.1

Step 2: HTTP 静态页面
  - SPIFFS/LittleFS 存放 index.html、style.css、app.js
  - 页面尽量轻量，避免大体积依赖

Step 3: WebSocket 状态通道
  - 2-10Hz 推送飞控状态和传感器快照
  - 心跳超时后自动断开

Step 4: 参数调节
  - 前端滑块/输入框
  - 后端白名单、范围检查、日志记录

Step 5: 摄像头画面
  - 优先 QVGA MJPEG over HTTP
  - 若内存不足，降低帧率或作为可选演示项
```

### Web 消息示例

```json
{
  "type": "telemetry",
  "armed": 1,
  "mode": "ANGLE",
  "altitude": 1.2,
  "tof_mm": 1450,
  "battery": 11.1,
  "lock_state": "ASSIST"
}
```

### 验收清单

| 编号 | 验证项 | 通过标准 |
| --- | --- | --- |
| V4.1 | WiFi AP | 手机可连接并打开页面 |
| V4.2 | WebSocket | 状态灯正常，断线可重连 |
| V4.3 | 数据展示 | 姿态/高度/测距/电压实时刷新 |
| V4.4 | 参数修改 | 修改后 ESP32 串口可看到更新 |
| V4.5 | 摄像头 | 能显示低帧率画面，不影响状态刷新 |

---

## 七、M5 系统集成与测试

### 目标

按风险从低到高推进：无桨桌面 -> 绑绳 -> 低高度手动 -> 近距离辅助 -> 户外演示。

### 测试流程

```text
Step 1: 无桨桌面测试
  - 验证通信、Web、传感器、参数调节
  - 模拟目标距离变化，检查控制输出方向和限幅

Step 2: 绑绳/约束测试
  - 验证定高、定距和遥控器接管
  - 任意振荡立即停测

Step 3: 低高度手动飞行
  - 验证飞控稳定性和 Web 状态展示
  - 不启用自主跟随

Step 4: 近距离辅助跟随
  - 目标低速移动
  - ESP32 只输出小幅辅助量
  - 遥控器保持随时接管

Step 5: 户外演示
  - 开阔场地、低风速、无围观人员靠近
  - 记录视频、日志和参数
```

### 安全门槛

| 门槛 | 要求 |
| --- | --- |
| 进入电机测试前 | 接线、电源、遥控器 failsafe 全部通过 |
| 进入装桨测试前 | 无桨电机顺序、转向、解锁/锁定验证通过 |
| 进入自主辅助前 | 手动悬停稳定，遥控器接管可用 |
| 进入户外测试前 | 绑绳测试无明显振荡，低电量和通信中断保护可触发 |

### 性能记录指标

| 指标 | 记录方式 |
| --- | --- |
| Web 延迟 | ESP32 时间戳到浏览器显示时间 |
| 测距稳定性 | 固定距离下 ToF 波动 |
| 悬停稳定性 | 30s 内高度/水平漂移 |
| 辅助跟随响应 | 目标移动到控制输出变化的延迟 |
| 续航 | 不同模式下飞行时间 |

---

## 八、全局文件结构建议

```text
drone_umbrella/
├── common/
│   ├── assist_cmd.h
│   ├── flight_state.h
│   ├── sensor_snapshot.h
│   └── params.h
├── modules/
│   ├── m2_flight_ctrl/
│   │   ├── msp_link.h
│   │   ├── msp_link.c
│   │   └── README.md
│   ├── m3_perception/
│   │   ├── drivers/
│   │   ├── fusion/
│   │   ├── control/
│   │   └── m3_main.c
│   └── m4_web/
│       ├── wifi_ap.c
│       ├── http_server.c
│       ├── ws_handler.c
│       ├── camera_stream.c
│       └── spiffs/www/
├── hardware/
│   ├── wiring_diagram.md
│   ├── power_tree.md
│   └── verify_checklist.md
├── tests/
│   ├── desktop_test.md
│   ├── tethered_test.md
│   └── outdoor_test.md
└── docs/
    ├── performance_report.md
    ├── demo_script.md
    └── budget.md
```

---

## 九、开发时序建议

```text
M1 硬件平台
  -> M2 手动可飞与安全保护
      -> M5 约束/户外测试
  -> M3 传感器与辅助跟随
      -> M5 桌面闭环
  -> M4 Web 交互
      -> M5 演示系统
```

如果时间不足，优先保留：

1. 遥控器安全飞行。
2. Web 实时状态面板。
3. 参数调节。
4. 桌面或绑绳状态下的定距辅助跟随。

UWB、高精度人体跟随、复杂环境自主避障作为后续扩展，不写成当前版本必然完成项。
