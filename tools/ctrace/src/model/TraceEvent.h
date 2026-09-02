/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_MODEL_TRACEEVENT_H
#define CTRACE_SRC_MODEL_TRACEEVENT_H

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <variant>

/** @brief Identifies whether a data-trace event represents a read or write. */
enum class AccessType {
  Read,
  Write,
};

/** @brief Describes an exception entry, exit, or return transition. */
enum class ExceptionAction {
  Entered,
  Exited,
  Returned,
  Unknown,
};

/** @brief Classifies decoder issues retained in the trace event stream. */
enum class TraceIssueSeverity {
  Warning,
  Error,
};

/** @brief Identifies one decoder issue without relying on textual state. */
enum class TraceIssueCode {
  DecodeError,
  DataLoss,
  InvalidExceptionAction,
  UnsupportedDwtAddressPayload,
  UnsupportedDwtPcSamplePayload,
  OpenCsdDecodeError,
  OpenCsdBadPacketSequence,
  OpenCsdInvalidPacketHeader,
  OpenCsdIncompleteTail,
  OpenCsdNoProgress,
  OpenCsdWaitTimeout,
  OpenCsdInitializationError,
};

/** @brief Contains one decoded ITM software stimulus event. */
struct SoftwareTraceEvent {
  std::uint32_t channel = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
};

/** @brief Contains a reconstructed DWT data access event. */
struct DwtDataTraceEvent {
  std::uint32_t comparator = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
  AccessType access = AccessType::Read;
  std::optional<std::uint32_t> addressLo16 = std::nullopt;
  std::optional<std::uint32_t> pc = std::nullopt;
};

/** @brief Identifies a DWT address event by program counter. */
struct DwtPcTraceLocation {
  std::uint32_t pc;
};

/** @brief Identifies a DWT address event by its low address bits. */
struct DwtOffsetTraceLocation {
  std::uint32_t addressLo16;
};

/** @brief Identifies a DWT address event by program counter and address offset. */
struct DwtPcAndOffsetTraceLocation {
  std::uint32_t pc;
  std::uint32_t addressLo16;
};

/** @brief Stores one of the supported DWT address location representations. */
using DwtAddressTraceLocation = std::variant<DwtPcTraceLocation, DwtOffsetTraceLocation, DwtPcAndOffsetTraceLocation>;

/** @brief Contains a reconstructed DWT address event. */
struct DwtAddressTraceEvent {
  std::uint32_t comparator;
  DwtAddressTraceLocation location;
};

/** @brief Returns the program counter carried by a DWT address event, if present. */
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

/** @brief Returns the address offset carried by a DWT address event, if present. */
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

/** @brief Stores a decoded Cortex-M exception number (9 bits on the wire). */
using ExceptionNumber = std::uint16_t;

/** @brief Contains a decoded exception transition. */
struct ExceptionTraceEvent {
  ExceptionNumber number = 0;
  ExceptionAction action = ExceptionAction::Unknown;
};

/** @brief Contains a decoded DWT event-counter packet. */
struct DwtEventTraceEvent {
  std::uint32_t discriminator = 0;
  std::uint8_t size = 0;
  std::uint32_t value = 0;
};

/** @brief Contains the OVn counter mask from a decoded PMU trace-on-overflow packet. */
struct PmuTraceEvent {
  std::uint8_t overflowMask = 0;
};

/** @brief Contains a periodic DWT PC sample or its processor-sleep indication. */
struct PcSampleTraceEvent {
  std::uint32_t pc = 0;
  bool sleeping = false;
};

/** @brief Marks a decoded local timestamp packet. */
struct LocalTimestampTraceEvent {};

/** @brief Contains a decoded global timestamp packet. */
struct GlobalTimestampTraceEvent {
  std::uint64_t value = 0;
  bool clockChange = false;
};

/** @brief Records a trace overflow and its diagnostic description. */
struct OverflowTraceEvent {
  std::string message;
};

/** @brief Marks a hardware synchronization packet. */
struct SyncTraceEvent {};

/** @brief Retains a decoder warning or error in output order. */
struct TraceIssueEvent {
  TraceIssueCode code = TraceIssueCode::DecodeError;
  TraceIssueSeverity severity = TraceIssueSeverity::Error;
  std::string message;
  std::optional<std::uint64_t> rawBytesConsumed = std::nullopt;
  std::optional<std::uint64_t> lastValidTcyc = std::nullopt;
};

/** @brief Stores the semantic payload of a decoded trace event. */
using TraceEventPayload = std::variant<SoftwareTraceEvent, DwtDataTraceEvent, DwtAddressTraceEvent, ExceptionTraceEvent,
                                       DwtEventTraceEvent, PmuTraceEvent, PcSampleTraceEvent, LocalTimestampTraceEvent,
                                       GlobalTimestampTraceEvent, OverflowTraceEvent, SyncTraceEvent, TraceIssueEvent>;

/** @brief Describes timestamp and data-loss quality at an event. */
struct TraceQuality {
  bool overflow = false;
  bool timestampReliable = false;
  std::uint64_t overflowCount = 0;
};

/** @brief Wraps a semantic payload with stream, position, time, and quality metadata. */
struct TraceEvent {
  /** @brief Constructs an event from one supported semantic payload. */
  template <typename Payload>
  explicit TraceEvent(Payload eventPayload)
    : payload(std::move(eventPayload))
  {
  }

  std::uint64_t index = 0;
  // CoreSight Trace Bus ID. ID 0 identifies unformatted single-source input.
  std::uint8_t traceBusId = 0U;
  std::optional<std::uint64_t> tcyc;

  // Quality is assigned atomically by the post-decoder. Wire/control events
  // without reconstructed DWT status leave it absent.
  std::optional<TraceQuality> quality;

  TraceEventPayload payload;
};

/** @brief Returns a const payload pointer when an event contains the requested type. */
template <typename Payload> const Payload* traceEventPayload(const TraceEvent& event)
{
  return std::get_if<Payload>(&event.payload);
}

/** @brief Returns a mutable payload pointer when an event contains the requested type. */
template <typename Payload> Payload* traceEventPayload(TraceEvent& event)
{
  return std::get_if<Payload>(&event.payload);
}

/** @brief Tests whether an event contains the requested payload type. */
template <typename Payload> bool isTraceEvent(const TraceEvent& event)
{
  return std::holds_alternative<Payload>(event.payload);
}

/** @brief Receives decoded semantic trace events. */
class TraceEventSink {
public:
  /** @brief Destroys a trace event sink through its interface. */
  virtual ~TraceEventSink() = default;
  /** @brief Appends one decoded event to the sink. */
  virtual void append(const TraceEvent& event) = 0;
};

#endif // CTRACE_SRC_MODEL_TRACEEVENT_H
