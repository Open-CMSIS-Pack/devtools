/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_TRACESTREAMID_H
#define CTRACE_SRC_MODEL_TRACESTREAMID_H

#include <cstdint>

namespace CoreSight {

inline constexpr std::uint8_t kMinAtbTraceId = 0x01U;
inline constexpr std::uint8_t kMaxAtbTraceId = 0x6fU;
inline constexpr std::uint8_t kUnformattedTraceBusId = 0x00U;
inline constexpr std::uint8_t kExcludedItmStimulusPort = 0U;
inline constexpr std::uint8_t kMaxItmStimulusPort = 31U;

/** @brief Tests whether a value is an architectural ITM stimulus port. */
constexpr bool isItmStimulusPort(std::uint32_t value)
{
  return value <= kMaxItmStimulusPort;
}

/** @brief Tests whether a value is a valid formatted CoreSight ATB trace ID. */
constexpr bool isAtbTraceId(std::uint32_t value)
{
  return value >= kMinAtbTraceId && value <= kMaxAtbTraceId;
}

/** @brief Tests whether a value is a supported formatted or unformatted Trace Bus ID. */
constexpr bool isTraceBusId(std::uint32_t value)
{
  return value <= kMaxAtbTraceId;
}

} // namespace CoreSight

#endif // CTRACE_SRC_MODEL_TRACESTREAMID_H
