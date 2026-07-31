/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "CliOptions.hpp"
#include "CtraceRunMeta.hpp"
#include "OutputRequirements.hpp"
#include "TraceRunConfig.hpp"
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

static TraceOutputRequest traceOutputRequest(const CliOptions& options)
{
  return {
      options.outputFormat == OutputFormat::Csv || options.outputFormat == OutputFormat::All,
      options.outputFormat == OutputFormat::Ctf || options.outputFormat == OutputFormat::All,
      options.selection,
  };
}

static TraceRunConfig backendRequirementsConfig()
{
  TraceRunConfig config;
  config.path = "BackendRequirements.ctrace-run.yml";
  TraceRunSetup setup;
  setup.timestamps = TraceRunTimestampSetup{400000000U, 1U};
  setup.data.push_back(TraceRunDataSetup{"double", 4U});
  config.setups.push_back(setup);
  TraceRunReference reference;
  reference.ctraceRef = "opaque/dwt";
  reference.type = "dwt";
  reference.sources = {0U};
  reference.dataSetupIndex = 0U;
  config.references.push_back(reference);
  return config;
}

TEST(CtraceUnitTests, testTimestampPrescalerAndBackendRequirements)
{
  TraceRunConfig traceRun;
  traceRun.path = "Board.ctrace-run.yml";
  TraceRunSetup setup;
  setup.timestamps = TraceRunTimestampSetup{280000000U, 16U};
  traceRun.setups.push_back(setup);
  const auto ctraceRunMeta = CtraceRunMeta::fromConfig(traceRun);

  require(ctraceRunMeta.timestampPrescaler() == std::optional<std::uint32_t>(16U), "trace-run prescaler mismatch");

  TraceRunConfig multicore;
  multicore.path = "Multicore.ctrace-run.yml";
  TraceRunSetup core0;
  core0.processorName = "core0";
  core0.timestamps = TraceRunTimestampSetup{400000000U, 1U};
  TraceRunSetup core1;
  core1.processorName = "core1";
  core1.timestamps = TraceRunTimestampSetup{400000000U, 4U};
  multicore.setups = {core0, core1};

  TraceRunReference core0Itm;
  core0Itm.ctraceRef = "opaque/core0-route";
  core0Itm.type = "itm";
  core0Itm.processorName = "core0";
  core0Itm.stream = 1U;
  core0Itm.sources = {1U};
  TraceRunReference core1Itm;
  core1Itm.ctraceRef = "opaque/core1-route";
  core1Itm.type = "itm";
  core1Itm.processorName = "core1";
  core1Itm.stream = 2U;
  core1Itm.sources = {1U};
  multicore.references = {core0Itm, core1Itm};

  const auto distinctPrescalerMeta = CtraceRunMeta::fromConfig(multicore);
  require(!distinctPrescalerMeta.timestampPrescaler().has_value() &&
              distinctPrescalerMeta.hasDistinctProcessorPrescalers(),
          "different processor prescalers must not be collapsed into an unformatted value");
  require(distinctPrescalerMeta.timestampPrescalersByTraceBusId() ==
              std::map<std::uint8_t, std::uint32_t>({{1U, 1U}, {2U, 4U}}),
          "processor prescalers must remain associated with their ATB streams");

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

  CliOptions ctfOptions;
  ctfOptions.outputFormat = OutputFormat::Ctf;
  CollectingDiagnosticSink allClockDiagnostics;
  const auto allClockPlan =
      planTraceOutputs(traceOutputRequest(ctfOptions), std::filesystem::path("captures/Multicore.SWO.raw"),
                       distinctClockMeta, allClockDiagnostics);
  require(!allClockPlan.ctf.has_value() && !allClockDiagnostics.events().empty() &&
              allClockDiagnostics.events()[0].code == "ctf-timestamp-clock-ambiguous",
          "CTF must reject selected Trace Bus IDs with different clocks");

  ctfOptions.selection.streams = {2U};
  CollectingDiagnosticSink selectedClockDiagnostics;
  const auto selectedClockPlan =
      planTraceOutputs(traceOutputRequest(ctfOptions), std::filesystem::path("captures/Multicore.SWO.raw"),
                       distinctClockMeta, selectedClockDiagnostics);
  require(selectedClockPlan.ctf.has_value() && selectedClockPlan.ctf->coreClockHz == 200000000U &&
              selectedClockDiagnostics.events().empty(),
          "a selected Trace Bus ID must use its processor's clock");
}

TEST(CtraceUnitTests, testDwtDataMetadataDefaultsAndValidation)
{
  TraceRunConfig config;
  config.path = "DwtSize.ctrace-run.yml";
  TraceRunSetup setup;
  setup.timestamps = TraceRunTimestampSetup{100000000U, 1U};
  setup.data = {
      TraceRunDataSetup{},
      TraceRunDataSetup{"unsigned int", 1U},
      TraceRunDataSetup{"signed int", 2U},
  };
  config.setups.push_back(std::move(setup));
  for (std::uint32_t comparator = 0U; comparator < 3U; ++comparator) {
    TraceRunReference reference;
    reference.ctraceRef = "opaque/dwt-" + std::to_string(comparator);
    reference.type = "dwt";
    reference.sources = {comparator};
    reference.dataSetupIndex = comparator;
    config.references.push_back(std::move(reference));
  }
  const auto meta = CtraceRunMeta::fromConfig(config);

  CliOptions allOptions;
  allOptions.outputFormat = OutputFormat::All;
  CollectingDiagnosticSink configurationDiagnostics;
  const auto outputPlan = planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("DwtSize.SWO.raw"),
                                           meta, configurationDiagnostics);
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
  const auto invalidSizePlan =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("DwtSize.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), invalidSizeDiagnostics);
  require(invalidSizePlan.csv.has_value() && !invalidSizePlan.ctf.has_value(),
          "invalid data.symbol-size must disable only CTF");
  require(invalidSizeDiagnostics.events().size() == 1U &&
              invalidSizeDiagnostics.events()[0].code == "ctf-dwt-symbol-size-invalid",
          "invalid CTF data.symbol-size diagnostic mismatch");
}

TEST(CtraceUnitTests, testOutputRequirementsAreBackendSpecific)
{
  auto config = backendRequirementsConfig();

  CliOptions csvOptions;
  csvOptions.outputFormat = OutputFormat::Csv;
  CollectingDiagnosticSink invalidTypeCsvDiagnostics;
  const auto invalidTypeCsv =
      planTraceOutputs(traceOutputRequest(csvOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), invalidTypeCsvDiagnostics);
  require(invalidTypeCsv.csv.has_value() && !invalidTypeCsv.ctf.has_value(),
          "an invalid explicit data.symbol-type must not disable CSV");
  require(invalidTypeCsvDiagnostics.events().empty(), "CSV must not inspect CTF-only data.symbol-type metadata");

  CliOptions allOptions;
  allOptions.outputFormat = OutputFormat::All;
  CollectingDiagnosticSink invalidTypeAllDiagnostics;
  const auto invalidTypeAll =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), invalidTypeAllDiagnostics);
  require(invalidTypeAll.csv.has_value() && !invalidTypeAll.ctf.has_value(),
          "an invalid explicit data.symbol-type must disable only CTF for --all");
  require(invalidTypeAllDiagnostics.events().size() == 1U &&
              invalidTypeAllDiagnostics.events()[0].code == "ctf-dwt-symbol-type-invalid",
          "invalid CTF data.symbol-type diagnostic mismatch");

  config.setups[0].data[0].symbolType.reset();
  config.setups[0].data[0].symbolSize.reset();
  CollectingDiagnosticSink missingTypeDiagnostics;
  const auto missingType =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), missingTypeDiagnostics);
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
  const auto malformedType =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), malformedTypeDiagnostics);
  require(malformedType.csv.has_value() && !malformedType.ctf.has_value(),
          "malformed data.symbol-type must disable only CTF");

  config.setups[0].data[0].symbolTypeError.reset();
  config.setups[0].data[0].symbolSizeError = "data.symbol-size must be unsigned";
  CollectingDiagnosticSink malformedSizeDiagnostics;
  const auto malformedSize =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), malformedSizeDiagnostics);
  require(malformedSize.csv.has_value() && !malformedSize.ctf.has_value() &&
              malformedSizeDiagnostics.events().size() == 1U &&
              malformedSizeDiagnostics.events()[0].code == "ctf-dwt-symbol-size-invalid",
          "malformed data.symbol-size must disable only CTF");
}

TEST(CtraceUnitTests, testCtfOutputRequiresAValidClock)
{
  auto config = backendRequirementsConfig();
  config.setups[0].data[0] = TraceRunDataSetup{};
  CliOptions allOptions;
  allOptions.outputFormat = OutputFormat::All;
  config.setups[0].timestamps->clockHz.reset();
  CollectingDiagnosticSink missingClockDiagnostics;
  const auto missingClock =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), missingClockDiagnostics);
  require(missingClock.csv.has_value() && !missingClock.ctf.has_value(),
          "missing timestamps.clock must disable only CTF");

  config.setups[0].timestamps->clockError = "timestamps.clock must be unsigned";
  CollectingDiagnosticSink malformedClockDiagnostics;
  const auto malformedClock =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), malformedClockDiagnostics);
  require(malformedClock.csv.has_value() && !malformedClock.ctf.has_value(),
          "malformed timestamps.clock must disable only CTF");

  config.setups[0].timestamps->clockError.reset();
  config.setups[0].timestamps->clockHz = 0U;
  CollectingDiagnosticSink zeroClockDiagnostics;
  const auto zeroClock =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), zeroClockDiagnostics);
  require(zeroClock.csv.has_value() && !zeroClock.ctf.has_value(), "zero timestamps.clock must disable only CTF");
}

TEST(CtraceUnitTests, testOutputRequirementsHonorFiltersAndCheckOnlyMode)
{
  const auto config = backendRequirementsConfig();
  CliOptions allOptions;
  allOptions.outputFormat = OutputFormat::All;
  allOptions.selection.types = {"itm"};
  CollectingDiagnosticSink filteredDiagnostics;
  const auto filtered =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), filteredDiagnostics);
  require(filtered.csv.has_value() && filtered.ctf.has_value(),
          "filtered-out DWT type metadata must not affect either backend");
  require(filteredDiagnostics.events().empty(), "filtered-out invalid DWT metadata must not produce diagnostics");

  CliOptions checkOnlyOptions;
  CollectingDiagnosticSink checkOnlyDiagnostics;
  const auto checkOnly =
      planTraceOutputs(traceOutputRequest(checkOnlyOptions), std::filesystem::path("BackendRequirements.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), checkOnlyDiagnostics);
  require(!checkOnly.hasRequestedOutputs() && checkOnlyDiagnostics.events().empty(),
          "check-only mode must not apply CSV or CTF metadata requirements");
}

TEST(CtraceUnitTests, testOutputPreflightRejectsAmbiguousRoutesForCtfOnly)
{
  TraceRunConfig config;
  config.path = "AmbiguousRoutes.ctrace-run.yml";
  TraceRunSetup setup;
  setup.timestamps = TraceRunTimestampSetup{400000000U, 1U};
  setup.data.push_back(TraceRunDataSetup{"unsigned int", 4U});
  config.setups.push_back(setup);

  TraceRunReference first;
  first.ctraceRef = "opaque/dwt-a";
  first.type = "dwt";
  first.stream = 1U;
  first.sources = {0U};
  first.dataSetupIndex = 0U;
  first.label = "core-one";
  TraceRunReference second = first;
  second.label = "core-two";
  config.references = {first, second};

  CliOptions allOptions;
  allOptions.outputFormat = OutputFormat::All;
  CollectingDiagnosticSink diagnostics;
  const auto plan =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("captures/AmbiguousRoutes.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), diagnostics);
  require(plan.csv.has_value() && !plan.ctf.has_value(),
          "conflicting CTF route labels must not disable independent CSV output");
  require(diagnostics.events().size() == 1U && diagnostics.events()[0].code == "ctf-trace-route-ambiguous",
          "CTF route ambiguity must be diagnosed during preflight");

  config.references[1].stream = 2U;
  config.references[1].label = "core-one";
  CollectingDiagnosticSink routeDiagnostics;
  const auto routePlan =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("captures/AmbiguousRoutes.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), routeDiagnostics);
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
  CliOptions processorCsvOptions;
  processorCsvOptions.outputFormat = OutputFormat::Csv;
  CollectingDiagnosticSink processorDiagnostics;
  const auto processorPlan = planTraceOutputs(traceOutputRequest(processorCsvOptions),
                                              std::filesystem::path("captures/AmbiguousProcessors.SWO.raw"),
                                              CtraceRunMeta::fromConfig(processorConfig), processorDiagnostics);
  require(processorPlan.csv.has_value() && processorDiagnostics.events().empty(),
          "CSV must preserve raw stream/source values without consuming processor metadata");

  allOptions.selection.streams = {1U};
  CollectingDiagnosticSink selectedDiagnostics;
  const auto selectedPlan =
      planTraceOutputs(traceOutputRequest(allOptions), std::filesystem::path("captures/AmbiguousRoutes.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), selectedDiagnostics);
  require(selectedPlan.csv.has_value() && selectedPlan.ctf.has_value() && selectedPlan.ctf->sources.size() == 1U &&
              selectedPlan.ctf->sources[0].label == std::optional<std::string>("core-one"),
          "an explicit stream selection must produce one resolved CTF route");
  require(selectedDiagnostics.events().empty(), "an unambiguous selected route must not produce preflight diagnostics");

  config.setups[0].data.push_back(TraceRunDataSetup{"unsigned int", 2U});
  config.references[1].stream = 1U;
  config.references[1].ctraceRef = "opaque/dwt-b";
  config.references[1].dataSetupIndex = 1U;
  CliOptions csvOptions;
  csvOptions.outputFormat = OutputFormat::Csv;
  CollectingDiagnosticSink sizeDiagnostics;
  const auto csvPlan =
      planTraceOutputs(traceOutputRequest(csvOptions), std::filesystem::path("captures/AmbiguousRoutes.SWO.raw"),
                       CtraceRunMeta::fromConfig(config), sizeDiagnostics);
  require(csvPlan.csv.has_value(), "CTF-only data.symbol-size metadata must not disable CSV");
  require(sizeDiagnostics.events().empty(), "CSV must not inspect CTF-only data.symbol-size metadata");
}

TEST(CtraceUnitTests, testOutputRequirementsValidateDefaultClockWithoutRoutes)
{
  CliOptions options;
  options.outputFormat = OutputFormat::Ctf;
  TraceRunConfig config;
  config.path = "Clock.ctrace-run.yml";
  TraceRunSetup setup;
  setup.timestamps = TraceRunTimestampSetup{};
  config.setups.push_back(setup);

  CollectingDiagnosticSink missingDiagnostics;
  auto plan = planTraceOutputs(traceOutputRequest(options), "Clock.SWO.raw", CtraceRunMeta::fromConfig(config),
                               missingDiagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  ASSERT_EQ(missingDiagnostics.events().size(), 1U);
  EXPECT_EQ(missingDiagnostics.events()[0].code, "ctf-timestamp-clock-missing");

  config.setups[0].timestamps->clockError = "clock must be an unsigned integer";
  CollectingDiagnosticSink malformedDiagnostics;
  plan = planTraceOutputs(traceOutputRequest(options), "Clock.SWO.raw", CtraceRunMeta::fromConfig(config),
                          malformedDiagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  ASSERT_EQ(malformedDiagnostics.events().size(), 1U);
  EXPECT_EQ(malformedDiagnostics.events()[0].code, "ctf-timestamp-clock-invalid");

  config.setups[0].timestamps->clockError.reset();
  config.setups[0].timestamps->clockHz = 0U;
  CollectingDiagnosticSink zeroDiagnostics;
  plan = planTraceOutputs(traceOutputRequest(options), "Clock.SWO.raw", CtraceRunMeta::fromConfig(config),
                          zeroDiagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  ASSERT_EQ(zeroDiagnostics.events().size(), 1U);
  EXPECT_EQ(zeroDiagnostics.events()[0].code, "ctf-timestamp-clock-invalid");
}

TEST(CtraceUnitTests, testOutputRequirementsRejectUnknownStreamWithMultipleClocks)
{
  TraceRunConfig config;
  config.path = "Multicore.ctrace-run.yml";
  TraceRunSetup first;
  first.processorName = "first";
  first.timestamps = TraceRunTimestampSetup{100U, 1U};
  TraceRunSetup second;
  second.processorName = "second";
  second.timestamps = TraceRunTimestampSetup{200U, 1U};
  config.setups = {first, second};

  TraceRunReference firstRoute;
  firstRoute.type = "itm";
  firstRoute.ctraceRef = "first/itm";
  firstRoute.processorName = "first";
  firstRoute.stream = 1U;
  firstRoute.sources = {1U};
  TraceRunReference secondRoute = firstRoute;
  secondRoute.ctraceRef = "second/itm";
  secondRoute.processorName = "second";
  secondRoute.stream = 2U;
  config.references = {firstRoute, secondRoute};

  CliOptions options;
  options.outputFormat = OutputFormat::Ctf;
  options.selection.streams = {99U};
  CollectingDiagnosticSink diagnostics;
  const auto plan = planTraceOutputs(traceOutputRequest(options), "Multicore.SWO.raw",
                                     CtraceRunMeta::fromConfig(config), diagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_EQ(diagnostics.events()[0].code, "ctf-timestamp-clock-ambiguous");
}

TEST(CtraceUnitTests, testOutputRequirementsRejectsInputWithoutArtifactName)
{
  CliOptions options;
  options.outputFormat = OutputFormat::Csv;
  CollectingDiagnosticSink diagnostics;
  EXPECT_THROW((void)planTraceOutputs(traceOutputRequest(options), {}, CtraceRunMeta::fromConfig({}), diagnostics),
               std::runtime_error);
}
