/**
 * @file    uwb.h
 * @brief   UWB (DW3000) 定位驱动接口
 * 
 * 基于 Decawave DW3000 芯片，通过双边双向测距 (DS-TWR)
 * 实现厘米级精度定位。ESP32-S3 AI 向量指令集用于加速
 * 测距数据的矩阵运算。
 */

#ifndef UWB_H
#define UWB_H

#include "config.h"
#include <Arduino.h>

/**
 * @brief 三维坐标结构体 (单位: mm)
 */
struct Vec3 {
    float x, y, z;
    
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
    
    float norm() const {
        return sqrtf(x*x + y*y + z*z);
    }
    
    Vec3 operator-(const Vec3& v) const {
        return Vec3(x - v.x, y - v.y, z - v.z);
    }
    
    Vec3 operator+(const Vec3& v) const {
        return Vec3(x + v.x, y + v.y, z + v.z);
    }
    
    Vec3 operator*(float s) const {
        return Vec3(x * s, y * s, z * s);
    }
} __attribute__((aligned(16)));  // 16字节对齐，利于SIMD加载

/**
 * @brief 单次测距结果
 */
struct RangeResult {
    uint32_t timestamp;      // 时间戳 (ms)
    uint16_t anchorId;       // 锚点编号
    float    distance;       // 测距值 (mm)
    float    rssi;           // 信号强度 (dBm)
    uint8_t  quality;        // 测距质量 0–100
    bool     valid;          // 数据有效标志
};

/**
 * @brief UWB 定位驱动类
 * 
 * 特性：
 * - DS-TWR 双边双向测距，消减时钟漂移
 * - 多锚点三边定位
 * - 利用 ESP32-S3 SIMD 指令加速矩阵乘法
 * - 自适应锚点选择 (RSSI/质量排序)
 */
class UWBLocalizer {
public:
    /**
     * @brief 初始化 UWB 模块
     * @return true 初始化成功
     */
    bool begin();
    
    /**
     * @brief 执行一轮测距 (非阻塞，由状态机驱动)
     * 
     * 调用后需轮询 isRangingComplete() 以判断是否完成。
     * 典型周期: 50ms (20Hz)。
     */
    void startRanging();
    
    /**
     * @brief 判断当前轮测距是否完成
     */
    bool isRangingComplete();
    
    /**
     * @brief 获取最新定位结果
     * @param position 输出三维坐标 (mm)
     * @param confidence 输出置信度 0.0–1.0
     * @return true 定位有效
     */
    bool getPosition(Vec3& position, float& confidence);
    
    /**
     * @brief 获取指定锚点的测距结果
     */
    RangeResult getRangeResult(uint8_t anchorIdx);
    
    /**
     * @brief 获取所有锚点测距结果
     */
    void getAllRanges(RangeResult* results, uint8_t& count);
    
    /**
     * @brief 设置锚点坐标 (用于坐标解算)
     */
    void setAnchorPosition(uint8_t index, const Vec3& pos);
    
    /**
     * @brief 设备是否正常工作
     */
    bool isHealthy();
    
private:
    // DS-TWR 状态机
    enum RangingState {
        STATE_IDLE,
        STATE_TX_POLL,
        STATE_RX_RESPONSE,
        STATE_TX_FINAL,
        STATE_COMPLETE
    };
    
    RangingState m_state = STATE_IDLE;
    
    // 锚点坐标 (固定世界坐标系)
    Vec3  m_anchors[UWB_ANCHOR_COUNT];
    
    // 测距结果缓存
    RangeResult m_ranges[UWB_ANCHOR_COUNT];
    uint8_t     m_rangeCount = 0;
    uint32_t    m_lastRangingTime = 0;
    
    // 定位结果
    Vec3  m_position;
    float m_confidence = 0.0f;
    bool  m_positionValid = false;
    
    // SPI 通信 (DW3000)
    void spiWrite(uint16_t addr, const uint8_t* data, uint16_t len);
    void spiRead(uint16_t addr, uint8_t* data, uint16_t len);
    void dw3000Reset();
    bool dw3000Init();
    
    /**
     * @brief 三边定位解算 (最小二乘)
     * 
     * 利用 ESP32-S3 AI 向量指令集加速:
     * - 矩阵乘法和求逆使用 ESP-DSP 库的 SIMD 函数
     * - 16 字节对齐确保高效加载
     */
    bool solveTrilateration(float& x, float& y, float& z);
    
    /**
     * @brief 锚点选择策略: 按 RSSI 排序，取最优 N 个
     */
    void selectBestAnchors(uint8_t n, uint8_t* indices);
};

// 全局单例
extern UWBLocalizer uwb;

#endif // UWB_H
