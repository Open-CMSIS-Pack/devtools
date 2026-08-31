/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OutputRequirements.h"

#include "CtraceRunMeta.h"
#include "ctf/CtfSchema.h"
#include "DiagnosticSink.h"
#include "TraceSelection.h"
#include "TraceOutputConfig.h"
#include "TraceRunConfig.h"

#include <cstdint>
#include <filesystem>
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
  auto ctfPath = outputDirectory / solutionSetName;
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
  return selection.includesType(source.type) && selection.includesStream(source.traceBusId);
}

static std::vector<std::pair<std::string, std::string>>
routeContext(const std::string_view& backend, const CtraceRunMeta& ctraceRunMeta, const CtraceRunSourceMeta& source)
{
  std::vector<std::pair<std::string, std::string>> context{
      {"backend", std::string(backend)},
      {"channel", std::string(source.type == "itm" ? "ITM" : "DWT") + std::to_string(source.source)},
      {"stream", std::to_string(source.traceBusId)},
  };
  if (!ctraceRunMeta.configPath().empty()) {
    context.emplace_back("config", ctraceRunMeta.configPath());
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
static bool validateCtfRouteIdentity(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
                                     DiagnosticSink& diagnostics)
{
  bool valid = true;
  std::map<std::pair<std::string, std::uint32_t>, const CtraceRunSourceMeta*> routes;
  std::set<std::pair<std::string, std::uint32_t>> reported;
  for (const auto& source : ctraceRunMeta.sources()) {
    if (!routeMatchesSelection(source, selection)) {
      continue;
    }
    const auto key = std::make_pair(source.type, source.source);
    const auto [found, inserted] = routes.emplace(key, &source);
    if (inserted) {
      continue;
    }
    const auto& first = *found->second;
    const auto sameMetadata = first.label == source.label && first.address == source.address &&
                              first.dataType == source.dataType && first.dataSize == source.dataSize &&
                              first.addressError == source.addressError &&
                              first.dataTypeError == source.dataTypeError &&
                              first.dataSizeError == source.dataSizeError;
    const auto indistinguishableProcessors =
        first.traceBusId == source.traceBusId && first.processorName != source.processorName;
    if ((sameMetadata && !indistinguishableProcessors) || !reported.insert(key).second) {
      continue;
    }

    valid = false;
    auto context = routeContext("ctf", ctraceRunMeta, source);
    context.emplace_back("type", source.type);
    context.emplace_back("firstProcessor", first.processorName.value_or("<unspecified>"));
    context.emplace_back("otherProcessor", source.processorName.value_or("<unspecified>"));
    context.emplace_back("firstStream", std::to_string(first.traceBusId));
    reportRequirementError(
        diagnostics,
        "CTF metadata cannot describe conflicting active type/source routes from different processors or Trace Bus IDs",
        std::move(context));
  }
  return valid;
}

/** @brief Stores an unambiguous clock or the diagnostics preventing selection. */
struct SelectedClockResolution {
  std::optional<std::uint64_t> clockHz;
  bool hasRoutes{false};
  bool valid{true};
};

/** @brief Resolves a common clock from all selected stream routes. */
static SelectedClockResolution resolveSelectedCtfClock(const CtraceRunMeta& ctraceRunMeta,
                                                       const TraceSelection& selection, DiagnosticSink& diagnostics)
{
  SelectedClockResolution result;
  for (const auto& [traceBusId, timestamp] : ctraceRunMeta.timestampsByTraceBusId()) {
    if (!selection.includesStream(traceBusId)) {
      continue;
    }
    result.hasRoutes = true;
    if (timestamp.clockError.has_value()) {
      result.valid = false;
      reportRequirementError(diagnostics,
                             "CTF output cannot use the configured timestamps.clock",
                             {
                                 {"backend", "ctf"},
                                 {"config", ctraceRunMeta.configPath()},
                                 {"stream", std::to_string(traceBusId)},
                                 {"pname", timestamp.processorName.value_or("<unspecified>")},
                                 {"error", *timestamp.clockError},
                             });
      continue;
    }
    if (!timestamp.clockHz.has_value()) {
      result.valid = false;
      reportRequirementError(diagnostics,
                             "CTF output requires timestamps.clock for the processor assigned to this Trace Bus ID",
                             {
                                 {"backend", "ctf"},
                                 {"config", ctraceRunMeta.configPath()},
                                 {"stream", std::to_string(traceBusId)},
                                 {"pname", timestamp.processorName.value_or("<unspecified>")},
                             });
      continue;
    }
    if (*timestamp.clockHz == 0U) {
      result.valid = false;
      reportRequirementError(diagnostics,
                             "CTF output requires timestamps.clock to be greater than zero",
                             {
                                 {"backend", "ctf"},
                                 {"config", ctraceRunMeta.configPath()},
                                 {"stream", std::to_string(traceBusId)},
                                 {"pname", timestamp.processorName.value_or("<unspecified>")},
                             });
      continue;
    }
    if (result.clockHz.has_value() && *result.clockHz != *timestamp.clockHz) {
      result.valid = false;
      reportRequirementError(diagnostics,
                             "CTF output cannot combine selected Trace Bus IDs with different timestamps.clock values",
                             {
                                 {"backend", "ctf"},
                                 {"config", ctraceRunMeta.configPath()},
                                 {"stream", std::to_string(traceBusId)},
                                 {"clock", std::to_string(*timestamp.clockHz)},
                             });
      continue;
    }
    result.clockHz = timestamp.clockHz;
  }
  return result;
}

/** @brief Resolves the fallback CTF clock when no selected route supplies one. */
static std::optional<std::uint64_t> resolveDefaultCtfClock(const CtraceRunMeta& ctraceRunMeta,
                                                           const TraceSelection& selection, DiagnosticSink& diagnostics)
{
  for (const auto& error : ctraceRunMeta.timestampClockErrors()) {
    reportRequirementError(diagnostics,
                           "CTF output cannot use the configured timestamps.clock",
                           {
                               {"backend", "ctf"},
                               {"config", ctraceRunMeta.configPath()},
                               {"error", error},
                           });
  }
  if (!ctraceRunMeta.timestampClockErrors().empty()) {
    return std::nullopt;
  }
  if (!ctraceRunMeta.timestampClockHz().has_value()) {
    if (!selection.streams.empty() && !ctraceRunMeta.timestampsByTraceBusId().empty()) {
      reportRequirementError(diagnostics,
                             "CTF output cannot assign unformatted or unknown Trace Bus IDs to processors with "
                             "different timestamps.clock values",
                             {
                                 {"backend", "ctf"},
                                 {"config", ctraceRunMeta.configPath()},
                             });
      return std::nullopt;
    }
    reportRequirementError(diagnostics,
                           "CTF output requires timestamps.clock from an active ctrace-setup; no default is assumed",
                           {
                               {"backend", "ctf"},
                               {"config", ctraceRunMeta.configPath()},
                           });
    return std::nullopt;
  }
  if (*ctraceRunMeta.timestampClockHz() == 0U) {
    reportRequirementError(diagnostics,
                           "CTF output requires timestamps.clock to be greater than zero",
                           {
                               {"backend", "ctf"},
                               {"config", ctraceRunMeta.configPath()},
                           });
    return std::nullopt;
  }
  return ctraceRunMeta.timestampClockHz();
}

/** @brief Resolves and validates the clock used by a CTF output. */
static std::optional<std::uint64_t> resolveCtfClock(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
                                                    DiagnosticSink& diagnostics)
{
  const auto selected = resolveSelectedCtfClock(ctraceRunMeta, selection, diagnostics);
  if (!selected.valid) {
    return std::nullopt;
  }
  if (selected.hasRoutes) {
    return selected.clockHz;
  }
  return resolveDefaultCtfClock(ctraceRunMeta, selection, diagnostics);
}

/** @brief Validates address, data type, and size metadata for selected DWT routes. */
static bool validateCtfDwtMetadata(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
                                   DiagnosticSink& diagnostics)
{
  bool valid = true;
  for (const auto& source : ctraceRunMeta.sources()) {
    if (source.type != "dwt" || !routeMatchesSelection(source, selection)) {
      continue;
    }
    bool sourceValid = true;
    if (source.addressError.has_value()) {
      valid = false;
      sourceValid = false;
      auto context = routeContext("ctf", ctraceRunMeta, source);
      context.emplace_back("error", *source.addressError);
      reportRequirementError(diagnostics, "CTF output cannot use the configured ctrace-run address",
                             std::move(context));
    }
    if (source.dataTypeError.has_value()) {
      valid = false;
      sourceValid = false;
      auto context = routeContext("ctf", ctraceRunMeta, source);
      context.emplace_back("error", *source.dataTypeError);
      reportRequirementError(diagnostics, "CTF output cannot use the configured ctrace-run data-type",
                             std::move(context));
    }
    if (source.dataSizeError.has_value()) {
      valid = false;
      sourceValid = false;
      auto context = routeContext("ctf", ctraceRunMeta, source);
      context.emplace_back("error", *source.dataSizeError);
      reportRequirementError(diagnostics, "CTF output cannot use the configured ctrace-run size",
                             std::move(context));
    }
    if (!sourceValid) {
      continue;
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
  }
  return valid;
}

/** @brief Converts selected trace-run sources into normalized CTF routes. */
static std::vector<ResolvedTraceSource> resolveCtfSources(const CtraceRunMeta& ctraceRunMeta,
                                                          const TraceSelection& selection)
{
  std::set<std::tuple<std::string, std::uint32_t, std::uint8_t>> resolvedKeys;
  std::vector<ResolvedTraceSource> sources;
  for (const auto& route : ctraceRunMeta.sources()) {
    if ((route.type != "itm" && route.type != "dwt") || (route.type == "itm" && route.source == 0U) ||
        !routeMatchesSelection(route, selection) ||
        !resolvedKeys.emplace(route.type, route.source, route.traceBusId).second) {
      continue;
    }

    sources.push_back({
        route.type,
        route.source,
        route.traceBusId,
        route.label,
        route.address,
        route.dataType,
        static_cast<std::uint8_t>(route.dataSize),
    });
  }
  return sources;
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
    auto clock = resolveCtfClock(ctraceRunMeta, request.selection, diagnostics);
    const auto validRoutes = validateCtfRouteIdentity(ctraceRunMeta, request.selection, diagnostics);
    const auto validTypes = validateCtfDwtMetadata(ctraceRunMeta, request.selection, diagnostics);
    auto sources =
        clock.has_value() && validRoutes && validTypes
            ? std::optional<std::vector<ResolvedTraceSource>>(resolveCtfSources(ctraceRunMeta, request.selection))
            : std::nullopt;
    if (clock.has_value() && validRoutes && validTypes && sources.has_value()) {
      plan.ctf = CtfOutputConfig{
          paths.ctf, paths.traceCompassXml, *clock, request.selection, std::move(*sources),
      };
    }
  }
  return plan;
}
