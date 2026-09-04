/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFSCHEMA_H
#define CTRACE_SRC_OUTPUT_CTF_CTFSCHEMA_H

#include "TraceEvent.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace CtfSchema {

inline constexpr std::uint32_t Magic = 0xC1FC1FC1U;
inline constexpr std::uint32_t SwoStreamId = 0U;

/** @brief Identifies the event records emitted by ctrace CTF output. */
enum class EventId : std::uint32_t {
  Itm = 0U,
  DwtValue = 1U,
  DwtAddress = 2U,
  TraceStatus = 3U,
  Exception = 4U,
  GlobalTimestamp = 5U,
  PcSample = 6U,
  DwtEvent = 7U,
  PmuEvent = 8U,
  DwtMatch = 9U,
};

/** @brief Classifies CTF trace-status records. */
enum class TraceStatusReason : std::uint8_t {
  TraceStart = 0U,
  Resync = 1U,
  Overflow = 2U,
  DecodeError = 3U,
  DataLoss = 4U,
};

/** @brief Encodes DWT read and write access in CTF records. */
enum class DwtAccess : std::uint8_t {
  Read = 0U,
  Write = 1U,
};

/** @brief Encodes exception entry, exit, and return actions in CTF records. */
enum class ExceptionAction : std::uint8_t {
  Entered = 0U,
  Exited = 1U,
  Returned = 2U,
};

/** @brief Identifies whether an exception record came from trace input or lane reconstruction. */
enum class ExceptionOrigin : std::uint8_t {
  Trace = 0U,
  Synthetic = 1U,
};

/** @brief Identifies whether a periodic PC sample carries a PC or reports processor sleep. */
enum class PcSampleState : std::uint8_t {
  Sleep = 0U,
  Pc = 1U,
};

/** @brief Identifies the supported CTF sample value encodings. */
enum class ValueTag : std::uint8_t {
  Signed8 = 0U,
  Unsigned8 = 1U,
  Signed16 = 2U,
  Unsigned16 = 3U,
  Signed32 = 4U,
  Unsigned32 = 5U,
  Float32 = 6U,
};

/** @brief Identifies the width of a raw DWT address offset in CTF. */
enum class DwtOffsetTag : std::uint8_t {
  None = 0U,
  U8 = 1U,
  U16 = 2U,
  U32 = 4U,
};

/** @brief Describes one CTF DWT address-offset representation. */
struct DwtOffsetVariant {
  DwtOffsetTag tag;
  std::string_view name;
  std::uint8_t byteSize;
};

inline constexpr std::array<DwtOffsetVariant, 4U> DwtOffsetVariants{{
    {DwtOffsetTag::None, "none", 1U},
    {DwtOffsetTag::U8, "u8", 1U},
    {DwtOffsetTag::U16, "u16", 2U},
    {DwtOffsetTag::U32, "u32", 4U},
}};

/** @brief Describes one supported CTF sample value encoding. */
struct ValueVariant {
  ValueTag tag;
  std::string_view name;
  std::string_view traceCompassType;
  std::uint8_t byteSize;
  bool floatingPoint;
  bool signedInteger;
};

inline constexpr std::array<ValueVariant, 7U> ValueVariants{{
    {ValueTag::Signed8, "i8", "long", 1U, false, true},
    {ValueTag::Unsigned8, "u8", "long", 1U, false, false},
    {ValueTag::Signed16, "i16", "long", 2U, false, true},
    {ValueTag::Unsigned16, "u16", "long", 2U, false, false},
    {ValueTag::Signed32, "i32", "long", 4U, false, true},
    {ValueTag::Unsigned32, "u32", "long", 4U, false, false},
    {ValueTag::Float32, "f32", "double", 4U, true, false},
}};

inline constexpr std::string_view ValueTypeRequirements =
    "supported data-type values are 'unsigned', 'signed', and 'float'; "
    "size must be 1, 2, or 4, and float requires size 4";

/** @brief Returns the schema descriptor for a value tag. */
constexpr const ValueVariant& valueVariant(ValueTag tag)
{
  return ValueVariants[static_cast<std::size_t>(tag)];
}

/** @brief Resolves a DWT address-offset payload width to its CTF representation. */
constexpr const DwtOffsetVariant* dwtOffsetVariantForSize(std::uint8_t byteSize)
{
  for (const auto& variant : DwtOffsetVariants) {
    if (variant.tag != DwtOffsetTag::None && variant.byteSize == byteSize) {
      return &variant;
    }
  }
  return nullptr;
}

/** @brief Resolves trace-run type metadata to a supported CTF value encoding. */
constexpr const ValueVariant* valueVariantForTraceRunType(const std::string_view& typeName, std::uint64_t byteSize)
{
  if (typeName == "float") {
    return byteSize == 4U ? &valueVariant(ValueTag::Float32) : nullptr;
  }

  const auto signedInteger = typeName == "signed";
  if (!signedInteger && typeName != "unsigned") {
    return nullptr;
  }
  if (byteSize == 1U) {
    return &valueVariant(signedInteger ? ValueTag::Signed8 : ValueTag::Unsigned8);
  }
  if (byteSize == 2U) {
    return &valueVariant(signedInteger ? ValueTag::Signed16 : ValueTag::Unsigned16);
  }
  if (byteSize == 4U) {
    return &valueVariant(signedInteger ? ValueTag::Signed32 : ValueTag::Unsigned32);
  }
  return nullptr;
}

inline constexpr std::uint8_t SampleFlagOverflow = 1U << 0U;
inline constexpr std::uint8_t SampleFlagTimestampReliable = 1U << 1U;
inline constexpr std::uint8_t SampleFlagBeforeFirstTimestamp = 1U << 2U;

/** @brief Returns the integer representation of an event ID. */
constexpr std::uint32_t value(EventId id)
{
  return static_cast<std::uint32_t>(id);
}

/** @brief Returns the integer representation of a trace-status reason. */
constexpr std::uint8_t value(TraceStatusReason reason)
{
  return static_cast<std::uint8_t>(reason);
}

/** @brief Returns the integer representation of a value tag. */
constexpr std::uint8_t value(ValueTag tag)
{
  return static_cast<std::uint8_t>(tag);
}

/** @brief Returns the integer representation of a DWT offset tag. */
constexpr std::uint8_t value(DwtOffsetTag tag)
{
  return static_cast<std::uint8_t>(tag);
}

/** @brief Returns the integer representation of a DWT access type. */
constexpr std::uint8_t value(DwtAccess access)
{
  return static_cast<std::uint8_t>(access);
}

/** @brief Returns the integer representation of an exception action. */
constexpr std::uint8_t value(ExceptionAction action)
{
  return static_cast<std::uint8_t>(action);
}

/** @brief Returns the integer representation of a PC-sample state. */
constexpr std::uint8_t value(PcSampleState state)
{
  return static_cast<std::uint8_t>(state);
}

/** @brief Returns the integer representation of an exception record origin. */
constexpr std::uint8_t value(ExceptionOrigin origin)
{
  return static_cast<std::uint8_t>(origin);
}

/** @brief Returns the CTF enumeration value for one DWT event counter. */
constexpr std::uint8_t value(DwtEventCounter counter)
{
  return static_cast<std::uint8_t>(counter);
}

/** @brief Returns the public counter name used by CTF metadata and Trace Compass. */
constexpr std::string_view dwtEventCounterName(DwtEventCounter counter)
{
  switch (counter) {
  case DwtEventCounter::Cpi:
    return "CPICNT";
  case DwtEventCounter::Exception:
    return "EXCCNT";
  case DwtEventCounter::Sleep:
    return "SLEEPCNT";
  case DwtEventCounter::LoadStore:
    return "LSUCNT";
  case DwtEventCounter::Fold:
    return "FOLDCNT";
  case DwtEventCounter::Cycle:
    return "CYCCNT";
  }
  return "UNKNOWN";
}

/** @brief Returns the CTF enumeration value for one programmable PMU event counter. */
constexpr std::uint8_t value(PmuEventCounter counter)
{
  return static_cast<std::uint8_t>(counter);
}

/** @brief Returns the provisional PMU counter name used until trace-run configuration can resolve it. */
constexpr std::string_view pmuEventCounterName(PmuEventCounter counter)
{
  switch (counter) {
  case PmuEventCounter::Event0:
    return "Event0";
  case PmuEventCounter::Event1:
    return "Event1";
  case PmuEventCounter::Event2:
    return "Event2";
  case PmuEventCounter::Event3:
    return "Event3";
  case PmuEventCounter::Event4:
    return "Event4";
  case PmuEventCounter::Event5:
    return "Event5";
  case PmuEventCounter::Event6:
    return "Event6";
  case PmuEventCounter::Event7:
    return "Event7";
  }
  return "UNKNOWN";
}

/** @brief Returns the stable schema name of a CTF event ID. */
constexpr std::string_view eventName(EventId id)
{
  switch (id) {
  case EventId::Itm:
    return "ITM";
  case EventId::DwtValue:
    return "DWT_VALUE";
  case EventId::TraceStatus:
    return "TRACE_STATUS";
  case EventId::DwtAddress:
    return "DWT_ADDR";
  case EventId::Exception:
    return "EXCEPTION";
  case EventId::GlobalTimestamp:
    return "GLOBAL_TIMESTAMP";
  case EventId::PcSample:
    return "PC_SAMPLE";
  case EventId::DwtEvent:
    return "DWT_EVENT";
  case EventId::PmuEvent:
    return "PMU_EVENT";
  case EventId::DwtMatch:
    return "DWT_MATCH";
  }
  return "UNKNOWN";
}

} // namespace CtfSchema

#endif // CTRACE_SRC_OUTPUT_CTF_CTFSCHEMA_H
