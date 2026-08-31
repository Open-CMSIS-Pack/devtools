/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtraceRunMeta.h"

#include "TraceRunConfig.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

/** @brief Accumulates normalized metadata for one processor. */
struct ProcessorMeta {
  std::optional<std::string> name;
  bool timestampsEnabled = false;
  std::optional<std::uint64_t> timestampClockHz;
  std::optional<std::string> timestampClockError;
  std::optional<std::uint32_t> timestampPrescaler;
  std::optional<std::uint32_t> itmEnableMask;
};

/** @brief Identifies one processor and its optional Trace Bus ID. */
struct ProcessorIdentity {
  bool multipleProcessors = false;
  std::optional<std::string> singleProcessorName;
  bool constrainedBySetups = false;
  std::set<std::string> setupNames;

  /** @brief Tests whether a reference is consistent with the active setups. */
  bool accepts(const TraceRunReference& reference) const
  {
    if (!constrainedBySetups || setupNames.empty()) {
      return true;
    }
    const auto name = TraceRunSchema::normalizedProcessorName(reference.processorName);
    if (!name.has_value()) {
      return setupNames.size() == 1U;
    }
    return setupNames.find(*name) != setupNames.end();
  }

  /** @brief Resolves an optional processor name to its canonical binding name. */
  std::optional<std::string> canonicalName(const std::optional<std::string>& name) const
  {
    if (!multipleProcessors) {
      return singleProcessorName;
    }
    return TraceRunSchema::normalizedProcessorName(name);
  }
};

using ReferenceProblem = TraceRunSchema::ReferenceProblem;

/** @brief Formats a trace-run validation error with source location. */
static std::string configError(const TraceRunConfig& config, std::size_t line, const std::string& message)
{
  auto location = config.path;
  if (line > 0U) {
    location += "(" + std::to_string(line) + ")";
  }
  return location + ": " + message;
}

/** @brief Builds warning context for one trace-run reference. */
static std::vector<std::pair<std::string, std::string>> warningContext(const TraceRunReference& reference)
{
  std::vector<std::pair<std::string, std::string>> context{
      {"ctraceRef", reference.ctraceRef},
      {"type", reference.type},
  };
  if (reference.line > 0U) {
    context.emplace_back("line", std::to_string(reference.line));
  }
  if (reference.processorName.has_value()) {
    context.emplace_back("pname", *reference.processorName);
  }
  if (reference.stream.has_value()) {
    context.emplace_back("stream", std::to_string(*reference.stream));
  }
  return context;
}

/** @brief Retains one ignored root-node inconsistency for non-fatal reporting. */
static void addRootInconsistency(std::vector<CtraceRunWarning>& warnings, std::string message,
                                 std::vector<std::pair<std::string, std::string>> context = {})
{
  warnings.push_back({std::move(message), std::move(context)});
}

/** @brief Formats the structural validation problem of one reference. */
static std::string referenceProblemMessage(const TraceRunConfig& config, const TraceRunReference& reference,
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
  return configError(config, reference.line,
                     reference.line > 0U ? "ITM 'source' must be between 0 and 31"
                                         : "ITM source must be between 0 and 31");
}

/** @brief Tests whether a setup contributes metadata to any consumed route. */
static bool consumesSetup(const TraceRunConfig& config, const TraceRunSetup& setup)
{
  if (setup.timestamps.has_value() || setup.itm.has_value()) {
    return true;
  }
  for (const auto& reference : config.references) {
    if (!TraceRunSchema::isUsableReference(reference) || reference.type != "dwt" ||
        !TraceRunSchema::processorNamesMayBind(setup.processorName, reference.processorName)) {
      continue;
    }
    const auto index = reference.dataSetupIndex;
    if (index.has_value() && *index < setup.data.size()) {
      return true;
    }
  }
  return false;
}

/** @brief Tests whether a reference can bind a processor to one stream. */
static bool isUsableStreamBinding(const TraceRunReference& reference)
{
  return TraceRunSchema::contributesStreamBinding(reference) &&
         TraceRunSchema::referenceProblem(reference) == ReferenceProblem::None;
}

/** @brief Resolves the unambiguous processor identity of a trace-run file. */
static ProcessorIdentity processorIdentity(const TraceRunConfig& config, std::vector<CtraceRunWarning>& warnings)
{
  // Active setups define the authoritative processor set when present.
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

  // Only usable stream bindings may contribute fallback processor identities.
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

  // Reconcile references according to the number and naming of active setups.
  if (setupCount > 1U) {
    if (unnamedSetup) {
      throw std::runtime_error(config.path +
                               ": pname is required for every ctrace-setup in a multi-processor configuration");
    }
    for (const auto& reference : config.references) {
      if (!isUsableStreamBinding(reference)) {
        continue;
      }
      const auto name = TraceRunSchema::normalizedProcessorName(reference.processorName);
      if (!name.has_value()) {
        addRootInconsistency(warnings,
                             "ignoring ctrace-ref without pname because multiple ctrace-setup processors are active",
                             warningContext(reference));
      } else if (setupNames.find(*name) == setupNames.end()) {
        addRootInconsistency(warnings, "ignoring ctrace-ref pname '" + *name +
                                           "' because it has no matching ctrace-setup",
                             warningContext(reference));
      }
    }
    return {true, std::nullopt, true, setupNames};
  }

  if (setupCount == 1U) {
    const auto setupName = TraceRunSchema::normalizedProcessorName(singleActiveSetup->processorName);
    if (setupName.has_value()) {
      for (const auto& reference : config.references) {
        if (!isUsableStreamBinding(reference)) {
          continue;
        }
        const auto name = TraceRunSchema::normalizedProcessorName(reference.processorName);
        if (name.has_value() && *name != *setupName) {
          addRootInconsistency(warnings, "ignoring ctrace-ref pname '" + *name +
                                             "' because it does not match ctrace-setup pname '" + *setupName + "'",
                               warningContext(reference));
        }
      }
      return {false, setupName, true, setupNames};
    }
    if (referenceNames.size() > 1U) {
      addRootInconsistency(warnings,
                           "ignoring conflicting ctrace-ref pnames because the single ctrace-setup has no pname",
                           {{"ctraceRefPnames", std::to_string(referenceNames.size())}});
    }
    return {
        false,
        referenceNames.size() == 1U ? std::optional<std::string>(*referenceNames.begin()) : std::nullopt,
        true,
        {},
    };
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
      false,
      {},
  };
}

/** @brief Resolves the data setup referenced by one DWT route. */
static const TraceRunDataSetup* referencedDataSetup(const TraceRunConfig& config, const TraceRunReference& reference)
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

/** @brief Converts one validated reference into normalized source metadata. */
static CtraceRunSourceMeta sourceMeta(const TraceRunConfig& config, const TraceRunReference& reference,
                                      std::uint32_t source, const ProcessorIdentity& processorIdentity)
{
  const auto* dataSetup = reference.type == "dwt" ? referencedDataSetup(config, reference) : nullptr;
  CtraceRunSourceMeta meta;
  meta.type = reference.type;
  meta.processorName = processorIdentity.canonicalName(reference.processorName);
  meta.traceBusId = static_cast<std::uint8_t>(reference.stream.value_or(0U));
  meta.source = source;
  meta.label = reference.label;
  if (reference.type != "dwt") {
    return meta;
  }

  meta.address = reference.address;
  meta.addressError = reference.addressError;
  if (reference.dataType.has_value() || reference.dataTypeError.has_value()) {
    meta.dataType = reference.dataType.value_or(std::string(TraceRunSchema::kDefaultDwtDataType));
    meta.dataTypeError = reference.dataTypeError;
  }
  if (reference.dataSize.has_value() || reference.dataSizeError.has_value()) {
    meta.dataSize = reference.dataSize.value_or(TraceRunSchema::kDefaultDwtDataSize);
    meta.dataSizeError = reference.dataSizeError;
  } else if (dataSetup != nullptr) {
    meta.dataSize = dataSetup->size.value_or(TraceRunSchema::kDefaultDwtDataSize);
    meta.dataSizeError = dataSetup->sizeError;
  }
  return meta;
}

/** @brief Returns or creates accumulated metadata for one processor. */
static ProcessorMeta& processorMeta(std::vector<ProcessorMeta>& processors, const std::optional<std::string>& name)
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
/** @brief Returns a setting only when all relevant processors agree. */
static std::optional<Value> commonProcessorSetting(const std::vector<ProcessorMeta>& processors,
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

/** @brief Finds accumulated metadata for an optional processor identity. */
static const ProcessorMeta* findProcessor(const std::vector<ProcessorMeta>& processors,
                                          const std::optional<std::string>& name)
{
  const auto found = std::find_if(processors.begin(), processors.end(),
                                  [&](const ProcessorMeta& processor) { return processor.name == name; });
  return found != processors.end() ? &*found : nullptr;
}

/** @brief Stores the result of binding one processor to a trace stream. */
struct ResolvedStreamBinding {
  std::size_t line = 0U;
  std::uint8_t traceBusId = 0U;
  std::optional<std::string> processorName;
  std::string ctraceRef;
  const ProcessorMeta* processor = nullptr;
};

/** @brief Builds warning context for one resolved stream binding. */
static std::vector<std::pair<std::string, std::string>> warningContext(const ResolvedStreamBinding& binding)
{
  std::vector<std::pair<std::string, std::string>> context{
      {"ctraceRef", binding.ctraceRef},
      {"stream", std::to_string(binding.traceBusId)},
  };
  if (binding.line > 0U) {
    context.emplace_back("line", std::to_string(binding.line));
  }
  if (binding.processorName.has_value()) {
    context.emplace_back("pname", *binding.processorName);
  }
  return context;
}

/** @brief Resolves validated processor-to-stream bindings for all references. */
static std::vector<ResolvedStreamBinding> resolveStreamBindings(const TraceRunConfig& config,
                                                                const ProcessorIdentity& processorIdentity,
                                                                const std::vector<ProcessorMeta>& processors)
{
  std::vector<ResolvedStreamBinding> bindings;
  for (const auto& reference : config.references) {
    if (!isUsableStreamBinding(reference) || !processorIdentity.accepts(reference)) {
      continue;
    }
    const auto processorName = processorIdentity.canonicalName(reference.processorName);
    bindings.push_back({
        reference.line,
        static_cast<std::uint8_t>(reference.stream.value_or(0U)),
        processorName,
        reference.ctraceRef,
        findProcessor(processors, processorName),
    });
  }
  return bindings;
}

static std::map<std::uint8_t, CtraceRunTimestampMeta>
buildTimestampsByTraceBusId(const std::vector<ResolvedStreamBinding>& bindings,
                            std::vector<CtraceRunWarning>& warnings)
{
  std::map<std::uint8_t, CtraceRunTimestampMeta> result;
  for (const auto& binding : bindings) {
    CtraceRunTimestampMeta candidate;
    candidate.processorName = binding.processorName;
    if (binding.processor != nullptr && binding.processor->timestampsEnabled) {
      candidate.clockHz = binding.processor->timestampClockHz;
      candidate.clockError = binding.processor->timestampClockError;
    }

    const auto [found, inserted] = result.emplace(binding.traceBusId, candidate);
    if (inserted || (found->second.processorName == candidate.processorName &&
                     found->second.clockHz == candidate.clockHz && found->second.clockError == candidate.clockError)) {
      continue;
    }
    addRootInconsistency(warnings,
                         "ignoring conflicting ctrace-setup timestamps.clock assignment for CoreSight Trace Bus ID " +
                             std::to_string(binding.traceBusId),
                         warningContext(binding));
  }
  return result;
}

static std::map<std::uint8_t, std::uint32_t>
buildTimestampPrescalersByTraceBusId(const std::vector<ResolvedStreamBinding>& bindings,
                                     std::vector<CtraceRunWarning>& warnings)
{
  std::map<std::uint8_t, std::uint32_t> result;
  for (const auto& binding : bindings) {
    const auto prescaler = binding.processor != nullptr && binding.processor->timestampPrescaler.has_value()
                               ? *binding.processor->timestampPrescaler
                               : TraceRunSchema::kDefaultTimestampPrescaler;
    const auto [found, inserted] = result.emplace(binding.traceBusId, prescaler);
    if (!inserted && found->second != prescaler) {
      addRootInconsistency(
          warnings,
          "ignoring conflicting ctrace-setup timestamps.itm-prescaler assignment for CoreSight Trace Bus ID " +
              std::to_string(binding.traceBusId),
          warningContext(binding));
    }
  }
  return result;
}

/** @brief Returns the common ITM enable mask across relevant processors. */
static std::optional<std::uint32_t> commonItmEnableMask(const std::vector<ProcessorMeta>& processors)
{
  std::optional<std::uint32_t> common;
  for (const auto& processor : processors) {
    if (!processor.itmEnableMask.has_value()) {
      return std::nullopt;
    }
    if (common.has_value() && common != processor.itmEnableMask) {
      return std::nullopt;
    }
    common = processor.itmEnableMask;
  }
  return common;
}

static std::map<std::uint8_t, std::uint32_t>
buildItmEnableMasksByTraceBusId(const std::vector<ResolvedStreamBinding>& bindings,
                               std::vector<CtraceRunWarning>& warnings)
{
  std::map<std::uint8_t, std::optional<std::uint32_t>> candidates;
  for (const auto& binding : bindings) {
    const auto enableMask = binding.processor != nullptr ? binding.processor->itmEnableMask : std::nullopt;
    const auto [found, inserted] = candidates.emplace(binding.traceBusId, enableMask);
    if (!inserted && found->second != enableMask) {
      addRootInconsistency(warnings,
                           "ignoring conflicting ctrace-setup itm.enable assignment for CoreSight Trace Bus ID " +
                               std::to_string(binding.traceBusId),
                           warningContext(binding));
    }
  }

  std::map<std::uint8_t, std::uint32_t> result;
  for (const auto& [traceBusId, enableMask] : candidates) {
    if (enableMask.has_value()) {
      result.emplace(traceBusId, *enableMask);
    }
  }
  return result;
}

/** @brief Tests whether processors require different timestamp prescalers. */
static bool containsDistinctProcessorPrescalers(const std::vector<ProcessorMeta>& processors)
{
  std::optional<std::uint32_t> first;
  for (const auto& processor : processors) {
    if (!processor.timestampsEnabled || !processor.timestampPrescaler.has_value()) {
      continue;
    }
    if (first.has_value() && first != processor.timestampPrescaler) {
      return true;
    }
    first = processor.timestampPrescaler;
  }
  return false;
}

CtraceRunMeta CtraceRunMeta::fromConfig(const TraceRunConfig& config)
{
  CtraceRunMeta ctraceRunMeta;
  ctraceRunMeta.m_configPath = config.path;

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

  const auto identity = processorIdentity(config, ctraceRunMeta.m_warnings);
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
        ctraceRunMeta.m_timestampClockErrors.push_back(*setup.timestamps->clockError);
      }
    }
    if (setup.itm.has_value()) {
      processor.itmEnableMask = setup.itm->enableMask;
    }
  }

  for (const auto& reference : config.references) {
    if (isUsableStreamBinding(reference) && identity.accepts(reference)) {
      (void)processorMeta(processors, identity.canonicalName(reference.processorName));
    }
    if (!TraceRunSchema::isUsableReference(reference) || !identity.accepts(reference)) {
      continue;
    }
    for (const auto source : reference.sources) {
      ctraceRunMeta.m_sources.push_back(sourceMeta(config, reference, source, identity));
    }
  }
  const auto streamBindings = resolveStreamBindings(config, identity, processors);
  ctraceRunMeta.m_timestampClockHz = commonProcessorSetting(processors, &ProcessorMeta::timestampClockHz);
  ctraceRunMeta.m_timestampsByTraceBusId =
      buildTimestampsByTraceBusId(streamBindings, ctraceRunMeta.m_warnings);
  ctraceRunMeta.m_timestampPrescaler = commonProcessorSetting(processors, &ProcessorMeta::timestampPrescaler);
  ctraceRunMeta.m_timestampPrescalersByTraceBusId =
      buildTimestampPrescalersByTraceBusId(streamBindings, ctraceRunMeta.m_warnings);
  ctraceRunMeta.m_itmEnableMask = commonItmEnableMask(processors);
  ctraceRunMeta.m_itmEnableMasksByTraceBusId =
      buildItmEnableMasksByTraceBusId(streamBindings, ctraceRunMeta.m_warnings);
  ctraceRunMeta.m_processorCount = processors.size();
  ctraceRunMeta.m_distinctProcessorPrescalers = containsDistinctProcessorPrescalers(processors);

  return ctraceRunMeta;
}

const std::string& CtraceRunMeta::configPath() const
{
  return m_configPath;
}

const std::optional<std::uint64_t>& CtraceRunMeta::timestampClockHz() const
{
  return m_timestampClockHz;
}

const std::map<std::uint8_t, CtraceRunTimestampMeta>& CtraceRunMeta::timestampsByTraceBusId() const
{
  return m_timestampsByTraceBusId;
}

const std::optional<std::uint32_t>& CtraceRunMeta::timestampPrescaler() const
{
  return m_timestampPrescaler;
}

const std::map<std::uint8_t, std::uint32_t>& CtraceRunMeta::timestampPrescalersByTraceBusId() const
{
  return m_timestampPrescalersByTraceBusId;
}

const std::optional<std::uint32_t>& CtraceRunMeta::itmEnableMask() const
{
  return m_itmEnableMask;
}

const std::map<std::uint8_t, std::uint32_t>& CtraceRunMeta::itmEnableMasksByTraceBusId() const
{
  return m_itmEnableMasksByTraceBusId;
}

const std::vector<std::string>& CtraceRunMeta::timestampClockErrors() const
{
  return m_timestampClockErrors;
}

bool CtraceRunMeta::hasDistinctProcessorPrescalers() const
{
  return m_distinctProcessorPrescalers;
}

std::size_t CtraceRunMeta::processorCount() const
{
  return m_processorCount;
}

const std::vector<CtraceRunSourceMeta>& CtraceRunMeta::sources() const
{
  return m_sources;
}

const std::vector<CtraceRunWarning>& CtraceRunMeta::warnings() const
{
  return m_warnings;
}
