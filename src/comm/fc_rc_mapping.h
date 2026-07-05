/**
 * @file fc_rc_mapping.h
 * @brief Pure helpers for Betaflight MSP RC channel ordering and assist gate.
 *
 * Betaflight MSP_RC / MSP_SET_RAW_RC uses functional channel order after the
 * configured channel map: roll, pitch, yaw, throttle, AUX1, AUX2...
 * Keep this header Arduino-free so host tests can lock the safety-critical
 * order without ESP32 hardware.
 */

#ifndef FC_RC_MAPPING_H
#define FC_RC_MAPPING_H

#include <stdint.h>

enum FCRawRCSlot : uint8_t {
    FC_RAW_RC_ROLL = 0,
    FC_RAW_RC_PITCH = 1,
    FC_RAW_RC_YAW = 2,
    FC_RAW_RC_THROTTLE = 3,
    FC_RAW_RC_AUX1 = 4,
    FC_RAW_RC_AUX2 = 5,
};

constexpr uint16_t FC_RAW_RC_MIN_VALUE = 1000;
constexpr uint16_t FC_RAW_RC_MAX_VALUE = 2000;

struct FCRawRCValues {
    uint16_t roll = 1500;
    uint16_t pitch = 1500;
    uint16_t yaw = 1500;
    uint16_t throttle = 1000;
    uint16_t aux1 = 1500;
    uint16_t aux2 = 1000;
};

struct MC6CReceiverReadiness {
    bool hasSixChannels = false;
    bool centerOk = false;
    bool throttleLow = false;
    bool aux1Valid = false;
    bool aux2Valid = false;
    bool aux1Low = false;
    bool aux2Low = false;
    bool benchReady = false;
};

enum class MC6CDirectionExpectation : uint8_t {
    Any,
    Up,
    Down,
    SwitchHigh,
};

struct MC6CBaselineCheck {
    bool hasSixChannels = false;
    bool centerOk = false;
    bool throttleLow = false;
    bool auxLow = false;
    bool ok = false;
};

struct MC6CDirectionStepCheck {
    bool ok = false;
    int32_t delta = 0;
};

inline uint16_t clampRawRCValue(uint16_t value)
{
    if (value < FC_RAW_RC_MIN_VALUE) return FC_RAW_RC_MIN_VALUE;
    if (value > FC_RAW_RC_MAX_VALUE) return FC_RAW_RC_MAX_VALUE;
    return value;
}

inline void buildBetaflightRawRC(const FCRawRCValues& values,
                                 uint16_t channels[16],
                                 uint16_t neutral = 1500)
{
    for (uint8_t i = 0; i < 16; i++) {
        channels[i] = clampRawRCValue(neutral);
    }

    channels[FC_RAW_RC_ROLL] = clampRawRCValue(values.roll);
    channels[FC_RAW_RC_PITCH] = clampRawRCValue(values.pitch);
    channels[FC_RAW_RC_YAW] = clampRawRCValue(values.yaw);
    channels[FC_RAW_RC_THROTTLE] = clampRawRCValue(values.throttle);
    channels[FC_RAW_RC_AUX1] = clampRawRCValue(values.aux1);
    channels[FC_RAW_RC_AUX2] = clampRawRCValue(values.aux2);
}

inline bool isAssistGateOpenFromRC(bool online,
                                   bool armed,
                                   const uint16_t channels[16],
                                   uint8_t channelCount,
                                   uint8_t auxChannel,
                                   uint16_t auxMin)
{
    if (!online || !armed) return false;
    if (auxChannel == 0 || auxChannel > 16) return false;
    if (channelCount < auxChannel) return false;
    return channels[auxChannel - 1] >= auxMin;
}

inline FCRawRCValues buildAssistOutputPreservingPilotChannels(
    uint16_t assistRoll,
    uint16_t assistPitch,
    uint16_t assistYaw,
    const uint16_t pilotChannels[16],
    uint8_t pilotChannelCount,
    uint16_t throttleFallback = 1000,
    uint16_t aux1Fallback = 1500,
    uint16_t aux2Fallback = 1000)
{
    FCRawRCValues values;
    values.roll = assistRoll;
    values.pitch = assistPitch;
    values.yaw = assistYaw;
    values.throttle = pilotChannelCount > FC_RAW_RC_THROTTLE
        ? pilotChannels[FC_RAW_RC_THROTTLE]
        : throttleFallback;
    values.aux1 = pilotChannelCount > FC_RAW_RC_AUX1
        ? pilotChannels[FC_RAW_RC_AUX1]
        : aux1Fallback;
    values.aux2 = pilotChannelCount > FC_RAW_RC_AUX2
        ? pilotChannels[FC_RAW_RC_AUX2]
        : aux2Fallback;
    return values;
}

inline bool isRcInRange(uint16_t value, uint16_t low, uint16_t high)
{
    return value >= low && value <= high;
}

inline bool isRcSwitchExtreme(uint16_t value,
                              uint16_t lowMax,
                              uint16_t highMin)
{
    return value <= lowMax || value >= highMin;
}

inline MC6CReceiverReadiness evaluateMC6CReceiverReadiness(
    const uint16_t channels[16],
    uint8_t channelCount,
    uint16_t centerMin,
    uint16_t centerMax,
    uint16_t lowMax,
    uint16_t highMin)
{
    MC6CReceiverReadiness result;
    result.hasSixChannels = channelCount >= 6;
    if (!result.hasSixChannels) return result;

    result.centerOk =
        isRcInRange(channels[FC_RAW_RC_ROLL], centerMin, centerMax) &&
        isRcInRange(channels[FC_RAW_RC_PITCH], centerMin, centerMax) &&
        isRcInRange(channels[FC_RAW_RC_YAW], centerMin, centerMax);
    result.throttleLow = channels[FC_RAW_RC_THROTTLE] <= lowMax;
    result.aux1Valid = isRcSwitchExtreme(channels[FC_RAW_RC_AUX1], lowMax, highMin);
    result.aux2Valid = isRcSwitchExtreme(channels[FC_RAW_RC_AUX2], lowMax, highMin);
    result.aux1Low = channels[FC_RAW_RC_AUX1] <= lowMax;
    result.aux2Low = channels[FC_RAW_RC_AUX2] <= lowMax;
    result.benchReady =
        result.centerOk &&
        result.throttleLow &&
        result.aux1Low &&
        result.aux2Low;
    return result;
}

inline MC6CBaselineCheck evaluateMC6CDirectionBaseline(
    const uint16_t channels[16],
    uint8_t channelCount,
    uint16_t centerMin,
    uint16_t centerMax,
    uint16_t throttleLowMax,
    uint16_t switchLowMax)
{
    MC6CBaselineCheck result;
    result.hasSixChannels = channelCount >= 6;
    if (!result.hasSixChannels) return result;

    result.centerOk =
        isRcInRange(channels[FC_RAW_RC_ROLL], centerMin, centerMax) &&
        isRcInRange(channels[FC_RAW_RC_PITCH], centerMin, centerMax) &&
        isRcInRange(channels[FC_RAW_RC_YAW], centerMin, centerMax);
    result.throttleLow = channels[FC_RAW_RC_THROTTLE] <= throttleLowMax;
    result.auxLow =
        channels[FC_RAW_RC_AUX1] <= switchLowMax &&
        channels[FC_RAW_RC_AUX2] <= switchLowMax;
    result.ok = result.centerOk && result.throttleLow && result.auxLow;
    return result;
}

inline MC6CDirectionStepCheck evaluateMC6CDirectionStep(
    const uint16_t baseline[16],
    const uint16_t current[16],
    uint8_t baselineCount,
    uint8_t currentCount,
    uint8_t channelIndex,
    MC6CDirectionExpectation expectation,
    uint16_t minDelta,
    uint16_t switchLowMax,
    uint16_t switchHighMin)
{
    MC6CDirectionStepCheck result;
    if (channelIndex >= 16) return result;
    if (baselineCount <= channelIndex || currentCount <= channelIndex) return result;

    const int32_t delta =
        static_cast<int32_t>(current[channelIndex]) -
        static_cast<int32_t>(baseline[channelIndex]);
    result.delta = delta;

    switch (expectation) {
    case MC6CDirectionExpectation::Any:
        result.ok = delta >= static_cast<int32_t>(minDelta) ||
                    delta <= -static_cast<int32_t>(minDelta);
        break;
    case MC6CDirectionExpectation::Up:
        result.ok = delta >= static_cast<int32_t>(minDelta);
        break;
    case MC6CDirectionExpectation::Down:
        result.ok = delta <= -static_cast<int32_t>(minDelta);
        break;
    case MC6CDirectionExpectation::SwitchHigh:
        result.ok = baseline[channelIndex] <= switchLowMax &&
                    current[channelIndex] >= switchHighMin;
        break;
    }
    return result;
}

#endif // FC_RC_MAPPING_H
