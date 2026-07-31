/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OutputRequirements.hpp"

#include "CtraceRunMeta.hpp"
#include "ctf/CtfSchema.hpp"
#include "DiagnosticSink.hpp"
#include "TraceSelection.hpp"
#include "TraceOutputConfig.hpp"
#include "TraceRunConfig.hpp"

#include <algorithm>
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

namespace {

struct OutputPaths {
  std::filesystem::path csv;
  std::filesystem::path ctf;
  std::filesystem::path traceCompassXml;
};

OutputPaths outputPaths(const std::filesystem::path& rawInputPath)
{
  const auto captureName = rawInputPath.filename().stem();
  const auto solutionSetName = captureName.stem();
  if (captureName.empty() || solutionSetName.empty()) { // LCOV_EXCL_BR_LINE: both invalid name forms are covered
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

bool routeMatchesSelection(const CtraceRunSourceMeta& source, const TraceSelection& selection)
{
  return selection.includesType(source.type) && selection.includesStream(source.traceBusId);
}

std::vector<std::pair<std::string, std::string>>
routeContext(std::string_view backend, const CtraceRunMeta& ctraceRunMeta, const CtraceRunSourceMeta& source)
{
  std::vector<std::pair<std::string, std::string>> context{
      {"backend", std::string(backend)},
      {"channel",
       std::string(source.type == "itm" ? "ITM" : "DWT") + std::to_string(source.source)}, // LCOV_EXCL_BR_LINE
      {"stream", std::to_string(source.traceBusId)},
  };
  if (!ctraceRunMeta.configPath().empty()) { // LCOV_EXCL_BR_LINE: present and absent config paths are covered
    context.emplace_back("config", ctraceRunMeta.configPath());
  }
  return context;
} // LCOV_EXCL_LINE: GCC attributes the generated context cleanup to this closing brace

void reportRequirementError(DiagnosticSink& diagnostics, std::string code, std::string message,
                            std::vector<std::pair<std::string, std::string>> context)
{
  diagnostics.report({
      DiagnosticSink::Severity::Error,
      DiagnosticSink::Category::Output,
      std::move(code),
      std::move(message),
      std::move(context),
  });
}

bool validateCtfRouteIdentity(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
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
    const auto sameMetadata =
        first.label == source.label && first.symbolAddress == source.symbolAddress && // LCOV_EXCL_BR_LINE
        first.valueType == source.valueType && first.valueSize == source.valueSize && // LCOV_EXCL_BR_LINE
        first.symbolTypeError == source.symbolTypeError &&                            // LCOV_EXCL_BR_LINE
        first.symbolSizeError == source.symbolSizeError;
    const auto indistinguishableProcessors =
        first.traceBusId == source.traceBusId && first.processorName != source.processorName; // LCOV_EXCL_BR_LINE
    if ((sameMetadata && !indistinguishableProcessors) || !reported.insert(key).second) {     // LCOV_EXCL_BR_LINE
      continue;
    }

    valid = false;
    auto context = routeContext("ctf", ctraceRunMeta, source);
    context.emplace_back("type", source.type);
    context.emplace_back("firstProcessor", first.processorName.value_or("<unspecified>"));
    context.emplace_back("otherProcessor", source.processorName.value_or("<unspecified>"));
    context.emplace_back("firstStream", std::to_string(first.traceBusId));
    reportRequirementError(
        diagnostics, "ctf-trace-route-ambiguous",
        "CTF metadata cannot describe conflicting active type/source routes from different processors or Trace Bus IDs",
        std::move(context));
  }
  return valid;
}

struct SelectedClockResolution {
  std::optional<std::uint64_t> clockHz;
  bool hasRoutes{false};
  bool valid{true};
};

SelectedClockResolution resolveSelectedCtfClock(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
                                                DiagnosticSink& diagnostics)
{
  SelectedClockResolution result;
  for (const auto& [traceBusId, timestamp] : ctraceRunMeta.timestampsByTraceBusId()) {
    if (!selection.includesStream(traceBusId)) {
      continue;
    }
    result.hasRoutes = true;
    if (timestamp.clockError.has_value()) {
      result.valid = false;
      reportRequirementError(diagnostics, "ctf-timestamp-clock-invalid", // LCOV_EXCL_BR_LINE
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
      reportRequirementError(diagnostics, "ctf-timestamp-clock-missing", // LCOV_EXCL_BR_LINE
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
      reportRequirementError(diagnostics, "ctf-timestamp-clock-invalid", // LCOV_EXCL_BR_LINE
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
      reportRequirementError(diagnostics, "ctf-timestamp-clock-ambiguous", // LCOV_EXCL_BR_LINE
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

std::optional<std::uint64_t> resolveDefaultCtfClock(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
                                                    DiagnosticSink& diagnostics)
{
  for (const auto& error : ctraceRunMeta.timestampClockErrors()) {
    reportRequirementError(diagnostics, "ctf-timestamp-clock-invalid", // LCOV_EXCL_BR_LINE
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
    if (!selection.streams.empty() && !ctraceRunMeta.timestampsByTraceBusId().empty()) { // LCOV_EXCL_BR_LINE
      reportRequirementError(diagnostics, "ctf-timestamp-clock-ambiguous",               // LCOV_EXCL_BR_LINE
                             "CTF output cannot assign unformatted or unknown Trace Bus IDs to processors with "
                             "different timestamps.clock values",
                             {
                                 {"backend", "ctf"},
                                 {"config", ctraceRunMeta.configPath()},
                             });
      return std::nullopt;
    }
    reportRequirementError(diagnostics, "ctf-timestamp-clock-missing", // LCOV_EXCL_BR_LINE
                           "CTF output requires timestamps.clock from an active ctrace-setup; no default is assumed",
                           {
                               {"backend", "ctf"},
                               {"config", ctraceRunMeta.configPath()},
                           });
    return std::nullopt;
  }
  if (*ctraceRunMeta.timestampClockHz() == 0U) {
    reportRequirementError(diagnostics, "ctf-timestamp-clock-invalid", // LCOV_EXCL_BR_LINE
                           "CTF output requires timestamps.clock to be greater than zero",
                           {
                               {"backend", "ctf"},
                               {"config", ctraceRunMeta.configPath()},
                           });
    return std::nullopt;
  }
  return ctraceRunMeta.timestampClockHz();
}

std::optional<std::uint64_t> resolveCtfClock(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
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

bool validateCtfDwtMetadata(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection,
                            DiagnosticSink& diagnostics)
{
  bool valid = true;
  for (const auto& source : ctraceRunMeta.sources()) {
    if (source.type != "dwt" || !routeMatchesSelection(source, selection)) {
      continue;
    }
    bool sourceValid = true;
    if (source.symbolTypeError.has_value()) {
      valid = false;
      sourceValid = false;
      auto context = routeContext("ctf", ctraceRunMeta, source);
      context.emplace_back("error", *source.symbolTypeError);
      reportRequirementError(diagnostics, "ctf-dwt-symbol-type-invalid",
                             "CTF output cannot use the configured ctrace-run data.symbol-type", std::move(context));
    }
    if (source.symbolSizeError.has_value()) {
      valid = false;
      sourceValid = false;
      auto context = routeContext("ctf", ctraceRunMeta, source);
      context.emplace_back("error", *source.symbolSizeError);
      reportRequirementError(diagnostics, "ctf-dwt-symbol-size-invalid",
                             "CTF output cannot use the configured ctrace-run data.symbol-size", std::move(context));
    }
    if (!sourceValid) {
      continue;
    }
    const auto validType = TraceRunSchema::isDwtDataType(source.valueType);
    const auto* valueVariant = CtfSchema::valueVariantForTraceRunType(source.valueType, source.valueSize);
    if (!validType) {
      valid = false;
      auto context = routeContext("ctf", ctraceRunMeta, source);
      context.emplace_back("dataType", source.valueType);
      reportRequirementError(diagnostics, "ctf-dwt-symbol-type-invalid",
                             "CTF output cannot use ctrace-run data.symbol-type '" + source.valueType + "'; " +
                                 std::string(CtfSchema::ValueTypeRequirements),
                             std::move(context));
    }
    if (!TraceRunSchema::isDwtDataSize(source.valueSize) ||
        (validType && valueVariant == nullptr)) { // LCOV_EXCL_BR_LINE
      valid = false;
      auto context = routeContext("ctf", ctraceRunMeta, source);
      context.emplace_back("dataType", source.valueType);
      context.emplace_back("dataSize", std::to_string(source.valueSize));
      reportRequirementError(diagnostics, "ctf-dwt-symbol-size-invalid",
                             "CTF output cannot use ctrace-run data.symbol-size " + std::to_string(source.valueSize) +
                                 " with data.symbol-type '" + source.valueType + "'; " +
                                 std::string(CtfSchema::ValueTypeRequirements),
                             std::move(context));
    }
  }
  return valid;
}

std::vector<ResolvedTraceSource> resolveCtfSources(const CtraceRunMeta& ctraceRunMeta, const TraceSelection& selection)
{
  std::set<std::tuple<std::string, std::uint32_t, std::uint8_t>> resolvedKeys;
  std::vector<ResolvedTraceSource> sources;
  for (const auto& route : ctraceRunMeta.sources()) {
    // LCOV_EXCL_BR_START: all behavioral alternatives are covered; GCC expands short-circuit bookkeeping
    if ((route.type != "itm" && route.type != "dwt") || (route.type == "itm" && route.source == 0U) ||
        !routeMatchesSelection(route, selection) ||
        !resolvedKeys.emplace(route.type, route.source, route.traceBusId).second) {
      continue;
    }
    // LCOV_EXCL_BR_STOP

    sources.push_back({
        route.type,
        route.source,
        route.traceBusId,
        route.label,
        route.symbolAddress,
        route.valueType,
        static_cast<std::uint8_t>(route.valueSize),
    });
  }
  return sources;
}

} // namespace

bool TraceOutputPlan::hasRequestedOutputs() const
{
  return csvRequested || ctfRequested;
}

bool TraceOutputPlan::hasEnabledOutputs() const
{
  return csv.has_value() || ctf.has_value(); // LCOV_EXCL_BR_LINE: all output combinations are covered
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
    // LCOV_EXCL_BR_START: validity inputs are tested independently; short-circuit permutations are equivalent
    auto sources =
        clock.has_value() && validRoutes && validTypes
            ? std::optional<std::vector<ResolvedTraceSource>>(resolveCtfSources(ctraceRunMeta, request.selection))
            : std::nullopt;
    if (clock.has_value() && validRoutes && validTypes && sources.has_value()) {
      plan.ctf = CtfOutputConfig{
          paths.ctf, paths.traceCompassXml, *clock, request.selection, std::move(*sources),
      };
    }
    // LCOV_EXCL_BR_STOP
  }
  return plan;
}
