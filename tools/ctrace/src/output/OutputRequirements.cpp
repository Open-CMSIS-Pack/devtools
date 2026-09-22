/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OutputRequirements.h"

#include "CtraceRunMeta.h"
#include "ctf/CtfMetadataModel.h"
#include "ctf/CtfSchema.h"
#include "DiagnosticSink.h"
#include "TraceSelection.h"
#include "TraceOutputConfig.h"
#include "TraceRunConfig.h"

#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

/** @brief Stores the derived CSV, CTF, and Trace Compass target paths. */
struct OutputPaths {
  std::filesystem::path csv;
  std::filesystem::path ctf;
  std::filesystem::path traceCompassXml;
};

/** @brief Derives all output targets from one raw input path. */
static OutputPaths outputPaths(const std::filesystem::path& rawInputPath)
{
  const auto captureName = rawInputPath.filename().stem();
  const auto solutionSetName = captureName.stem();
  if (captureName.empty() || solutionSetName.empty()) {
    throw std::runtime_error("cannot derive trace artifact names from " + rawInputPath.string());
  }
  const auto outputDirectory = rawInputPath.parent_path();
  auto csvPath = outputDirectory / captureName;
  csvPath += ".csv";
  auto ctfPath = outputDirectory / captureName;
  ctfPath += ".ctf";
  auto traceCompassXmlPath = outputDirectory / captureName;
  traceCompassXmlPath += ".traceanalysis.xml";
  return {
      std::move(csvPath),
      std::move(ctfPath),
      std::move(traceCompassXmlPath),
  };
}

/** @brief Tests whether one configured source route is selected for output. */
static bool routeMatchesSelection(const CtraceRunSourceMeta& source, const TraceSelection& selection)
{
  return selection.includesType(source.type) && selection.includesRoute(source.route);
}

static std::vector<std::pair<std::string, std::string>>
routeContext(const std::string_view& backend, const CtraceRunMeta& ctraceRunMeta, const CtraceRunSourceMeta& source)
{
  std::vector<std::pair<std::string, std::string>> context{
      {"backend", std::string(backend)},
      {"channel", std::string(source.type == "itm" ? "ITM" : "DWT") + std::to_string(source.source)},
  };
  if (source.route.traceBusId.has_value()) {
    context.emplace_back("stream", std::to_string(*source.route.traceBusId));
  }
  if (!ctraceRunMeta.configPath().empty()) {
    context.emplace_back("config", ctraceRunMeta.configPath());
  }
  return context;
}

/** @brief Builds public diagnostic context for one normalized route. */
static std::vector<std::pair<std::string, std::string>>
routeContext(const std::string_view& backend, const CtraceRunMeta& ctraceRunMeta, const CtraceRunRoute& route)
{
  std::vector<std::pair<std::string, std::string>> context{{"backend", std::string(backend)}};
  if (!ctraceRunMeta.configPath().empty()) {
    context.emplace_back("config", ctraceRunMeta.configPath());
  }
  if (route.identity.traceBusId.has_value()) {
    context.emplace_back("stream", std::to_string(*route.identity.traceBusId));
  }
  if (route.processorName.has_value()) {
    context.emplace_back("pname", *route.processorName);
  }
  return context;
}

/** @brief Reports one output preflight failure with optional context. */
static void reportRequirementError(DiagnosticSink& diagnostics, std::string message,
                                   std::vector<std::pair<std::string, std::string>> context)
{
  diagnostics.report({
      DiagnosticSink::Severity::Error,
      std::move(message),
      std::move(context),
  });
}

/** @brief Validates that selected CTF routes have unambiguous stream identities. */
static bool validateCtfSourceIdentity(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
                                      DiagnosticSink& diagnostics)
{
  bool valid = true;
  std::map<std::tuple<TraceRouteId, std::string, std::uint32_t>, const CtraceRunSourceMeta*> sources;
  std::set<std::tuple<TraceRouteId, std::string, std::uint32_t>> reported;
  for (const auto& route : ctraceRunMeta.routes()) {
    for (const auto& source : route.sources) {
      if (!routeMatchesSelection(source, selection)) {
        continue;
      }
      const auto key = std::make_tuple(source.route.id, source.type, source.source);
      const auto [found, inserted] = sources.emplace(key, &source);
      if (inserted) {
        continue;
      }
      const auto& first = *found->second;
      const auto sameMetadata = first.label == source.label && first.address == source.address &&
                                first.dataType == source.dataType && first.dataSize == source.dataSize &&
                                first.addressError == source.addressError &&
                                first.dataTypeError == source.dataTypeError &&
                                first.dataSizeError == source.dataSizeError;
      const auto sameBinding = first.route == source.route && first.processorName == source.processorName;
      if ((sameMetadata && sameBinding) || !reported.insert(key).second) {
        continue;
      }

      valid = false;
      auto context = routeContext("ctf", ctraceRunMeta, source);
      context.emplace_back("type", source.type);
      context.emplace_back("firstProcessor", first.processorName.value_or("<unspecified>"));
      context.emplace_back("otherProcessor", source.processorName.value_or("<unspecified>"));
      reportRequirementError(diagnostics,
                             "CTF metadata cannot describe conflicting active metadata for one route/type/source key",
                             std::move(context));
    }
  }
  return valid;
}

/** @brief Returns exactly the normalized routes selected for CTF output. */
static std::vector<const CtraceRunRoute*> selectedCtfRoutes(const CtraceRunMeta& ctraceRunMeta,
                                                            const TraceSelection& selection)
{
  std::vector<const CtraceRunRoute*> routes;
  for (const auto& route : ctraceRunMeta.routes()) {
    if (selection.includesRoute(route.identity)) {
      routes.push_back(&route);
    }
  }
  return routes;
}

/** @brief Resolves route-specific CTF stream and clock-domain descriptors. */
static std::optional<CtfMetadataTopology>
resolveCtfTopology(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection, DiagnosticSink& diagnostics)
{
  const auto routes = selectedCtfRoutes(ctraceRunMeta, selection);
  if (routes.empty()) {
    return CtfMetadataTopology{};
  }

  const auto legacy = ctraceRunMeta.routes().size() == 1U &&
                      !ctraceRunMeta.routes().front().identity.traceBusId.has_value() &&
                      ctraceRunMeta.traceFormat() != TraceRunFormat::Formatted;

  bool valid = true;
  for (const auto* route : routes) {
    auto context = routeContext("ctf", ctraceRunMeta, *route);
    if (route->timestampClockError.has_value()) {
      valid = false;
      context.emplace_back("error", *route->timestampClockError);
      reportRequirementError(diagnostics, "CTF output cannot use the configured timestamps.clock", std::move(context));
    } else if (!route->timestampClockHz.has_value()) {
      valid = false;
      reportRequirementError(diagnostics, "CTF output requires timestamps.clock; no default is assumed",
                             std::move(context));
    } else if (*route->timestampClockHz == 0U) {
      valid = false;
      reportRequirementError(diagnostics, "CTF output requires timestamps.clock to be greater than zero",
                             std::move(context));
    }
  }
  if (!valid) {
    return std::nullopt;
  }

  CtfMetadataTopology topology;
  if (legacy) {
    topology.clockDomains.push_back(
        {CtfClockDomainId{0U}, "swo_clock", std::nullopt, *routes.front()->timestampClockHz, false});
    topology.streams.push_back(
        {CtfStreamClassId{0U}, routes.front()->identity, routes.front()->processorName, CtfClockDomainId{0U}});
    return topology;
  }

  for (const auto* route : routes) {
    const auto domainId = CtfClockDomainId{static_cast<std::uint32_t>(topology.clockDomains.size() + 1U)};
    topology.clockDomains.push_back({domainId, "cmsis_clock_" + std::to_string(domainId.value()), CtfUuid::randomV4(),
                                     *route->timestampClockHz, false});
    const auto streamClassId = CtfStreamClassId{route->identity.traceBusId.value_or(0U)};
    topology.streams.push_back({streamClassId, route->identity, route->processorName, domainId});
  }
  return std::optional<CtfMetadataTopology>{std::move(topology)};
}

/** @brief Reports one deferred trace-run field error for a selected DWT source. */
static bool reportCtfDwtFieldError(const CtraceRunMeta& ctraceRunMeta, const CtraceRunSourceMeta& source,
                                   const std::optional<std::string>& error, const char* message,
                                   DiagnosticSink& diagnostics)
{
  if (!error.has_value()) {
    return false;
  }
  auto context = routeContext("ctf", ctraceRunMeta, source);
  context.emplace_back("error", *error);
  reportRequirementError(diagnostics, message, std::move(context));
  return true;
}

/** @brief Reports every deferred trace-run metadata error for one DWT source. */
static bool validateCtfDwtParsedMetadata(const CtraceRunMeta& ctraceRunMeta, const CtraceRunSourceMeta& source,
                                         DiagnosticSink& diagnostics)
{
  bool valid = true;
  if (reportCtfDwtFieldError(ctraceRunMeta, source, source.addressError,
                             "CTF output cannot use the configured ctrace-run address", diagnostics)) {
    valid = false;
  }
  if (reportCtfDwtFieldError(ctraceRunMeta, source, source.dataTypeError,
                             "CTF output cannot use the configured ctrace-run data-type", diagnostics)) {
    valid = false;
  }
  if (reportCtfDwtFieldError(ctraceRunMeta, source, source.dataSizeError,
                             "CTF output cannot use the configured ctrace-run size", diagnostics)) {
    valid = false;
  }
  return valid;
}

/** @brief Validates the resolved comparator, type, and size of one DWT source. */
static bool validateCtfDwtShape(const CtraceRunMeta& ctraceRunMeta, const CtraceRunSourceMeta& source,
                                DiagnosticSink& diagnostics)
{
  bool valid = true;
  if (source.source > 3U) {
    valid = false;
    reportRequirementError(diagnostics, "CTF output requires DWT comparator sources between 0 and 3",
                           routeContext("ctf", ctraceRunMeta, source));
  }

  const auto validType = TraceRunSchema::isDwtDataType(source.dataType);
  const auto* valueVariant = CtfSchema::valueVariantForTraceRunType(source.dataType, source.dataSize);
  if (!validType) {
    valid = false;
    auto context = routeContext("ctf", ctraceRunMeta, source);
    context.emplace_back("dataType", source.dataType);
    reportRequirementError(diagnostics,
                           "CTF output cannot use ctrace-run data-type '" + source.dataType + "'; " +
                               std::string(CtfSchema::ValueTypeRequirements),
                           std::move(context));
  }
  if (!TraceRunSchema::isDwtDataSize(source.dataSize) || (validType && valueVariant == nullptr)) {
    valid = false;
    auto context = routeContext("ctf", ctraceRunMeta, source);
    context.emplace_back("dataType", source.dataType);
    context.emplace_back("dataSize", std::to_string(source.dataSize));
    reportRequirementError(diagnostics,
                           "CTF output cannot use ctrace-run size " + std::to_string(source.dataSize) +
                               " with data-type '" + source.dataType + "'; " +
                               std::string(CtfSchema::ValueTypeRequirements),
                           std::move(context));
  }
  return valid;
}

/** @brief Validates the resolved address range of one DWT source. */
static bool validateCtfDwtAddressRange(const CtraceRunMeta& ctraceRunMeta, const CtraceRunSourceMeta& source,
                                       DiagnosticSink& diagnostics)
{
  const auto* valueVariant = CtfSchema::valueVariantForTraceRunType(source.dataType, source.dataSize);
  if (!source.address.has_value() || valueVariant == nullptr) {
    return true;
  }
  const auto extent = source.dataSize - 1U;
  if (*source.address <= std::numeric_limits<std::uint64_t>::max() - extent) {
    return true;
  }

  auto context = routeContext("ctf", ctraceRunMeta, source);
  context.emplace_back("address", std::to_string(*source.address));
  context.emplace_back("dataSize", std::to_string(source.dataSize));
  reportRequirementError(diagnostics, "CTF output cannot represent the configured DWT address range",
                         std::move(context));
  return false;
}

/** @brief Validates all CTF requirements for one selected DWT source. */
static bool validateCtfDwtSource(const CtraceRunMeta& ctraceRunMeta, const CtraceRunSourceMeta& source,
                                 DiagnosticSink& diagnostics)
{
  if (!validateCtfDwtParsedMetadata(ctraceRunMeta, source, diagnostics)) {
    return false;
  }
  const auto shapeValid = validateCtfDwtShape(ctraceRunMeta, source, diagnostics);
  const auto addressValid = validateCtfDwtAddressRange(ctraceRunMeta, source, diagnostics);
  return shapeValid && addressValid;
}

/** @brief Validates address, data type, and size metadata for selected DWT routes. */
static bool validateCtfDwtMetadata(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
                                   DiagnosticSink& diagnostics)
{
  bool valid = true;
  for (const auto& route : ctraceRunMeta.routes()) {
    for (const auto& source : route.sources) {
      if (source.type != "dwt" || !routeMatchesSelection(source, selection)) {
        continue;
      }
      if (!validateCtfDwtSource(ctraceRunMeta, source, diagnostics)) {
        valid = false;
      }
    }
  }
  return valid;
}

/** @brief Converts selected trace-run sources into normalized CTF routes. */
static std::vector<CtfSourceDescriptor> resolveCtfSources(const CtraceRunMeta& ctraceRunMeta,
                                                          const TraceSelection& selection)
{
  std::set<std::tuple<std::string, std::uint32_t, TraceRouteId>> resolvedKeys;
  std::vector<CtfSourceDescriptor> sources;
  for (const auto& route : ctraceRunMeta.routes()) {
    for (const auto& source : route.sources) {
      if ((source.type != "itm" && source.type != "dwt") || (source.type == "itm" && source.source == 0U) ||
          !routeMatchesSelection(source, selection) ||
          !resolvedKeys.emplace(source.type, source.source, source.route.id).second) {
        continue;
      }

      sources.push_back({
          source.type,
          source.source,
          source.route,
          source.label,
          source.address,
          source.dataType,
          static_cast<std::uint8_t>(source.dataSize),
      });
    }
  }
  return sources;
}

/** @brief Copies the complete normalized route catalogue for CTF state validation. */
static std::vector<TraceRouteIdentity> resolveCtfRoutes(const CtraceRunMeta& ctraceRunMeta)
{
  std::vector<TraceRouteIdentity> routes;
  for (const auto& route : ctraceRunMeta.routes()) {
    routes.push_back(route.identity);
  }
  return routes;
}

bool TraceOutputPlan::hasRequestedOutputs() const
{
  return csvRequested || ctfRequested;
}

bool TraceOutputPlan::hasEnabledOutputs() const
{
  return csv.has_value() || ctf.has_value();
}

TraceOutputPlan planTraceOutputs(const TraceOutputRequest& request, const std::filesystem::path& rawInputPath,
                                 const CtraceRunMeta& ctraceRunMeta, DiagnosticSink& diagnostics)
{
  TraceOutputPlan plan;
  plan.csvRequested = request.csv;
  plan.ctfRequested = request.ctf;
  if (!plan.hasRequestedOutputs()) {
    return plan;
  }

  const auto paths = outputPaths(rawInputPath);
  if (plan.csvRequested) {
    plan.csv = CsvOutputConfig{
        paths.csv,
        request.selection,
    };
  }
  if (plan.ctfRequested) {
    auto metadata = resolveCtfTopology(ctraceRunMeta, request.selection, diagnostics);
    const auto validRoutes = validateCtfSourceIdentity(ctraceRunMeta, request.selection, diagnostics);
    const auto validTypes = validateCtfDwtMetadata(ctraceRunMeta, request.selection, diagnostics);
    if (metadata.has_value() && validRoutes && validTypes) {
      metadata->sources = resolveCtfSources(ctraceRunMeta, request.selection);
      plan.ctf = CtfOutputConfig{
          paths.ctf, paths.traceCompassXml, request.selection, std::move(*metadata), resolveCtfRoutes(ctraceRunMeta),
      };
    }
  }
  return plan;
}
