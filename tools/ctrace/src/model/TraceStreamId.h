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

constexpr bool isAtbTraceId(std::uint32_t value)
{
  return value >= kMinAtbTraceId && value <= kMaxAtbTraceId;
}

constexpr bool isTraceBusId(std::uint32_t value)
{
  return value <= kMaxAtbTraceId;
}

} // namespace CoreSight

#endif  // CTRACE_SRC_MODEL_TRACESTREAMID_H
