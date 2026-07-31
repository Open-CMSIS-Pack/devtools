/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"
#include "TraceRunTestSupport.hpp"
#include <gtest/gtest.h>
#include "CtraceRunMeta.hpp"
#include "OutputRequirements.hpp"
#include "TraceRunConfig.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>

static TraceOutputRequest outputRequest(bool csv, bool ctf)
{
  return {csv, ctf, {}};
}

static TraceOutputPlan planOutputs(const TraceOutputRequest& request, const std::filesystem::path& rawInputPath,
                                   const CtraceRunMeta& meta, DiagnosticSink& diagnostics)
{
  return planTraceOutputs(request, rawInputPath, meta, diagnostics);
}

static TraceOutputPlan planOutputs(const TraceOutputRequest& request, const std::filesystem::path& rawInputPath,
                                   const TraceRunConfig& config, DiagnosticSink& diagnostics)
{
  return planOutputs(request, rawInputPath, CtraceRunMeta::fromConfig(config), diagnostics);
}

static TraceRunConfig backendRequirementsConfig()
{
  TraceRunConfig config;
  config.path = "BackendRequirements.ctrace-run.yml";
  auto setup = TraceRunTestSupport::makeTimestampSetup(std::nullopt, 400000000U, 1U);
  setup.data.push_back(TraceRunDataSetup{"double", 4U});
  config.setups.push_back(setup);
  auto reference = TraceRunTestSupport::makeReference("dwt", std::nullopt, std::nullopt, {0U}, "opaque/dwt");
  reference.dataSetupIndex = 0U;
  config.references.push_back(reference);
  return config;
}

TEST(CtraceUnitTests, testBackendRequirementsUsePerStreamMetadata)
{
  TraceRunConfig multicore;
  multicore.path = "Multicore.ctrace-run.yml";
  auto core0 = TraceRunTestSupport::makeTimestampSetup("core0", 400000000U, 1U);
  auto core1 = TraceRunTestSupport::makeTimestampSetup("core1", 400000000U, 4U);
  multicore.setups = {core0, core1};

  const auto core0Itm = TraceRunTestSupport::makeReference("itm", "core0", 1U, {1U}, "opaque/core0-route");
  const auto core1Itm = TraceRunTestSupport::makeReference("itm", "core1", 2U, {1U}, "opaque/core1-route");
  multicore.references = {core0Itm, core1Itm};

  core1.timestamps = TraceRunTimestampSetup{400000000U, std::nullopt};
  multicore.setups = {core0, core1};
  const auto missingPrescalerMeta = CtraceRunMeta::fromConfig(multicore);
  require(missingPrescalerMeta.timestampPrescaler() ==
              std::optional<std::uint32_t>(TraceRunSchema::kDefaultTimestampPrescaler),
          "an omitted processor prescaler must resolve to the specified default");
  require(missingPrescalerMeta.timestampPrescalersByTraceBusId() ==
              std::map<std::uint8_t, std::uint32_t>({{1U, 1U}, {2U, 1U}}),
          "the default prescaler must be retained per ATB stream");

  core0.timestamps = TraceRunTimestampSetup{400000000U, 4U};
  multicore.setups = {core0, core1};
  const auto mixedMissingPrescalerMeta = CtraceRunMeta::fromConfig(multicore);
  require(mixedMissingPrescalerMeta.hasDistinctProcessorPrescalers(),
          "an explicit prescaler must be compared with another processor's default");
  require(mixedMissingPrescalerMeta.timestampPrescalersByTraceBusId() ==
              std::map<std::uint8_t, std::uint32_t>({{1U, 4U}, {2U, 1U}}),
          "different explicit and default prescalers must remain stream-specific");

  core1.timestamps = TraceRunTimestampSetup{200000000U, 1U};
  multicore.setups = {core0, core1};
  const auto distinctClockMeta = CtraceRunMeta::fromConfig(multicore);
  require(distinctClockMeta.timestampsByTraceBusId().at(1U).clockHz == std::optional<std::uint64_t>(400000000U) &&
              distinctClockMeta.timestampsByTraceBusId().at(2U).clockHz == std::optional<std::uint64_t>(200000000U),
          "processor clocks must remain associated with their Trace Bus IDs");

  auto ctfRequest = outputRequest(false, true);
  CollectingDiagnosticSink allClockDiagnostics;
  const auto allClockPlan =
      planOutputs(ctfRequest, "captures/Multicore.SWO.raw", distinctClockMeta, allClockDiagnostics);
  require(!allClockPlan.ctf.has_value(), "CTF must reject selected Trace Bus IDs with different clocks");
  allClockDiagnostics.singleEvent("ctf-timestamp-clock-ambiguous");

  ctfRequest.selection.streams = {2U};
  CollectingDiagnosticSink selectedClockDiagnostics;
  const auto selectedClockPlan =
      planOutputs(ctfRequest, "captures/Multicore.SWO.raw", distinctClockMeta, selectedClockDiagnostics);
  require(selectedClockPlan.ctf.has_value() && selectedClockPlan.ctf->coreClockHz == 200000000U &&
              selectedClockDiagnostics.events().empty(),
          "a selected Trace Bus ID must use its processor's clock");
}

TEST(CtraceUnitTests, testDwtDataMetadataDefaultsAndValidation)
{
  TraceRunConfig config;
  config.path = "DwtSize.ctrace-run.yml";
  auto setup = TraceRunTestSupport::makeTimestampSetup(std::nullopt, 100000000U, 1U);
  setup.data = {
      TraceRunDataSetup{},
      TraceRunDataSetup{"unsigned int", 1U},
      TraceRunDataSetup{"signed int", 2U},
  };
  config.setups.push_back(std::move(setup));
  for (std::uint32_t comparator = 0U; comparator < 3U; ++comparator) {
    auto reference = TraceRunTestSupport::makeReference("dwt", std::nullopt, std::nullopt, {comparator},
                                                        "opaque/dwt-" + std::to_string(comparator));
    reference.dataSetupIndex = comparator;
    config.references.push_back(std::move(reference));
  }
  const auto meta = CtraceRunMeta::fromConfig(config);

  const auto allRequest = outputRequest(true, true);
  CollectingDiagnosticSink configurationDiagnostics;
  const auto outputPlan = planOutputs(allRequest, "DwtSize.SWO.raw", meta, configurationDiagnostics);
  require(outputPlan.csv.has_value() && outputPlan.ctf.has_value(),
          "valid or missing DWT metadata must not disable CSV or CTF");
  require(configurationDiagnostics.events().empty(), "valid or missing DWT metadata must not produce diagnostics");
  require(outputPlan.ctf->sources.size() == 3U && outputPlan.ctf->sources[0].valueType == "unsigned int" &&
              outputPlan.ctf->sources[0].valueSize == 4U && outputPlan.ctf->sources[1].valueType == "unsigned int" &&
              outputPlan.ctf->sources[1].valueSize == 1U && outputPlan.ctf->sources[2].valueType == "signed int" &&
              outputPlan.ctf->sources[2].valueSize == 2U,
          "CTF data.symbol-type/data.symbol-size defaults or explicit values mismatch");

  config.setups[0].data[1].symbolSize = 0U;
  CollectingDiagnosticSink invalidSizeDiagnostics;
  const auto invalidSizePlan = planOutputs(allRequest, "DwtSize.SWO.raw", config, invalidSizeDiagnostics);
  require(invalidSizePlan.csv.has_value() && !invalidSizePlan.ctf.has_value(),
          "invalid data.symbol-size must disable only CTF");
  invalidSizeDiagnostics.singleEvent("ctf-dwt-symbol-size-invalid");
}

TEST(CtraceUnitTests, testOutputRequirementsAreBackendSpecific)
{
  auto config = backendRequirementsConfig();

  const auto csvRequest = outputRequest(true, false);
  CollectingDiagnosticSink invalidTypeCsvDiagnostics;
  const auto invalidTypeCsv = planOutputs(csvRequest, "BackendRequirements.SWO.raw", config, invalidTypeCsvDiagnostics);
  require(invalidTypeCsv.csv.has_value() && !invalidTypeCsv.ctf.has_value(),
          "an invalid explicit data.symbol-type must not disable CSV");
  require(invalidTypeCsvDiagnostics.events().empty(), "CSV must not inspect CTF-only data.symbol-type metadata");

  const auto allRequest = outputRequest(true, true);
  CollectingDiagnosticSink invalidTypeAllDiagnostics;
  const auto invalidTypeAll = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, invalidTypeAllDiagnostics);
  require(invalidTypeAll.csv.has_value() && !invalidTypeAll.ctf.has_value(),
          "an invalid explicit data.symbol-type must disable only CTF for --all");
  invalidTypeAllDiagnostics.singleEvent("ctf-dwt-symbol-type-invalid");

  config.setups[0].data[0].symbolType.reset();
  config.setups[0].data[0].symbolSize.reset();
  CollectingDiagnosticSink missingTypeDiagnostics;
  const auto missingType = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, missingTypeDiagnostics);
  require(missingType.csv.has_value() && missingType.ctf.has_value(),
          "missing data.symbol-type/data.symbol-size must use CTF defaults and leave CSV enabled");
  require(missingType.csv->outputPath == std::filesystem::path("BackendRequirements.SWO.csv") &&
              missingType.ctf->outputDirectory == std::filesystem::path("BackendRequirements.ctf") &&
              missingType.ctf->traceCompassXmlPath ==
                  std::filesystem::path("BackendRequirements.SWO.traceanalysis.xml") &&
              missingType.ctf->coreClockHz == 400000000U && missingType.ctf->sources.size() == 1U &&
              missingType.ctf->sources[0].valueType == "unsigned int" && missingType.ctf->sources[0].valueSize == 4U,
          "output preflight must resolve artifact paths, clock, routes, and defaults");
  require(missingTypeDiagnostics.events().empty(),
          "missing optional data.symbol-type/data.symbol-size must not produce diagnostics");

  config.setups[0].data[0].symbolTypeError = "data.symbol-type must be scalar";
  CollectingDiagnosticSink malformedTypeDiagnostics;
  const auto malformedType = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, malformedTypeDiagnostics);
  require(malformedType.csv.has_value() && !malformedType.ctf.has_value(),
          "malformed data.symbol-type must disable only CTF");

  config.setups[0].data[0].symbolTypeError.reset();
  config.setups[0].data[0].symbolSizeError = "data.symbol-size must be unsigned";
  CollectingDiagnosticSink malformedSizeDiagnostics;
  const auto malformedSize = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, malformedSizeDiagnostics);
  require(malformedSize.csv.has_value() && !malformedSize.ctf.has_value(),
          "malformed data.symbol-size must disable only CTF");
  malformedSizeDiagnostics.singleEvent("ctf-dwt-symbol-size-invalid");
}

TEST(CtraceUnitTests, testCtfOutputRequiresAValidClock)
{
  auto config = backendRequirementsConfig();
  config.setups[0].data[0] = TraceRunDataSetup{};
  const auto allRequest = outputRequest(true, true);
  config.setups[0].timestamps->clockHz.reset();
  CollectingDiagnosticSink missingClockDiagnostics;
  const auto missingClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, missingClockDiagnostics);
  require(missingClock.csv.has_value() && !missingClock.ctf.has_value(),
          "missing timestamps.clock must disable only CTF");

  config.setups[0].timestamps->clockError = "timestamps.clock must be unsigned";
  CollectingDiagnosticSink malformedClockDiagnostics;
  const auto malformedClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, malformedClockDiagnostics);
  require(malformedClock.csv.has_value() && !malformedClock.ctf.has_value(),
          "malformed timestamps.clock must disable only CTF");

  config.setups[0].timestamps->clockError.reset();
  config.setups[0].timestamps->clockHz = 0U;
  CollectingDiagnosticSink zeroClockDiagnostics;
  const auto zeroClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, zeroClockDiagnostics);
  require(zeroClock.csv.has_value() && !zeroClock.ctf.has_value(), "zero timestamps.clock must disable only CTF");
}

TEST(CtraceUnitTests, testOutputRequirementsHonorFiltersAndCheckOnlyMode)
{
  const auto config = backendRequirementsConfig();
  auto allRequest = outputRequest(true, true);
  allRequest.selection.types = {"itm"};
  CollectingDiagnosticSink filteredDiagnostics;
  const auto filtered = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, filteredDiagnostics);
  require(filtered.csv.has_value() && filtered.ctf.has_value(),
          "filtered-out DWT type metadata must not affect either backend");
  require(filteredDiagnostics.events().empty(), "filtered-out invalid DWT metadata must not produce diagnostics");

  CollectingDiagnosticSink checkOnlyDiagnostics;
  const auto checkOnly =
      planOutputs(outputRequest(false, false), "BackendRequirements.SWO.raw", config, checkOnlyDiagnostics);
  require(!checkOnly.hasRequestedOutputs() && checkOnlyDiagnostics.events().empty(),
          "check-only mode must not apply CSV or CTF metadata requirements");
}

TEST(CtraceUnitTests, testOutputPreflightRejectsAmbiguousRoutesForCtfOnly)
{
  TraceRunConfig config;
  config.path = "AmbiguousRoutes.ctrace-run.yml";
  auto setup = TraceRunTestSupport::makeTimestampSetup(std::nullopt, 400000000U, 1U);
  setup.data.push_back(TraceRunDataSetup{"unsigned int", 4U});
  config.setups.push_back(setup);

  auto first = TraceRunTestSupport::makeReference("dwt", std::nullopt, 1U, {0U}, "opaque/dwt-a");
  first.dataSetupIndex = 0U;
  first.label = "core-one";
  TraceRunReference second = first;
  second.label = "core-two";
  config.references = {first, second};

  auto allRequest = outputRequest(true, true);
  CollectingDiagnosticSink diagnostics;
  const auto plan = planOutputs(allRequest, "captures/AmbiguousRoutes.SWO.raw", config, diagnostics);
  require(plan.csv.has_value() && !plan.ctf.has_value(),
          "conflicting CTF route labels must not disable independent CSV output");
  diagnostics.singleEvent("ctf-trace-route-ambiguous");

  config.references[1].stream = 2U;
  config.references[1].label = "core-one";
  CollectingDiagnosticSink routeDiagnostics;
  const auto routePlan = planOutputs(allRequest, "captures/AmbiguousRoutes.SWO.raw", config, routeDiagnostics);
  require(routePlan.csv.has_value() && routePlan.ctf.has_value() && routePlan.ctf->sources.size() == 2U,
          "CTF must retain equivalent routes with distinct Trace Bus IDs");
  require(routeDiagnostics.events().empty(), "equivalent CTF metadata on distinct Trace Bus IDs must not be ambiguous");

  TraceRunConfig processorConfig;
  processorConfig.path = "AmbiguousProcessors.ctrace-run.yml";
  auto core0Setup = setup;
  core0Setup.processorName = "core0";
  auto core1Setup = setup;
  core1Setup.processorName = "core1";
  processorConfig.setups = {core0Setup, core1Setup};
  auto core0Reference = first;
  core0Reference.processorName = "core0";
  auto core1Reference = first;
  core1Reference.processorName = "core1";
  processorConfig.references = {core0Reference, core1Reference};
  CollectingDiagnosticSink processorDiagnostics;
  const auto processorPlan = planOutputs(outputRequest(true, false), "captures/AmbiguousProcessors.SWO.raw",
                                         processorConfig, processorDiagnostics);
  require(processorPlan.csv.has_value() && processorDiagnostics.events().empty(),
          "CSV must preserve raw stream/source values without consuming processor metadata");

  allRequest.selection.streams = {1U};
  CollectingDiagnosticSink selectedDiagnostics;
  const auto selectedPlan = planOutputs(allRequest, "captures/AmbiguousRoutes.SWO.raw", config, selectedDiagnostics);
  require(selectedPlan.csv.has_value() && selectedPlan.ctf.has_value() && selectedPlan.ctf->sources.size() == 1U &&
              selectedPlan.ctf->sources[0].label == std::optional<std::string>("core-one"),
          "an explicit stream selection must produce one resolved CTF route");
  require(selectedDiagnostics.events().empty(), "an unambiguous selected route must not produce preflight diagnostics");

  config.setups[0].data.push_back(TraceRunDataSetup{"unsigned int", 2U});
  config.references[1].stream = 1U;
  config.references[1].ctraceRef = "opaque/dwt-b";
  config.references[1].dataSetupIndex = 1U;
  CollectingDiagnosticSink sizeDiagnostics;
  const auto csvPlan =
      planOutputs(outputRequest(true, false), "captures/AmbiguousRoutes.SWO.raw", config, sizeDiagnostics);
  require(csvPlan.csv.has_value(), "CTF-only data.symbol-size metadata must not disable CSV");
  require(sizeDiagnostics.events().empty(), "CSV must not inspect CTF-only data.symbol-size metadata");
}

TEST(CtraceUnitTests, testOutputRequirementsValidateDefaultClockWithoutRoutes)
{
  const auto ctfRequest = outputRequest(false, true);
  TraceRunConfig config;
  config.path = "Clock.ctrace-run.yml";
  TraceRunSetup setup;
  setup.timestamps = TraceRunTimestampSetup{};
  config.setups.push_back(setup);

  CollectingDiagnosticSink missingDiagnostics;
  auto plan = planOutputs(ctfRequest, "Clock.SWO.raw", config, missingDiagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  missingDiagnostics.singleEvent("ctf-timestamp-clock-missing");

  config.setups[0].timestamps->clockError = "clock must be an unsigned integer";
  CollectingDiagnosticSink malformedDiagnostics;
  plan = planOutputs(ctfRequest, "Clock.SWO.raw", config, malformedDiagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  malformedDiagnostics.singleEvent("ctf-timestamp-clock-invalid");

  config.setups[0].timestamps->clockError.reset();
  config.setups[0].timestamps->clockHz = 0U;
  CollectingDiagnosticSink zeroDiagnostics;
  plan = planOutputs(ctfRequest, "Clock.SWO.raw", config, zeroDiagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  zeroDiagnostics.singleEvent("ctf-timestamp-clock-invalid");
}

TEST(CtraceUnitTests, testOutputRequirementsRejectUnknownStreamWithMultipleClocks)
{
  TraceRunConfig config;
  config.path = "Multicore.ctrace-run.yml";
  config.setups = {
      TraceRunTestSupport::makeTimestampSetup("first", 100U, 1U),
      TraceRunTestSupport::makeTimestampSetup("second", 200U, 1U),
  };
  config.references = {
      TraceRunTestSupport::makeReference("itm", "first", 1U, {1U}, "first/itm"),
      TraceRunTestSupport::makeReference("itm", "second", 2U, {1U}, "second/itm"),
  };

  auto ctfRequest = outputRequest(false, true);
  ctfRequest.selection.streams = {99U};
  CollectingDiagnosticSink diagnostics;
  const auto plan = planOutputs(ctfRequest, "Multicore.SWO.raw", config, diagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  diagnostics.singleEvent("ctf-timestamp-clock-ambiguous");
}

TEST(CtraceUnitTests, testOutputRequirementsRejectsInputWithoutArtifactName)
{
  CollectingDiagnosticSink diagnostics;
  EXPECT_THROW((void)planTraceOutputs(outputRequest(true, false), {}, CtraceRunMeta::fromConfig({}), diagnostics),
               std::runtime_error);
}
