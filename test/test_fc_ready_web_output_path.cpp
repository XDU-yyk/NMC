/**
 * @file test_fc_ready_web_output_path.cpp
 * @brief Host integration test for Web direction intent -> FC-ready RAW_RC.
 *
 * The FC-ready firmware may use Web direction intent for roll/pitch/yaw assist,
 * but real throttle and AUX channels must remain the live MC6C/FC values.
 */

#include "comm/fc_rc_mapping.h"
#include "control/manual_control.h"
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

static bool wouldQueueRealOverride(const ManualController& manual)
{
    // Mirrors unified_web_main.cpp: FC-ready queues real output only when the
    // manual controller is not in neutral-hold.
    return !manual.isNeutralHold();
}

int main()
{
    std::printf("== FC-ready Web output path tests ==\n");

    uint16_t pilot[16] = {1500};
    for (uint8_t i = 0; i < 16; i++) {
        pilot[i] = 1500;
    }
    pilot[FC_RAW_RC_ROLL] = 1500;
    pilot[FC_RAW_RC_PITCH] = 1500;
    pilot[FC_RAW_RC_YAW] = 1500;
    pilot[FC_RAW_RC_THROTTLE] = 1123; // MC6C low/near-idle throttle.
    pilot[FC_RAW_RC_AUX1] = 1900;     // MC6C ARM/mode channel.
    pilot[FC_RAW_RC_AUX2] = 1800;     // MC6C assist permission high.

    DirectionCommand cmd;
    cmd.forward = 0.75f;
    cmd.right = -0.50f;
    cmd.yaw = 0.40f;
    cmd.throttle = 1.0f; // Web simulation asks for full climb.

    const RcChannels web = mapCommandToChannels(cmd);
    check(web.throttle > pilot[FC_RAW_RC_THROTTLE],
          "web throttle can rise for simulation");

    const FCRawRCValues out = buildAssistOutputPreservingPilotChannels(
        web.roll,
        web.pitch,
        web.yaw,
        pilot,
        6);

    uint16_t raw[16];
    buildBetaflightRawRC(out, raw, MC_RC_MID);

    checkEq(raw[FC_RAW_RC_ROLL], web.roll, "real RAW_RC roll follows assist");
    checkEq(raw[FC_RAW_RC_PITCH], web.pitch, "real RAW_RC pitch follows assist");
    checkEq(raw[FC_RAW_RC_YAW], web.yaw, "real RAW_RC yaw follows assist");
    checkEq(raw[FC_RAW_RC_THROTTLE], pilot[FC_RAW_RC_THROTTLE],
            "real RAW_RC throttle preserves MC6C");
    checkEq(raw[FC_RAW_RC_AUX1], pilot[FC_RAW_RC_AUX1],
            "real RAW_RC AUX1/ARM preserves MC6C");
    checkEq(raw[FC_RAW_RC_AUX2], pilot[FC_RAW_RC_AUX2],
            "real RAW_RC AUX2 permission preserves MC6C");

    check(raw[FC_RAW_RC_THROTTLE] != web.throttle,
          "web simulation throttle is not forwarded to real FC");

    pilot[FC_RAW_RC_THROTTLE] = 1677;
    const FCRawRCValues descendOut = buildAssistOutputPreservingPilotChannels(
        mapAxisToRC(-0.4f, MC_AXIS_SPAN, MC_DEADZONE),
        mapAxisToRC(-0.6f, MC_AXIS_SPAN, MC_DEADZONE),
        mapAxisToRC(0.0f, MC_AXIS_SPAN, MC_DEADZONE),
        pilot,
        6);
    buildBetaflightRawRC(descendOut, raw, MC_RC_MID);
    checkEq(raw[FC_RAW_RC_THROTTLE], 1677,
            "changed MC6C throttle is preserved on later assist frame");

    ManualController manual;
    manual.begin();
    manual.setCommand(cmd, 1000);
    RcChannels activeWeb = manual.update(1000);
    check(!manual.isNeutralHold(), "fresh Web direction leaves neutral hold");
    check(wouldQueueRealOverride(manual), "fresh Web direction may queue real override");
    checkEq(activeWeb.roll, web.roll, "ManualController roll matches Web mapping");

    manual.setPilotOverride(true);
    RcChannels takeoverWeb = manual.update(1010);
    check(manual.isNeutralHold(), "takeover forces neutral hold");
    check(!wouldQueueRealOverride(manual), "takeover prevents real FC override queue");
    checkEq(takeoverWeb.roll, MC_RC_MID, "takeover neutralizes roll");
    checkEq(takeoverWeb.throttle, MC_THROTTLE_MIN, "takeover lowers Web throttle channel");

    manual.begin();
    manual.setCommand(cmd, 2000);
    (void)manual.update(2000);
    RcChannels timeoutWeb = manual.update(2000 + MC_INPUT_TIMEOUT_MS + 1);
    check(manual.isNeutralHold(), "stale Web direction times out to neutral hold");
    check(!wouldQueueRealOverride(manual), "stale Web direction prevents real FC override queue");
    checkEq(timeoutWeb.pitch, MC_RC_MID, "timeout neutralizes pitch");
    checkEq(timeoutWeb.throttle, MC_THROTTLE_MIN, "timeout lowers Web throttle channel");

    std::printf("\n== result: %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
