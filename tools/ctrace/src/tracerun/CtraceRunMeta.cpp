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
#include <string_view>
#include <utility>
#include <vector>

/** @brief Accumulates normalized metadata for one processor. */
struct ProcessorMeta {
  std::optional<std::string> name;
  bool timestampsEnabled = false;
  std::optional<std::uint64_t> timestampClockHz;
  std::optional<std::string> timestampClockError;
  std::optional<std::uint32_t> timestampPrescaler;
  bool itmEnableConflict = false;
  std::optional<std::uint32_t> itmEnableMask;
};

/** @brief Tests whether a feature leaf ends in one non-empty decimal index. */
static bool hasDecimalSuffix(const std::string_view leaf, const std::string_view prefix)
{
  if (leaf.size() <= prefix.size() || leaf.substr(0U, prefix.size()) != prefix) {
    return false;
  }
  return std::all_of(leaf.begin() + static_cast<std::ptrdiff_t>(prefix.size()), leaf.end(),
                     [](const char character) { return character >= '0' && character <= '9'; });
}

/** @brief Tests whether a reference type accepts one processor-scoped feature leaf. */
static bool isProcessorScopedFeature(const std::string_view type, const std::string_view leaf)
{
  if (type == "itm") {
    return leaf == "itm" || leaf == "timestamps";
  }
  if (type == "dwt") {
    return leaf == "timestamps" || leaf == "synchronization" || hasDecimalSuffix(leaf, "data#");
  }
  if (type == "exception") {
    return leaf == "exceptions";
  }
  if (type == "event" || type == "pmu") {
    return hasDecimalSuffix(leaf, "events#");
  }
  if (type == "pcsample") {
    return leaf == "pcsampling";
  }
  if (type == "overflow") {
    return leaf == "overflow";
  }
  return type == "global_ts" && leaf == "timesync";
}

/** @brief Returns the single non-leading separator of one `[pname/]feature` path. */
static std::optional<std::size_t> processorFeatureSeparator(const std::string_view path)
{
  const auto separator = path.find('/');
  if (separator == std::string_view::npos || separator == 0U) {
    return std::nullopt;
  }
  if (path.find('/', separator + 1U) != std::string_view::npos) {
    return std::nullopt;
  }
  return separator;
}

/** @brief Returns the feature leaf selected by one ctrace reference path. */
static std::string_view referenceLeaf(const std::string_view path)
{
  const auto separator = path.rfind('/');
  return separator == std::string_view::npos ? path : path.substr(separator + 1U);
}

/** @brief Derives a processor name from a one-segment `[pname/]feature` reference path. */
static std::optional<std::string> referencePathProcessorName(const TraceRunReference& reference)
{
  const auto separator = processorFeatureSeparator(reference.ctraceRef);
  if (!separator.has_value()) {
    return std::nullopt;
  }

  const auto leaf = std::string_view(reference.ctraceRef).substr(*separator + 1U);
  if (!isProcessorScopedFeature(reference.type, leaf)) {
    return std::nullopt;
  }
  return reference.ctraceRef.substr(0U, *separator);
}

/** @brief Resolves processor evidence without repeating a previously performed consistency check. */
static std::optional<std::string> uncheckedReferenceProcessorName(const TraceRunReference& reference)
{
  const auto explicitName = TraceRunSchema::normalizedProcessorName(reference.processorName);
  return explicitName.has_value() ? explicitName : referencePathProcessorName(reference);
}

/** @brief Resolves and cross-checks explicit and path-derived processor evidence. */
static std::optional<std::string> checkedReferenceProcessorName(const TraceRunConfig& config,
                                                                const TraceRunReference& reference);

/** @brief Identifies one processor and its optional Trace Bus ID. */
struct ProcessorIdentity {
  bool multipleProcessors = false;
  std::optional<std::string> singleProcessorName;
  bool constrainedBySetups = false;
  std::set<std::string> setupNames;
  std::set<std::uint32_t> singleProcessorStreams;

  /** @brief Tests whether a reference is consistent with the active setups. */
  bool accepts(const TraceRunReference& reference) const
  {
    if (!constrainedBySetups || setupNames.empty()) {
      return true;
    }
    const auto name = uncheckedReferenceProcessorName(reference);
    if (!multipleProcessors && singleProcessorName.has_value() && setupNames.size() > 1U) {
      if (name.has_value()) {
        return name == singleProcessorName;
      }
      return reference.stream.has_value() &&
             singleProcessorStreams.find(*reference.stream) != singleProcessorStreams.end();
    }
    if (!name.has_value()) {
      return setupNames.size() == 1U;
    }
    return setupNames.find(*name) != setupNames.end();
  }

  /** @brief Tests whether one active setup belongs to the selected SINGLE processor. */
  bool acceptsSetup(const TraceRunSetup& setup) const
  {
    if (!constrainedBySetups || multipleProcessors || !singleProcessorName.has_value() || setupNames.size() <= 1U) {
      return true;
    }
    return TraceRunSchema::normalizedProcessorName(setup.processorName) == singleProcessorName;
  }

  /** @brief Resolves an optional processor name to its canonical binding name. */
  std::optional<std::string> canonicalName(const std::optional<std::string>& name) const
  {
    if (!multipleProcessors) {
      return singleProcessorName;
    }
    return TraceRunSchema::normalizedProcessorName(name);
  }

  /** @brief Resolves a reference's canonical processor binding. */
  std::optional<std::string> canonicalReferenceName(const TraceRunReference& reference) const
  {
    return multipleProcessors ? uncheckedReferenceProcessorName(reference) : singleProcessorName;
  }
};

using ReferenceProblem = TraceRunSchema::ReferenceProblem;

/** @brief Tests whether producer diagnostics permit discarding only invalid source metadata. */
static bool isDiscardableSourceProblem(const TraceRunReference& reference, ReferenceProblem problem)
{
  return (!reference.stream.has_value() || CoreSight::isAtbTraceId(*reference.stream)) && !reference.error.empty() &&
         (problem == ReferenceProblem::DuplicateSource || problem == ReferenceProblem::InvalidItmSource);
}

/** @brief Formats a trace-run validation error with source location. */
static std::string configError(const TraceRunConfig& config, std::size_t line, const std::string& message)
{
  auto location = config.path;
  if (line > 0U) {
    location += "(" + std::to_string(line) + ")";
  }
  return location + ": " + message;
}

/** @brief Preserves reader locations while locating programmatically supplied setup errors. */
static std::string itmEnableError(const TraceRunConfig& config, const TraceRunSetup& setup)
{
  const auto& error = *setup.itm->enableError;
  const auto pathLocation = config.path + ':';
  const auto lineLocation = config.path + '(';
  if (!config.path.empty() &&
      (error.compare(0U, pathLocation.size(), pathLocation) == 0 ||
       error.compare(0U, lineLocation.size(), lineLocation) == 0)) {
    return error;
  }
  return configError(config, setup.line, error);
}

/** @brief Merges one optional clock fragment without treating an absent scalar as a conflict. */
static void mergeTimestampClock(std::optional<std::uint64_t>& clockHz, std::optional<std::string>& clockError,
                                const std::optional<std::uint64_t>& candidateClock,
                                const std::optional<std::string>& candidateError,
                                const std::string_view conflictMessage)
{
  if (candidateError.has_value()) {
    if (clockError.has_value() && clockError != candidateError) {
      clockError = conflictMessage;
    } else if (!clockError.has_value()) {
      clockError = candidateError;
    }
    clockHz.reset();
    return;
  }
  if (clockError.has_value() || !candidateClock.has_value()) {
    return;
  }
  if (clockHz.has_value() && clockHz != candidateClock) {
    clockHz.reset();
    clockError = conflictMessage;
  } else {
    clockHz = candidateClock;
  }
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

static bool setupContainsReference(const TraceRunSetup& setup, const TraceRunReference& reference);

/** @brief Tests whether a setup contributes metadata to any consumed route. */
static bool consumesSetup(const TraceRunConfig& config, const TraceRunSetup& setup)
{
  if (setup.disabled) {
    return false;
  }
  if (setup.timestamps.has_value() || setup.itm.has_value()) {
    return true;
  }
  for (const auto& reference : config.references) {
    if (!TraceRunSchema::consumesReferenceMetadata(reference.type)) {
      continue;
    }
    if (!setup.featurePaths.empty() && setupContainsReference(setup, reference)) {
      return true;
    }
    if (reference.type == "dwt" && TraceRunSchema::isUsableReference(reference) &&
        TraceRunSchema::processorNamesMayBind(setup.processorName, uncheckedReferenceProcessorName(reference))) {
      const auto index = reference.dataSetupIndex;
      if (setup.dataError.has_value() ||
          (index.has_value() && *index < setup.data.size() && setup.data[*index].present)) {
        return true;
      }
    }
  }
  return false;
}

/** @brief Tests whether retained reference metadata can identify one processor. */
static bool isUsableProcessorBinding(const TraceRunReference& reference)
{
  const auto problem = TraceRunSchema::referenceProblem(reference);
  const auto structurallyUsable = problem == ReferenceProblem::None || isDiscardableSourceProblem(reference, problem);
  return structurallyUsable && TraceRunSchema::consumesReferenceMetadata(reference.type) &&
         (uncheckedReferenceProcessorName(reference).has_value() || reference.stream.has_value() ||
          TraceRunSchema::contributesStreamBinding(reference));
}

/** @brief Stores one usable reference and its already validated processor name. */
struct ProcessorReferenceEvidence {
  const TraceRunReference* reference = nullptr;
  std::optional<std::string> name;
};

/** @brief Indexes processor evidence before resolving SINGLE trace identity rules. */
struct ProcessorIdentityEvidence {
  std::set<std::string> setupNames;
  bool unnamedSetup = false;
  std::vector<ProcessorReferenceEvidence> references;
  std::set<std::string> referenceNames;
  bool unnamedReference = false;

  std::size_t setupCount() const
  {
    return !setupNames.empty() ? setupNames.size() : (unnamedSetup ? 1U : 0U);
  }
};

/** @brief Collects and validates all setup and reference processor evidence once. */
static ProcessorIdentityEvidence collectProcessorIdentityEvidence(const TraceRunConfig& config)
{
  ProcessorIdentityEvidence evidence;
  for (const auto& setup : config.setups) {
    if (!consumesSetup(config, setup)) {
      continue;
    }
    const auto name = TraceRunSchema::normalizedProcessorName(setup.processorName);
    if (name.has_value()) {
      evidence.setupNames.insert(*name);
    } else {
      evidence.unnamedSetup = true;
    }
  }

  for (const auto& reference : config.references) {
    if (!isUsableProcessorBinding(reference)) {
      continue;
    }
    const auto name = checkedReferenceProcessorName(config, reference);
    evidence.references.push_back({&reference, name});
    if (name.has_value()) {
      evidence.referenceNames.insert(*name);
    } else {
      evidence.unnamedReference = true;
    }
  }
  return evidence;
}

/** @brief Resolves identity when multiple named setup processors are active. */
static ProcessorIdentity resolveMultiSetupProcessorIdentity(const TraceRunConfig& config,
                                                            const ProcessorIdentityEvidence& evidence,
                                                            std::vector<CtraceRunWarning>& warnings)
{
  if (evidence.unnamedSetup) {
    throw std::runtime_error(config.path +
                             ": pname is required for every ctrace-setup in a multi-processor configuration");
  }

  std::set<std::string> matchingReferenceNames;
  for (const auto& binding : evidence.references) {
    if (!binding.name.has_value()) {
      continue;
    }
    if (evidence.setupNames.find(*binding.name) == evidence.setupNames.end()) {
      addRootInconsistency(warnings,
                           "ignoring ctrace-ref pname '" + *binding.name + "' because it has no matching ctrace-setup",
                           warningContext(*binding.reference));
      continue;
    }
    matchingReferenceNames.insert(*binding.name);
  }

  if (matchingReferenceNames.size() != 1U) {
    for (const auto& binding : evidence.references) {
      if (!binding.name.has_value()) {
        addRootInconsistency(warnings,
                             "ignoring ctrace-ref without pname because multiple ctrace-setup processors are active",
                             warningContext(*binding.reference));
      }
    }
    return {true, std::nullopt, true, evidence.setupNames};
  }

  const auto selectedName = *matchingReferenceNames.begin();
  std::set<std::uint32_t> selectedStreams;
  for (const auto& binding : evidence.references) {
    const auto& reference = *binding.reference;
    if (binding.name.has_value() && *binding.name == selectedName && reference.stream.has_value()) {
      selectedStreams.insert(*reference.stream);
    }
  }
  for (const auto& binding : evidence.references) {
    if (binding.name.has_value()) {
      continue;
    }
    const auto& reference = *binding.reference;
    if (!reference.stream.has_value() || selectedStreams.find(*reference.stream) == selectedStreams.end()) {
      addRootInconsistency(warnings, "ignoring ctrace-ref without pname because its processor binding is ambiguous",
                           warningContext(reference));
    }
  }
  return {false, selectedName, true, evidence.setupNames, selectedStreams};
}

/** @brief Resolves identity when exactly one setup processor group is active. */
static ProcessorIdentity resolveSingleSetupProcessorIdentity(const TraceRunConfig& config,
                                                             const ProcessorIdentityEvidence& evidence,
                                                             std::vector<CtraceRunWarning>& warnings)
{
  if (!evidence.setupNames.empty()) {
    const auto setupName = *evidence.setupNames.begin();
    for (const auto& binding : evidence.references) {
      if (binding.name.has_value() && *binding.name != setupName) {
        addRootInconsistency(warnings,
                             "ignoring ctrace-ref pname '" + *binding.name +
                                 "' because it does not match ctrace-setup pname '" + setupName + "'",
                             warningContext(*binding.reference));
      }
    }
    return {false, setupName, true, evidence.setupNames};
  }

  if (evidence.referenceNames.size() > 1U) {
    throw std::runtime_error(config.path +
                             ": unformatted SINGLE trace requires one unambiguous processor metadata binding");
  }
  const auto processorName = evidence.referenceNames.empty()
                                 ? std::nullopt
                                 : std::optional<std::string>(*evidence.referenceNames.begin());
  return {false, processorName, true, {}};
}

/** @brief Resolves identity from references when no active setup constrains it. */
static ProcessorIdentity resolveReferenceProcessorIdentity(const TraceRunConfig& config,
                                                           const ProcessorIdentityEvidence& evidence)
{
  if (evidence.referenceNames.size() > 1U) {
    if (evidence.unnamedReference) {
      throw std::runtime_error(config.path +
                               ": pname is required for every ctrace-ref in a multi-processor configuration");
    }
    return {true, std::nullopt};
  }
  const auto processorName = evidence.referenceNames.empty()
                                 ? std::nullopt
                                 : std::optional<std::string>(*evidence.referenceNames.begin());
  return {false, processorName, false, {}};
}

/** @brief Resolves the unambiguous processor identity of a trace-run file. */
static ProcessorIdentity processorIdentity(const TraceRunConfig& config, std::vector<CtraceRunWarning>& warnings)
{
  const auto evidence = collectProcessorIdentityEvidence(config);
  const auto setupCount = evidence.setupCount();
  if (setupCount > 1U) {
    return resolveMultiSetupProcessorIdentity(config, evidence, warnings);
  }
  if (setupCount == 1U) {
    return resolveSingleSetupProcessorIdentity(config, evidence, warnings);
  }
  return resolveReferenceProcessorIdentity(config, evidence);
}

/** @brief Resolves compatible data metadata from every matching active setup fragment. */
static std::optional<TraceRunDataSetup> referencedDataSetup(const TraceRunConfig& config,
                                                            const TraceRunReference& reference, std::size_t index)
{
  std::optional<TraceRunDataSetup> resolved;
  std::optional<std::uint64_t> effectiveSize;
  bool conflict = false;
  for (const auto& setup : config.setups) {
    if (setup.disabled ||
        !TraceRunSchema::processorNamesMayBind(setup.processorName, uncheckedReferenceProcessorName(reference))) {
      continue;
    }
    TraceRunDataSetup malformedContainer;
    malformedContainer.sizeError = setup.dataError;
    const auto* candidate =
        setup.dataError.has_value() ? &malformedContainer : (index < setup.data.size() ? &setup.data[index] : nullptr);
    if (candidate == nullptr || !candidate->present) {
      continue;
    }
    if (!resolved.has_value()) {
      resolved = *candidate;
      effectiveSize = candidate->size.value_or(TraceRunSchema::kDefaultDwtDataSize);
    } else {
      const auto candidateSize = candidate->size.value_or(TraceRunSchema::kDefaultDwtDataSize);
      if (*effectiveSize != candidateSize) {
        conflict = true;
      }
    }
    if (candidate->sizeError.has_value()) {
      if (resolved->sizeError.has_value() && resolved->sizeError != candidate->sizeError) {
        conflict = true;
      } else if (!resolved->sizeError.has_value()) {
        resolved->sizeError = candidate->sizeError;
      }
    }
  }
  if (resolved.has_value()) {
    resolved->size = effectiveSize;
  }
  if (conflict) {
    resolved->size.reset();
    resolved->sizeError = "conflicting active ctrace-setup data.size values";
  }
  return resolved;
}

/** @brief Converts one validated reference into normalized source metadata. */
static CtraceRunSourceMeta sourceMeta(const TraceRunConfig& config, const TraceRunReference& reference,
                                      std::uint32_t source, const ProcessorIdentity& processorIdentity)
{
  CtraceRunSourceMeta meta;
  meta.type = reference.type;
  meta.processorName = processorIdentity.canonicalReferenceName(reference);
  auto boundReference = reference;
  boundReference.processorName = meta.processorName;
  const auto dataSetup = reference.type == "dwt"
                             ? referencedDataSetup(config, boundReference, *reference.dataSetupIndex)
                             : std::optional<TraceRunDataSetup>{};
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
  } else if (dataSetup.has_value()) {
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

/** @brief Returns a timestamp clock only when every processor candidate supplies the same value. */
static std::optional<std::uint64_t> commonTimestampClock(const std::vector<ProcessorMeta>& processors)
{
  std::optional<std::uint64_t> common;
  for (const auto& processor : processors) {
    const auto candidate = processor.timestampsEnabled ? processor.timestampClockHz : std::nullopt;
    if (!candidate.has_value() || (common.has_value() && common != candidate)) {
      return std::nullopt;
    }
    common = candidate;
  }
  return common;
}

/** @brief Returns a prescaler only when every processor candidate agrees after applying the default. */
static std::optional<std::uint32_t> commonTimestampPrescaler(const std::vector<ProcessorMeta>& processors)
{
  std::optional<std::uint32_t> common;
  for (const auto& processor : processors) {
    const auto candidate = processor.timestampsEnabled
                               ? processor.timestampPrescaler.value_or(TraceRunSchema::kDefaultTimestampPrescaler)
                               : TraceRunSchema::kDefaultTimestampPrescaler;
    if (common.has_value() && common != candidate) {
      return std::nullopt;
    }
    common = candidate;
  }
  return common;
}

/** @brief Returns the ITM enable mask only when all processor candidates agree. */
static std::optional<std::uint32_t> commonItmEnableMask(const std::vector<ProcessorMeta>& processors)
{
  std::optional<std::uint32_t> common;
  for (const auto& processor : processors) {
    if (!processor.itmEnableMask.has_value() || (common.has_value() && common != processor.itmEnableMask)) {
      return std::nullopt;
    }
    common = processor.itmEnableMask;
  }
  return common;
}

/** @brief Tests whether processor candidates supply different explicit ITM masks. */
static bool hasDistinctItmEnableMasks(const std::vector<ProcessorMeta>& processors)
{
  std::optional<std::uint32_t> first;
  for (const auto& processor : processors) {
    if (!processor.itmEnableMask.has_value()) {
      continue;
    }
    if (first.has_value() && first != processor.itmEnableMask) {
      return true;
    }
    first = processor.itmEnableMask;
  }
  return false;
}

/** @brief Retains a common clock error or describes ambiguous SINGLE clock candidates. */
static std::optional<std::string> commonTimestampClockError(const std::vector<ProcessorMeta>& processors)
{
  bool found = false;
  std::optional<std::uint64_t> clockHz;
  std::optional<std::string> clockError;
  for (const auto& processor : processors) {
    const auto candidateClock = processor.timestampsEnabled ? processor.timestampClockHz : std::nullopt;
    const auto candidateError = processor.timestampsEnabled ? processor.timestampClockError : std::nullopt;
    if (found && (clockHz != candidateClock || clockError != candidateError)) {
      return "unformatted SINGLE trace has ambiguous timestamps.clock values across processor candidates";
    }
    found = true;
    clockHz = candidateClock;
    clockError = candidateError;
  }
  return clockError;
}

/** @brief Tests whether a ctrace reference uses the specified `[pname/]feature` path form. */
static bool hasFeaturePath(const TraceRunReference& reference, const std::string_view& leaf)
{
  if (reference.ctraceRef == leaf) {
    return true;
  }
  const auto separator = processorFeatureSeparator(reference.ctraceRef);
  if (!separator.has_value()) {
    return false;
  }
  return std::string_view(reference.ctraceRef).substr(*separator + 1U) == leaf;
}

/** @brief Tests whether a reference path denotes the authoritative processor ITM anchor. */
static bool hasProcessorItmPath(const TraceRunReference& reference)
{
  return hasFeaturePath(reference, "itm");
}

/** @brief Tests whether one reference is an authoritative processor ITM anchor. */
static bool isProcessorItmAnchor(const TraceRunReference& reference)
{
  return hasProcessorItmPath(reference) && reference.type == "itm";
}

/** @brief Tests the fixed feature names that may establish a formatted route without an ITM anchor. */
static bool isNamedFormattedRouteFallback(const std::string_view type, const std::string_view leaf)
{
  if (type == "itm") {
    return leaf == "timestamps";
  }
  if (type == "exception") {
    return leaf == "exceptions";
  }
  if (type == "pcsample") {
    return leaf == "pcsampling";
  }
  if (type == "event" || type == "pmu") {
    return hasDecimalSuffix(leaf, "events#");
  }
  return false;
}

/** @brief Tests whether one reference is permitted to establish a formatted ITM route without an anchor. */
static bool isFormattedRouteFallback(const TraceRunReference& reference)
{
  const auto leaf = referenceLeaf(reference.ctraceRef);
  if (!hasFeaturePath(reference, leaf)) {
    return false;
  }
  if (reference.type == "dwt") {
    if (leaf == "timestamps" || leaf == "synchronization") {
      return true;
    }
    return reference.dataSetupIndex.has_value() && hasDecimalSuffix(leaf, "data#");
  }
  return isNamedFormattedRouteFallback(reference.type, leaf);
}

/** @brief Tests whether one reference may describe an already established ITM route. */
static bool describesFormattedRoute(const TraceRunReference& reference)
{
  return isProcessorItmAnchor(reference) || isFormattedRouteFallback(reference) ||
         (reference.type == "overflow" && hasFeaturePath(reference, "overflow")) ||
         (reference.type == "global_ts" && hasFeaturePath(reference, "timesync"));
}

/** @brief Indexes active setup fragments by their optional processor identity. */
struct ActiveSetupIndex {
  std::vector<const TraceRunSetup*> fragments;
  std::set<std::string> namedProcessors;
  bool hasUnnamedProcessor = false;

  std::size_t processorGroupCount() const
  {
    return !namedProcessors.empty() ? namedProcessors.size() : (hasUnnamedProcessor ? 1U : 0U);
  }
};

/** @brief Collects active setup fragments without treating repeated processor fragments as duplicates. */
static ActiveSetupIndex activeSetupIndex(const TraceRunConfig& config)
{
  ActiveSetupIndex index;
  for (const auto& setup : config.setups) {
    if (!consumesSetup(config, setup)) {
      continue;
    }
    index.fragments.push_back(&setup);
    const auto name = TraceRunSchema::normalizedProcessorName(setup.processorName);
    if (name.has_value()) {
      index.namedProcessors.insert(*name);
    } else {
      index.hasUnnamedProcessor = true;
    }
  }
  return index;
}

/** @brief Resolves and cross-checks processor evidence carried by one formatted reference. */
static std::optional<std::string> checkedReferenceProcessorName(const TraceRunConfig& config,
                                                                const TraceRunReference& reference)
{
  const auto explicitName = TraceRunSchema::normalizedProcessorName(reference.processorName);
  const auto pathName = referencePathProcessorName(reference);
  if (explicitName.has_value() && pathName.has_value() && explicitName != pathName) {
    throw std::runtime_error(
        configError(config, reference.line, "ctrace-ref path processor conflicts with pname '" + *explicitName + "'"));
  }
  return explicitName.has_value() ? explicitName : pathName;
}

/** @brief Resolves a formatted reference to an explicit or uniquely inferred processor identity. */
static std::optional<std::string> formattedProcessorName(const TraceRunConfig& config, const ActiveSetupIndex& setups,
                                                         const TraceRunReference& reference)
{
  const auto referenceName = checkedReferenceProcessorName(config, reference);
  if (referenceName.has_value()) {
    if (setups.fragments.empty() || setups.namedProcessors.find(*referenceName) != setups.namedProcessors.end() ||
        (setups.namedProcessors.empty() && setups.hasUnnamedProcessor)) {
      return referenceName;
    }
    throw std::runtime_error(
        configError(config, reference.line,
                    "ctrace-ref pname '" + *referenceName + "' has no matching active ctrace-setup processor"));
  }
  if (setups.processorGroupCount() == 1U) {
    return setups.namedProcessors.empty() ? std::nullopt : std::optional<std::string>(*setups.namedProcessors.begin());
  }
  if (setups.processorGroupCount() == 0U) {
    return std::nullopt;
  }
  throw std::runtime_error(configError(
      config, reference.line,
      "pname is required for a formatted ctrace-ref when multiple active ctrace-setup processors are available"));
}

/** @brief Tests whether a setup feature path resolves one reference within the same fragment. */
static bool setupContainsReference(const TraceRunSetup& setup, const TraceRunReference& reference)
{
  const auto referenceName = uncheckedReferenceProcessorName(reference);
  const auto setupName = TraceRunSchema::normalizedProcessorName(setup.processorName);
  if (referenceName.has_value() && setupName.has_value() && referenceName != setupName) {
    return false;
  }
  auto referencePath = reference.ctraceRef;
  if (referencePath.find('/') == std::string::npos && referenceName.has_value()) {
    referencePath = *referenceName + "/" + referencePath;
  }
  const auto matchesFeaturePath = [](const std::string_view setupPath, const std::string_view referencePath) {
    return setupPath == referencePath ||
           (referencePath.size() > setupPath.size() && referencePath.substr(0U, setupPath.size()) == setupPath &&
            (referencePath[setupPath.size()] == '/' || referencePath[setupPath.size()] == '#'));
  };
  if (std::any_of(setup.featurePaths.begin(), setup.featurePaths.end(),
                  [&](const std::string& featurePath) { return matchesFeaturePath(featurePath, referencePath); })) {
    return true;
  }
  if (!setupName.has_value()) {
    auto relativePath = std::string_view(referencePath);
    auto separator = relativePath.find('/');
    if (referenceName.has_value()) {
      const auto expectedPrefix = *referenceName + "/";
      separator = relativePath.substr(0U, expectedPrefix.size()) == expectedPrefix ? expectedPrefix.size() - 1U
                                                                                   : std::string_view::npos;
    }
    if (separator != std::string_view::npos) {
      relativePath.remove_prefix(separator + 1U);
      if (std::any_of(setup.featurePaths.begin(), setup.featurePaths.end(),
                      [&](const std::string& featurePath) { return matchesFeaturePath(featurePath, relativePath); })) {
        return true;
      }
    }
  }
  const auto mayBindUnnamedPath =
      !setupName.has_value() || (!referenceName.has_value() && reference.ctraceRef.find('/') == std::string::npos);
  return mayBindUnnamedPath &&
         std::any_of(setup.featurePaths.begin(), setup.featurePaths.end(), [&](const std::string& featurePath) {
           return referenceLeaf(featurePath) == referenceLeaf(reference.ctraceRef);
         });
}

/** @brief Rejects a generated reference that resolves exclusively to disabled setup fragments. */
static void validateDisabledReferences(const TraceRunConfig& config)
{
  for (const auto& reference : config.references) {
    const TraceRunSetup* disabledMatch = nullptr;
    bool activeMatch = false;
    for (const auto& setup : config.setups) {
      if (!setupContainsReference(setup, reference)) {
        continue;
      }
      if (setup.disabled) {
        disabledMatch = &setup;
      } else {
        activeMatch = true;
      }
    }
    if (disabledMatch != nullptr && !activeMatch) {
      throw std::runtime_error(configError(config, reference.line,
                                           "ctrace-ref '" + reference.ctraceRef +
                                               "' resolves only to disabled ctrace-setup fragment " +
                                               std::to_string(disabledMatch->ordinal)));
    }
  }
}

/** @brief Validates all routing-relevant fields retained from one formatted reference. */
static void validateFormattedReference(const TraceRunConfig& config, const TraceRunReference& reference)
{
  if (referenceLeaf(reference.ctraceRef) == "itm" && !hasProcessorItmPath(reference)) {
    throw std::runtime_error(
        configError(config, reference.line, "processor ITM route anchor path must use '[pname/]itm'"));
  }
  if (hasProcessorItmPath(reference) && reference.type != "itm") {
    throw std::runtime_error(
        configError(config, reference.line, "processor ITM route anchor must use reference type 'itm'"));
  }
  if (hasFeaturePath(reference, "timestamps") && reference.type != "itm" && reference.type != "dwt") {
    throw std::runtime_error(
        configError(config, reference.line, "timestamps reference must use type 'itm' or transitional type 'dwt'"));
  }
  const auto pathSeparator = reference.ctraceRef.find('/');
  const auto processorName = TraceRunSchema::normalizedProcessorName(reference.processorName);
  if (processorName.has_value() && pathSeparator != std::string::npos &&
      pathSeparator == reference.ctraceRef.rfind('/') &&
      std::string_view(reference.ctraceRef).substr(0U, pathSeparator) != *processorName &&
      describesFormattedRoute(reference)) {
    throw std::runtime_error(
        configError(config, reference.line, "ctrace-ref path processor conflicts with pname '" + *processorName + "'"));
  }
  const auto problem = TraceRunSchema::referenceProblem(reference);
  if (problem != ReferenceProblem::None && !isDiscardableSourceProblem(reference, problem)) {
    throw std::runtime_error(referenceProblemMessage(config, reference, problem));
  }
  if (isProcessorItmAnchor(reference) && !reference.stream.has_value()) {
    throw std::runtime_error(
        configError(config, reference.line, "processor ITM route anchor requires a CoreSight Trace Bus ID"));
  }
}

/** @brief Registers a bound processor route and rejects one processor mapped to two ITM IDs. */
static void registerBoundRoute(const TraceRunConfig& config, const TraceRunReference& reference,
                               const CtraceRunRoute& route, std::uint8_t traceBusId,
                               std::map<std::string, std::uint8_t>& boundRoutes)
{
  if (!route.processorName.has_value()) {
    return;
  }
  const auto [found, inserted] = boundRoutes.emplace(*route.processorName, traceBusId);
  if (!inserted && found->second != traceBusId) {
    throw std::runtime_error(configError(config, reference.line,
                                         "processor '" + *route.processorName +
                                             "' has ITM routes bound to multiple CoreSight Trace Bus IDs"));
  }
}

/** @brief Adds compatible evidence to one formatted route or rejects an ID-to-processor conflict. */
static CtraceRunRoute& mergeFormattedRoute(const TraceRunConfig& config, const TraceRunReference& reference,
                                           const std::optional<std::string>& processorName,
                                           std::map<std::uint8_t, CtraceRunRoute>& routes,
                                           std::map<std::string, std::uint8_t>& boundRoutes)
{
  const auto traceBusId = static_cast<std::uint8_t>(*reference.stream);
  auto [found, inserted] = routes.emplace(traceBusId, CtraceRunRoute{});
  auto& route = found->second;
  if (inserted) {
    route.processorName = processorName;
  } else if (route.processorName.has_value() && processorName.has_value() && route.processorName != processorName) {
    throw std::runtime_error(configError(config, reference.line,
                                         "CoreSight Trace Bus ID " + std::to_string(traceBusId) +
                                             " has conflicting ITM processor bindings"));
  } else if (!route.processorName.has_value() && processorName.has_value()) {
    route.processorName = processorName;
  }
  registerBoundRoute(config, reference, route, traceBusId, boundRoutes);
  return route;
}

/** @brief Resolves setup and source metadata for the formatted routes of one trace run. */
class FormattedRouteMetadata final {
public:
  /** @brief Binds the immutable input model and warning collector. */
  FormattedRouteMetadata(const TraceRunConfig& config, const ActiveSetupIndex& setups,
                         std::vector<CtraceRunWarning>& warnings)
    : m_config(config),
      m_setups(setups),
      m_warnings(warnings)
  {
  }

  /** @brief Applies compatible active setup fragments to one normalized route. */
  void applySetup(CtraceRunRoute& route) const
  {
    bool enableMaskConflict = false;
    std::optional<std::uint64_t> clockHz;
    std::optional<std::string> clockError;
    std::optional<std::uint32_t> prescaler;
    std::optional<std::uint32_t> enableMask;

    for (const auto* setup : setupFragments(route)) {
      if (setup->timestamps.has_value()) {
        const auto& timestamps = *setup->timestamps;
        const auto candidatePrescaler =
            timestamps.timestampPrescaler.value_or(TraceRunSchema::kDefaultTimestampPrescaler);
        if (!TraceRunSchema::isTimestampPrescaler(candidatePrescaler)) {
          throw std::runtime_error(configError(
              m_config, timestamps.line,
              timestamps.line > 0U ? "'timestamps.itm-prescaler' must be one of 1, 4, 16, or 64"
                                    : "ctrace-setup timestamps.itm-prescaler must be one of 1, 4, 16, or 64"));
        }
        if (prescaler.has_value() && *prescaler != candidatePrescaler) {
          throw std::runtime_error(configError(
              m_config, timestamps.line,
              "conflicting timestamps.itm-prescaler values for one formatted processor ITM route"));
        }
        prescaler = candidatePrescaler;

        mergeTimestampClock(clockHz, clockError, timestamps.clockHz, timestamps.clockError,
                            "conflicting active ctrace-setup timestamps.clock values");
      }
      if (setup->itm.has_value()) {
        if (setup->itm->enableError.has_value()) {
          throw std::runtime_error(itmEnableError(m_config, *setup));
        }
        const auto candidateMask = setup->itm->enableMask;
        if (!candidateMask.has_value()) {
          continue;
        }
        if (enableMask.has_value() && *enableMask != *candidateMask) {
          if (!enableMaskConflict) {
            addRootInconsistency(
                m_warnings, "ignoring conflicting ctrace-setup itm.enable assignment for one formatted ITM route",
                {{"pname", route.processorName.value_or("<unnamed>")}, {"line", std::to_string(setup->line)}});
          }
          enableMaskConflict = true;
          continue;
        }
        enableMask = *candidateMask;
      }
    }

    route.timestampPrescaler = prescaler.value_or(TraceRunSchema::kDefaultTimestampPrescaler);
    if (!enableMaskConflict) {
      route.itmEnableMask = enableMask;
    }
    route.timestampClockHz = clockHz;
    route.timestampClockError = clockError;
  }

  /** @brief Converts one validated reference into route-local source metadata. */
  CtraceRunSourceMeta source(const TraceRunReference& reference, std::uint32_t source,
                             const CtraceRunRoute& route) const
  {
    ProcessorIdentity identity;
    identity.multipleProcessors = true;
    auto boundReference = reference;
    boundReference.processorName = route.processorName;
    auto meta = sourceMeta(m_config, boundReference, source, identity);
    meta.processorName = route.processorName;
    meta.route = route.identity;
    return meta;
  }

private:
  /** @brief Returns setup fragments that unambiguously supply metadata for one route. */
  std::vector<const TraceRunSetup*> setupFragments(const CtraceRunRoute& route) const
  {
    std::vector<const TraceRunSetup*> matches;
    if (route.processorName.has_value()) {
      for (const auto* setup : m_setups.fragments) {
        if (TraceRunSchema::normalizedProcessorName(setup->processorName) == route.processorName) {
          matches.push_back(setup);
        }
      }
      const auto unnamedCanBind =
          m_setups.hasUnnamedProcessor && m_setups.namedProcessors.size() <= 1U &&
          (m_setups.namedProcessors.empty() ||
           m_setups.namedProcessors.find(*route.processorName) != m_setups.namedProcessors.end());
      if (!unnamedCanBind) {
        return matches;
      }
    } else if (m_setups.processorGroupCount() != 1U || !m_setups.hasUnnamedProcessor) {
      return matches;
    }

    for (const auto* setup : m_setups.fragments) {
      if (!TraceRunSchema::normalizedProcessorName(setup->processorName).has_value()) {
        matches.push_back(setup);
      }
    }
    return matches;
  }

  const TraceRunConfig& m_config;
  const ActiveSetupIndex& m_setups;
  std::vector<CtraceRunWarning>& m_warnings;
};

/** @brief Finds the established route described by resolved streamless processor evidence. */
static std::optional<std::uint8_t> streamlessRouteId(
    const std::optional<std::string>& processorName, const std::map<std::uint8_t, CtraceRunRoute>& states,
    const std::map<std::string, std::uint8_t>& boundRoutes)
{
  if (processorName.has_value()) {
    const auto bound = boundRoutes.find(*processorName);
    if (bound != boundRoutes.end()) {
      return bound->second;
    }
    if (states.size() != 1U || states.begin()->second.processorName.has_value()) {
      return std::nullopt;
    }
  }
  return states.size() == 1U ? std::optional<std::uint8_t>(states.begin()->first) : std::nullopt;
}

/** @brief Applies processor evidence from a streamless reference to its unique formatted route. */
static void bindStreamlessRoute(const TraceRunConfig& config, const TraceRunReference& reference,
                                std::uint8_t traceBusId, const std::optional<std::string>& processorName,
                                std::map<std::uint8_t, CtraceRunRoute>& states,
                                std::map<std::string, std::uint8_t>& boundRoutes)
{
  auto& route = states.at(traceBusId);
  if (!route.processorName.has_value() && processorName.has_value()) {
    route.processorName = processorName;
  }
  registerBoundRoute(config, reference, route, traceBusId, boundRoutes);
}

/** @brief Retains formatted route state and the final route selected for every reference. */
struct FormattedRouteBindings {
  std::map<std::uint8_t, CtraceRunRoute> routes;
  std::map<std::string, std::uint8_t> processorRoutes;
  std::vector<std::optional<std::uint8_t>> referenceRoutes;
};

/** @brief Validates processor constraints imposed by active formatted setup fragments. */
static void validateFormattedSetupProcessors(const TraceRunConfig& config, const ActiveSetupIndex& setups)
{
  if (setups.namedProcessors.size() > 1U && setups.hasUnnamedProcessor) {
    throw std::runtime_error(
        config.path + ": pname is required for active ctrace-setup fragments in a multi-processor configuration");
  }
  if (setups.namedProcessors.empty() && setups.hasUnnamedProcessor) {
    std::set<std::string> referenceNames;
    for (const auto& reference : config.references) {
      if (!describesFormattedRoute(reference)) {
        continue;
      }
      const auto name = checkedReferenceProcessorName(config, reference);
      if (name.has_value()) {
        referenceNames.insert(*name);
      }
    }
    if (referenceNames.size() > 1U) {
      throw std::runtime_error(config.path +
                               ": one unnamed ctrace-setup processor cannot bind multiple formatted pnames");
    }
  }
}

/** @brief Validates all routing-relevant formatted references before building shared state. */
static void validateFormattedReferences(const TraceRunConfig& config)
{
  for (const auto& reference : config.references) {
    validateFormattedReference(config, reference);
  }
}

/** @brief Registers authoritative formatted ITM route anchors. */
static void registerFormattedRouteAnchors(const TraceRunConfig& config, const ActiveSetupIndex& setups,
                                          FormattedRouteBindings& bindings)
{
  for (const auto& reference : config.references) {
    if (isProcessorItmAnchor(reference)) {
      mergeFormattedRoute(config, reference, formattedProcessorName(config, setups, reference), bindings.routes,
                          bindings.processorRoutes);
    }
  }
}

/** @brief Registers compatible formatted route fallbacks after every anchor. */
static void registerFormattedRouteFallbacks(const TraceRunConfig& config, const ActiveSetupIndex& setups,
                                            FormattedRouteBindings& bindings)
{
  for (const auto& reference : config.references) {
    if (isFormattedRouteFallback(reference) && reference.stream.has_value()) {
      mergeFormattedRoute(config, reference, formattedProcessorName(config, setups, reference), bindings.routes,
                          bindings.processorRoutes);
    }
  }
}

/** @brief Rejects an empty or ambiguous formatted route set after anchor discovery. */
static void validateFormattedRouteSet(const TraceRunConfig& config, const ActiveSetupIndex& setups,
                                      const FormattedRouteBindings& bindings)
{
  if (setups.namedProcessors.empty() && setups.hasUnnamedProcessor && bindings.routes.size() > 1U) {
    throw std::runtime_error(config.path +
                             ": one unnamed ctrace-setup processor cannot bind multiple formatted ITM routes");
  }

  if (bindings.routes.empty()) {
    throw std::runtime_error(config.path +
                             ": formatted trace input requires an ITM route anchor or supported feature fallback");
  }
}

/** @brief Resolves every compatible formatted reference to one established route. */
static void bindFormattedReferences(const TraceRunConfig& config, const ActiveSetupIndex& setups,
                                    FormattedRouteBindings& bindings)
{
  bindings.referenceRoutes.resize(config.references.size());
  for (std::size_t index = 0U; index < config.references.size(); ++index) {
    const auto& reference = config.references[index];
    if (reference.stream.has_value()) {
      const auto traceBusId = static_cast<std::uint8_t>(*reference.stream);
      if (bindings.routes.find(traceBusId) == bindings.routes.end()) {
        throw std::runtime_error(configError(config, reference.line,
                                             "ctrace-ref describes CoreSight Trace Bus ID " +
                                                 std::to_string(traceBusId) +
                                                 " without an ITM route anchor or supported feature fallback"));
      }
      mergeFormattedRoute(config, reference, formattedProcessorName(config, setups, reference), bindings.routes,
                          bindings.processorRoutes);
      bindings.referenceRoutes[index] = traceBusId;
      continue;
    }
    if (!describesFormattedRoute(reference)) {
      continue;
    }

    const auto processorName = formattedProcessorName(config, setups, reference);
    const auto routeId = streamlessRouteId(processorName, bindings.routes, bindings.processorRoutes);
    if (!routeId.has_value()) {
      throw std::runtime_error(configError(
          config, reference.line, "streamless ctrace-ref cannot be associated with one formatted ITM route"));
    }
    bindStreamlessRoute(config, reference, *routeId, processorName, bindings.routes, bindings.processorRoutes);
  }

  // Match materialization against the final processor bindings after every reference has been merged.
  for (std::size_t index = 0U; index < config.references.size(); ++index) {
    const auto& reference = config.references[index];
    if (reference.stream.has_value() || !describesFormattedRoute(reference)) {
      continue;
    }
    const auto processorName = formattedProcessorName(config, setups, reference);
    bindings.referenceRoutes[index] = streamlessRouteId(processorName, bindings.routes, bindings.processorRoutes);
  }
}

/** @brief Materializes stable identities and source metadata for resolved formatted routes. */
static std::vector<CtraceRunRoute> materializeFormattedRoutes(const TraceRunConfig& config,
                                                              const ActiveSetupIndex& setups,
                                                              std::vector<CtraceRunWarning>& warnings,
                                                              FormattedRouteBindings bindings)
{
  const FormattedRouteMetadata routeMetadata(config, setups, warnings);
  std::vector<CtraceRunRoute> routes;
  routes.reserve(bindings.routes.size());
  std::uint32_t routeOrdinal = 0U;
  for (auto& [traceBusId, route] : bindings.routes) {
    route.identity = {TraceRouteId{routeOrdinal}, traceBusId};
    ++routeOrdinal;
    routeMetadata.applySetup(route);

    for (std::size_t index = 0U; index < config.references.size(); ++index) {
      const auto& reference = config.references[index];
      const auto routeId = bindings.referenceRoutes[index];
      if (!routeId.has_value() || *routeId != traceBusId || !TraceRunSchema::isUsableReference(reference)) {
        continue;
      }
      for (const auto source : reference.sources) {
        route.sources.push_back(routeMetadata.source(reference, source, route));
      }
    }
    routes.push_back(std::move(route));
  }
  return routes;
}

/** @brief Builds the strict formatted route catalogue without constructing decoder objects. */
static std::vector<CtraceRunRoute> formattedRoutes(const TraceRunConfig& config,
                                                   std::vector<CtraceRunWarning>& warnings)
{
  const auto setups = activeSetupIndex(config);
  validateFormattedSetupProcessors(config, setups);
  validateFormattedReferences(config);

  FormattedRouteBindings bindings;
  // Anchors are authoritative, so register all of them before compatibility fallbacks.
  registerFormattedRouteAnchors(config, setups, bindings);
  registerFormattedRouteFallbacks(config, setups, bindings);
  validateFormattedRouteSet(config, setups, bindings);
  bindFormattedReferences(config, setups, bindings);
  return materializeFormattedRoutes(config, setups, warnings, std::move(bindings));
}

/** @brief Validates routing-relevant fields retained from unformatted references. */
static void validateUnformattedReferences(const TraceRunConfig& config)
{
  for (const auto& reference : config.references) {
    const auto problem = TraceRunSchema::referenceProblem(reference);
    if (problem == ReferenceProblem::InvalidStream) {
      throw std::runtime_error(referenceProblemMessage(config, reference, problem));
    }
    if (!TraceRunSchema::hasConsumedRouteShape(reference) && !TraceRunSchema::contributesStreamBinding(reference)) {
      continue;
    }
    if (problem != ReferenceProblem::None && !isDiscardableSourceProblem(reference, problem)) {
      throw std::runtime_error(referenceProblemMessage(config, reference, problem));
    }
  }
}

/** @brief Tests whether one setup contributes to the selected unformatted processor identity. */
static bool isSelectedUnformattedSetup(const TraceRunConfig& config, const ProcessorIdentity& identity,
                                       const TraceRunSetup& setup)
{
  return identity.acceptsSetup(setup) && consumesSetup(config, setup);
}

/** @brief Validates selected setup scalars before merging unformatted metadata. */
static void validateUnformattedSetups(const TraceRunConfig& config, const ProcessorIdentity& identity)
{
  for (const auto& setup : config.setups) {
    if (!isSelectedUnformattedSetup(config, identity, setup)) {
      continue;
    }
    if (setup.timestamps.has_value() && setup.timestamps->timestampPrescaler.has_value() &&
        !TraceRunSchema::isTimestampPrescaler(*setup.timestamps->timestampPrescaler)) {
      throw std::runtime_error(
          configError(config, setup.timestamps->line,
                      setup.timestamps->line > 0U ? "'timestamps.itm-prescaler' must be one of 1, 4, 16, or 64"
                                                  : "ctrace-setup timestamps.itm-prescaler must be one of 1, 4, 16, or 64"));
    }
    if (setup.itm.has_value() && setup.itm->enableError.has_value()) {
      throw std::runtime_error(itmEnableError(config, setup));
    }
  }
}

/** @brief Merges one selected setup's timestamp metadata into its processor accumulator. */
static void mergeUnformattedTimestamps(const TraceRunConfig& config, const TraceRunSetup& setup,
                                       ProcessorMeta& processor)
{
  if (!setup.timestamps.has_value()) {
    return;
  }
  const auto prescaler = setup.timestamps->timestampPrescaler.value_or(TraceRunSchema::kDefaultTimestampPrescaler);
  if (processor.timestampsEnabled && processor.timestampPrescaler != prescaler) {
    throw std::runtime_error(
        configError(config, setup.timestamps->line,
                    "unformatted SINGLE trace has conflicting timestamps.itm-prescaler values for one processor"));
  }
  mergeTimestampClock(processor.timestampClockHz, processor.timestampClockError, setup.timestamps->clockHz,
                      setup.timestamps->clockError, "conflicting active ctrace-setup timestamps.clock values");
  processor.timestampsEnabled = true;
  processor.timestampPrescaler = prescaler;
}

/** @brief Merges one selected setup's ITM mask into its processor accumulator. */
static void mergeUnformattedItm(const TraceRunSetup& setup, const std::optional<std::string>& processorName,
                                ProcessorMeta& processor, std::vector<CtraceRunWarning>& warnings)
{
  if (!setup.itm.has_value() || !setup.itm->enableMask.has_value() || processor.itmEnableConflict) {
    return;
  }
  if (processor.itmEnableMask.has_value() && processor.itmEnableMask != setup.itm->enableMask) {
    processor.itmEnableConflict = true;
    processor.itmEnableMask.reset();
    addRootInconsistency(warnings,
                         "ignoring conflicting ctrace-setup itm.enable values for unformatted SINGLE trace",
                         {{"pname", processorName.value_or("<unnamed>")}});
    return;
  }
  processor.itmEnableMask = setup.itm->enableMask;
}

/** @brief Accumulates metadata from all setups selected for an unformatted input. */
static std::vector<ProcessorMeta> unformattedProcessors(const TraceRunConfig& config, const ProcessorIdentity& identity,
                                                        std::vector<CtraceRunWarning>& warnings)
{
  std::vector<ProcessorMeta> processors;
  for (const auto& setup : config.setups) {
    if (!isSelectedUnformattedSetup(config, identity, setup)) {
      continue;
    }
    const auto processorName = identity.canonicalName(setup.processorName);
    auto& processor = processorMeta(processors, processorName);
    mergeUnformattedTimestamps(config, setup, processor);
    mergeUnformattedItm(setup, processorName, processor, warnings);
  }
  return processors;
}

/** @brief Collects usable sources and reference-only processor candidates for an unformatted input. */
static std::vector<CtraceRunSourceMeta> unformattedSources(const TraceRunConfig& config,
                                                          const ProcessorIdentity& identity,
                                                          std::vector<ProcessorMeta>& processors)
{
  std::vector<CtraceRunSourceMeta> sources;
  for (const auto& reference : config.references) {
    if (isUsableProcessorBinding(reference) && identity.accepts(reference)) {
      (void)processorMeta(processors, identity.canonicalReferenceName(reference));
    }
    if (!TraceRunSchema::isUsableReference(reference) || !identity.accepts(reference)) {
      continue;
    }
    for (const auto source : reference.sources) {
      sources.push_back(sourceMeta(config, reference, source, identity));
    }
  }
  return sources;
}

/** @brief Materializes the one synthetic route used for unformatted SINGLE input. */
static CtraceRunRoute makeUnformattedRoute(const TraceRunConfig& config, const std::vector<ProcessorMeta>& processors,
                                           std::vector<CtraceRunSourceMeta> sources,
                                           std::vector<CtraceRunWarning>& warnings)
{
  const auto timestampPrescaler = commonTimestampPrescaler(processors);
  if (!processors.empty() && !timestampPrescaler.has_value()) {
    throw std::runtime_error(
        config.path + ": unformatted SINGLE trace cannot choose between different timestamps.itm-prescaler values");
  }
  const auto timestampClockHz = commonTimestampClock(processors);
  const auto itmEnableMask = commonItmEnableMask(processors);
  if (hasDistinctItmEnableMasks(processors)) {
    addRootInconsistency(
        warnings,
        "ignoring different ctrace-setup itm.enable values across processor candidates for unformatted SINGLE trace");
  }
  const auto clockError = commonTimestampClockError(processors);

  CtraceRunRoute syntheticRoute;
  syntheticRoute.sources = std::move(sources);
  syntheticRoute.timestampClockHz = timestampClockHz;
  syntheticRoute.timestampPrescaler =
      timestampPrescaler.value_or(TraceRunSchema::kDefaultTimestampPrescaler);
  syntheticRoute.itmEnableMask = itmEnableMask;
  syntheticRoute.timestampClockError = clockError;
  if (processors.size() == 1U) {
    syntheticRoute.processorName = processors.front().name;
    syntheticRoute.timestampClockHz = processors.front().timestampClockHz;
    syntheticRoute.timestampClockError = processors.front().timestampClockError;
    syntheticRoute.timestampPrescaler =
        processors.front().timestampPrescaler.value_or(TraceRunSchema::kDefaultTimestampPrescaler);
    syntheticRoute.itmEnableMask = processors.front().itmEnableMask;
  }
  return syntheticRoute;
}

/** @brief Builds the synthetic route and warnings for an unformatted SINGLE input. */
static CtraceRunRoute unformattedRoute(const TraceRunConfig& config, std::vector<CtraceRunWarning>& warnings)
{
  validateUnformattedReferences(config);
  const auto identity = processorIdentity(config, warnings);
  validateUnformattedSetups(config, identity);
  auto processors = unformattedProcessors(config, identity, warnings);
  auto sources = unformattedSources(config, identity, processors);
  return makeUnformattedRoute(config, processors, std::move(sources), warnings);
}

CtraceRunMeta CtraceRunMeta::fromConfig(const TraceRunConfig& config)
{
  CtraceRunMeta ctraceRunMeta;
  ctraceRunMeta.m_configPath = config.path;
  ctraceRunMeta.m_traceFormat = config.traceFormat;
  validateDisabledReferences(config);

  if (TraceRunSchema::effectiveTraceFormat(config.traceFormat) == TraceRunFormat::Formatted) {
    ctraceRunMeta.m_routes = formattedRoutes(config, ctraceRunMeta.m_warnings);
  } else {
    ctraceRunMeta.m_routes.push_back(unformattedRoute(config, ctraceRunMeta.m_warnings));
  }

  return ctraceRunMeta;
}

const std::string& CtraceRunMeta::configPath() const
{
  return m_configPath;
}

const std::optional<TraceRunFormat>& CtraceRunMeta::traceFormat() const
{
  return m_traceFormat;
}

const std::vector<CtraceRunRoute>& CtraceRunMeta::routes() const
{
  return m_routes;
}

const std::vector<CtraceRunWarning>& CtraceRunMeta::warnings() const
{
  return m_warnings;
}
