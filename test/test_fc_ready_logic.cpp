/**
 * @file test_fc_ready_logic.cpp
 * @brief Host tests for FC-ready MSP RC ordering and assist gate.
 */

#include "comm/fc_rc_mapping.h"
#include "comm/msp_status_flags.h"
#include <cstdio>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char* name)
{
    if (cond) {
        g_pass++;
        std::printf("  [PASS] %s\n", name);
    } else {
        g_fail++;
        std::printf("  [FAIL] %s\n", name);
    }
}

static void checkEq(uint16_t got, uint16_t exp, const char* name)
{
    if (got == exp) {
        g_pass++;
        std::printf("  [PASS] %s (=%u)\n", name, got);
    } else {
        g_fail++;
        std::printf("  [FAIL] %s (got %u, expected %u)\n", name, got, exp);
    }
}

static void checkEq32(int32_t got, int32_t exp, const char* name)
{
    if (got == exp) {
        g_pass++;
        std::printf("  [PASS] %s (=%ld)\n", name, static_cast<long>(got));
    } else {
        g_fail++;
        std::printf("  [FAIL] %s (got %ld, expected %ld)\n",
                    name,
                    static_cast<long>(got),
                    static_cast<long>(exp));
    }
}

static void copyChannels(const uint16_t src[16], uint16_t dst[16])
{
    for (uint8_t i = 0; i < 16; i++) {
        dst[i] = src[i];
    }
}

int main()
{
    std::printf("== FC-ready MSP RC logic tests ==\n");

    std::printf("\n[1] Betaflight RAW_RC functional order\n");
    FCRawRCValues values;
    values.roll = 1101;
    values.pitch = 1202;
    values.yaw = 1303;
    values.throttle = 1404;
    values.aux1 = 1505;
    values.aux2 = 1606;

    uint16_t channels[16];
    buildBetaflightRawRC(values, channels, 1500);

    checkEq(channels[FC_RAW_RC_ROLL], 1101, "slot 0 roll");
    checkEq(channels[FC_RAW_RC_PITCH], 1202, "slot 1 pitch");
    checkEq(channels[FC_RAW_RC_YAW], 1303, "slot 2 yaw");
    checkEq(channels[FC_RAW_RC_THROTTLE], 1404, "slot 3 throttle");
    checkEq(channels[FC_RAW_RC_AUX1], 1505, "slot 4 AUX1");
    checkEq(channels[FC_RAW_RC_AUX2], 1606, "slot 5 AUX2");
    checkEq(channels[6], 1500, "unused AUX slot neutral");

    values.roll = 999;
    values.pitch = 2001;
    values.yaw = 2500;
    values.throttle = 800;
    values.aux1 = 65535;
    values.aux2 = 1000;
    buildBetaflightRawRC(values, channels, 2500);
    checkEq(channels[FC_RAW_RC_ROLL], 1000, "RAW_RC clamps low roll");
    checkEq(channels[FC_RAW_RC_PITCH], 2000, "RAW_RC clamps high pitch");
    checkEq(channels[FC_RAW_RC_YAW], 2000, "RAW_RC clamps high yaw");
    checkEq(channels[FC_RAW_RC_THROTTLE], 1000, "RAW_RC clamps low throttle");
    checkEq(channels[FC_RAW_RC_AUX1], 2000, "RAW_RC clamps high AUX1");
    checkEq(channels[6], 2000, "RAW_RC clamps neutral fill");

    std::printf("\n[2] Assist gate requires online + armed + CH6 high\n");
    values.roll = 1101;
    values.pitch = 1202;
    values.yaw = 1303;
    values.throttle = 1404;
    values.aux1 = 1505;
    values.aux2 = 1606;
    buildBetaflightRawRC(values, channels, 1500);
    check(!isAssistGateOpenFromRC(false, true, channels, 6, 6, 1700), "offline blocks");
    check(!isAssistGateOpenFromRC(true, false, channels, 6, 6, 1700), "disarmed blocks");
    check(!isAssistGateOpenFromRC(true, true, channels, 5, 6, 1700), "missing CH6 blocks");
    check(!isAssistGateOpenFromRC(true, true, channels, 6, 6, 1700), "CH6 low blocks");
    channels[FC_RAW_RC_AUX2] = 1800;
    check(isAssistGateOpenFromRC(true, true, channels, 6, 6, 1700), "CH6 high opens");
    check(!isAssistGateOpenFromRC(true, true, channels, 6, 0, 1700), "invalid aux channel 0 blocks");
    check(!isAssistGateOpenFromRC(true, true, channels, 6, 17, 1700), "invalid aux channel 17 blocks");

    std::printf("\n[3] FC-ready assist preserves MC6C pilot channels\n");
    channels[FC_RAW_RC_ROLL] = 1510;
    channels[FC_RAW_RC_PITCH] = 1490;
    channels[FC_RAW_RC_YAW] = 1520;
    channels[FC_RAW_RC_THROTTLE] = 1234;
    channels[FC_RAW_RC_AUX1] = 1000;
    channels[FC_RAW_RC_AUX2] = 1800;
    FCRawRCValues assist = buildAssistOutputPreservingPilotChannels(
        1650, 1350, 1550, channels, 6);
    checkEq(assist.roll, 1650, "assist replaces roll");
    checkEq(assist.pitch, 1350, "assist replaces pitch");
    checkEq(assist.yaw, 1550, "assist replaces yaw");
    checkEq(assist.throttle, 1234, "assist preserves MC6C throttle");
    checkEq(assist.aux1, 1000, "assist preserves MC6C AUX1/ARM");
    checkEq(assist.aux2, 1800, "assist preserves MC6C AUX2 permission");

    assist = buildAssistOutputPreservingPilotChannels(1600, 1400, 1500, channels, 3);
    checkEq(assist.throttle, 1000, "missing throttle falls back low");
    checkEq(assist.aux1, 1500, "missing AUX1 falls back neutral");
    checkEq(assist.aux2, 1000, "missing AUX2 falls back low");

    std::printf("\n[4] Betaflight MSP_STATUS armed bit is explicit\n");
    check(!betaflightMspStatusReportsArmed(0), "zero status flags are disarmed");
    check(betaflightMspStatusReportsArmed(BETAFLIGHT_MSP_STATUS_ARM_BOX_MASK),
          "ARM active-box bit reports armed");
    check(!betaflightMspStatusReportsArmed((uint32_t)1 << 1),
          "other mode bit alone does not report armed");
    check(betaflightMspStatusReportsArmed(0xffffffffu),
          "all status bits set includes armed");

    std::printf("\n[5] MC6C receiver readiness is conservative\n");
    channels[FC_RAW_RC_ROLL] = 1500;
    channels[FC_RAW_RC_PITCH] = 1500;
    channels[FC_RAW_RC_YAW] = 1500;
    channels[FC_RAW_RC_THROTTLE] = 1000;
    channels[FC_RAW_RC_AUX1] = 1000;
    channels[FC_RAW_RC_AUX2] = 1000;
    MC6CReceiverReadiness rx = evaluateMC6CReceiverReadiness(
        channels, 6, 1400, 1600, 1200, 1700);
    check(rx.hasSixChannels, "six channels present");
    check(rx.centerOk, "roll/pitch/yaw centered");
    check(rx.throttleLow, "throttle low");
    check(rx.aux1Valid && rx.aux2Valid, "AUX switches at clear extremes");
    check(rx.aux1Low && rx.aux2Low, "AUX switches low for bench baseline");
    check(rx.benchReady, "bench ready when all receiver baseline checks pass");

    channels[FC_RAW_RC_AUX2] = 1800;
    rx = evaluateMC6CReceiverReadiness(channels, 6, 1400, 1600, 1200, 1700);
    check(rx.aux2Valid && !rx.aux2Low, "AUX2 high is valid but not baseline low");
    check(!rx.benchReady, "AUX2 high blocks bench baseline ready");
    channels[FC_RAW_RC_AUX2] = 1000;

    channels[FC_RAW_RC_AUX1] = 1800;
    rx = evaluateMC6CReceiverReadiness(channels, 6, 1400, 1600, 1200, 1700);
    check(rx.aux1Valid && !rx.aux1Low, "AUX1 high is valid but not baseline low");
    check(!rx.benchReady, "AUX1 high blocks bench baseline ready");
    channels[FC_RAW_RC_AUX1] = 1000;

    rx = evaluateMC6CReceiverReadiness(channels, 5, 1400, 1600, 1200, 1700);
    check(!rx.benchReady && !rx.hasSixChannels, "five channels blocks bench ready");
    channels[FC_RAW_RC_ROLL] = 1700;
    rx = evaluateMC6CReceiverReadiness(channels, 6, 1400, 1600, 1200, 1700);
    check(!rx.benchReady && !rx.centerOk, "off-center roll blocks bench ready");
    channels[FC_RAW_RC_ROLL] = 1500;
    channels[FC_RAW_RC_THROTTLE] = 1300;
    rx = evaluateMC6CReceiverReadiness(channels, 6, 1400, 1600, 1200, 1700);
    check(!rx.benchReady && !rx.throttleLow, "non-low throttle blocks bench ready");
    channels[FC_RAW_RC_THROTTLE] = 1000;
    channels[FC_RAW_RC_AUX1] = 1500;
    rx = evaluateMC6CReceiverReadiness(channels, 6, 1400, 1600, 1200, 1700);
    check(!rx.benchReady && !rx.aux1Valid, "mid AUX1 blocks bench ready");

    std::printf("\n[6] MC6C direction self-test helpers\n");
    channels[FC_RAW_RC_ROLL] = 1500;
    channels[FC_RAW_RC_PITCH] = 1500;
    channels[FC_RAW_RC_YAW] = 1500;
    channels[FC_RAW_RC_THROTTLE] = 1000;
    channels[FC_RAW_RC_AUX1] = 1000;
    channels[FC_RAW_RC_AUX2] = 1000;

    MC6CBaselineCheck baseline = evaluateMC6CDirectionBaseline(
        channels, 6, 1400, 1600, 1200, 1300);
    check(baseline.hasSixChannels, "baseline sees six channels");
    check(baseline.centerOk, "baseline roll/pitch/yaw centered");
    check(baseline.throttleLow, "baseline throttle low");
    check(baseline.auxLow, "baseline AUX switches low");
    check(baseline.ok, "baseline accepted when all safe preconditions pass");

    baseline = evaluateMC6CDirectionBaseline(channels, 5, 1400, 1600, 1200, 1300);
    check(!baseline.ok && !baseline.hasSixChannels, "baseline missing channel blocks");
    channels[FC_RAW_RC_ROLL] = 1700;
    baseline = evaluateMC6CDirectionBaseline(channels, 6, 1400, 1600, 1200, 1300);
    check(!baseline.ok && !baseline.centerOk, "baseline off-center stick blocks");
    channels[FC_RAW_RC_ROLL] = 1500;
    channels[FC_RAW_RC_THROTTLE] = 1300;
    baseline = evaluateMC6CDirectionBaseline(channels, 6, 1400, 1600, 1200, 1300);
    check(!baseline.ok && !baseline.throttleLow, "baseline non-low throttle blocks");
    channels[FC_RAW_RC_THROTTLE] = 1000;
    channels[FC_RAW_RC_AUX2] = 1500;
    baseline = evaluateMC6CDirectionBaseline(channels, 6, 1400, 1600, 1200, 1300);
    check(!baseline.ok && !baseline.auxLow, "baseline non-low AUX blocks");
    channels[FC_RAW_RC_AUX2] = 1000;

    uint16_t baseChannels[16];
    uint16_t currentChannels[16];
    copyChannels(channels, baseChannels);
    copyChannels(channels, currentChannels);

    currentChannels[FC_RAW_RC_ROLL] = 1800;
    MC6CDirectionStepCheck step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_ROLL,
        MC6CDirectionExpectation::Up, 250, 1300, 1700);
    check(step.ok, "AIL right passes when roll rises");
    checkEq32(step.delta, 300, "AIL right delta recorded");

    currentChannels[FC_RAW_RC_ROLL] = 1200;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_ROLL,
        MC6CDirectionExpectation::Up, 250, 1300, 1700);
    check(!step.ok, "AIL wrong direction fails");

    copyChannels(baseChannels, currentChannels);
    currentChannels[FC_RAW_RC_ROLL] = 1200;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_ROLL,
        MC6CDirectionExpectation::Down, 250, 1300, 1700);
    check(step.ok, "AIL left passes when roll falls");
    checkEq32(step.delta, -300, "AIL left delta recorded");

    currentChannels[FC_RAW_RC_ROLL] = 1800;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_ROLL,
        MC6CDirectionExpectation::Down, 250, 1300, 1700);
    check(!step.ok, "AIL left wrong direction fails");

    copyChannels(baseChannels, currentChannels);
    currentChannels[FC_RAW_RC_PITCH] = 1800;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_PITCH,
        MC6CDirectionExpectation::Any, 250, 1300, 1700);
    check(step.ok, "ELE forward passes on significant positive movement");
    currentChannels[FC_RAW_RC_PITCH] = 1200;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_PITCH,
        MC6CDirectionExpectation::Any, 250, 1300, 1700);
    check(step.ok, "ELE forward passes on significant negative movement");
    currentChannels[FC_RAW_RC_PITCH] = 1600;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_PITCH,
        MC6CDirectionExpectation::Any, 250, 1300, 1700);
    check(!step.ok, "ELE small movement fails");

    copyChannels(baseChannels, currentChannels);
    currentChannels[FC_RAW_RC_PITCH] = 1200;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_PITCH,
        MC6CDirectionExpectation::Any, 250, 1300, 1700);
    check(step.ok, "ELE back passes on significant movement");

    copyChannels(baseChannels, currentChannels);
    currentChannels[FC_RAW_RC_THROTTLE] = 1800;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_THROTTLE,
        MC6CDirectionExpectation::Up, 250, 1300, 1700);
    check(step.ok, "THR high passes when throttle rises");
    currentChannels[FC_RAW_RC_THROTTLE] = 900;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_THROTTLE,
        MC6CDirectionExpectation::Up, 250, 1300, 1700);
    check(!step.ok, "THR wrong direction fails");

    copyChannels(baseChannels, currentChannels);
    currentChannels[FC_RAW_RC_YAW] = 1800;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_YAW,
        MC6CDirectionExpectation::Up, 250, 1300, 1700);
    check(step.ok, "RUD right passes when yaw rises");
    currentChannels[FC_RAW_RC_YAW] = 1200;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_YAW,
        MC6CDirectionExpectation::Up, 250, 1300, 1700);
    check(!step.ok, "RUD wrong direction fails");

    copyChannels(baseChannels, currentChannels);
    currentChannels[FC_RAW_RC_YAW] = 1200;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_YAW,
        MC6CDirectionExpectation::Down, 250, 1300, 1700);
    check(step.ok, "RUD left passes when yaw falls");
    checkEq32(step.delta, -300, "RUD left delta recorded");

    currentChannels[FC_RAW_RC_YAW] = 1800;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_YAW,
        MC6CDirectionExpectation::Down, 250, 1300, 1700);
    check(!step.ok, "RUD left wrong direction fails");

    copyChannels(baseChannels, currentChannels);
    currentChannels[FC_RAW_RC_AUX1] = 1800;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_AUX1,
        MC6CDirectionExpectation::SwitchHigh, 250, 1300, 1700);
    check(step.ok, "CH5 switch high passes from low baseline");
    currentChannels[FC_RAW_RC_AUX1] = 1500;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_AUX1,
        MC6CDirectionExpectation::SwitchHigh, 250, 1300, 1700);
    check(!step.ok, "CH5 switch mid fails");

    copyChannels(baseChannels, currentChannels);
    baseChannels[FC_RAW_RC_AUX2] = 1500;
    currentChannels[FC_RAW_RC_AUX2] = 1800;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_AUX2,
        MC6CDirectionExpectation::SwitchHigh, 250, 1300, 1700);
    check(!step.ok, "CH6 switch high requires low baseline");
    baseChannels[FC_RAW_RC_AUX2] = 1000;
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, FC_RAW_RC_AUX2,
        MC6CDirectionExpectation::SwitchHigh, 250, 1300, 1700);
    check(step.ok, "CH6 switch high passes from low baseline");

    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 5, 6, FC_RAW_RC_AUX2,
        MC6CDirectionExpectation::SwitchHigh, 250, 1300, 1700);
    check(!step.ok, "direction step missing baseline channel blocks");
    step = evaluateMC6CDirectionStep(
        baseChannels, currentChannels, 6, 6, 16,
        MC6CDirectionExpectation::Up, 250, 1300, 1700);
    check(!step.ok, "direction step invalid channel index blocks");

    std::printf("\n== result: %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
