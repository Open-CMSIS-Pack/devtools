/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TraceSelection.h"

#include "TraceEvent.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

static std::optional<TraceEventType> typeFor(const SoftwareTraceEvent&)
{
  return TraceEventType::Itm;
}

static std::optional<TraceEventType> typeFor(const DwtDataTraceEvent&)
{
  return TraceEventType::Dwt;
}

static std::optional<TraceEventType> typeFor(const DwtAddressTraceEvent&)
{
  return TraceEventType::Dwt;
}

static std::optional<TraceEventType> typeFor(const ExceptionTraceEvent&)
{
  return TraceEventType::Exception;
}

static std::optional<TraceEventType> typeFor(const DwtEventTraceEvent&)
{
  return std::nullopt;
}

static std::optional<TraceEventType> typeFor(const PmuTraceEvent&)
{
  return std::nullopt;
}

static std::optional<TraceEventType> typeFor(const LocalTimestampTraceEvent&)
{
  return std::nullopt;
}

static std::optional<TraceEventType> typeFor(const GlobalTimestampTraceEvent&)
{
  return TraceEventType::GlobalTimestamp;
}

static std::optional<TraceEventType> typeFor(const OverflowTraceEvent&)
{
  return TraceEventType::Overflow;
}

static std::optional<TraceEventType> typeFor(const SyncTraceEvent&)
{
  return std::nullopt;
}

static std::optional<TraceEventType> typeFor(const TraceIssueEvent&)
{
  return TraceEventType::Error;
}

std::string_view traceEventTypeName(TraceEventType type)
{
  const auto index = static_cast<std::size_t>(type);
  return index < kTraceEventTypeNames.size() ? kTraceEventTypeNames[index] : std::string_view("unknown");
}

std::string traceEventTypeList(std::string_view separator)
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

std::optional<TraceEventType> parseTraceEventType(std::string_view value)
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

bool TraceSelection::empty() const
{
  return types.empty() && streams.empty();
}

bool TraceSelection::includesType(std::string_view type) const
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
  if (software != nullptr && software->channel == 0U) {
    return false;
  }
  if (!selection.includesStream(event.traceBusId)) {
    return false;
  }
  return type.has_value() && selection.includesType(traceEventTypeName(*type));
}
