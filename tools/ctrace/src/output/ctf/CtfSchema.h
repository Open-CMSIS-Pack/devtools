/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_OUTPUT_CTF_CTFSCHEMA_H
#define CTRACE_SRC_OUTPUT_CTF_CTFSCHEMA_H

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
  DwtValue = 4U,
  TraceStatus = 5U,
  DwtAddress = 6U,
  Exception = 7U,
  GlobalTimestamp = 8U,
};

/** @brief Classifies CTF trace-status records. */
enum class TraceStatusReason : std::uint8_t {
  TraceStart = 0U,
  Resync = 1U,
  Overflow = 2U,
  DecodeError = 4U,
  DataLoss = 5U,
};

/** @brief Encodes DWT read and write access in CTF records. */
enum class DwtAccess : std::uint8_t {
  Read = 0U,
  Write = 1U,
};

/** @brief Encodes exception entry and exit actions in CTF records. */
enum class ExceptionAction : std::uint8_t {
  Entered = 1U,
  Exited = 2U,
};

/** @brief Identifies the supported CTF sample value encodings. */
enum class ValueTag : std::uint8_t {
  Character8 = 0U,
  Signed8 = 1U,
  Unsigned8 = 2U,
  Signed16 = 3U,
  Unsigned16 = 4U,
  Signed32 = 5U,
  Unsigned32 = 6U,
  Float32 = 7U,
};

/** @brief Describes one supported CTF sample value encoding. */
struct ValueVariant {
  ValueTag tag;
  std::string_view name;
  std::string_view traceCompassType;
  std::uint8_t byteSize;
  bool floatingPoint;
  bool signedInteger;
};

inline constexpr std::array<ValueVariant, 8U> ValueVariants{{
    {ValueTag::Character8, "c8", "long", 1U, false, false},
    {ValueTag::Signed8, "i8", "long", 1U, false, true},
    {ValueTag::Unsigned8, "u8", "long", 1U, false, false},
    {ValueTag::Signed16, "i16", "long", 2U, false, true},
    {ValueTag::Unsigned16, "u16", "long", 2U, false, false},
    {ValueTag::Signed32, "i32", "long", 4U, false, true},
    {ValueTag::Unsigned32, "u32", "long", 4U, false, false},
    {ValueTag::Float32, "f32", "double", 4U, true, false},
}};

inline constexpr std::string_view ValueTypeRequirements =
    "supported data.symbol-type values are 'unsigned int', 'signed int', and 'float'; "
    "data.symbol-size must be 1, 2, or 4, and float requires size 4";

/** @brief Returns the schema descriptor for a value tag. */
constexpr const ValueVariant& valueVariant(ValueTag tag)
{
  return ValueVariants[static_cast<std::size_t>(tag)];
}

/** @brief Resolves trace-run type metadata to a supported CTF value encoding. */
constexpr const ValueVariant* valueVariantForTraceRunType(const std::string_view& typeName, std::uint64_t byteSize)
{
  if (typeName == "float") {
    return byteSize == 4U ? &valueVariant(ValueTag::Float32) : nullptr;
  }

  const auto signedInteger = typeName == "signed int";
  if (!signedInteger && typeName != "unsigned int") {
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
  }
  return "UNKNOWN";
}

} // namespace CtfSchema

#endif  // CTRACE_SRC_OUTPUT_CTF_CTFSCHEMA_H
