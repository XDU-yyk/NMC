#ifndef FC_TELEMETRY_POLICY_H
#define FC_TELEMETRY_POLICY_H

#include <stdint.h>

inline bool isFCTelemetryFresh(uint32_t now, uint32_t updatedAt, uint32_t maxAge)
{
    return updatedAt != 0 && static_cast<uint32_t>(now - updatedAt) <= maxAge;
}

inline const char* selectFCTelemetrySource(bool presentationMode, bool realOnline)
{
    if (presentationMode) return "presentation";
    return realOnline ? "real_msp" : "offline";
}

#endif
