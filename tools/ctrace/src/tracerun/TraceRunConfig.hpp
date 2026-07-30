/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#pragma once

#include "TraceStreamId.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace TraceRunSchema {

inline constexpr std::array<std::uint32_t, 4> kTimestampPrescalers{{
    1U,
    4U,
    16U,
    64U,
}};
inline constexpr std::uint32_t kDefaultTimestampPrescaler = 1U;
inline constexpr std::string_view kDefaultDwtDataType = "unsigned int";
inline constexpr std::uint8_t kDefaultDwtDataSize = 4U;

template <typename Value, std::size_t Size>
constexpr bool contains(const std::array<Value, Size>& values, const Value& candidate)
{
  for (const auto& value : values) {
    if (value == candidate) {
      return true;
    }
  }
  return false;
}

constexpr bool isTimestampPrescaler(std::uint32_t prescaler)
{
  return contains(kTimestampPrescalers, prescaler);
}

constexpr bool isDwtDataType(std::string_view type)
{
  return type == "unsigned int" || type == "signed int" || type == "float";
}

constexpr bool isDwtDataSize(std::uint64_t size)
{
  return size == 1U || size == 2U || size == 4U;
}

constexpr bool supportsSource(std::string_view type)
{
  return type == "dwt" || type == "itm";
}

constexpr bool consumesReferenceMetadata(std::string_view type)
{
  return type == "dwt" || type == "itm" || type == "event" || type == "pmu" || type == "pcsample";
}

constexpr bool supportsSourceArray(std::string_view type)
{
  return type == "dwt";
}

constexpr bool isItmSource(std::uint32_t source)
{
  return source <= 31U;
}

inline std::optional<std::string> normalizedProcessorName(std::optional<std::string> name)
{
  if (name.has_value() && name->empty()) {
    name.reset();
  }
  return name;
}

inline bool processorNamesMayBind(const std::optional<std::string>& left, const std::optional<std::string>& right)
{
  const auto normalizedLeft = normalizedProcessorName(left);
  const auto normalizedRight = normalizedProcessorName(right);
  return !normalizedLeft.has_value() || !normalizedRight.has_value() || normalizedLeft == normalizedRight;
}

} // namespace TraceRunSchema

struct TraceRunReference {
  std::string ctraceRef;
  std::string type;
  std::optional<std::string> processorName;
  std::optional<std::string> info;
  std::optional<std::string> warning;
  std::optional<std::string> error;
  std::optional<std::uint64_t> symbolAddress;
  std::optional<std::string> label;
  // The reader retains the complete YAML value. CtraceRunMeta validates and
  // narrows it to the CoreSight ATB trace-ID domain.
  std::optional<std::uint32_t> stream;
  std::vector<std::uint32_t> sources;
  // Index of the referenced ctrace-setup.data entry, derived from the
  // specified ctrace-ref path form [<pname>/]data#<index>.
  std::optional<std::size_t> dataSetupIndex;
  std::size_t line = 0U;
};

namespace TraceRunSchema {

enum class ReferenceProblem {
  None,
  DuplicateSource,
  InvalidStream,
  UnsupportedSourceArray,
  InvalidItmSource,
};

inline bool isItmChannelZero(const TraceRunReference& reference)
{
  return reference.type == "itm" && reference.sources.size() == 1U && reference.sources.front() == 0U;
}

inline bool hasConsumedRouteShape(const TraceRunReference& reference)
{
  if (!supportsSource(reference.type) || reference.sources.empty()) {
    return false;
  }
  if (reference.type == "itm") {
    return !isItmChannelZero(reference);
  }
  return true;
}

inline bool isTimestampReference(const TraceRunReference& reference)
{
  constexpr std::string_view name = "timestamps";
  const auto separator = reference.ctraceRef.rfind('/');
  const auto leaf = separator == std::string::npos ? std::string_view(reference.ctraceRef)
                                                   : std::string_view(reference.ctraceRef).substr(separator + 1U);
  return leaf == name;
}

inline bool contributesStreamBinding(const TraceRunReference& reference)
{
  return (reference.type == "dwt" || reference.type == "itm") &&
         (!reference.sources.empty() || isTimestampReference(reference));
}

inline ReferenceProblem referenceProblem(const TraceRunReference& reference)
{
  for (std::size_t left = 0U; left < reference.sources.size(); ++left) {
    for (std::size_t right = left + 1U; right < reference.sources.size(); ++right) {
      if (reference.sources[left] == reference.sources[right]) {
        return ReferenceProblem::DuplicateSource;
      }
    }
  }
  if (reference.stream.has_value() && !CoreSight::isAtbTraceId(*reference.stream)) {
    return ReferenceProblem::InvalidStream;
  }
  if (!supportsSourceArray(reference.type) && reference.sources.size() > 1U) {
    return ReferenceProblem::UnsupportedSourceArray;
  }
  if (reference.type == "itm") {
    for (const auto source : reference.sources) {
      if (!isItmSource(source)) {
        return ReferenceProblem::InvalidItmSource;
      }
    }
  }
  return ReferenceProblem::None;
}

inline bool isUsableReference(const TraceRunReference& reference)
{
  return hasConsumedRouteShape(reference) && referenceProblem(reference) == ReferenceProblem::None;
}

} // namespace TraceRunSchema

struct TraceRunTimestampSetup {
  std::optional<std::uint64_t> clockHz = std::nullopt;
  std::optional<std::uint32_t> timestampPrescaler = std::nullopt;
  std::optional<std::string> clockError = std::nullopt;
  std::size_t line = 0U;
};

struct TraceRunDataSetup {
  std::optional<std::string> symbolType = std::nullopt;
  std::optional<std::uint64_t> symbolSize = std::nullopt;
  std::optional<std::string> symbolTypeError = std::nullopt;
  std::optional<std::string> symbolSizeError = std::nullopt;
};

struct TraceRunItmSetup {
  std::uint32_t enableMask = 0U;
};

struct TraceRunSetup {
  std::optional<std::string> processorName;
  std::optional<TraceRunTimestampSetup> timestamps;
  std::optional<TraceRunItmSetup> itm;
  std::vector<TraceRunDataSetup> data;
  std::size_t line = 0U;
};

struct TraceRunConfig {
  std::string path;
  std::vector<TraceRunReference> references;
  std::vector<TraceRunSetup> setups;
};
