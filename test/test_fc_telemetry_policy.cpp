#include "comm/fc_telemetry_policy.h"

#include <cstdio>
#include <cstring>

static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char* name)
{
    if (condition) {
        g_pass++;
        std::printf("  [PASS] %s\n", name);
    } else {
        g_fail++;
        std::printf("  [FAIL] %s\n", name);
    }
}

int main()
{
    std::printf("== FC telemetry policy tests ==\n");

    check(!isFCTelemetryFresh(1000, 0, 500), "zero timestamp is invalid");
    check(isFCTelemetryFresh(1000, 500, 500), "freshness includes boundary");
    check(!isFCTelemetryFresh(1001, 500, 500), "stale value is rejected");
    check(isFCTelemetryFresh(5, 0xfffffff0u, 32), "millis wrap remains fresh");

    check(std::strcmp(selectFCTelemetrySource(false, false), "offline") == 0,
          "absent FC reports offline");
    check(std::strcmp(selectFCTelemetrySource(false, true), "real_msp") == 0,
          "online FC reports real MSP");
    check(std::strcmp(selectFCTelemetrySource(true, false), "presentation") == 0,
          "presentation mode is explicit");
    check(std::strcmp(selectFCTelemetrySource(true, true), "presentation") == 0,
          "presentation mode never claims real MSP");

    std::printf("== result: %d passed, %d failed ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
