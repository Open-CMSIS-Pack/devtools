/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceSelection.h"

#include "TraceEvent.h"
#include "TraceStreamId.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

/** @brief Maps an ITM payload to its selectable event type. */
static std::optional<TraceEventType> typeFor(const SoftwareTraceEvent&)
{
  return TraceEventType::Itm;
}

/** @brief Maps a DWT data payload to its selectable event type. */
static std::optional<TraceEventType> typeFor(const DwtDataTraceEvent&)
{
  return TraceEventType::Dwt;
}

/** @brief Maps a DWT address payload to its selectable event type. */
static std::optional<TraceEventType> typeFor(const DwtAddressTraceEvent&)
{
  return TraceEventType::Dwt;
}

/** @brief Maps a comparator-only DWT match payload to data trace output. */
static std::optional<TraceEventType> typeFor(const DwtMatchTraceEvent&)
{
  return TraceEventType::Dwt;
}

/** @brief Maps an exception payload to its selectable event type. */
static std::optional<TraceEventType> typeFor(const ExceptionTraceEvent&)
{
  return TraceEventType::Exception;
}

/** @brief Exposes DWT event-counter payloads through their public output selector. */
static std::optional<TraceEventType> typeFor(const DwtEventTraceEvent&)
{
  return TraceEventType::Event;
}

/** @brief Exposes PMU trace-on-overflow payloads through their public output selector. */
static std::optional<TraceEventType> typeFor(const PmuTraceEvent&)
{
  return TraceEventType::Pmu;
}

/** @brief Exposes periodic DWT PC samples through their public output selector. */
static std::optional<TraceEventType> typeFor(const PcSampleTraceEvent&)
{
  return TraceEventType::PcSample;
}

/** @brief Excludes local timestamp control packets from type selection. */
static std::optional<TraceEventType> typeFor(const LocalTimestampTraceEvent&)
{
  return std::nullopt;
}

/** @brief Exposes global timestamp packets through their public output selector. */
static std::optional<TraceEventType> typeFor(const GlobalTimestampTraceEvent&)
{
  return TraceEventType::GlobalTimestamp;
}

/** @brief Exposes overflow packets through their public output selector. */
static std::optional<TraceEventType> typeFor(const OverflowTraceEvent&)
{
  return TraceEventType::Overflow;
}

/** @brief Excludes synchronization control packets from type selection. */
static std::optional<TraceEventType> typeFor(const SyncTraceEvent&)
{
  return std::nullopt;
}

/** @brief Exposes retained diagnostic events through the error selector. */
static std::optional<TraceEventType> typeFor(const TraceIssueEvent&)
{
  return TraceEventType::Error;
}

std::string_view traceEventTypeName(TraceEventType type)
{
  const auto index = static_cast<std::size_t>(type);
  return index < kTraceEventTypeNames.size() ? kTraceEventTypeNames[index] : std::string_view("unknown");
}

std::string traceEventTypeList(const std::string_view& separator)
{
  std::string result;
  for (const auto name : kTraceEventTypeNames) {
    if (!result.empty()) {
      result += separator;
    }
    result += name;
  }
  return result;
}

std::optional<TraceEventType> parseTraceEventType(const std::string_view& value)
{
  for (std::size_t index = 0; index < kTraceEventTypeNames.size(); ++index) {
    if (kTraceEventTypeNames[index] == value) {
      return static_cast<TraceEventType>(index);
    }
  }
  return std::nullopt;
}

std::optional<TraceEventType> traceEventType(const TraceEvent& event)
{
  return std::visit([](const auto& payload) { return typeFor(payload); }, event.payload);
}

bool TraceSelection::includesType(const std::string_view& type) const
{
  return types.empty() || std::find(types.begin(), types.end(), type) != types.end();
}

bool TraceSelection::includesStream(std::uint8_t traceBusId) const
{
  return streams.empty() || std::find(streams.begin(), streams.end(), traceBusId) != streams.end();
}

bool traceEventSelectedForOutput(const TraceEvent& event, const TraceSelection& selection)
{
  const auto type = traceEventType(event);
  const auto* software = traceEventPayload<SoftwareTraceEvent>(event);
  if (software != nullptr && software->channel == CoreSight::kExcludedItmStimulusPort) {
    return false;
  }
  if (!selection.includesStream(event.traceBusId)) {
    return false;
  }
  return type.has_value() && selection.includesType(traceEventTypeName(*type));
}
