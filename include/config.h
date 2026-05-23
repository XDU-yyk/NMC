/**
 * @file    config.h
 * @brief   智能无人机伞 — 全局配置
 * 
 * 本文件定义系统所有可调参数，涵盖传感器、控制、跟随、
 * Web 服务及安全边界。修改前请确认硬件连接与安全约束。
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ================================================================
 * 系统标识
 * ================================================================ */
#define FIRMWARE_VERSION    "v1.0.0-nmc"
#define DEVICE_NAME         "NMC-SmartUmbrella"
#define DEVICE_MDNS         "smart-umbrella"

/* ================================================================
 * 调试开关
 * ================================================================ */
#define DEBUG_ENABLE            1
#define DEBUG_SERIAL            Serial
#define DEBUG_BAUD              115200
#define LOG_TAG_FUSION          "[FUSION]"
#define LOG_TAG_CTRL            "[CTRL]"
#define LOG_TAG_FOLLOW          "[FOLLOW]"
#define LOG_TAG_WEB             "[WEB]"
#define LOG_TAG_UWB             "[UWB]"

#if DEBUG_ENABLE
  #define LOG(tag, fmt, ...)    DEBUG_SERIAL.printf("%s " fmt "\n", tag, ##__VA_ARGS__)
#else
  #define LOG(...)              ((void)0)
#endif

/* ================================================================
 * 引脚映射 (ESP32-S3)
 * ================================================================ */

// --- UWB (DW3000) SPI ---
#define UWB_SPI_HOST        SPI2_HOST
#define UWB_SCK             12
#define UWB_MISO            13
#define UWB_MOSI            11
#define UWB_CS              10
#define UWB_IRQ             9
#define UWB_RST             8

// --- IMU (MPU6050 / ICM-20948) I2C ---
#define IMU_I2C_PORT        Wire
#define IMU_SDA             4
#define IMU_SCL             5
#define IMU_I2C_ADDR        0x68
#define IMU_I2C_FREQ        400000

// --- 气压计 (BMP280) I2C ---
#define BARO_I2C_PORT       Wire1
#define BARO_SDA            6
#define BARO_SCL            7
#define BARO_I2C_ADDR       0x76
#define BARO_I2C_FREQ       400000

// --- 电机 PWM (四旋翼 X 布局) ---
#define MOTOR1_PIN          40   // 前右
#define MOTOR2_PIN          41   // 后右
#define MOTOR3_PIN          42   // 后左
#define MOTOR4_PIN          43   // 前左

#define MOTOR_PWM_FREQ      490
#define MOTOR_PWM_RES       12     // 12-bit: 0–4095
#define MOTOR_IDLE_DUTY     200    // 怠速占空比
#define MOTOR_MAX_DUTY      3800

// --- 安全开关 ---
#define SAFETY_SWITCH_PIN   21    // 低电平使能电机

/* ================================================================
 * 传感器融合参数
 * ================================================================ */

// 采样频率 (Hz)
#define IMU_SAMPLE_RATE     200
#define BARO_SAMPLE_RATE    50
#define UWB_SAMPLE_RATE     20
#define FUSION_OUTPUT_RATE  100

// 卡尔曼滤波器参数
#define KF_PROCESS_NOISE_Q  0.01f   // 过程噪声协方差
#define KF_MEASURE_NOISE_R  0.1f    // 测量噪声协方差 (UWB)
#define KF_BARO_NOISE_R     0.05f   // 测量噪声协方差 (气压计高度)

// 互补滤波器系数 (加速度计/陀螺仪融合)
#define COMPLEMENTARY_ALPHA 0.96f   // 陀螺仪权重

// UWB 锚点坐标 (mm) — 基准坐标系
// Anchor0 为原点，其余锚点按实际布设填写
#define UWB_ANCHOR_COUNT    4
// 锚点坐标在 uwb.cpp 中以数组定义

/* ================================================================
 * 跟随控制参数
 * ================================================================ */

// 安全距离 & 高度
#define FOLLOW_TARGET_DIST      1500.0f  // 目标水平距离 (mm)
#define FOLLOW_TARGET_HEIGHT    2000.0f  // 目标悬停高度 (mm)
#define FOLLOW_MAX_DIST          5000.0f  // 最大跟随距离 (mm)
#define FOLLOW_MIN_DIST           800.0f  // 最小安全距离 (mm)
#define FOLLOW_MAX_SPEED          2000.0f  // 最大跟随速度 (mm/s)

// 高度保持
#define ALTITUDE_HOLD_DEADBAND    50.0f   // 高度死区 (mm)
#define ALTITUDE_MAX_RATE         500.0f  // 最大升降速率 (mm/s)

// 姿态控制 PID
#define PID_ROLL_P      1.8f
#define PID_ROLL_I      0.05f
#define PID_ROLL_D      0.15f
#define PID_ROLL_MAX    400.0f

#define PID_PITCH_P     1.8f
#define PID_PITCH_I     0.05f
#define PID_PITCH_D     0.15f
#define PID_PITCH_MAX   400.0f

#define PID_YAW_P       2.5f
#define PID_YAW_I       0.08f
#define PID_YAW_D       0.12f
#define PID_YAW_MAX     300.0f

#define PID_ALT_P       2.0f
#define PID_ALT_I       0.1f
#define PID_ALT_D       0.3f
#define PID_ALT_MAX     600.0f

/* ================================================================
 * Web 服务器
 * ================================================================ */
#define WEB_SERVER_PORT     80
#define WS_MAX_CLIENTS      4
#define WS_HEARTBEAT_MS     5000

/* ================================================================
 * FreeRTOS 任务
 * ================================================================ */
#define TASK_STACK_DEEP     8192
#define TASK_STACK_MEDIUM   4096
#define TASK_STACK_SHALLOW  2048

// 任务优先级 (数值越大优先级越高，0–24)
#define PRIO_SENSOR_READ    6
#define PRIO_FUSION         7
#define PRIO_CONTROL        8       // 姿态控制最高
#define PRIO_FOLLOW         7
#define PRIO_WEB_SERVER     4
#define PRIO_WEB_SOCKET     5
#define PRIO_SAFETY         9       // 安全监控最高
#define PRIO_TELEMETRY      3

// 任务间隔 (ms)
#define INTERVAL_SENSOR_READ    5     // 传感器读取 ~200Hz
#define INTERVAL_FUSION         10    // 融合更新 ~100Hz
#define INTERVAL_CONTROL        5     // 控制输出 ~200Hz
#define INTERVAL_FOLLOW         50    // 跟随决策 ~20Hz
#define INTERVAL_SAFETY         20    // 安全检查 ~50Hz
#define INTERVAL_TELEMETRY      100   // 遥测广播 ~10Hz

/* ================================================================
 * 安全约束
 * ================================================================ */
#define MAX_TILT_ANGLE      35.0f    // 最大倾角 (度)
#define LOW_BATTERY_THRESH  3.5f     // 低电压阈值 (V)
#define CRITICAL_BATTERY    3.3f     // 临界电压 — 强制降落
#define SIGNAL_LOST_TIMEOUT 3000     // 信号丢失超时 (ms)

#endif // CONFIG_H
