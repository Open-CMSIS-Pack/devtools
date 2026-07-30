/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

enum class AccessType {
  Read,
  Write,
};

enum class ExceptionAction {
  Entered,
  Exited,
  Returned,
  Unknown,
};

enum class TraceIssueSeverity {
  Warning,
  Error,
};

struct SoftwareTraceEvent {
  std::uint32_t channel = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
};

struct DwtDataTraceEvent {
  std::uint32_t comparator = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
  AccessType access = AccessType::Read;
  std::optional<std::uint32_t> addressLo16 = std::nullopt;
  std::optional<std::uint32_t> pc = std::nullopt;
};

struct DwtPcTraceLocation {
  std::uint32_t pc;
};

struct DwtOffsetTraceLocation {
  std::uint32_t addressLo16;
};

struct DwtPcAndOffsetTraceLocation {
  std::uint32_t pc;
  std::uint32_t addressLo16;
};

using DwtAddressTraceLocation = std::variant<DwtPcTraceLocation, DwtOffsetTraceLocation, DwtPcAndOffsetTraceLocation>;

struct DwtAddressTraceEvent {
  std::uint32_t comparator;
  DwtAddressTraceLocation location;
};

inline std::optional<std::uint32_t> dwtAddressPc(const DwtAddressTraceEvent& event)
{
  if (const auto* pc = std::get_if<DwtPcTraceLocation>(&event.location)) {
    return pc->pc;
  }
  if (const auto* combined = std::get_if<DwtPcAndOffsetTraceLocation>(&event.location)) {
    return combined->pc;
  }
  return std::nullopt;
}

inline std::optional<std::uint32_t> dwtAddressOffset(const DwtAddressTraceEvent& event)
{
  if (const auto* offset = std::get_if<DwtOffsetTraceLocation>(&event.location)) {
    return offset->addressLo16;
  }
  if (const auto* combined = std::get_if<DwtPcAndOffsetTraceLocation>(&event.location)) {
    return combined->addressLo16;
  }
  return std::nullopt;
}

struct ExceptionTraceEvent {
  std::uint32_t number = 0;
  ExceptionAction action = ExceptionAction::Unknown;
};

struct DwtEventTraceEvent {
  std::uint32_t discriminator = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
};

struct PmuTraceEvent {
  std::uint32_t discriminator = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
};

struct LocalTimestampTraceEvent {};

struct GlobalTimestampTraceEvent {
  std::uint64_t value = 0;
  bool clockChange = false;
};

struct OverflowTraceEvent {
  std::string message;
};

struct SyncTraceEvent {};

struct TraceIssueEvent {
  std::string code;
  TraceIssueSeverity severity = TraceIssueSeverity::Error;
  std::string message;
  std::optional<std::uint64_t> rawBytesConsumed = std::nullopt;
  std::optional<std::uint64_t> lastValidTcyc = std::nullopt;
};

using TraceEventPayload = std::variant<SoftwareTraceEvent, DwtDataTraceEvent, DwtAddressTraceEvent, ExceptionTraceEvent,
                                       DwtEventTraceEvent, PmuTraceEvent, LocalTimestampTraceEvent,
                                       GlobalTimestampTraceEvent, OverflowTraceEvent, SyncTraceEvent, TraceIssueEvent>;

struct TraceQuality {
  bool overflow = false;
  bool timestampReliable = false;
  std::uint64_t overflowCount = 0;
};

struct TraceEvent {
  template <typename Payload> explicit TraceEvent(Payload eventPayload) : payload(std::move(eventPayload)) {}

  std::uint64_t index = 0;
  // CoreSight Trace Bus ID. ID 0 identifies unformatted single-source input.
  std::uint8_t traceBusId = 0U;
  std::optional<std::uint64_t> tcyc;

  // Quality is assigned atomically by the post-decoder. Wire/control events
  // without reconstructed DWT status leave it absent.
  std::optional<TraceQuality> quality;

  TraceEventPayload payload;
};

template <typename Payload> const Payload* traceEventPayload(const TraceEvent& event)
{
  return std::get_if<Payload>(&event.payload);
}

template <typename Payload> Payload* traceEventPayload(TraceEvent& event)
{
  return std::get_if<Payload>(&event.payload);
}

template <typename Payload> bool isTraceEvent(const TraceEvent& event)
{
  return std::holds_alternative<Payload>(event.payload);
}

class TraceEventSink {
public:
  virtual ~TraceEventSink() = default;
  virtual void append(const TraceEvent& event) = 0;
};
