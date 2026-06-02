# 智能无人机伞 — 模块并行开发工作流

> **用途**：个人开发路线图，按模块并行推进。
> **方案**：模块并行 + 接口约定（方案 B）。
> **原则**：只描述工作流，不涉及具体代码。伪代码仅用于说明逻辑关系和接口约定。

---

## 一、总体架构与模块划分

整个系统按功能拆为 **5 个开发模块**，模块间通过预定义的接口约定解耦：

```
┌─────────────────────────────────────────────────┐
│              模块 5：系统集成与测试               │
│         (户外飞行测试、安全验证、性能调优)          │
└──────────┬──────────┬──────────┬────────────────┘
           │          │          │
    ┌──────┴──┐ ┌─────┴───┐ ┌───┴──────────┐
    │ 模块 2  │ │ 模块 3  │ │  模块 4      │
    │飞控基础 │◄┤感知跟随 │▸│  Web 交互    │
    │  系统   │ │  系统   │ │   系统       │
    └────┬────┘ └────┬────┘ └──────┬───────┘
         │           │             │
         └───────────┴─────────────┘
                     │
              ┌──────┴──────┐
              │  模块 1     │
              │ 硬件平台搭建 │
              └─────────────┘
```

| 模块 | 名称 | 核心任务 | 依赖 |
|------|------|----------|------|
| M1 | 硬件平台搭建 | 组装、供电、接线、基础通电验证 | 无 |
| M2 | 飞控基础系统 | F4V3S 调参、电机/桨叶配置、遥控器对码、基础悬停 | M1 |
| M3 | 感知与跟随系统 | GPS/ToF/IMU/气压计驱动、多源融合、跟随算法 | M1（M2 仅需飞控通信协议） |
| M4 | Web 交互系统 | WebSocket 服务、摄像头推流、Web 前端、参数调节 | M1（独立于 M2/M3） |
| M5 | 系统集成与测试 | 模块联调、户外飞行验证、安全兜底、性能评估 | M2+M3+M4 |

### 关键设计决策

- M2、M3、M4 之间通过 **接口文档** 而非代码耦合。M3 输出"目标速度/方向指令"，M2 接收并执行；M4 读取 M3 的传感器数据用于展示，写入参数调节指令。
- M3 和 M4 可以并行开发（不同技术栈），M2 需要先完成才能联调飞行。
- 遥控器（MC6C）作为 M2 的安全兜底，贯穿始终。

---

## 二、模块接口约定（M2 / M3 / M4 之间的数据契约）

在并行开发前，先锁定三个模块间的数据格式和通信方式。这是方案 B 的核心——接口约定先行。

### 通信物理层

```
ESP32-S3 ──UART──→ F4V3S PLUS (飞控)
ESP32-S3 ──I²C───→ VL53L1X (ToF)
ESP32-S3 ──UART──→ NEO-M8N (GPS)
ESP32-S3 ──DVP───→ OV2640 (摄像头)
ESP32-S3 ──WiFi──→ 手机浏览器 (WebSocket)
```

### 数据帧约定

**M3 → M2：跟随控制指令**（ESP32-S3 发给飞控，50Hz）

```c
// 文件: common/protocol.h — 共享数据结构定义
typedef struct {
    float roll;      // 横滚角 [-30°, +30°]
    float pitch;     // 俯仰角 [-30°, +30°]
    float yaw;       // 偏航速率 [°/s]
    float throttle;  // 油门 [0, 1]
    uint8_t aux;     // 模式位: bit0=解锁, bit1=跟随模式, bit2=返航
} control_cmd_t;
```

**M3 → M4：传感器数据快照**（ESP32-S3 内部内存共享，10Hz）

```c
// 文件: common/sensor_data.h
typedef struct {
    float lat, lon;          // GPS 经纬度
    uint16_t tof_distance;   // 激光测距 mm
    float altitude_baro;     // 气压高度 m
    float heading;           // 航向角 °
    float accel[3], gyro[3]; // IMU 原始值
    uint8_t fix_type;        // GPS 定位类型
} sensor_snapshot_t;
```

**M4 → M3：参数调节指令**（Web 前端 → 跟随算法，JSON over 内部队列）

```json
{
  "cmd": "set_param",
  "key": "follow_distance",
  "value": 1500
}
```

### 文件包含关系

```
common/
├── protocol.h        // M2 引用, M3 引用  ← 控制帧定义
├── sensor_data.h     // M3 引用, M4 引用  ← 传感器快照定义
└── params.h          // M3 引用, M4 引用  ← 可调参数表

modules/
├── m2_flight_ctrl/   // 包含 common/protocol.h
├── m3_perception/    // 包含 common/protocol.h, sensor_data.h, params.h
└── m4_web/           // 包含 sensor_data.h, params.h
```

> 每个模块的 `.h` 文件都在 `common/` 下，三方引用同一份头文件保证结构一致。M2 只关心控制帧，M4 只关心数据和参数，M3 承上启下。

---

## 三、M1 — 硬件平台搭建

### 目标

完成全部硬件的物理组装、供电接线、基础通电验证，确保各模块能正常上电并被 ESP32-S3 识别。

### 工作流程（7 步）

```
Step 1: 机架组装
  ├─ 安装电机到机臂 → 固定电调 → 连接香蕉头电源线
  └─ 输出: 机架+动力系统物理就位

Step 2: 飞控安装与接线
  ├─ F4V3S 固定到机架中心 → 电调信号线接入飞控 PWM 口
  ├─ 飞控供电线接入分电板
  └─ 输出: 飞控-电调-电机链路连通

Step 3: 主控安装
  ├─ ESP32-S3 固定在机架上（避开电机干扰源）
  ├─ ESP32-S3 ←UART→ F4V3S 接线
  └─ 输出: 主控-飞控通信物理层就绪

Step 4: 传感器安装与接线
  ├─ NEO-M8N GPS → ESP32-S3 UART2
  ├─ VL53L1X ToF → ESP32-S3 I²C (SCL/SDA)
  ├─ OV2640 摄像头 → ESP32-S3 DVP 接口
  └─ 输出: 全部传感器接入主控

Step 5: 供电系统搭建
  ├─ 航模电池 → 分电板 → 飞控/电调（动力电）
  ├─ 分电板 → 降压模块 → ESP32-S3 + 传感器（5V/3.3V）
  └─ 输出: 双路供电隔离方案就绪

Step 6: 遥控器对码与安全配置
  ├─ MC6C 接收机 → F4V3S SBUS
  ├─ 遥控器通道映射、失控保护配置
  └─ 输出: 手动操控链路可用

Step 7: 通电验证
  ├─ 各模块上电检查（LED/蜂鸣器确认）
  ├─ ESP32-S3 烧录 blink 固件 → 串口输出确认
  ├─ I²C 扫描确认 VL53L1X 地址
  └─ 输出: 硬件全链路通电 OK
```

### 验证清单

| 序号 | 验证项 | 方法 | 通过标准 |
|------|--------|------|----------|
| V1.1 | 电机转向正确 | 逐个电机测试 | 全部按飞控预期方向旋转 |
| V1.2 | 飞控-主控 UART 通 | 飞控发心跳，ESP32 打印 | 串口收到稳定数据 |
| V1.3 | I²C 设备识别 | `i2c_scanner` 固件 | 读到 VL53L1X 地址 0x29 |
| V1.4 | GPS 定位 | 室外上电等待 | 串口输出有效 NMEA 语句 |
| V1.5 | 摄像头画面 | 拍照存 SD / 串口输出 | 图像数据可读取 |
| V1.6 | 遥控器响应 | Betaflight 中查看通道值 | 摇杆/开关映射正确 |

### 产出物

- `hardware/wiring_diagram.md` — 接线图记录
- `hardware/power_tree.md` — 供电树状图
- `hardware/verify_checklist.md` — 验证结果打勾表

---

## 四、M2 — 飞控基础系统

### 目标

完成 F4V3S PLUS 的固件配置、PID 调参、基础悬停能力，使飞行器在遥控器操控下稳定飞行，并通过 UART 正确响应主控指令。

### 工作流程（6 步）

```
Step 1: 飞控固件烧录与基础配置
  ├─ Betaflight/INAV 固件烧录
  ├─ 机架类型选择（自定义）→ 电机协议设置（DSHOT）
  ├─ 端口配置：UART1→SBUS(遥控器), UART3→MSP(ESP32-S3)
  └─ 输出: 飞控可正常连接调参软件

Step 2: 传感器校准
  ├─ 加速度计校准（六面校准）
  ├─ 陀螺仪校准
  ├─ 磁力计校准（室外远离金属）
  └─ 输出: 飞控姿态读数准确

Step 3: 电机与桨叶配置
  ├─ 电机转向确认 → 桨叶正反安装
  ├─ 电调校准（统一行程）
  └─ 输出: 动力系统就绪

Step 4: PID 基础调参
  ├─ 室内悬停测试 → 调整 Rate/P/I/D
  ├─ 抗风测试 → 微调 I 项
  └─ 输出: 稳定悬停 ±30cm

Step 5: 遥控器通道映射与模式开关
  ├─ ARM/DISARM 通道 → 解锁逻辑
  ├─ 飞行模式开关：自稳 / 半自稳 / 手动
  ├─ 失控保护：信号丢失 → 自动降落
  └─ 输出: 遥控器完整操控链路

Step 6: MSP 协议通信验证
  ├─ ESP32-S3 通过 MSP 读取飞控姿态 → 对比飞控 OSD 数据
  ├─ ESP32-S3 通过 MSP 发送控制指令 → 桌面级验证（不装桨）
  └─ 输出: 双向通信链路验证通过
```

### 核心数据流

```c
// m2_flight_ctrl/msp_link.c

// ESP32 → 飞控：发送控制指令（包含在 common/protocol.h）
void msp_send_rc(msp_handle_t* msp, const control_cmd_t* cmd) {
    // RC_CHANNELS_OVERRIDE 帧: roll/pitch/yaw/throttle → 对应飞控通道
    // 频率: 50Hz, 由 M3 跟随算法线程驱动
}

// 飞控 → ESP32：读取姿态数据
void msp_read_attitude(msp_handle_t* msp, attitude_t* att) {
    // MSP_ATTITUDE 响应: roll/pitch/yaw 角度
    // 频率: 10Hz, 供 M3 传感器融合使用
}
```

### 文件结构

```
modules/m2_flight_ctrl/
├── msp_link.h         // MSP 通信封装声明（read_attitude, send_rc, arm, disarm）
├── msp_link.c         // 基于 MSP 协议库实现
├── msp_protocol.h     // MSP 帧格式定义（参考 Betaflight MSP 文档）
└── README.md          // 飞控配置参数表（PID/Filter/Rates）
```

### 验证清单

| 序号 | 验证项 | 方法 | 通过标准 |
|------|--------|------|----------|
| V2.1 | 飞控姿态读数准确 | Betaflight 仪表盘对比 | 三轴偏差 < 1° |
| V2.2 | 电机解锁/锁定 | 遥控器 ARM 开关 | 电机启停正常 |
| V2.3 | 悬停稳定 | 室内操控悬停 30s | 水平漂移 < 30cm |
| V2.4 | MSP 读姿态 | ESP32 串口打印 | 与飞控 OSD 一致 |
| V2.5 | MSP 写控制 | 桌面级验证（无桨） | 电机转速随指令变化 |

---

## 五、M3 — 感知与跟随系统

### 目标

融合 GPS / ToF / 气压计 / IMU 多源数据，实现目标锁定、距离保持与航向跟随，输出控制指令给 M2。

### 工作流程（7 步）

```
Step 1: 传感器驱动层
  ├─ NEO-M8N UART 驱动 → NMEA 解析 → lat/lon/fix_type
  ├─ VL53L1X I²C 驱动 → 连续测距模式 → distance_mm
  ├─ 板载气压计读取（通过 MSP 从飞控获取）→ altitude_baro
  ├─ IMU 读取（通过 MSP 从飞控获取）→ accel/gyro
  └─ 输出: 四个传感器原始数据流就绪

Step 2: 传感器数据预处理
  ├─ GPS 坐标 → 本地 ENU 坐标系转换（以起飞点为原点）
  ├─ ToF 距离中值滤波（去野值）
  ├─ 气压高度滑动平均滤波（窗口 20）
  ├─ IMU 低通滤波 + 姿态解算
  └─ 输出: 预处理后的归一化数据（填充 sensor_snapshot_t）

Step 3: 多源融合定位
  ├─ 短距(<4m): ToF 为主, IMU 补偿姿态偏斜, GPS 消抖
  ├─ 中距(4-8m): GPS + ToF 卡尔曼加权, 气压计辅助高度
  ├─ 远距(>8m): GPS 为主, IMU 航位推算填隙
  └─ 输出: 目标相对位置 (x, y, z) 估计值 @ 10Hz

Step 4: 目标锁定逻辑
  ├─ 初始锁定: 以当前 GPS 坐标为锚点, 最近 ToF 目标为跟随对象
  ├─ 失锁判定: ToF 超量程 + GPS 漂移 > 阈值, 持续 > 2s
  ├─ 重锁: 原地旋转扫描 ToF + GPS 区域搜索
  └─ 输出: 锁定状态机 (LOCKED / LOST / SEARCHING)

Step 5: 跟随控制算法
  ├─ 距离环: PID 控制前后距离 (目标 1.5m) → 输出 pitch
  ├─ 航向环: P 控制左右偏移 → 输出 roll/yaw
  ├─ 高度环: PI 控制相对高度 → 输出 throttle
  └─ 输出: control_cmd_t 指令 @ 50Hz → 发送给 M2

Step 6: 安全边界
  ├─ 地理围栏: 距起飞点 > 30m → 自动减速悬停
  ├─ 低电量: 电压 < 阈值 → 强制返航（复用 M5）
  ├─ ToF 异常: 距离突变 > 1m/帧 → 丢弃, 惯性保持
  └─ 输出: 安全状态机 (SAFE / WARN / EMERGENCY)

Step 7: 参数热调接口
  ├─ 注册可调参数: follow_distance, pid_kp, pid_ki, pid_kd, max_speed...
  ├─ 参数变更回调: M4 通过 params.h 更新后立即生效
  └─ 输出: 参数表读写接口
```

### 核心数据流

```c
// m3_perception/sensor_fusion.c

// 传感器数据 → 预处理 → 融合 → 目标位置
void sensor_fusion_update(sensor_snapshot_t* raw, position_est_t* out) {
    // Step 2: 预处理
    gps_to_enu(&raw->lat, &raw->lon, &enu_x, &enu_y);
    tof_filtered = median_filter(raw->tof_distance, /*window=*/5);
    baro_filtered = moving_avg(raw->altitude_baro, /*window=*/20);
    
    // Step 3: 融合（根据距离选择策略）
    if (tof_filtered < 4000) {
        // 短距：ToF 主导
        out->x = enu_x * 0.2 + tof_x * 0.8;
    } else if (tof_filtered < 8000) {
        // 中距：卡尔曼加权
        kalman_update(&kf, enu_x, enu_y, tof_filtered, &out->x, &out->y);
    } else {
        // 远距：GPS 主导 + IMU 填隙
        out->x = enu_x;
        out->y = enu_y;
    }
    out->z = baro_filtered;
}

// m3_perception/follow_controller.c

// 目标位置 → PID 控制 → 飞控指令
void follow_control_step(position_est_t* target, control_cmd_t* cmd) {
    float dist_err = target->x - params.follow_distance;
    
    // 距离环 PID → pitch
    cmd->pitch = pid_compute(&pid_dist, dist_err);
    
    // 航向环 P → roll/yaw
    cmd->roll  = p_compute(target->y, params.lateral_gain);
    cmd->yaw   = p_compute(target->y, params.yaw_gain);
    
    // 高度环 PI → throttle
    cmd->throttle = pi_compute(&pi_alt, target->z);
    
    // 限幅
    clamp_cmd(cmd);
    cmd->aux = LOCK_MODE_BIT;  // 告知飞控当前为跟随模式
}
```

### 文件结构

```
modules/m3_perception/
├── drivers/
│   ├── gps_nmea.h / .c       // NEO-M8N 驱动
│   ├── tof_vl53l1x.h / .c    // VL53L1X 驱动
│   └── baro_msp.h / .c       // 气压计(通过 MSP 获取)
├── fusion/
│   ├── sensor_fusion.h / .c  // 多源融合主逻辑
│   ├── kalman_filter.h / .c  // 卡尔曼滤波器
│   └── coordinate.h / .c     // 坐标系转换
├── control/
│   ├── follow_controller.h/.c// 跟随 PID 控制器
│   ├── lock_state.h / .c     // 锁定状态机
│   └── safety_bound.h / .c   // 安全边界检查
├── params.h                  // 可调参数注册（关联 common/params.h）
└── m3_main.c                 // 模块入口: 数据采集→融合→控制 主循环
```

### 验证清单

| 序号 | 验证项 | 方法 | 通过标准 |
|------|--------|------|----------|
| V3.1 | GPS 坐标正确 | 串口输出 NMEA, 对比手机 GPS | 误差 < 2m |
| V3.2 | ToF 测距准确 | 卷尺对比 0.5/1/2/3m | 误差 < 3cm |
| V3.3 | 融合位置稳定 | 桌面模拟：固定目标记录波动 | 抖动 < 10cm |
| V3.4 | 锁定/失锁切换 | 遮挡 ToF 2s 看状态变化 | 正确进入 LOST→SEARCHING |
| V3.5 | PID 跟随模拟 | 桌面级：移动目标板看指令输出 | 指令方向与趋势正确 |
| V3.6 | 安全边界触发 | 模拟超距/低电压输入 | 状态机正确跳转 |

---

## 六、M4 — Web 交互系统

### 目标

在 ESP32-S3 上架设 WebSocket + HTTP 服务器，手机浏览器无需安装 App 即可查看飞行数据、调节参数、观看摄像头画面。

### 工作流程（6 步）

```
Step 1: WiFi 与 HTTP 服务器搭建
  ├─ ESP32-S3 AP 模式启动（SSID: DroneUmbrella_XXX）
  ├─ 轻量 HTTP 服务器启动（端口 80）
  ├─ 静态文件服务：HTML / CSS / JS 从 SPIFFS 读取
  └─ 输出: 手机连上 WiFi 后能打开 Web 页面

Step 2: WebSocket 服务端
  ├─ WebSocket 升级握手（同端口 80，路径 /ws）
  ├─ 双向消息通道：服务器 ↔ 浏览器
  ├─ 心跳保活：每 5s ping，10s 未响应断开
  └─ 输出: WebSocket 连接建立，双向通信可用

Step 3: 传感器数据实时推送
  ├─ 定时读取 sensor_snapshot_t（从 M3 共享内存）
  ├─ JSON 序列化 → WebSocket 广播（10Hz）
  ├─ 前端实时渲染：数据仪表盘、跟随状态、信号强度
  └─ 输出: 手机浏览器实时显示飞行数据

Step 4: 摄像头推流
  ├─ OV2640 初始化 → JPEG 抓帧（QVGA 320×240, 10fps）
  ├─ 方案选择:
  │   ├─ 方案 A: MJPEG over HTTP（简单，延迟 ~200ms）
  │   └─ 方案 B: 帧通过 WebSocket 推送（统一通道，延迟 ~100ms）
  ├─ 前端 <img> 或 <canvas> 渲染
  └─ 输出: 手机浏览器看到实时画面

Step 5: 参数调节面板
  ├─ 前端表单：跟随距离、PID 参数、最大速度等滑块/输入框
  ├─ 用户调整 → WebSocket 发送 JSON 指令
  ├─ ESP32 接收 → 写入 M3 params.h 参数表
  └─ 输出: 参数实时可调，生效延迟 < 100ms

Step 6: 前端 UI 完善
  ├─ 仪表盘布局：左侧数据面板 + 中央视频 + 右侧参数面板
  ├─ 移动端响应式适配（320px 宽起）
  ├─ 状态指示灯：连接状态 / 跟随模式 / 安全警告
  └─ 输出: 可用 UI 界面
```

### 核心数据流

```c
// m4_web/websocket_handler.c

// 传感器快照 → JSON → WebSocket 广播
void ws_broadcast_sensor_data(sensor_snapshot_t* snap) {
    // 填充在 common/sensor_data.h 定义的结构体
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumber(root, "lat",       snap->lat);
    cJSON_AddNumber(root, "lon",       snap->lon);
    cJSON_AddNumber(root, "distance",  snap->tof_distance);
    cJSON_AddNumber(root, "altitude",  snap->altitude_baro);
    cJSON_AddNumber(root, "fix_type",  snap->fix_type);
    cJSON_AddNumber(root, "heading",   snap->heading);
    
    char* json_str = cJSON_PrintUnformatted(root);
    ws_send_all(json_str);  // 广播给所有已连接客户端
    cJSON_free(json_str);
    cJSON_Delete(root);
}

// WebSocket 接收 → 参数更新
void ws_on_message(const char* msg) {
    cJSON* root = cJSON_Parse(msg);
    char* cmd = cJSON_GetStringValue(cJSON_GetObjectItem(root, "cmd"));
    
    if (strcmp(cmd, "set_param") == 0) {
        char* key = cJSON_GetStringValue(cJSON_GetObjectItem(root, "key"));
        float val = cJSON_GetNumberValue(cJSON_GetObjectItem(root, "value"));
        param_set(key, val);  // 写入 M3 参数表 (common/params.h)
    }
    cJSON_Delete(root);
}
```

```javascript
// m4_web/spiffs/www/app.js — 前端核心逻辑（伪代码）

const ws = new WebSocket(`ws://${location.host}/ws`);

ws.onmessage = (e) => {
    const data = JSON.parse(e.data);
    // 更新仪表盘
    document.getElementById('lat').textContent = data.lat.toFixed(6);
    document.getElementById('dist').textContent = (data.distance / 1000).toFixed(1) + 'm';
    document.getElementById('alt').textContent = data.altitude.toFixed(1) + 'm';
};

// 参数调节滑块 → 即时发送
document.querySelectorAll('.param-slider').forEach(slider => {
    slider.oninput = () => {
        ws.send(JSON.stringify({
            cmd: 'set_param',
            key: slider.dataset.key,
            value: parseFloat(slider.value)
        }));
    };
});
```

### 文件结构

```
modules/m4_web/
├── wifi_ap.h / .c            // WiFi AP 模式初始化
├── http_server.h / .c        // HTTP 静态文件服务
├── ws_handler.h / .c         // WebSocket 连接管理 + 消息处理
├── camera_stream.h / .c      // OV2640 抓帧 + MJPEG 编码
├── spiffs/
│   └── www/
│       ├── index.html        // SPA 入口
│       ├── app.js            // WebSocket 客户端 + 仪表盘逻辑
│       └── style.css         // 移动端响应式样式
└── m4_main.c                 // 模块入口: WiFi→HTTP→WS→Camera 初始化
```

```
common/
├── sensor_data.h             // M3 引用, M4 引用 ← 快照结构体
└── params.h                  // M3 引用, M4 引用 ← 参数注册表
```

### 验证清单

| 序号 | 验证项 | 方法 | 通过标准 |
|------|--------|------|----------|
| V4.1 | WiFi AP 可连接 | 手机搜索并连接 | 成功获取 IP |
| V4.2 | Web 页面加载 | 手机浏览器访问 192.168.4.1 | 页面完整显示 |
| V4.3 | WebSocket 连接 | 页面状态灯变绿 | 连接建立，心跳正常 |
| V4.4 | 数据实时刷新 | 晃动无人机看数值变化 | 延迟 < 200ms |
| V4.5 | 摄像头画面 | 页面中看到实时画面 | 帧率 > 5fps |
| V4.6 | 参数调节生效 | 拖动滑块, 串口打印参数值 | 参数即时更新 |

---

## 七、M5 — 系统集成与测试

### 目标

将 M2/M3/M4 集成联调，通过分级测试从桌面验证推进到户外飞行，确保系统稳定、安全、可演示。

### 工作流程（5 步）

```
Step 1: 桌面级集成（无桨）
  ├─ M2+M3 联调：M3 输出 control_cmd → M2 飞控响应（电机转速变化验证）
  ├─ M3+M4 联调：传感器数据 → Web 页面实时展示
  ├─ M4+M2 联调：Web 参数调节 → M2 响应验证
  └─ 输出: 三模块桌面级链路通畅（含日志）

Step 2: 约束飞行测试（绑绳/室内）
  ├─ 绑绳悬停：M3 锁定静态目标，观察悬停稳定性
  ├─ 约束跟随：目标慢速行走，无人机绑绳跟随
  ├─ Web 参数实时调参验证（在线调 PID）
  └─ 输出: 约束环境下跟随功能初步验证

Step 3: 户外飞行测试
  ├─ 场地选择: 开阔草地，GPS 信号良好，风速 < 3级
  ├─ 测试用例:
  │   ├─ TC3.1: 基础悬停 (无跟随, 30s)
  │   ├─ TC3.2: 近距离跟随 (1-2m, 步行速度)
  │   ├─ TC3.3: 正常跟随 (2-5m, 快步速度)
  │   ├─ TC3.4: 跟随转弯 (目标 90° 转弯)
  │   ├─ TC3.5: 遮挡恢复 (短暂遮挡 ToF, 自动重锁)
  │   ├─ TC3.6: 远距离测试 (8-15m GPS 主导)
  │   └─ TC3.7: Web 实时查看 + 参数调节
  └─ 输出: 测试记录表 + 飞行日志

Step 4: 安全兜底验证
  ├─ 遥控器切回手动模式（一键接管）
  ├─ 失控保护: 关闭遥控器 → 自动降落
  ├─ 低电量返航: 模拟电压下降 → 触发 RTL
  ├─ 地理围栏: 飞越 30m 边界 → 自动减速悬停
  └─ 输出: 所有安全机制验证通过

Step 5: 性能评估与调优
  ├─ 跟随精度: 统计各距离段的位置误差
  ├─ 响应延迟: 目标移动 → 无人机响应时间
  ├─ Web 延迟: 传感器数据到手机显示的时间
  ├─ 续航评估: 不同工作模式下的飞行时间
  └─ 输出: 性能评估报告
```

### 集成测试用例

```c
// tests/integration_test.h — 测试用例结构

typedef struct {
    const char* id;
    const char* name;
    bool (*run)(test_result_t* out);  // 测试函数指针
    float pass_threshold;
} test_case_t;

// 示例：TC3.2 近距离跟随测试
bool test_close_follow(test_result_t* out) {
    // 1. 目标以 0.5m/s 行走 10m
    // 2. 记录无人机位置 vs 目标位置
    // 3. 统计距离误差 RMSE
    out->metric = compute_rmse(log_drone_pos, log_target_pos);
    return out->metric < 0.3;  // 误差 < 0.3m 通过
}
```

### 测试记录模板

| 用例 | 环境 | 风速 | 结果 | 误差(m) | 备注 |
|------|------|------|------|---------|------|
| TC3.1 悬停 | 户外草地 | 2级 | ✅ | 0.15 | 正常 |
| TC3.2 近距跟随 | 户外草地 | 2级 | ✅ | 0.22 | — |
| TC3.3 正常跟随 | 户外草地 | 1级 | ⚠️ | 0.48 | 转弯时有超调 |
| TC3.5 遮挡恢复 | 户外草地 | 3级 | ✅ | — | 2.5s 重锁 |
| ... | ... | ... | ... | ... | ... |

### 文件结构

```
tests/
├── integration_test.h     // 测试用例注册表
├── desktop_test.c         // Step 1: 桌面级联调
├── outdoor_test.c         // Step 3: 户外飞行测试
└── safety_test.c          // Step 4: 安全兜底验证

docs/
├── test_log_template.md   // 测试记录模板
└── performance_report.md  // 性能评估报告模板
```

### 验证清单

| 序号 | 验证项 | 方法 | 通过标准 |
|------|--------|------|----------|
| V5.1 | 三模块桌面链路通 | 全模块上电，桌面测试脚本 | 数据流闭环 |
| V5.2 | 约束跟随可飞 | 绑绳测试 5 次 | 3/5 次稳定跟随 |
| V5.3 | 户外跟随精度 | TC3.2-TC3.6 全部执行 | RMSE < 0.5m |
| V5.4 | 安全兜底全触发 | 逐一触发安全条件 | 全部正确响应 |
| V5.5 | Web 交互可用 | 户外实测手机操控 | 延迟 < 500ms |

---

## 附录 A：全局文件树总览

```
drone_umbrella/
├── common/
│   ├── protocol.h          // M2↔M3 控制帧定义
│   ├── sensor_data.h       // M3↔M4 传感器快照定义
│   └── params.h            // M3↔M4 参数注册表
├── modules/
│   ├── m2_flight_ctrl/
│   │   ├── msp_link.h / .c
│   │   ├── msp_protocol.h
│   │   └── README.md
│   ├── m3_perception/
│   │   ├── drivers/
│   │   │   ├── gps_nmea.h / .c
│   │   │   ├── tof_vl53l1x.h / .c
│   │   │   └── baro_msp.h / .c
│   │   ├── fusion/
│   │   │   ├── sensor_fusion.h / .c
│   │   │   ├── kalman_filter.h / .c
│   │   │   └── coordinate.h / .c
│   │   ├── control/
│   │   │   ├── follow_controller.h / .c
│   │   │   ├── lock_state.h / .c
│   │   │   └── safety_bound.h / .c
│   │   ├── params.h
│   │   └── m3_main.c
│   └── m4_web/
│       ├── wifi_ap.h / .c
│       ├── http_server.h / .c
│       ├── ws_handler.h / .c
│       ├── camera_stream.h / .c
│       ├── spiffs/www/
│       │   ├── index.html
│       │   ├── app.js
│       │   └── style.css
│       └── m4_main.c
├── tests/
│   ├── integration_test.h
│   ├── desktop_test.c
│   ├── outdoor_test.c
│   └── safety_test.c
├── hardware/
│   ├── wiring_diagram.md
│   ├── power_tree.md
│   └── verify_checklist.md
└── docs/
    ├── test_log_template.md
    └── performance_report.md
```

---

## 附录 B：开发时序建议

M1 完成后，建议推进顺序：

```
M1 (硬件) ──完成──→ M2 (飞控) ──基础悬停OK──→ M5 Step1 (桌面集成)
                          │                          │
                          └────并行────→ M3 (感知) ──┘
                          │                          │
                          └────并行────→ M4 (Web)  ──┘
                                                     │
                                          M5 Step2-5 (飞行测试)
```

> - M2 是 M3 飞行联调的前置，但 M3 的传感器驱动和融合逻辑可以独立于 M2 在桌面开发（读取模拟数据）。
> - M4 完全独立，随时可以开发。
> - 三个模块可在一人开发时串行推进，也可多人并行。
> - 遥控器始终作为 M2 的安全兜底，任何阶段出现异常都可切回手动接管。
