/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceRunMeta.hpp"

#include "TraceRunConfig.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct ProcessorMeta {
  std::optional<std::string> name;
  bool timestampsEnabled = false;
  std::optional<std::uint64_t> timestampClockHz;
  std::optional<std::string> timestampClockError;
  std::optional<std::uint32_t> timestampPrescaler;
  std::optional<std::uint32_t> itmEnableMask;
};

struct ProcessorIdentity {
  bool multipleProcessors = false;
  std::optional<std::string> singleProcessorName;

  std::optional<std::string> canonicalName(const std::optional<std::string>& name) const
  {
    if (!multipleProcessors) {
      return singleProcessorName;
    }
    return TraceRunSchema::normalizedProcessorName(name);
  }
};

using ReferenceProblem = TraceRunSchema::ReferenceProblem;

std::string configError(const TraceRunConfig& config, std::size_t line, const std::string& message)
{
  auto location = config.path;
  if (line > 0U) {
    location += "(" + std::to_string(line) + ")";
  }
  return location + ": " + message;
}

std::string referenceProblemMessage(const TraceRunConfig& config, const TraceRunReference& reference,
                                    ReferenceProblem problem)
{
  if (problem == ReferenceProblem::DuplicateSource) {
    return configError(config, reference.line,
                       reference.line > 0U ? "duplicate value in 'source' array" : "duplicate value in source array");
  }
  if (problem == ReferenceProblem::InvalidStream) {
    return configError(config, reference.line,
                       reference.line > 0U ? "'stream' must be a CoreSight ATB trace ID between 1 and 111"
                                           : "stream must be a CoreSight ATB trace ID between 1 and 111");
  }
  if (problem == ReferenceProblem::UnsupportedSourceArray) {
    return configError(config, reference.line, "source arrays are only supported for DWT references");
  }
  return configError(config, reference.line,
                     reference.line > 0U ? "ITM 'source' must be between 0 and 31"
                                         : "ITM source must be between 0 and 31");
}

bool consumesSetup(const TraceRunConfig& config, const TraceRunSetup& setup)
{
  // LCOV_EXCL_BR_START: all setup alternatives are covered; GCC expands short-circuit bookkeeping
  if (setup.timestamps.has_value() || setup.itm.has_value()) {
    return true;
  }
  // LCOV_EXCL_BR_STOP
  for (const auto& reference : config.references) {
    // LCOV_EXCL_BR_START: all binding alternatives are covered; GCC expands short-circuit bookkeeping
    if (!TraceRunSchema::isUsableReference(reference) || reference.type != "dwt" ||
        !TraceRunSchema::processorNamesMayBind(setup.processorName, reference.processorName)) {
      continue;
    }
    // LCOV_EXCL_BR_STOP
    const auto index = reference.dataSetupIndex;
    // LCOV_EXCL_BR_START: valid, absent, and out-of-range data routes are covered
    if (index.has_value() && *index < setup.data.size()) {
      return true;
    }
    // LCOV_EXCL_BR_STOP
  }
  return false;
}

bool isUsableStreamBinding(const TraceRunReference& reference)
{
  return TraceRunSchema::contributesStreamBinding(reference) &&
         TraceRunSchema::referenceProblem(reference) == ReferenceProblem::None;
}

ProcessorIdentity processorIdentity(const TraceRunConfig& config)
{
  std::set<std::string> setupNames;
  std::set<std::optional<std::string>> uniqueSetupNames;
  bool unnamedSetup = false;
  std::size_t setupCount = 0U;
  const TraceRunSetup* singleActiveSetup = nullptr;
  for (const auto& setup : config.setups) {
    if (!consumesSetup(config, setup)) {
      continue;
    }
    ++setupCount;
    singleActiveSetup = &setup;
    const auto name = TraceRunSchema::normalizedProcessorName(setup.processorName);
    if (!uniqueSetupNames.insert(name).second) {
      throw std::runtime_error(configError(config, setup.line,
                                           (setup.line > 0U ? "duplicate active 'ctrace-setup' for pname '"
                                                            : "duplicate active ctrace-setup for pname '") +
                                               name.value_or("<unnamed>") + "'"));
    }
    if (name.has_value()) {
      setupNames.insert(*name);
    } else {
      unnamedSetup = true;
    }
  }

  std::set<std::string> referenceNames;
  bool unnamedReference = false;
  for (const auto& reference : config.references) {
    if (!isUsableStreamBinding(reference)) {
      continue;
    }
    const auto name = TraceRunSchema::normalizedProcessorName(reference.processorName);
    if (name.has_value()) {
      referenceNames.insert(*name);
    } else {
      unnamedReference = true;
    }
  }

  if (setupCount > 1U) {
    if (unnamedSetup || unnamedReference) { // LCOV_EXCL_BR_LINE: both missing-name sources have the same diagnostic
      throw std::runtime_error(config.path + ": pname is required for every ctrace-setup and ctrace-ref "
                                             "in a multi-processor configuration");
    }
    for (const auto& name : referenceNames) {
      if (setupNames.find(name) == setupNames.end()) {
        throw std::runtime_error(config.path + ": ctrace-ref pname '" + name + "' has no matching ctrace-setup");
      }
    }
    return {true, std::nullopt};
  }

  if (setupCount == 1U) {
    const auto setupName = TraceRunSchema::normalizedProcessorName(singleActiveSetup->processorName);
    if (setupName.has_value()) {
      for (const auto& name : referenceNames) {
        if (name != *setupName) {
          throw std::runtime_error(config.path + ": ctrace-ref pname '" + name +
                                   "' has no matching ctrace-setup pname '" + *setupName + "'");
        }
      }
      return {false, setupName};
    }
    if (referenceNames.size() > 1U) {
      throw std::runtime_error(config.path + ": pname is required for ctrace-setup in a multi-processor configuration");
    }
    return {
        false,
        referenceNames.empty() ? std::nullopt : std::optional<std::string>(*referenceNames.begin()),
    }; // LCOV_EXCL_BR_LINE: generated optional return exception edge
  }

  if (referenceNames.size() > 1U) {
    if (unnamedReference) {
      throw std::runtime_error(config.path +
                               ": pname is required for every ctrace-ref in a multi-processor configuration");
    }
    return {true, std::nullopt};
  }
  return {
      false,
      referenceNames.empty() ? std::nullopt : std::optional<std::string>(*referenceNames.begin()),
  };
}

const TraceRunDataSetup* referencedDataSetup(const TraceRunConfig& config, const TraceRunReference& reference)
{
  const auto index = reference.dataSetupIndex;
  if (!index.has_value()) {
    return nullptr;
  }
  for (const auto& setup : config.setups) {
    if (!TraceRunSchema::processorNamesMayBind(setup.processorName, reference.processorName)) {
      continue;
    }
    const auto* candidate = *index < setup.data.size() ? &setup.data[*index] : nullptr;
    if (candidate != nullptr) {
      return candidate;
    }
  }
  return nullptr;
}

CtraceRunSourceMeta sourceMeta(const TraceRunConfig& config, const TraceRunReference& reference, std::uint32_t source,
                               const ProcessorIdentity& processorIdentity)
{
  const auto* dataSetup = reference.type == "dwt" ? referencedDataSetup(config, reference) : nullptr;
  return {
      reference.type,
      processorIdentity.canonicalName(reference.processorName),
      static_cast<std::uint8_t>(reference.stream.value_or(0U)),
      source,
      reference.label,
      reference.type == "dwt" ? reference.symbolAddress : std::nullopt,
      dataSetup != nullptr
          ? dataSetup->symbolType.value_or(std::string(TraceRunSchema::kDefaultDwtDataType)) // LCOV_EXCL_BR_LINE
          : std::string(TraceRunSchema::kDefaultDwtDataType),
      dataSetup != nullptr ? dataSetup->symbolSize.value_or(TraceRunSchema::kDefaultDwtDataSize)
                           : TraceRunSchema::kDefaultDwtDataSize,
      dataSetup != nullptr ? dataSetup->symbolTypeError : std::nullopt,
      dataSetup != nullptr ? dataSetup->symbolSizeError : std::nullopt,
  }; // LCOV_EXCL_BR_LINE: generated aggregate exception edge
}

ProcessorMeta& processorMeta(std::vector<ProcessorMeta>& processors, const std::optional<std::string>& name)
{
  const auto found = std::find_if(processors.begin(), processors.end(),
                                  [&](const ProcessorMeta& processor) { return processor.name == name; });
  if (found != processors.end()) {
    return *found;
  }
  processors.emplace_back();
  processors.back().name = name;
  return processors.back();
}

template <typename Value>
std::optional<Value> commonProcessorSetting(const std::vector<ProcessorMeta>& processors,
                                            const std::optional<Value> ProcessorMeta::* member)
{
  std::optional<Value> common;
  for (const auto& processor : processors) {
    if (!processor.timestampsEnabled) {
      continue;
    }
    const auto& candidate = processor.*member;
    if (!candidate.has_value()) {
      return std::nullopt;
    }
    if (common.has_value() && common != candidate) {
      return std::nullopt;
    }
    common = candidate;
  }
  return common;
}

const ProcessorMeta* findProcessor(const std::vector<ProcessorMeta>& processors, const std::optional<std::string>& name)
{
  const auto found = std::find_if(processors.begin(), processors.end(),
                                  [&](const ProcessorMeta& processor) { return processor.name == name; });
  return found != processors.end() ? &*found : nullptr; // LCOV_EXCL_BR_LINE: found and absent processors are covered
}

struct ResolvedStreamBinding {
  std::size_t line = 0U;
  std::uint8_t traceBusId = 0U;
  std::optional<std::string> processorName;
  const ProcessorMeta* processor = nullptr;
};

std::vector<ResolvedStreamBinding> resolveStreamBindings(const TraceRunConfig& config,
                                                         const ProcessorIdentity& processorIdentity,
                                                         const std::vector<ProcessorMeta>& processors)
{
  std::vector<ResolvedStreamBinding> bindings;
  for (const auto& reference : config.references) {
    if (!isUsableStreamBinding(reference)) {
      continue;
    }
    const auto processorName = processorIdentity.canonicalName(reference.processorName);
    bindings.push_back({
        reference.line,
        static_cast<std::uint8_t>(reference.stream.value_or(0U)),
        processorName,
        findProcessor(processors, processorName),
    });
  }
  return bindings;
} // LCOV_EXCL_LINE: GCC attributes the generated binding cleanup to this closing brace

std::map<std::uint8_t, CtraceRunTimestampMeta>
buildTimestampsByTraceBusId(const std::vector<ResolvedStreamBinding>& bindings)
{
  std::map<std::uint8_t, CtraceRunTimestampMeta> result;
  for (const auto& binding : bindings) {
    CtraceRunTimestampMeta candidate;
    candidate.processorName = binding.processorName;
    if (binding.processor != nullptr && binding.processor->timestampsEnabled) { // LCOV_EXCL_BR_LINE
      candidate.clockHz = binding.processor->timestampClockHz;
      candidate.clockError = binding.processor->timestampClockError;
    }

    const auto [found, inserted] = result.emplace(binding.traceBusId, candidate);
    // LCOV_EXCL_BR_START: all conflict alternatives are covered; GCC expands short-circuit bookkeeping
    if (inserted || (found->second.processorName == candidate.processorName &&
                     found->second.clockHz == candidate.clockHz && found->second.clockError == candidate.clockError)) {
      continue;
    }
    // LCOV_EXCL_BR_STOP
    found->second.clockHz.reset();
    found->second.clockError = "CoreSight Trace Bus ID " + std::to_string(binding.traceBusId) +
                               " is assigned to multiple processors with different timestamps.clock settings";
  }
  return result;
} // LCOV_EXCL_LINE: GCC attributes the generated map cleanup to this closing brace

std::map<std::uint8_t, std::uint32_t>
buildTimestampPrescalersByTraceBusId(const TraceRunConfig& config, const std::vector<ResolvedStreamBinding>& bindings)
{
  std::map<std::uint8_t, std::uint32_t> result;
  for (const auto& binding : bindings) {
    const auto prescaler = binding.processor != nullptr && binding.processor->timestampPrescaler.has_value()
                               ? *binding.processor->timestampPrescaler // LCOV_EXCL_BR_LINE
                               : TraceRunSchema::kDefaultTimestampPrescaler;
    const auto [found, inserted] = result.emplace(binding.traceBusId, prescaler);
    if (!inserted && found->second != prescaler) {
      throw std::runtime_error(configError(config, binding.line,
                                           "CoreSight Trace Bus ID " + std::to_string(binding.traceBusId) +
                                               " is assigned to processors with different "
                                               "timestamps.itm-prescaler values"));
    }
  }
  return result;
}

std::optional<std::uint32_t> commonItmEnableMask(const std::vector<ProcessorMeta>& processors)
{
  std::optional<std::uint32_t> common;
  for (const auto& processor : processors) {
    if (!processor.itmEnableMask.has_value()) {
      return std::nullopt;
    }
    // LCOV_EXCL_BR_START: equal and conflicting masks are covered; GCC expands short-circuit bookkeeping
    if (common.has_value() && common != processor.itmEnableMask) {
      return std::nullopt;
    }
    // LCOV_EXCL_BR_STOP
    common = processor.itmEnableMask;
  }
  return common;
}

std::map<std::uint8_t, std::uint32_t>
buildItmEnableMasksByTraceBusId(const std::vector<ResolvedStreamBinding>& bindings)
{
  std::map<std::uint8_t, std::optional<std::uint32_t>> candidates;
  std::set<std::uint8_t> ambiguous;
  for (const auto& binding : bindings) {
    const auto enableMask =
        binding.processor != nullptr ? binding.processor->itmEnableMask : std::nullopt; // LCOV_EXCL_BR_LINE
    const auto [found, inserted] = candidates.emplace(binding.traceBusId, enableMask);
    if (!inserted && found->second != enableMask) {
      ambiguous.insert(binding.traceBusId);
    }
  }

  std::map<std::uint8_t, std::uint32_t> result;
  for (const auto& [traceBusId, enableMask] : candidates) {
    if (enableMask.has_value() && ambiguous.find(traceBusId) == ambiguous.end()) {
      result.emplace(traceBusId, *enableMask);
    }
  }
  return result;
}

bool containsDistinctProcessorPrescalers(const std::vector<ProcessorMeta>& processors)
{
  std::optional<std::uint32_t> first;
  for (const auto& processor : processors) {
    if (!processor.timestampsEnabled || !processor.timestampPrescaler.has_value()) { // LCOV_EXCL_BR_LINE
      continue;
    }
    if (first.has_value() && first != processor.timestampPrescaler) {
      return true;
    }
    first = processor.timestampPrescaler;
  }
  return false;
}

} // namespace

CtraceRunMeta CtraceRunMeta::fromConfig(const TraceRunConfig& config)
{
  CtraceRunMeta ctraceRunMeta;
  ctraceRunMeta.configPath_ = config.path;

  for (const auto& reference : config.references) {
    if (!TraceRunSchema::hasConsumedRouteShape(reference) && !TraceRunSchema::contributesStreamBinding(reference)) {
      continue;
    }
    const auto problem = TraceRunSchema::referenceProblem(reference);
    if (problem != ReferenceProblem::None && !reference.error.has_value()) {
      throw std::runtime_error(referenceProblemMessage(config, reference, problem));
    }
  }
  for (const auto& setup : config.setups) {
    if (!consumesSetup(config, setup) || !setup.timestamps.has_value() ||
        !setup.timestamps->timestampPrescaler.has_value() ||
        TraceRunSchema::isTimestampPrescaler(*setup.timestamps->timestampPrescaler)) {
      continue;
    }
    throw std::runtime_error(configError(config, setup.timestamps->line,
                                         setup.timestamps->line > 0U
                                             ? "'timestamps.itm-prescaler' must be one of 1, 4, 16, or 64"
                                             : "ctrace-setup timestamps.itm-prescaler must be one of 1, 4, 16, or 64"));
  }

  const auto identity = processorIdentity(config);
  std::vector<ProcessorMeta> processors;

  for (const auto& setup : config.setups) {
    if (!consumesSetup(config, setup)) {
      continue;
    }
    const auto processorName = identity.canonicalName(setup.processorName);
    auto& processor = processorMeta(processors, processorName);
    if (setup.timestamps.has_value()) {
      processor.timestampsEnabled = true;
      processor.timestampClockHz = setup.timestamps->clockHz;
      processor.timestampClockError = setup.timestamps->clockError;
      processor.timestampPrescaler =
          setup.timestamps->timestampPrescaler.value_or(TraceRunSchema::kDefaultTimestampPrescaler);
      if (setup.timestamps->clockError.has_value()) {
        ctraceRunMeta.timestampClockErrors_.push_back(*setup.timestamps->clockError);
      }
    }
    if (setup.itm.has_value()) {
      processor.itmEnableMask = setup.itm->enableMask;
    }
  }

  for (const auto& reference : config.references) {
    if (isUsableStreamBinding(reference)) {
      (void)processorMeta(processors, identity.canonicalName(reference.processorName));
    }
    if (!TraceRunSchema::isUsableReference(reference)) {
      continue;
    }
    for (const auto source : reference.sources) {
      ctraceRunMeta.sources_.push_back(sourceMeta(config, reference, source, identity));
    }
  }
  const auto streamBindings = resolveStreamBindings(config, identity, processors);
  ctraceRunMeta.timestampClockHz_ = commonProcessorSetting(processors, &ProcessorMeta::timestampClockHz);
  ctraceRunMeta.timestampsByTraceBusId_ = buildTimestampsByTraceBusId(streamBindings);
  ctraceRunMeta.timestampPrescaler_ = commonProcessorSetting(processors, &ProcessorMeta::timestampPrescaler);
  ctraceRunMeta.timestampPrescalersByTraceBusId_ = buildTimestampPrescalersByTraceBusId(config, streamBindings);
  ctraceRunMeta.itmEnableMask_ = commonItmEnableMask(processors);
  ctraceRunMeta.itmEnableMasksByTraceBusId_ = buildItmEnableMasksByTraceBusId(streamBindings);
  ctraceRunMeta.processorCount_ = processors.size();
  ctraceRunMeta.distinctProcessorPrescalers_ = containsDistinctProcessorPrescalers(processors);

  return ctraceRunMeta;
}

const std::string& CtraceRunMeta::configPath() const
{
  return configPath_;
}

const std::optional<std::uint64_t>& CtraceRunMeta::timestampClockHz() const
{
  return timestampClockHz_;
}

const std::map<std::uint8_t, CtraceRunTimestampMeta>& CtraceRunMeta::timestampsByTraceBusId() const
{
  return timestampsByTraceBusId_;
}

const std::optional<std::uint32_t>& CtraceRunMeta::timestampPrescaler() const
{
  return timestampPrescaler_;
}

const std::map<std::uint8_t, std::uint32_t>& CtraceRunMeta::timestampPrescalersByTraceBusId() const
{
  return timestampPrescalersByTraceBusId_;
}

const std::optional<std::uint32_t>& CtraceRunMeta::itmEnableMask() const
{
  return itmEnableMask_;
}

const std::map<std::uint8_t, std::uint32_t>& CtraceRunMeta::itmEnableMasksByTraceBusId() const
{
  return itmEnableMasksByTraceBusId_;
}

const std::vector<std::string>& CtraceRunMeta::timestampClockErrors() const
{
  return timestampClockErrors_;
}

bool CtraceRunMeta::hasDistinctProcessorPrescalers() const
{
  return distinctProcessorPrescalers_;
}

std::size_t CtraceRunMeta::processorCount() const
{
  return processorCount_;
}

const std::vector<CtraceRunSourceMeta>& CtraceRunMeta::sources() const
{
  return sources_;
}
