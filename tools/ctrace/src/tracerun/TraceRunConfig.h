/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#ifndef CTRACE_SRC_TRACERUN_TRACERUNCONFIG_H
#define CTRACE_SRC_TRACERUN_TRACERUNCONFIG_H

#include "TraceStreamId.h"

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

/** @brief Tests whether a fixed array contains a value. */
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

/** @brief Tests whether an ITM timestamp prescaler is supported. */
constexpr bool isTimestampPrescaler(std::uint32_t prescaler)
{
  return contains(kTimestampPrescalers, prescaler);
}

/** @brief Tests whether a DWT data type is supported. */
constexpr bool isDwtDataType(const std::string_view& type)
{
  return type == "unsigned int" || type == "signed int" || type == "float";
}

/** @brief Tests whether a DWT data size is supported. */
constexpr bool isDwtDataSize(std::uint64_t size)
{
  return size == 1U || size == 2U || size == 4U;
}

/** @brief Tests whether a trace source type is decoded in the first release. */
constexpr bool supportsSource(const std::string_view& type)
{
  return type == "dwt" || type == "itm";
}

/** @brief Tests whether ctrace consumes metadata for a reference type. */
constexpr bool consumesReferenceMetadata(const std::string_view& type)
{
  return type == "dwt" || type == "itm" || type == "event" || type == "pmu" || type == "pcsample";
}

/** @brief Tests whether an ITM stimulus port number is valid. */
constexpr bool isItmSource(std::uint32_t source)
{
  return CoreSight::isItmStimulusPort(source);
}

/** @brief Converts empty processor names to an absent value. */
inline std::optional<std::string> normalizedProcessorName(std::optional<std::string> name)
{
  if (name.has_value() && name->empty()) {
    name.reset();
  }
  return name;
}

/** @brief Tests whether two optional processor names can describe the same route. */
inline bool processorNamesMayBind(const std::optional<std::string>& left, const std::optional<std::string>& right)
{
  const auto normalizedLeft = normalizedProcessorName(left);
  const auto normalizedRight = normalizedProcessorName(right);
  return !normalizedLeft.has_value() || !normalizedRight.has_value() || normalizedLeft == normalizedRight;
}

} // namespace TraceRunSchema

/** @brief Stores one parsed ctrace reference and its resolved routing metadata. */
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

/** @brief Classifies structural problems in a parsed trace reference. */
enum class ReferenceProblem {
  None,
  DuplicateSource,
  InvalidStream,
  InvalidItmSource,
};

/** @brief Tests whether a reference selects ITM stimulus port zero, which is excluded from output. */
inline bool isItmChannelZero(const TraceRunReference& reference)
{
  return reference.type == "itm" && reference.sources.size() == 1U &&
         reference.sources.front() == CoreSight::kExcludedItmStimulusPort;
}

/** @brief Tests whether a reference has the fields needed for a decoded route. */
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

/** @brief Tests whether a reference represents timestamp configuration. */
inline bool isTimestampReference(const TraceRunReference& reference)
{
  constexpr std::string_view name = "timestamps";
  const auto separator = reference.ctraceRef.rfind('/');
  const auto leaf = separator == std::string::npos ? std::string_view(reference.ctraceRef)
                                                   : std::string_view(reference.ctraceRef).substr(separator + 1U);
  return leaf == name;
}

/** @brief Tests whether a reference configures the processor ITM, including its ATB stream ID. */
inline bool isProcessorItmReference(const TraceRunReference& reference)
{
  constexpr std::string_view name = "itm";
  const auto separator = reference.ctraceRef.rfind('/');
  const auto leaf = separator == std::string::npos ? std::string_view(reference.ctraceRef)
                                                   : std::string_view(reference.ctraceRef).substr(separator + 1U);
  return reference.type == "itm" && leaf == name;
}

/** @brief Tests whether a reference participates in processor-to-stream binding. */
inline bool contributesStreamBinding(const TraceRunReference& reference)
{
  return (reference.type == "dwt" || reference.type == "itm") &&
         (!reference.sources.empty() || isTimestampReference(reference) || isProcessorItmReference(reference));
}

/** @brief Returns the first structural problem detected in a reference. */
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
  if (reference.type == "itm") {
    for (const auto source : reference.sources) {
      if (!isItmSource(source)) {
        return ReferenceProblem::InvalidItmSource;
      }
    }
  }
  return ReferenceProblem::None;
}

/** @brief Tests whether a reference is structurally valid and usable by ctrace. */
inline bool isUsableReference(const TraceRunReference& reference)
{
  return hasConsumedRouteShape(reference) && referenceProblem(reference) == ReferenceProblem::None;
}

} // namespace TraceRunSchema

/** @brief Stores timestamp-related fields copied from one trace setup. */
struct TraceRunTimestampSetup {
  std::optional<std::uint64_t> clockHz = std::nullopt;
  std::optional<std::uint32_t> timestampPrescaler = std::nullopt;
  std::optional<std::string> clockError = std::nullopt;
  std::size_t line = 0U;
};

/** @brief Stores data-type fields copied from one DWT data setup. */
struct TraceRunDataSetup {
  std::optional<std::string> symbolType = std::nullopt;
  std::optional<std::uint64_t> symbolSize = std::nullopt;
  std::optional<std::string> symbolTypeError = std::nullopt;
  std::optional<std::string> symbolSizeError = std::nullopt;
};

/** @brief Stores ITM stimulus-port configuration copied from one trace setup. */
struct TraceRunItmSetup {
  std::uint32_t enableMask = 0U;
};

/** @brief Stores the ctrace setup metadata consumed by the decoder. */
struct TraceRunSetup {
  std::optional<std::string> processorName;
  std::optional<TraceRunTimestampSetup> timestamps;
  std::optional<TraceRunItmSetup> itm;
  std::vector<TraceRunDataSetup> data;
  std::size_t line = 0U;
};

/** @brief Stores a parsed `*.ctrace-run.yml` input. */
struct TraceRunConfig {
  std::string path;
  std::vector<TraceRunReference> references;
  std::vector<TraceRunSetup> setups;
};

#endif // CTRACE_SRC_TRACERUN_TRACERUNCONFIG_H
