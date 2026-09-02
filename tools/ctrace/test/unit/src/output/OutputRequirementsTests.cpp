/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"
#include "TraceRunTestSupport.h"
#include <gtest/gtest.h>
#include "CtraceRunMeta.h"
#include "OutputRequirements.h"
#include "TraceOutputConfig.h"
#include "TraceRunConfig.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

/** @brief Creates an output request selecting test formats. */
static TraceOutputRequest outputRequest(bool csv, bool ctf)
{
  return {csv, ctf, {}};
}

/** @brief Plans outputs using explicitly supplied trace metadata. */
static TraceOutputPlan planOutputs(const TraceOutputRequest& request, const std::filesystem::path& rawInputPath,
                                   const CtraceRunMeta& meta, DiagnosticSink& diagnostics)
{
  return planTraceOutputs(request, rawInputPath, meta, diagnostics);
}

/** @brief Plans outputs after normalizing a trace-run configuration. */
static TraceOutputPlan planOutputs(const TraceOutputRequest& request, const std::filesystem::path& rawInputPath,
                                   const TraceRunConfig& config, DiagnosticSink& diagnostics)
{
  return planOutputs(request, rawInputPath, CtraceRunMeta::fromConfig(config), diagnostics);
}

/** @brief Creates metadata satisfying the default backend requirements. */
static TraceRunConfig backendRequirementsConfig()
{
  TraceRunConfig config;
  config.path = "BackendRequirements.ctrace-run.yml";
  auto setup = TraceRunTestSupport::makeTimestampSetup(std::nullopt, 400000000U, 1U);
  setup.data.push_back(TraceRunDataSetup{4U});
  config.setups.push_back(setup);
  auto reference = TraceRunTestSupport::makeReference("dwt", std::nullopt, std::nullopt, {0U}, "opaque/dwt");
  reference.dataSetupIndex = 0U;
  reference.dataType = "double";
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
  ASSERT_TRUE(missingPrescalerMeta.timestampPrescaler() ==
              std::optional<std::uint32_t>(TraceRunSchema::kDefaultTimestampPrescaler))
      << "an omitted processor prescaler must resolve to the specified default";
  ASSERT_TRUE((missingPrescalerMeta.timestampPrescalersByTraceBusId() ==
               std::map<std::uint8_t, std::uint32_t>({{1U, 1U}, {2U, 1U}})))
      << "the default prescaler must be retained per ATB stream";

  core0.timestamps = TraceRunTimestampSetup{400000000U, 4U};
  multicore.setups = {core0, core1};
  const auto mixedMissingPrescalerMeta = CtraceRunMeta::fromConfig(multicore);
  ASSERT_TRUE(mixedMissingPrescalerMeta.hasDistinctProcessorPrescalers())
      << "an explicit prescaler must be compared with another processor's default";
  ASSERT_TRUE((mixedMissingPrescalerMeta.timestampPrescalersByTraceBusId() ==
               std::map<std::uint8_t, std::uint32_t>({{1U, 4U}, {2U, 1U}})))
      << "different explicit and default prescalers must remain stream-specific";

  core1.timestamps = TraceRunTimestampSetup{200000000U, 1U};
  multicore.setups = {core0, core1};
  const auto distinctClockMeta = CtraceRunMeta::fromConfig(multicore);
  ASSERT_TRUE(distinctClockMeta.timestampsByTraceBusId().at(1U).clockHz == std::optional<std::uint64_t>(400000000U) &&
              distinctClockMeta.timestampsByTraceBusId().at(2U).clockHz == std::optional<std::uint64_t>(200000000U))
      << "processor clocks must remain associated with their Trace Bus IDs";

  auto ctfRequest = outputRequest(false, true);
  CollectingDiagnosticSink allClockDiagnostics;
  const auto allClockPlan =
      planOutputs(ctfRequest, "captures/Multicore.SWO.raw", distinctClockMeta, allClockDiagnostics);
  ASSERT_TRUE(!allClockPlan.ctf.has_value()) << "CTF must reject selected Trace Bus IDs with different clocks";
  allClockDiagnostics.singleEvent();

  ctfRequest.selection.streams = {2U};
  CollectingDiagnosticSink selectedClockDiagnostics;
  const auto selectedClockPlan =
      planOutputs(ctfRequest, "captures/Multicore.SWO.raw", distinctClockMeta, selectedClockDiagnostics);
  ASSERT_TRUE(selectedClockPlan.ctf.has_value() && selectedClockPlan.ctf->coreClockHz == 200000000U &&
              selectedClockDiagnostics.events().empty())
      << "a selected Trace Bus ID must use its processor's clock";
}

TEST(CtraceUnitTests, testDwtDataMetadataDefaultsAndValidation)
{
  TraceRunConfig config;
  config.path = "DwtSize.ctrace-run.yml";
  auto setup = TraceRunTestSupport::makeTimestampSetup(std::nullopt, 100000000U, 1U);
  setup.data = {
      TraceRunDataSetup{},
      TraceRunDataSetup{1U},
      TraceRunDataSetup{2U},
  };
  config.setups.push_back(std::move(setup));
  for (std::uint32_t comparator = 0U; comparator < 3U; ++comparator) {
    auto reference = TraceRunTestSupport::makeReference("dwt", std::nullopt, std::nullopt, {comparator},
                                                        "opaque/dwt-" + std::to_string(comparator));
    reference.dataSetupIndex = comparator;
    if (comparator == 2U) {
      reference.dataType = "signed";
    }
    config.references.push_back(std::move(reference));
  }
  const auto meta = CtraceRunMeta::fromConfig(config);

  const auto allRequest = outputRequest(true, true);
  CollectingDiagnosticSink configurationDiagnostics;
  const auto outputPlan = planOutputs(allRequest, "DwtSize.SWO.raw", meta, configurationDiagnostics);
  ASSERT_TRUE(outputPlan.csv.has_value() && outputPlan.ctf.has_value())
      << "valid or missing DWT metadata must not disable CSV or CTF";
  ASSERT_TRUE(configurationDiagnostics.events().empty())
      << "valid or missing DWT metadata must not produce diagnostics";
  ASSERT_TRUE(outputPlan.ctf->sources.size() == 3U && outputPlan.ctf->sources[0].dataType == "unsigned" &&
              outputPlan.ctf->sources[0].dataSize == 4U && outputPlan.ctf->sources[1].dataType == "unsigned" &&
              outputPlan.ctf->sources[1].dataSize == 1U && outputPlan.ctf->sources[2].dataType == "signed" &&
              outputPlan.ctf->sources[2].dataSize == 2U)
      << "CTF data-type/size defaults or explicit values mismatch";

  config.setups[0].data[1].size = 0U;
  CollectingDiagnosticSink invalidSizeDiagnostics;
  const auto invalidSizePlan = planOutputs(allRequest, "DwtSize.SWO.raw", config, invalidSizeDiagnostics);
  ASSERT_TRUE(invalidSizePlan.csv.has_value() && !invalidSizePlan.ctf.has_value())
      << "invalid ctrace-setup data.size must disable only CTF";
  invalidSizeDiagnostics.singleEvent();
}

TEST(CtraceUnitTests, testOutputRequirementsAreBackendSpecific)
{
  auto config = backendRequirementsConfig();

  const auto csvRequest = outputRequest(true, false);
  CollectingDiagnosticSink invalidTypeCsvDiagnostics;
  const auto invalidTypeCsv = planOutputs(csvRequest, "BackendRequirements.SWO.raw", config, invalidTypeCsvDiagnostics);
  ASSERT_TRUE(invalidTypeCsv.csv.has_value() && !invalidTypeCsv.ctf.has_value())
      << "an invalid explicit data-type must not disable CSV";
  ASSERT_TRUE(invalidTypeCsvDiagnostics.events().empty()) << "CSV must not inspect CTF-only data-type metadata";

  const auto allRequest = outputRequest(true, true);
  CollectingDiagnosticSink invalidTypeAllDiagnostics;
  const auto invalidTypeAll = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, invalidTypeAllDiagnostics);
  ASSERT_TRUE(invalidTypeAll.csv.has_value() && !invalidTypeAll.ctf.has_value())
      << "an invalid explicit data-type must disable only CTF for --all";
  EXPECT_EQ(invalidTypeAllDiagnostics.singleEvent().message,
            "CTF output cannot use ctrace-run data-type 'double'; supported data-type values are 'unsigned', "
            "'signed', and 'float'; size must be 1, 2, or 4, and float requires size 4");
  EXPECT_TRUE(invalidTypeAllDiagnostics.containsContext("backend", "ctf"));

  config.references[0].dataType.reset();
  CollectingDiagnosticSink missingTypeDiagnostics;
  const auto missingType = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, missingTypeDiagnostics);
  ASSERT_TRUE(missingType.csv.has_value() && missingType.ctf.has_value())
      << "missing data-type must use the CTF default and leave CSV enabled";
  ASSERT_TRUE(
      (missingType.csv->outputPath == std::filesystem::path("BackendRequirements.SWO.csv") &&
       missingType.ctf->outputDirectory == std::filesystem::path("BackendRequirements.ctf") &&
       missingType.ctf->traceCompassXmlPath == std::filesystem::path("BackendRequirements.SWO.traceanalysis.xml") &&
       missingType.ctf->coreClockHz == 400000000U && missingType.ctf->sources.size() == 1U &&
       missingType.ctf->sources[0].dataType == "unsigned" && missingType.ctf->sources[0].dataSize == 4U))
      << "output preflight must resolve artifact paths, clock, routes, and defaults";
  ASSERT_TRUE(missingTypeDiagnostics.events().empty())
      << "missing optional data-type must not produce diagnostics";

  config.references[0].dataTypeError = "data-type must be scalar";
  CollectingDiagnosticSink malformedTypeDiagnostics;
  const auto malformedType = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, malformedTypeDiagnostics);
  ASSERT_TRUE(malformedType.csv.has_value() && !malformedType.ctf.has_value())
      << "malformed data-type must disable only CTF";

  config.references[0].dataTypeError.reset();
  config.references[0].dataSizeError = "size must be unsigned";
  CollectingDiagnosticSink malformedSizeDiagnostics;
  const auto malformedSize = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, malformedSizeDiagnostics);
  ASSERT_TRUE(malformedSize.csv.has_value() && !malformedSize.ctf.has_value())
      << "malformed size must disable only CTF";
  malformedSizeDiagnostics.singleEvent();

  config.references[0].dataSizeError.reset();
  config.references[0].dataType = "signed";
  config.references[0].dataSize = 1U;
  CollectingDiagnosticSink currentMetadataDiagnostics;
  const auto currentMetadata = planOutputs(allRequest, "BackendRequirements.SWO.raw", config,
                                           currentMetadataDiagnostics);
  ASSERT_TRUE(currentMetadata.ctf.has_value() && currentMetadata.ctf->sources[0].dataType == "signed" &&
              currentMetadata.ctf->sources[0].dataSize == 1U)
      << "reference data-type/size must be retained for CTF";
  ASSERT_TRUE(currentMetadataDiagnostics.events().empty());

  config.references[0].addressError = "address must be unsigned";
  CollectingDiagnosticSink malformedAddressDiagnostics;
  const auto malformedAddress = planOutputs(allRequest, "BackendRequirements.SWO.raw", config,
                                            malformedAddressDiagnostics);
  ASSERT_TRUE(malformedAddress.csv.has_value() && !malformedAddress.ctf.has_value())
      << "malformed address must disable only CTF";
  malformedAddressDiagnostics.singleEvent();
}

TEST(CtraceUnitTests, testCtfOutputRequiresAValidClock)
{
  auto config = backendRequirementsConfig();
  config.setups[0].data[0] = TraceRunDataSetup{};
  const auto allRequest = outputRequest(true, true);
  config.setups[0].timestamps->clockHz.reset();
  CollectingDiagnosticSink missingClockDiagnostics;
  const auto missingClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, missingClockDiagnostics);
  ASSERT_TRUE(missingClock.csv.has_value() && !missingClock.ctf.has_value())
      << "missing timestamps.clock must disable only CTF";

  config.setups[0].timestamps->clockError = "timestamps.clock must be unsigned";
  CollectingDiagnosticSink malformedClockDiagnostics;
  const auto malformedClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, malformedClockDiagnostics);
  ASSERT_TRUE(malformedClock.csv.has_value() && !malformedClock.ctf.has_value())
      << "malformed timestamps.clock must disable only CTF";

  config.setups[0].timestamps->clockError.reset();
  config.setups[0].timestamps->clockHz = 0U;
  CollectingDiagnosticSink zeroClockDiagnostics;
  const auto zeroClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, zeroClockDiagnostics);
  ASSERT_TRUE(zeroClock.csv.has_value() && !zeroClock.ctf.has_value()) << "zero timestamps.clock must disable only CTF";
}

TEST(CtraceUnitTests, testOutputRequirementsHonorFiltersAndCheckOnlyMode)
{
  const auto config = backendRequirementsConfig();
  auto allRequest = outputRequest(true, true);
  allRequest.selection.types = {"itm"};
  CollectingDiagnosticSink filteredDiagnostics;
  const auto filtered = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, filteredDiagnostics);
  ASSERT_TRUE(filtered.csv.has_value() && filtered.ctf.has_value())
      << "filtered-out DWT type metadata must not affect either backend";
  ASSERT_TRUE(filteredDiagnostics.events().empty()) << "filtered-out invalid DWT metadata must not produce diagnostics";

  CollectingDiagnosticSink checkOnlyDiagnostics;
  const auto checkOnly =
      planOutputs(outputRequest(false, false), "BackendRequirements.SWO.raw", config, checkOnlyDiagnostics);
  ASSERT_TRUE(!checkOnly.hasRequestedOutputs() && checkOnlyDiagnostics.events().empty())
      << "check-only mode must not apply CSV or CTF metadata requirements";
}

TEST(CtraceUnitTests, testOutputPreflightRejectsAmbiguousRoutesForCtfOnly)
{
  TraceRunConfig config;
  config.path = "AmbiguousRoutes.ctrace-run.yml";
  auto setup = TraceRunTestSupport::makeTimestampSetup(std::nullopt, 400000000U, 1U);
  setup.data.push_back(TraceRunDataSetup{4U});
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
  ASSERT_TRUE(plan.csv.has_value() && !plan.ctf.has_value())
      << "conflicting CTF route labels must not disable independent CSV output";
  diagnostics.singleEvent();

  config.references[1].stream = 2U;
  config.references[1].label = "core-one";
  CollectingDiagnosticSink routeDiagnostics;
  const auto routePlan = planOutputs(allRequest, "captures/AmbiguousRoutes.SWO.raw", config, routeDiagnostics);
  ASSERT_TRUE(routePlan.csv.has_value() && routePlan.ctf.has_value() && routePlan.ctf->sources.size() == 2U)
      << "CTF must retain equivalent routes with distinct Trace Bus IDs";
  ASSERT_TRUE(routeDiagnostics.events().empty())
      << "equivalent CTF metadata on distinct Trace Bus IDs must not be ambiguous";

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
  ASSERT_TRUE(processorPlan.csv.has_value() && processorDiagnostics.events().empty())
      << "CSV must preserve raw stream/source values without consuming processor metadata";

  allRequest.selection.streams = {1U};
  CollectingDiagnosticSink selectedDiagnostics;
  const auto selectedPlan = planOutputs(allRequest, "captures/AmbiguousRoutes.SWO.raw", config, selectedDiagnostics);
  ASSERT_TRUE(selectedPlan.csv.has_value() && selectedPlan.ctf.has_value() && selectedPlan.ctf->sources.size() == 1U &&
              selectedPlan.ctf->sources[0].label == std::optional<std::string>("core-one"))
      << "an explicit stream selection must produce one resolved CTF route";
  ASSERT_TRUE(selectedDiagnostics.events().empty())
      << "an unambiguous selected route must not produce preflight diagnostics";

  config.setups[0].data.push_back(TraceRunDataSetup{2U});
  config.references[1].stream = 1U;
  config.references[1].ctraceRef = "opaque/dwt-b";
  config.references[1].dataSetupIndex = 1U;
  CollectingDiagnosticSink sizeDiagnostics;
  const auto csvPlan =
      planOutputs(outputRequest(true, false), "captures/AmbiguousRoutes.SWO.raw", config, sizeDiagnostics);
  ASSERT_TRUE(csvPlan.csv.has_value()) << "CTF-only data.size metadata must not disable CSV";
  ASSERT_TRUE(sizeDiagnostics.events().empty()) << "CSV must not inspect CTF-only data.size metadata";
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
  missingDiagnostics.singleEvent();

  config.setups[0].timestamps->clockError = "clock must be an unsigned integer";
  CollectingDiagnosticSink malformedDiagnostics;
  plan = planOutputs(ctfRequest, "Clock.SWO.raw", config, malformedDiagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  malformedDiagnostics.singleEvent();

  config.setups[0].timestamps->clockError.reset();
  config.setups[0].timestamps->clockHz = 0U;
  CollectingDiagnosticSink zeroDiagnostics;
  plan = planOutputs(ctfRequest, "Clock.SWO.raw", config, zeroDiagnostics);
  ASSERT_FALSE(plan.ctf.has_value());
  zeroDiagnostics.singleEvent();
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
  diagnostics.singleEvent();
}

TEST(CtraceUnitTests, testOutputRequirementsRejectsInputWithoutArtifactName)
{
  CollectingDiagnosticSink diagnostics;
  EXPECT_THROW((void)planTraceOutputs(outputRequest(true, false), {}, CtraceRunMeta::fromConfig({}), diagnostics),
               std::runtime_error);
}
