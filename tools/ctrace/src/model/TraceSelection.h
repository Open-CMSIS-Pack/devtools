/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_TRACESELECTION_H
#define CTRACE_SRC_MODEL_TRACESELECTION_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct TraceEvent;

enum class TraceEventType : std::size_t {
  Itm,
  Dwt,
  Exception,
  GlobalTimestamp,
  Overflow,
  Error,
  Count,
};

inline constexpr std::array<std::string_view, static_cast<std::size_t>(TraceEventType::Count)> kTraceEventTypeNames{{
    "itm",
    "dwt",
    "exception",
    "global_ts",
    "overflow",
    "error",
}};

std::string_view traceEventTypeName(TraceEventType type);
std::string traceEventTypeList(std::string_view separator);
std::optional<TraceEventType> parseTraceEventType(std::string_view value);
std::optional<TraceEventType> traceEventType(const TraceEvent& event);

struct TraceSelection {
  std::vector<std::string> types;
  std::vector<std::uint8_t> streams;

  bool empty() const;
  bool includesType(std::string_view type) const;
  bool includesStream(std::uint8_t traceBusId) const;
};

bool traceEventSelectedForOutput(const TraceEvent& event, const TraceSelection& selection);

#endif  // CTRACE_SRC_MODEL_TRACESELECTION_H
