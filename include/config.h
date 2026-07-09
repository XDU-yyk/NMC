/**
 * @file    config.h
 * @brief   NMC 智能无人机伞 — 全局配置 v2.0
 * 
 * 架构: ESP32-S3 (协处理器) + F4V3S PLUS (飞控) 双处理器
 * 
 * 本文件定义系统所有可调参数，涵盖传感器、通信、跟随、
 * Web 服务、摄像头及安全边界。修改前请确认硬件连接。
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ================================================================
 * 系统标识
 * ================================================================ */
#define FIRMWARE_VERSION    "v2.0.0-nmc"
#define DEVICE_NAME         "NMC-SmartUmbrella"
#define DEVICE_MDNS         "smart-umbrella"

/* ================================================================
 * 调试开关
 * ================================================================ */
#define DEBUG_ENABLE            1
#define DEBUG_SERIAL            Serial
#define DEBUG_BAUD              115200
#define LOG_TAG_FC              "[FC]"
#define LOG_TAG_FUSION          "[FUSION]"
#define LOG_TAG_FOLLOW          "[FOLLOW]"
#define LOG_TAG_WEB             "[WEB]"
#define LOG_TAG_UWB             "[UWB]"
#define LOG_TAG_GPS             "[GPS]"
#define LOG_TAG_TOF             "[TOF]"
#define LOG_TAG_CAM             "[CAM]"
#define LOG_TAG_MISSION         "[MISSION]"

#if DEBUG_ENABLE
  #define LOG(tag, fmt, ...)    DEBUG_SERIAL.printf("%s " fmt "\n", tag, ##__VA_ARGS__)
#else
  #define LOG(...)              ((void)0)
#endif

/* ================================================================
 * 引脚映射 (ESP32-S3-DevKitC-1)
 * 
 * 注意: 所有引脚均经过冲突检查, 但实际 PCB 布线可能需要调整。
 *       标注 [#] 的引脚在多外设同时使用时需特别注意。
 * ================================================================ */

/* ── F4V3S PLUS 飞控通信 (MSP 协议, UART2, ESP32 GPIO 16/17) ── */
/* 飞控端: 使用 UART3 端口 (R3 / T3, 2.0mm 焊盘)               */
/* 接线:    F4V3S T3/TX3 → ESP32 GPIO 17 (RX)                  */
/*          F4V3S R3/RX3 → ESP32 GPIO 16 (TX)                  */
/*          F4V3S GND    → ESP32 GND                           */
/* 注意: R6/T6 是 CRSF 接收机接口，不做 MSP。UART3 才是 MSP。    */
#define FC_UART_NUM         2
#define FC_RX_PIN           17
#define FC_TX_PIN           16
#define FC_BAUD             115200
#define FC_MSP_TIMEOUT_MS   100

/* ── GPS NEO-M8N (UART1) ── */
#define GPS_UART_NUM        1
#define GPS_RX_PIN          18
#define GPS_TX_PIN          15
#define GPS_BAUD            9600

/* ── VL53L1X ToF 激光测距 (I2C Wire) ── */
/* 引脚 GPIO4/5 不与摄像头 DVP 总线冲突，两个固件可共用接线。 */
/* 注意：原 GPIO4/5 曾预留为 UWB SPI SCK/MISO，UWB 无实物故让出。 */
#define TOF_I2C_PORT        Wire
#define TOF_SDA             4
#define TOF_SCL             5
#define TOF_I2C_FREQ        10000
#define TOF_TIMING_BUDGET_MS 100    // conservative bench diagnostic budget
#define TOF_XSHUT_PIN       10      // VL53L1X XSHUT, safe

/* ── UWB DW3000 (SPI2) [保留/未来扩展] ── */
/* 注意：UWB 不是当前硬件。以下引脚定义为未来扩展预留，
   在 UWB 模块实际购买、接线和测试之前不会启用。        */
#define UWB_SPI_HOST        SPI2_HOST
#define UWB_SCK             4
#define UWB_MISO            5
#define UWB_MOSI            21
#define UWB_CS              47
#define UWB_IRQ             48
#define UWB_RST             14

/* ── OV2640 摄像头 (DVP 8-bit 并行接口 + SCCB) ── */
// ESP32-S3 标准相机引脚配置
// 注意: Y8/D6(GPIO41) 和 Y5/D5(GPIO42) 仍用原引脚，不再与 ToF I2C 冲突
// ToF 已改为 GPIO4(SDA)/GPIO5(SCL)，两者可在不同固件中独立运行
#define CAM_PIN_PWDN        -1      // 未使用
#define CAM_PIN_RESET       -1      // 未使用 (或接 GPIO -1 表示软复位)
#define CAM_PIN_XCLK        -1      // 模块自带晶振，不接
#define CAM_PIN_SIOD        2       // SCCB SDA / SIOD (verified on current wiring)
#define CAM_PIN_SIOC        1       // SCCB SCL / SIOC (verified on current wiring)
#define CAM_PIN_Y9          39      // D7
#define CAM_PIN_Y8          41      // D6
#define CAM_PIN_Y7          42      // D5
#define CAM_PIN_Y6          12      // D4
#define CAM_PIN_Y5          11      // D3
#define CAM_PIN_Y4          14      // D2
#define CAM_PIN_Y3          9       // D1
#define CAM_PIN_Y2          8       // D0
#define CAM_PIN_VSYNC       6
#define CAM_PIN_HREF        7
#define CAM_PIN_PCLK        13

// 相机图像参数
#define CAM_FRAME_SIZE      FRAMESIZE_SVGA  // 800×600 JPEG
#define CAM_JPEG_QUALITY    12              // 0-63, 越小质量越高
#define CAM_FB_COUNT        2               // 帧缓冲数

/* ── 安全开关 (保留) ── */
#define SAFETY_SWITCH_PIN   3       // 低电平使能 (通过 MSP 转发给飞控)

/* ================================================================
 * 飞控接口 (MSP 协议) — 消息 ID 定义在 src/comm/msp.h enum MSP_CMD
 * ================================================================ */

// 飞控默认悬停油门 (MSP RC 通道中值)
#define FC_RC_MID           1500

/* ================================================================
 * 电池参数 (2S/3S/4S 兼容)
 * ================================================================ */
#define BATTERY_CELLS_DEFAULT   3       // 默认 3S (11.1V)
#define BATTERY_CELL_MIN        3.3f    // 单节最低电压
#define BATTERY_CELL_MAX        4.2f    // 单节满电电压
#define LOW_BATTERY_THRESH      (BATTERY_CELLS_DEFAULT * BATTERY_CELL_MIN)   // 低电压告警
#define CRITICAL_BATTERY        3.3f    // 临界电压 (单节) — 强制降落

/* ================================================================
 * 传感器采样频率 (Hz)
 * ================================================================ */
#define GPS_SAMPLE_RATE         10      // NEO-M8N 最高 10Hz
#define TOF_SAMPLE_RATE         1       // VL53L1X bench diagnostic rate
#define UWB_SAMPLE_RATE         20
#define FC_ATTITUDE_RATE        100     // 飞控姿态读取频率
#define FUSION_OUTPUT_RATE      100     // EKF 输出频率
#define CAMERA_FPS              15      // 图传帧率

/* ================================================================
 * 卡尔曼滤波器参数 (EKF v2 — 9状态)
 * ================================================================ */
// 状态向量: [x, y, z, vx, vy, vz, ax_bias, ay_bias, az_bias]
// 观测: GPS (x,y,z绝对) + UWB (x,y,z相对) + ToF (z低空) + FC气压计 (z)
#define KF_STATE_DIM            9
#define KF_PROCESS_NOISE_ACCEL  0.5f    // 加速度过程噪声
#define KF_GPS_NOISE_POS        2.0f    // GPS 位置测量噪声 (m)
#define KF_UWB_NOISE_POS        0.1f    // UWB 位置测量噪声 (m)
#define KF_TOF_NOISE_Z          0.03f   // ToF 高度测量噪声 (m)
#define KF_BARO_NOISE_Z         0.5f    // 气压计高度噪声 (m)

/* ================================================================
 * 跟随控制参数 (通过 MSP 输出姿态指令)
 * ================================================================ */

// 安全距离 & 高度 (mm)
#define FOLLOW_TARGET_DIST      1500.0f
#define FOLLOW_TARGET_HEIGHT    2000.0f
#define FOLLOW_MAX_DIST          5000.0f
#define FOLLOW_MIN_DIST           800.0f
#define FOLLOW_MAX_SPEED          2000.0f

// 高度保持
#define ALTITUDE_HOLD_DEADBAND    50.0f
#define ALTITUDE_MAX_RATE         500.0f

// 位置环 PID (ES32-S3 计算 → MSP 姿态指令给飞控)
#define POS_PID_P      0.8f
#define POS_PID_I      0.02f
#define POS_PID_D      0.05f
#define POS_PID_MAX    500.0f

#define ALT_PID_P       2.0f
#define ALT_PID_I       0.1f
#define ALT_PID_D       0.3f
#define ALT_PID_MAX     600.0f

/* ================================================================
 * Web 服务器 (运行于 ESP32-S3)
 * ================================================================ */
#define WEB_SERVER_PORT     80
#define WS_SERVER_PORT      81
#define WS_MAX_CLIENTS      4
#define WS_HEARTBEAT_MS     5000
#define TELEMETRY_INTERVAL  100     // 遥测广播周期 (ms)

/* ================================================================
 * FreeRTOS 任务配置
 * ================================================================ */
#define TASK_STACK_DEEP     8192
#define TASK_STACK_MEDIUM   4096
#define TASK_STACK_SHALLOW  2048

// 任务优先级 (数值越大优先级越高，ESP-IDF: 0–24)
#define PRIO_FC_COMM        6       // 飞控通信 (MSP 收发)
#define PRIO_GPS_READ       5       // GPS 数据读取
#define PRIO_TOF_READ       5       // ToF 测距
#define PRIO_UWB_READ       5       // UWB 定位读取
#define PRIO_FUSION         7       // 传感器融合 (EKF)
#define PRIO_FOLLOW         7       // 跟随决策
#define PRIO_MISSION        8       // 任务规划 (最高优先级)
#define PRIO_WEB_SERVER     4       // HTTP 请求处理
#define PRIO_WEB_SOCKET     5       // WebSocket 事件
#define PRIO_SAFETY         9       // 安全监控 (最高)
#define PRIO_TELEMETRY      3       // 遥测广播
#define PRIO_CAMERA         4       // 摄像头帧采集

// 任务间隔 (ms)
#define INTERVAL_FC_COMM        10
#define INTERVAL_GPS            100
#define INTERVAL_TOF            33
#define INTERVAL_UWB            50
#define INTERVAL_FUSION         10
#define INTERVAL_FOLLOW         50
#define INTERVAL_MISSION        20
#define INTERVAL_SAFETY         20
#define INTERVAL_TELEMETRY      100
#define INTERVAL_CAMERA         66      // ~15fps

/* ================================================================
 * 安全约束
 * ================================================================ */
#define MAX_TILT_ANGLE      35.0f
#define SIGNAL_LOST_TIMEOUT 3000     // 信号丢失超时 (ms)
#define RC_FAILSAFE_TIMEOUT 1000     // 遥控信号丢失超时 (ms)

/* ================================================================
 * 真机飞控输出安全门 (方向控制)
 * ================================================================
 *
 * ⚠️ 硬约束 (来自 AGENTS.md): 飞控损坏期间禁止任何真机 MSP 写入 /
 *    RC override / 解锁 / 电机测试。
 *
 * ENABLE_REAL_FC_OUTPUT = 0 时: 方向控制只驱动仿真 (fc_sim), 绝不写真飞控。
 * 只有在以下条件全部满足时, 才可改为 1:
 *   1) 已更换/修复飞控且通过 MSP 通信验证;
 *   2) 已完成无桨桌面检查、电机方向/顺序检查、拴绳受限测试;
 *   3) 遥控器一键接管已验证有效, 安全开关就位。
 *   4) Betaflight 已配置并无桨验证 MSP Override / MSP OVERRIDE 模式。
 * 即使置 1, 运行时仍由 FCBridge 执行在线/解锁/CH6/新鲜度多重门控。
 */
#ifndef ENABLE_REAL_FC_OUTPUT
#define ENABLE_REAL_FC_OUTPUT   0    // 默认禁用: 仅仿真, 不写真飞控
#endif

/*
 * Future FC-ready assist gate.
 *
 * MC6C channel plan:
 *   CH1 AIL -> roll
 *   CH2 ELE -> pitch
 *   CH3 THR -> throttle
 *   CH4 RUD -> yaw
 *   CH5/AUX1 -> arm/mode on the flight controller
 *   CH6/AUX2 -> ESP32 assist-output permission
 *
 * Even in an ENABLE_REAL_FC_OUTPUT build, ESP32 MSP RC output stays locked unless
 * the FC is online, Betaflight MSP_STATUS reports the ARM active-box bit, and
 * CH6/AUX2 is above this threshold.
 * Betaflight must also map CH6 high to MSP Override; CH6 here is only the
 * ESP32-side permission check.
 */
#ifndef REAL_FC_ASSIST_AUX_CHANNEL
#define REAL_FC_ASSIST_AUX_CHANNEL 6
#endif

#ifndef REAL_FC_ASSIST_AUX_MIN
#define REAL_FC_ASSIST_AUX_MIN     1700
#endif

// Receiver sanity thresholds for MC6C bring-up. These do not authorize flight;
// they only tell the Web page whether it is safe to continue no-prop bench checks.
#ifndef MC6C_RC_CENTER_MIN
#define MC6C_RC_CENTER_MIN         1400
#endif

#ifndef MC6C_RC_CENTER_MAX
#define MC6C_RC_CENTER_MAX         1600
#endif

#ifndef MC6C_RC_LOW_MAX
#define MC6C_RC_LOW_MAX            1200
#endif

#ifndef MC6C_RC_HIGH_MIN
#define MC6C_RC_HIGH_MIN           1700
#endif

// FC-ready builds must refresh real RC output continuously. If the Web/control
// loop stops refreshing, FCBridge stops repeating the last command.
#ifndef REAL_FC_OUTPUT_HOLD_MS
#define REAL_FC_OUTPUT_HOLD_MS     120
#endif

// ESP32 must not arm/disarm the flight controller in this project. Keep the
// API compiled for legacy modules, but make FCBridge::arm/disarm no-op unless a
// future bench-only build explicitly overrides this macro.
#ifndef ENABLE_ESP32_ARM_DISARM
#define ENABLE_ESP32_ARM_DISARM    0
#endif

// Legacy FollowController PID output is not part of the replacement-FC MC6C
// acceptance path. Keep it compiled for older code, but do not let it queue real
// FC output unless a future bench-only build explicitly opts in after the MC6C
// manual direction gate is proven.
#ifndef ENABLE_LEGACY_FOLLOW_FC_OUTPUT
#define ENABLE_LEGACY_FOLLOW_FC_OUTPUT 0
#endif

#endif // CONFIG_H
