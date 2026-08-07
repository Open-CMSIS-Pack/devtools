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

/** @brief Identifies event families exposed by the public output filters. */
enum class TraceEventType : std::size_t {
  Itm,
  Dwt,
  Event,
  Pmu,
  Exception,
  PcSample,
  GlobalTimestamp,
  Overflow,
  Error,
  Count,
};

inline constexpr std::array<std::string_view, static_cast<std::size_t>(TraceEventType::Count)> kTraceEventTypeNames{{
    "itm",
    "dwt",
    "event",
    "pmu",
    "exception",
    "pcsample",
    "global_ts",
    "overflow",
    "error",
}};

/** @brief Returns the stable command-line name of an event type. */
std::string_view traceEventTypeName(TraceEventType type);
/** @brief Returns all stable event type names joined by a separator. */
std::string traceEventTypeList(const std::string_view& separator);
/** @brief Parses a stable event type name. */
std::optional<TraceEventType> parseTraceEventType(const std::string_view& value);
/** @brief Returns the selectable type represented by an event, if any. */
std::optional<TraceEventType> traceEventType(const TraceEvent& event);

/** @brief Stores optional event-type and Trace Bus ID output filters. */
struct TraceSelection {
  std::vector<std::string> types;
  std::vector<std::uint8_t> streams;

  /** @brief Tests whether an event type is included. */
  bool includesType(const std::string_view& type) const;
  /** @brief Tests whether a Trace Bus ID is included. */
  bool includesStream(std::uint8_t traceBusId) const;
};

/** @brief Tests whether an event passes a complete output selection. */
bool traceEventSelectedForOutput(const TraceEvent& event, const TraceSelection& selection);

#endif  // CTRACE_SRC_MODEL_TRACESELECTION_H
