/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceRunConfig.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace TraceRunTestSupport {

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
