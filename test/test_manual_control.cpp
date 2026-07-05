/**
 * @file    test_manual_control.cpp
 * @brief   方向控制核心逻辑 host 端单元测试 (不需要 ESP32 硬件)
 *
 * 编译运行 (在项目根目录):
 *   g++ -std=c++17 -I src test/test_manual_control.cpp src/control/manual_control.cpp -o /tmp/mc_test
 *   /tmp/mc_test
 *
 * 目的: 在飞控损坏、无法烧录真机的情况下, 证明"方向意图 → RC 通道"这条
 *       映射链 (死区/限幅/超时归中/手柄接管) 逻辑正确。这是"无硬件可验证"
 *       部分的核心证据。
 */

#include "control/manual_control.h"
#include <cstdio>
#include <cmath>
#include <limits>

static int g_pass = 0, g_fail = 0;

static void check(bool cond, const char* name) {
    if (cond) { g_pass++; printf("  [PASS] %s\n", name); }
    else      { g_fail++; printf("  [FAIL] %s\n", name); }
}

static void checkEq(uint16_t got, uint16_t exp, const char* name) {
    if (got == exp) { g_pass++; printf("  [PASS] %s (=%u)\n", name, got); }
    else            { g_fail++; printf("  [FAIL] %s (got %u, expected %u)\n", name, got, exp); }
}

int main() {
    printf("== 方向控制核心逻辑测试 ==\n");

    // 1) 中位: 零输入 → 姿态轴全 1500
    printf("\n[1] 零输入归中位\n");
    checkEq(mapAxisToRC(0.0f, MC_AXIS_SPAN, MC_DEADZONE), MC_RC_MID, "roll轴 0 -> 1500");
    checkEq(mapAxisToRC(0.0f, MC_AXIS_SPAN, MC_DEADZONE), MC_RC_MID, "pitch轴 0 -> 1500");

    // 2) 满偏: +1/-1 → 中值 ± span
    printf("\n[2] 满偏映射\n");
    checkEq(mapAxisToRC(+1.0f, MC_AXIS_SPAN, MC_DEADZONE), MC_RC_MID + MC_AXIS_SPAN, "满前 -> 1800");
    checkEq(mapAxisToRC(-1.0f, MC_AXIS_SPAN, MC_DEADZONE), MC_RC_MID - MC_AXIS_SPAN, "满后 -> 1200");

    // 3) 死区: 小输入被吃掉
    printf("\n[3] 死区抑制零点漂移\n");
    checkEq(mapAxisToRC(0.03f, MC_AXIS_SPAN, MC_DEADZONE), MC_RC_MID, "死区内 0.03 -> 1500");
    check(mapAxisToRC(0.5f, MC_AXIS_SPAN, MC_DEADZONE) > MC_RC_MID, "死区外 0.5 有输出");

    // 4) 限幅: 超范围输入不越界
    printf("\n[4] 限幅保护\n");
    check(mapAxisToRC(+5.0f, MC_AXIS_SPAN, MC_DEADZONE) <= MC_RC_MAX, "超大正输入 <=2000");
    check(mapAxisToRC(-5.0f, MC_AXIS_SPAN, MC_DEADZONE) >= MC_RC_MIN, "超大负输入 >=1000");
    checkEq(clampRC(3000), MC_RC_MAX, "clampRC(3000) -> 2000");
    checkEq(clampRC(-100), MC_RC_MIN, "clampRC(-100) -> 1000");
    checkEq(mapAxisToRC(std::numeric_limits<float>::quiet_NaN(), MC_AXIS_SPAN, MC_DEADZONE), MC_RC_MID, "NaN axis -> 1500");
    checkEq(mapAxisToRC(std::numeric_limits<float>::infinity(), MC_AXIS_SPAN, MC_DEADZONE), MC_RC_MID + MC_AXIS_SPAN, "Inf axis -> max span");

    // 5) 油门: 悬停中心 + 受限范围
    printf("\n[5] 油门映射\n");
    checkEq(mapThrottleToRC(0.0f, MC_DEADZONE), MC_THROTTLE_HOVER, "油门 0 -> 悬停1500");
    check(mapThrottleToRC(+1.0f, MC_DEADZONE) <= MC_THROTTLE_MAX, "满上 <=1800");
    check(mapThrottleToRC(-1.0f, MC_DEADZONE) >= MC_THROTTLE_MIN, "满下 >=1100");

    // 6) 组合映射: 前进+右 → pitch/roll 都 >1500
    printf("\n[6] 组合方向意图\n");
    checkEq(mapThrottleToRC(std::numeric_limits<float>::quiet_NaN(), MC_DEADZONE), MC_THROTTLE_HOVER, "NaN throttle -> hover");

    DirectionCommand cmd;
    cmd.forward = 0.8f; cmd.right = 0.5f; cmd.yaw = 0.0f; cmd.throttle = 0.0f;
    RcChannels ch = mapCommandToChannels(cmd);
    check(ch.pitch > MC_RC_MID, "前进 -> pitch>1500");
    check(ch.roll  > MC_RC_MID, "右移 -> roll>1500");
    checkEq(ch.yaw, MC_RC_MID, "无偏航 -> yaw=1500");

    // 7) 输入超时 → 自动归中位 (失控兜底)
    printf("\n[7] 输入超时归中位\n");
    ManualController mc;
    mc.begin();
    DirectionCommand fwd; fwd.forward = 1.0f;
    mc.setCommand(fwd, 1000);              // t=1000ms 给满前进
    RcChannels a = mc.update(1100);        // 100ms 后: 仍在超时窗内
    check(a.pitch > MC_RC_MID && !mc.isNeutralHold(), "超时窗内 pitch>1500 正常输出");
    RcChannels b = mc.update(1000 + MC_INPUT_TIMEOUT_MS + 50); // 超时后
    check(b.pitch == MC_RC_MID && mc.isNeutralHold(), "超时后 pitch归中 且 neutralHold");

    // 8) Web 暂停/接管语义 → 立即归中位, 忽略方向意图
    printf("\n[8] Web暂停/接管优先级\n");
    ManualController mc2;
    mc2.begin();
    mc2.setCommand(fwd, 2000);
    mc2.setPilotOverride(true);
    RcChannels c = mc2.update(2050);       // 虽在超时窗内, 但接管生效
    check(c.pitch == MC_RC_MID && mc2.isNeutralHold(), "暂停时 pitch归中");
    mc2.setPilotOverride(false);
    mc2.setCommand(fwd, 3000);
    RcChannels d = mc2.update(3050);
    check(d.pitch > MC_RC_MID, "释放接管后 恢复方向输出");

    printf("\n== 结果: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
