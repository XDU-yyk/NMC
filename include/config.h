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

/* ── F4V3S PLUS 飞控通信 (MSP 协议, UART2) ── */
#define FC_UART_NUM         2
#define FC_RX_PIN           17
#define FC_TX_PIN           16
#define FC_BAUD             115200
#define FC_MSP_TIMEOUT_MS   100

/* ── GPS NEO-M8N (UART1) ── */
#define GPS_UART_NUM        1
#define GPS_RX_PIN          18
#define GPS_TX_PIN          15
#define GPS_BAUD            115200

/* ── VL53L1X ToF 激光测距 (I2C Wire) ── */
#define TOF_I2C_PORT        Wire1
#define TOF_SDA             41
#define TOF_SCL             42
#define TOF_I2C_FREQ        10000
#define TOF_TIMING_BUDGET_MS 100    // conservative bench diagnostic budget
#define TOF_XSHUT_PIN       10      // VL53L1X XSHUT; conflicts with camera D2 if OV2640 is enabled later

/* ── UWB DW3000 (SPI2) ── */
#define UWB_SPI_HOST        SPI2_HOST
#define UWB_SCK             4
#define UWB_MISO            5
#define UWB_MOSI            21
#define UWB_CS              47
#define UWB_IRQ             48
#define UWB_RST             14

/* ── OV2640 摄像头 (DVP 8-bit 并行接口 + SCCB) ── */
// ESP32-S3 标准相机引脚配置
#define CAM_PIN_PWDN        -1      // 未使用
#define CAM_PIN_RESET       -1      // 未使用 (或接 GPIO -1 表示软复位)
#define CAM_PIN_XCLK        15
#define CAM_PIN_SIOD        33      // SCCB SDA
#define CAM_PIN_SIOC        34      // SCCB SCL
#define CAM_PIN_Y9          39      // D7
#define CAM_PIN_Y8          41      // D6
#define CAM_PIN_Y7          42      // D5
#define CAM_PIN_Y6          12      // D4
#define CAM_PIN_Y5          11      // D3
#define CAM_PIN_Y4          10      // D2
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

#endif // CONFIG_H
