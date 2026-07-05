/**
 * @file msp_status_flags.h
 * @brief Pure helpers for Betaflight MSP_STATUS flag parsing.
 *
 * Betaflight's MSP_STATUS serializes active box states via
 * packFlightModeFlags(). In current Betaflight, BOXARM is the first active
 * box and getBoxIdState(BOXARM) returns ARMING_FLAG(ARMED), so bit 0 of the
 * first 32 status mode bits is the real armed state for Betaflight targets.
 *
 * Keep this Arduino-free so host tests can lock the safety-critical
 * assumption. Re-check before using this helper with non-Betaflight firmware.
 */

#ifndef MSP_STATUS_FLAGS_H
#define MSP_STATUS_FLAGS_H

#include <stdint.h>

constexpr uint8_t BETAFLIGHT_MSP_STATUS_ARM_BOX_BIT = 0;
constexpr uint32_t BETAFLIGHT_MSP_STATUS_ARM_BOX_MASK =
    (uint32_t)1 << BETAFLIGHT_MSP_STATUS_ARM_BOX_BIT;

inline bool betaflightMspStatusReportsArmed(uint32_t statusModeFlags)
{
    return (statusModeFlags & BETAFLIGHT_MSP_STATUS_ARM_BOX_MASK) != 0;
}

#endif // MSP_STATUS_FLAGS_H
