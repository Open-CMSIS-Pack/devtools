/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_TEST_UNIT_SUPPORT_TRACERUNTESTSUPPORT_H
#define CTRACE_TEST_UNIT_SUPPORT_TRACERUNTESTSUPPORT_H

#include "TraceRunConfig.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace TraceRunTestSupport {

/** @brief Creates a trace-run reference with common test defaults. */
inline TraceRunReference makeReference(std::string type, std::optional<std::string> processorName,
                                       std::optional<std::uint32_t> stream, std::vector<std::uint32_t> sources,
                                       std::string ctraceRef = "route")
{
  TraceRunReference reference;
  reference.type = std::move(type);
  reference.processorName = std::move(processorName);
  reference.stream = stream;
  reference.sources = std::move(sources);
  reference.ctraceRef = std::move(ctraceRef);
  return reference;
}

/** @brief Creates a timestamp setup with optional ITM configuration. */
inline TraceRunSetup makeTimestampSetup(std::optional<std::string> processorName,
                                        std::optional<std::uint64_t> clock = 1U,
                                        std::optional<std::uint32_t> prescaler = 1U,
                                        std::optional<std::uint32_t> enableMask = std::nullopt)
{
  TraceRunSetup setup;
  setup.processorName = std::move(processorName);
  setup.timestamps = TraceRunTimestampSetup{clock, prescaler};
  if (enableMask.has_value()) {
    setup.itm = TraceRunItmSetup{*enableMask};
  }
  return setup;
}

} // namespace TraceRunTestSupport

#endif  // CTRACE_TEST_UNIT_SUPPORT_TRACERUNTESTSUPPORT_H
