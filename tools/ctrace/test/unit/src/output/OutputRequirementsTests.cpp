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
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

/** @brief Counts source metadata directly from canonical normalized routes. */
static std::size_t sourceCount(const CtraceRunMeta& meta)
{
  std::size_t count = 0U;
  for (const auto& route : meta.routes()) {
    count += route.sources.size();
  }
  return count;
}

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

/** @brief Finds one configured CTF stream class by its public numeric ID. */
static const CtfStreamDescriptor* findCtfStream(const CtfOutputConfig& config, std::uint32_t streamClassId)
{
  const auto found = std::find_if(
      config.metadata.streams.begin(), config.metadata.streams.end(),
      [&](const CtfStreamDescriptor& stream) { return stream.streamClassId == CtfStreamClassId{streamClassId}; });
  return found == config.metadata.streams.end() ? nullptr : &*found;
}

/** @brief Resolves the clock descriptor referenced by one configured CTF stream. */
static const CtfClockDomainDescriptor* clockForStream(const CtfOutputConfig& config, std::uint32_t streamClassId)
{
  const auto* stream = findCtfStream(config, streamClassId);
  if (stream == nullptr) {
    return nullptr;
  }
  const auto found =
      std::find_if(config.metadata.clockDomains.begin(), config.metadata.clockDomains.end(),
                   [&](const CtfClockDomainDescriptor& clock) { return clock.id == stream->clockDomainId; });
  return found == config.metadata.clockDomains.end() ? nullptr : &*found;
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

TEST(CtraceUnitTests, testOutputPathsRetainChannelAndCompleteSolutionSetName)
{
  auto config = backendRequirementsConfig();
  config.references.front().dataType.reset();
  for (const auto* solutionSet : {"Board", "Blinky.v2+Board.Debug"}) {
    for (const auto* channel : {"SWO", "TB", "TB_ETB"}) {
      const auto capture = std::string(solutionSet) + "." + channel;
      SCOPED_TRACE(capture);
      CollectingDiagnosticSink diagnostics;
      const auto plan =
          planOutputs(outputRequest(true, true), std::filesystem::path("captures") / (capture + ".raw"), config,
                      diagnostics);
      ASSERT_TRUE(plan.csv.has_value());
      ASSERT_TRUE(plan.ctf.has_value());
      EXPECT_EQ(plan.csv->outputPath, std::filesystem::path("captures") / (capture + ".csv"));
      EXPECT_EQ(plan.ctf->outputDirectory, std::filesystem::path("captures") / (capture + ".ctf"));
      EXPECT_EQ(plan.ctf->traceCompassXmlPath,
                std::filesystem::path("captures") / (capture + ".traceanalysis.xml"));
      EXPECT_TRUE(diagnostics.events().empty());
    }
  }
}

TEST(CtraceUnitTests, testBackendRequirementsUsePerStreamMetadata)
{
  TraceRunConfig multicore;
  multicore.path = "Multicore.ctrace-run.yml";
  multicore.traceFormat = TraceRunFormat::Formatted;
  auto core0 = TraceRunTestSupport::makeTimestampSetup("core0", 400000000U, 1U);
  auto core1 = TraceRunTestSupport::makeTimestampSetup("core1", 400000000U, 4U);
  multicore.setups = {core0, core1};

  const auto core0Itm = TraceRunTestSupport::makeReference("itm", "core0", 1U, {1U}, "core0/itm");
  const auto core1Itm = TraceRunTestSupport::makeReference("itm", "core1", 2U, {1U}, "core1/itm");
  multicore.references = {core0Itm, core1Itm};

  core1.timestamps = TraceRunTimestampSetup{400000000U, std::nullopt};
  multicore.setups = {core0, core1};
  const auto missingPrescalerMeta = CtraceRunMeta::fromConfig(multicore);
  ASSERT_EQ(missingPrescalerMeta.routes().size(), 2U);
  ASSERT_EQ(missingPrescalerMeta.routes()[0].timestampPrescaler, TraceRunSchema::kDefaultTimestampPrescaler);
  ASSERT_EQ(missingPrescalerMeta.routes()[1].timestampPrescaler, TraceRunSchema::kDefaultTimestampPrescaler)
      << "the default prescaler must be retained per ATB stream";

  CollectingDiagnosticSink equalClockDiagnostics;
  const auto equalClockPlan = planOutputs(outputRequest(false, true), "captures/Multicore.SWO.raw",
                                          missingPrescalerMeta, equalClockDiagnostics);
  ASSERT_TRUE(equalClockPlan.ctf.has_value());
  ASSERT_EQ(equalClockPlan.ctf->metadata.streams.size(), 2U);
  ASSERT_EQ(equalClockPlan.ctf->metadata.clockDomains.size(), 2U)
      << "equal frequency must not merge independent processor clock domains";
  const auto* equalCore0Clock = clockForStream(*equalClockPlan.ctf, 1U);
  const auto* equalCore1Clock = clockForStream(*equalClockPlan.ctf, 2U);
  ASSERT_NE(equalCore0Clock, nullptr);
  ASSERT_NE(equalCore1Clock, nullptr);
  ASSERT_TRUE(equalCore0Clock->uuid.has_value());
  ASSERT_TRUE(equalCore1Clock->uuid.has_value());
  EXPECT_NE(equalCore0Clock->id, equalCore1Clock->id);
  EXPECT_NE(equalCore0Clock->uuid, equalCore1Clock->uuid);
  EXPECT_EQ(equalCore0Clock->frequencyHz, 400000000U);
  EXPECT_EQ(equalCore1Clock->frequencyHz, 400000000U);
  EXPECT_TRUE(equalClockDiagnostics.events().empty());

  core0.timestamps = TraceRunTimestampSetup{400000000U, 4U};
  multicore.setups = {core0, core1};
  const auto mixedMissingPrescalerMeta = CtraceRunMeta::fromConfig(multicore);
  ASSERT_EQ(mixedMissingPrescalerMeta.routes().size(), 2U);
  ASSERT_EQ(mixedMissingPrescalerMeta.routes()[0].timestampPrescaler, 4U);
  ASSERT_EQ(mixedMissingPrescalerMeta.routes()[1].timestampPrescaler, TraceRunSchema::kDefaultTimestampPrescaler)
      << "different explicit and default prescalers must remain stream-specific";

  core1.timestamps = TraceRunTimestampSetup{200000000U, 1U};
  multicore.setups = {core0, core1};
  const auto distinctClockMeta = CtraceRunMeta::fromConfig(multicore);
  ASSERT_TRUE(distinctClockMeta.routes()[0].timestampClockHz == std::optional<std::uint64_t>(400000000U) &&
              distinctClockMeta.routes()[1].timestampClockHz == std::optional<std::uint64_t>(200000000U))
      << "processor clocks must remain associated with their Trace Bus IDs";

  auto ctfRequest = outputRequest(false, true);
  CollectingDiagnosticSink allClockDiagnostics;
  const auto allClockPlan =
      planOutputs(ctfRequest, "captures/Multicore.SWO.raw", distinctClockMeta, allClockDiagnostics);
  ASSERT_TRUE(allClockPlan.ctf.has_value()) << "independent CTF clock domains may use different frequencies";
  ASSERT_EQ(allClockPlan.ctf->metadata.streams.size(), 2U);
  ASSERT_EQ(allClockPlan.ctf->metadata.clockDomains.size(), 2U);
  const auto* core0Clock = clockForStream(*allClockPlan.ctf, 1U);
  const auto* core1Clock = clockForStream(*allClockPlan.ctf, 2U);
  ASSERT_NE(core0Clock, nullptr);
  ASSERT_NE(core1Clock, nullptr);
  EXPECT_EQ(core0Clock->frequencyHz, 400000000U);
  EXPECT_EQ(core1Clock->frequencyHz, 200000000U);
  EXPECT_NE(core0Clock->id, core1Clock->id);
  EXPECT_TRUE(allClockDiagnostics.events().empty());

  ctfRequest.selection.streams = {2U};
  CollectingDiagnosticSink selectedClockDiagnostics;
  const auto selectedClockPlan =
      planOutputs(ctfRequest, "captures/Multicore.SWO.raw", distinctClockMeta, selectedClockDiagnostics);
  ASSERT_TRUE(selectedClockPlan.ctf.has_value());
  ASSERT_EQ(selectedClockPlan.ctf->metadata.streams.size(), 1U);
  ASSERT_EQ(selectedClockPlan.ctf->metadata.clockDomains.size(), 1U);
  const auto* selectedClock = clockForStream(*selectedClockPlan.ctf, 2U);
  ASSERT_NE(selectedClock, nullptr);
  ASSERT_TRUE(selectedClock->uuid.has_value());
  EXPECT_EQ(selectedClock->frequencyHz, 200000000U);
  EXPECT_EQ(selectedClockPlan.ctf->routes.size(), 2U)
      << "the normalized route catalogue remains authoritative beyond the output filter";
  ASSERT_TRUE(selectedClockDiagnostics.events().empty()) << "a selected Trace Bus ID must use its processor's clock";
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
  const auto& sources = outputPlan.ctf->metadata.sources;
  ASSERT_TRUE(sources.size() == 3U && sources[0].dataType == "unsigned" && sources[0].dataSize == 4U &&
              sources[1].dataType == "unsigned" && sources[1].dataSize == 1U && sources[2].dataType == "signed" &&
              sources[2].dataSize == 2U)
      << "CTF data-type/size defaults or explicit values mismatch";

  config.setups[0].data[1].size = 0U;
  CollectingDiagnosticSink invalidSizeDiagnostics;
  const auto invalidSizePlan = planOutputs(allRequest, "DwtSize.SWO.raw", config, invalidSizeDiagnostics);
  ASSERT_TRUE(invalidSizePlan.csv.has_value() && !invalidSizePlan.ctf.has_value())
      << "invalid ctrace-setup data.size must disable only CTF";
  invalidSizeDiagnostics.singleEvent();
}

TEST(CtraceUnitTests, testOutputRequirementsValidateDwtAddressRangeForCtfOnly)
{
  auto config = backendRequirementsConfig();
  config.references[0].dataType = "unsigned";
  const auto maximumAddress = std::numeric_limits<std::uint64_t>::max();
  config.references[0].address = maximumAddress - 3U;

  const auto allRequest = outputRequest(true, true);
  CollectingDiagnosticSink maximumRangeDiagnostics;
  const auto maximumRange = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, maximumRangeDiagnostics);
  ASSERT_TRUE(maximumRange.csv.has_value() && maximumRange.ctf.has_value())
      << "a DWT size-4 range ending exactly at UINT64_MAX must remain representable";
  ASSERT_EQ(maximumRange.ctf->metadata.sources.size(), 1U);
  EXPECT_EQ(maximumRange.ctf->metadata.sources.front().address, std::optional<std::uint64_t>(maximumAddress - 3U));
  EXPECT_TRUE(maximumRangeDiagnostics.events().empty());

  config.references[0].address = maximumAddress - 2U;
  CollectingDiagnosticSink overflowDiagnostics;
  const auto overflow = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, overflowDiagnostics);
  ASSERT_TRUE(overflow.csv.has_value() && !overflow.ctf.has_value())
      << "an overflowing DWT address range must disable only CTF for --all";
  EXPECT_EQ(overflowDiagnostics.singleEvent().message, "CTF output cannot represent the configured DWT address range");
  EXPECT_TRUE(overflowDiagnostics.containsContext("backend", "ctf"));
  EXPECT_TRUE(overflowDiagnostics.containsContext("address", std::to_string(maximumAddress - 2U)));
  EXPECT_TRUE(overflowDiagnostics.containsContext("dataSize", "4"));
  EXPECT_GT(overflowDiagnostics.failureCount(), 0U);
}

TEST(CtraceUnitTests, testOutputRequirementsRejectDwtComparatorOutsideCtfDomainOnly)
{
  auto config = backendRequirementsConfig();
  config.references[0].sources = {4U};
  config.references[0].dataType = "unsigned";

  CollectingDiagnosticSink diagnostics;
  const auto plan = planOutputs(outputRequest(true, true), "BackendRequirements.SWO.raw", config, diagnostics);
  ASSERT_TRUE(plan.csv.has_value() && !plan.ctf.has_value())
      << "a DWT comparator outside the CTF schema must disable only CTF for --all";
  EXPECT_EQ(diagnostics.singleEvent().message, "CTF output requires DWT comparator sources between 0 and 3");
  EXPECT_TRUE(diagnostics.containsContext("backend", "ctf"));
  EXPECT_TRUE(diagnostics.containsContext("channel", "DWT4"));
  EXPECT_GT(diagnostics.failureCount(), 0U);
}

TEST(CtraceUnitTests, testOutputRequirementsReportRepeatedRouteSourceConflictOnce)
{
  auto config = backendRequirementsConfig();
  auto first = config.references.front();
  first.dataType = "unsigned";
  auto second = first;
  second.dataSize = 4U;
  second.dataSizeError = "second invalid size";
  auto third = first;
  third.dataSize = 4U;
  third.dataSizeError = "third invalid size";
  config.references = {first, second, third};

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(sourceCount(meta), 3U)
      << "normalization must retain conflicting route-local source metadata for output-specific validation";
  CollectingDiagnosticSink diagnostics;
  const auto plan = planOutputs(outputRequest(true, true), "BackendRequirements.SWO.raw", meta, diagnostics);
  ASSERT_TRUE(plan.csv.has_value() && !plan.ctf.has_value())
      << "conflicting CTF-only source metadata must not disable CSV for --all";
  const auto conflictCount =
      std::count_if(diagnostics.events().begin(), diagnostics.events().end(), [](const auto& event) {
        return event.message ==
               "CTF metadata cannot describe conflicting active metadata for one route/type/source key";
      });
  EXPECT_EQ(conflictCount, 1U) << "repeated conflicts for one route/type/source key must be diagnosed exactly once";
  EXPECT_EQ(diagnostics.events().size(), 3U) << "each malformed source must retain its own targeted size diagnostic";
  EXPECT_TRUE(diagnostics.containsContext("backend", "ctf"));
  EXPECT_TRUE(diagnostics.containsContext("channel", "DWT0"));
  EXPECT_GT(diagnostics.failureCount(), 0U);
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
       missingType.ctf->outputDirectory == std::filesystem::path("BackendRequirements.SWO.ctf") &&
       missingType.ctf->traceCompassXmlPath == std::filesystem::path("BackendRequirements.SWO.traceanalysis.xml") &&
       missingType.ctf->metadata.clockDomains.size() == 1U &&
       missingType.ctf->metadata.clockDomains[0].frequencyHz == 400000000U &&
       missingType.ctf->metadata.sources.size() == 1U && missingType.ctf->metadata.sources[0].dataType == "unsigned" &&
       missingType.ctf->metadata.sources[0].dataSize == 4U))
      << "output preflight must resolve artifact paths, clock, routes, and defaults";
  ASSERT_TRUE(missingTypeDiagnostics.events().empty()) << "missing optional data-type must not produce diagnostics";

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
  const auto currentMetadata =
      planOutputs(allRequest, "BackendRequirements.SWO.raw", config, currentMetadataDiagnostics);
  ASSERT_TRUE(currentMetadata.ctf.has_value() && currentMetadata.ctf->metadata.sources[0].dataType == "signed" &&
              currentMetadata.ctf->metadata.sources[0].dataSize == 1U)
      << "reference data-type/size must be retained for CTF";
  ASSERT_TRUE(currentMetadataDiagnostics.events().empty());

  config.references[0].addressError = "address must be unsigned";
  CollectingDiagnosticSink malformedAddressDiagnostics;
  const auto malformedAddress =
      planOutputs(allRequest, "BackendRequirements.SWO.raw", config, malformedAddressDiagnostics);
  ASSERT_TRUE(malformedAddress.csv.has_value() && !malformedAddress.ctf.has_value())
      << "malformed address must disable only CTF";
  malformedAddressDiagnostics.singleEvent();
}

TEST(CtraceUnitTests, testCtfOutputRequiresAValidClock)
{
  auto config = backendRequirementsConfig();
  config.setups[0].data[0] = TraceRunDataSetup{};
  config.references[0].dataType.reset();
  const auto allRequest = outputRequest(true, true);
  config.setups[0].timestamps->clockHz.reset();
  CollectingDiagnosticSink missingClockDiagnostics;
  const auto missingClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, missingClockDiagnostics);
  ASSERT_TRUE(missingClock.csv.has_value() && !missingClock.ctf.has_value())
      << "missing timestamps.clock must disable only CTF";
  EXPECT_EQ(missingClockDiagnostics.singleEvent().message,
            "CTF output requires timestamps.clock; no default is assumed");
  EXPECT_TRUE(missingClockDiagnostics.containsContext("backend", "ctf"));
  EXPECT_GT(missingClockDiagnostics.failureCount(), 0U)
      << "--all must remain unsuccessful when its requested CTF backend is invalid";

  config.setups[0].timestamps->clockError = "timestamps.clock must be unsigned";
  CollectingDiagnosticSink malformedClockDiagnostics;
  const auto malformedClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, malformedClockDiagnostics);
  ASSERT_TRUE(malformedClock.csv.has_value() && !malformedClock.ctf.has_value())
      << "malformed timestamps.clock must disable only CTF";
  EXPECT_EQ(malformedClockDiagnostics.singleEvent().message, "CTF output cannot use the configured timestamps.clock");
  EXPECT_GT(malformedClockDiagnostics.failureCount(), 0U);

  config.setups[0].timestamps->clockError.reset();
  config.setups[0].timestamps->clockHz = 0U;
  CollectingDiagnosticSink zeroClockDiagnostics;
  const auto zeroClock = planOutputs(allRequest, "BackendRequirements.SWO.raw", config, zeroClockDiagnostics);
  ASSERT_TRUE(zeroClock.csv.has_value() && !zeroClock.ctf.has_value()) << "zero timestamps.clock must disable only CTF";
  EXPECT_EQ(zeroClockDiagnostics.singleEvent().message, "CTF output requires timestamps.clock to be greater than zero");
  EXPECT_GT(zeroClockDiagnostics.failureCount(), 0U);

  CollectingDiagnosticSink csvOnlyDiagnostics;
  const auto csvOnly =
      planOutputs(outputRequest(true, false), "BackendRequirements.SWO.raw", config, csvOnlyDiagnostics);
  EXPECT_TRUE(csvOnly.csv.has_value());
  EXPECT_FALSE(csvOnly.ctf.has_value());
  EXPECT_TRUE(csvOnlyDiagnostics.events().empty())
      << "CSV-only planning must not inspect or report CTF clock requirements";
}

TEST(CtraceUnitTests, testCtfOutputRejectsConflictingClockFragmentsForSharedProcessorRoute)
{
  TraceRunConfig config;
  config.path = "SharedProcessorClock.ctrace-run.yml";
  config.traceFormat = TraceRunFormat::Formatted;
  config.setups = {
      TraceRunTestSupport::makeTimestampSetup("core", 100000000U, 1U),
      TraceRunTestSupport::makeTimestampSetup("core", 200000000U, 1U),
  };
  config.references = {
      TraceRunTestSupport::makeReference("itm", "core", 1U, {1U}, "core/itm"),
  };

  CollectingDiagnosticSink diagnostics;
  const auto plan = planOutputs(outputRequest(true, true), "SharedProcessorClock.TB.raw", config, diagnostics);
  EXPECT_TRUE(plan.csv.has_value());
  EXPECT_FALSE(plan.ctf.has_value());
  const auto& error = diagnostics.singleEvent();
  EXPECT_EQ(error.message, "CTF output cannot use the configured timestamps.clock");
  EXPECT_TRUE(diagnostics.containsContext("backend", "ctf"));
  EXPECT_TRUE(diagnostics.containsContext("stream", "1"));
  EXPECT_TRUE(diagnostics.containsContext("pname", "core"));
  EXPECT_TRUE(diagnostics.containsContext("error", "conflicting active ctrace-setup timestamps.clock values"));
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
  config.traceFormat = TraceRunFormat::Formatted;
  auto setup = TraceRunTestSupport::makeTimestampSetup(std::nullopt, 400000000U, 1U);
  setup.data.push_back(TraceRunDataSetup{4U});
  auto routeOneSetup = setup;
  routeOneSetup.processorName = "core0";
  auto routeTwoSetup = setup;
  routeTwoSetup.processorName = "core1";
  config.setups = {routeOneSetup, routeTwoSetup};

  auto first = TraceRunTestSupport::makeReference("dwt", "core0", 1U, {0U}, "core0/data#0");
  first.dataSetupIndex = 0U;
  first.label = "core-one";
  TraceRunReference second = first;
  second.label = "core-two";
  second.address = 0x2000U;
  second.dataType = "signed";
  second.dataSize = 2U;
  const auto firstAnchor = TraceRunTestSupport::makeReference("itm", "core0", 1U, {}, "core0/itm");
  const auto secondAnchor = TraceRunTestSupport::makeReference("itm", "core1", 2U, {}, "core1/itm");
  config.references = {first, second, firstAnchor, secondAnchor};

  auto allRequest = outputRequest(true, true);
  CollectingDiagnosticSink diagnostics;
  const auto plan = planOutputs(allRequest, "captures/AmbiguousRoutes.SWO.raw", config, diagnostics);
  ASSERT_TRUE(plan.csv.has_value() && !plan.ctf.has_value())
      << "conflicting CTF route labels must not disable independent CSV output";
  diagnostics.singleEvent();

  config.references[1].stream = 2U;
  config.references[1].processorName = "core1";
  config.references[1].ctraceRef = "core1/data#0";
  CollectingDiagnosticSink routeDiagnostics;
  const auto routePlan = planOutputs(allRequest, "captures/AmbiguousRoutes.SWO.raw", config, routeDiagnostics);
  ASSERT_TRUE(routePlan.csv.has_value() && routePlan.ctf.has_value() && routePlan.ctf->metadata.sources.size() == 2U)
      << "CTF must retain the same source number independently on distinct routes";
  const auto& routeSources = routePlan.ctf->metadata.sources;
  EXPECT_EQ(routeSources[0].route.traceBusId, 1U);
  EXPECT_EQ(routeSources[0].label, std::optional<std::string>("core-one"));
  EXPECT_EQ(routeSources[0].dataType, "unsigned");
  EXPECT_EQ(routeSources[0].dataSize, 4U);
  EXPECT_EQ(routeSources[1].route.traceBusId, 2U);
  EXPECT_EQ(routeSources[1].label, std::optional<std::string>("core-two"));
  EXPECT_EQ(routeSources[1].address, std::optional<std::uint64_t>(0x2000U));
  EXPECT_EQ(routeSources[1].dataType, "signed");
  EXPECT_EQ(routeSources[1].dataSize, 2U);
  ASSERT_TRUE(routeDiagnostics.events().empty())
      << "route-local CTF metadata with the same source number must not be ambiguous";

  TraceRunConfig processorConfig;
  processorConfig.path = "AmbiguousProcessors.ctrace-run.yml";
  processorConfig.traceFormat = TraceRunFormat::Formatted;
  processorConfig.setups = config.setups;
  auto core0Reference = first;
  core0Reference.processorName = "core0";
  core0Reference.ctraceRef = "core0/data#0";
  auto core1Reference = first;
  core1Reference.processorName = "core1";
  core1Reference.ctraceRef = "core1/data#0";
  core1Reference.stream = 2U;
  processorConfig.references = {core0Reference, core1Reference, firstAnchor, secondAnchor};
  CollectingDiagnosticSink processorDiagnostics;
  const auto processorPlan = planOutputs(outputRequest(true, false), "captures/AmbiguousProcessors.SWO.raw",
                                         processorConfig, processorDiagnostics);
  ASSERT_TRUE(processorPlan.csv.has_value() && processorDiagnostics.events().empty())
      << "CSV must preserve raw stream/source values without consuming processor metadata";

  allRequest.selection.streams = {1U};
  CollectingDiagnosticSink selectedDiagnostics;
  const auto selectedPlan = planOutputs(allRequest, "captures/AmbiguousRoutes.SWO.raw", config, selectedDiagnostics);
  ASSERT_TRUE(selectedPlan.csv.has_value() && selectedPlan.ctf.has_value() &&
              selectedPlan.ctf->metadata.sources.size() == 1U &&
              selectedPlan.ctf->metadata.sources[0].label == std::optional<std::string>("core-one"))
      << "an explicit stream selection must produce one resolved CTF route";
  ASSERT_TRUE(selectedDiagnostics.events().empty())
      << "an unambiguous selected route must not produce preflight diagnostics";

  config.setups[0].data.push_back(TraceRunDataSetup{2U});
  config.references[1].stream = 1U;
  config.references[1].processorName = "core0";
  config.references[1].ctraceRef = "core0/data#1";
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

TEST(CtraceUnitTests, testOutputRequirementsDeferUnformattedSingleClockAmbiguityToCtf)
{
  TraceRunConfig config;
  config.path = "SingleCandidates.ctrace-run.yml";
  config.setups = {
      TraceRunTestSupport::makeTimestampSetup("first", 100U, 4U),
      TraceRunTestSupport::makeTimestampSetup("second", 200U, 4U),
  };
  config.references = {
      TraceRunTestSupport::makeReference("itm", "first", 1U, {1U}),
      TraceRunTestSupport::makeReference("itm", "second", 1U, {2U}),
  };

  CollectingDiagnosticSink checkDiagnostics;
  const auto checkPlan = planOutputs(outputRequest(false, false), "captures/Single.SWO.raw", config, checkDiagnostics);
  EXPECT_FALSE(checkPlan.hasRequestedOutputs());
  EXPECT_TRUE(checkDiagnostics.events().empty());

  CollectingDiagnosticSink csvDiagnostics;
  const auto csvPlan = planOutputs(outputRequest(true, false), "captures/Single.SWO.raw", config, csvDiagnostics);
  EXPECT_TRUE(csvPlan.csv.has_value());
  EXPECT_TRUE(csvDiagnostics.events().empty());

  CollectingDiagnosticSink allDiagnostics;
  const auto allPlan = planOutputs(outputRequest(true, true), "captures/Single.SWO.raw", config, allDiagnostics);
  EXPECT_TRUE(allPlan.csv.has_value());
  EXPECT_FALSE(allPlan.ctf.has_value());
  EXPECT_EQ(allDiagnostics.singleEvent().message, "CTF output cannot use the configured timestamps.clock");

  config.setups[1].timestamps->clockHz = 100U;
  CollectingDiagnosticSink equivalentDiagnostics;
  const auto equivalent =
      planOutputs(outputRequest(true, true), "captures/Single.SWO.raw", config, equivalentDiagnostics);
  EXPECT_TRUE(equivalent.csv.has_value());
  EXPECT_TRUE(equivalent.ctf.has_value());
  EXPECT_TRUE(equivalentDiagnostics.events().empty());

  TraceRunSetup noTimestamps;
  noTimestamps.processorName = "second";
  noTimestamps.itm = TraceRunItmSetup{1U};
  config.setups[0].timestamps->timestampPrescaler = 1U;
  config.setups[1] = noTimestamps;
  CollectingDiagnosticSink missingCandidateDiagnostics;
  const auto missingCandidate =
      planOutputs(outputRequest(true, true), "captures/Single.SWO.raw", config, missingCandidateDiagnostics);
  EXPECT_TRUE(missingCandidate.csv.has_value());
  EXPECT_FALSE(missingCandidate.ctf.has_value());
  EXPECT_EQ(missingCandidateDiagnostics.singleEvent().message, "CTF output cannot use the configured timestamps.clock");
}

TEST(CtraceUnitTests, testOutputRequirementsTreatUnknownStreamsAsPureFilters)
{
  TraceRunConfig config;
  config.path = "Multicore.ctrace-run.yml";
  config.traceFormat = TraceRunFormat::Formatted;
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
  ASSERT_TRUE(plan.ctf.has_value());
  EXPECT_TRUE(plan.ctf->metadata.streams.empty());
  EXPECT_TRUE(plan.ctf->metadata.clockDomains.empty());
  EXPECT_TRUE(plan.ctf->metadata.sources.empty());
  EXPECT_EQ(plan.ctf->routes.size(), 2U);
  EXPECT_TRUE(diagnostics.events().empty()) << "an unknown-only stream filter must not inspect unrelated route clocks";

  ctfRequest.selection.streams = {1U, 99U};
  CollectingDiagnosticSink mixedDiagnostics;
  const auto mixedPlan = planOutputs(ctfRequest, "Multicore.SWO.raw", config, mixedDiagnostics);
  ASSERT_TRUE(mixedPlan.ctf.has_value());
  ASSERT_EQ(mixedPlan.ctf->metadata.streams.size(), 1U);
  ASSERT_EQ(mixedPlan.ctf->metadata.clockDomains.size(), 1U);
  const auto* firstClock = clockForStream(*mixedPlan.ctf, 1U);
  ASSERT_NE(firstClock, nullptr);
  EXPECT_EQ(firstClock->frequencyHz, 100U);
  EXPECT_EQ(mixedPlan.ctf->routes.size(), 2U);
  EXPECT_TRUE(mixedDiagnostics.events().empty());

  config.setups[1].timestamps->clockHz.reset();
  config.setups[1].timestamps->clockError = "invalid second processor clock";
  CollectingDiagnosticSink unselectedErrorDiagnostics;
  const auto unselectedErrorPlan = planOutputs(ctfRequest, "Multicore.SWO.raw", config, unselectedErrorDiagnostics);
  ASSERT_TRUE(unselectedErrorPlan.ctf.has_value());
  EXPECT_EQ(unselectedErrorPlan.ctf->metadata.streams.size(), 1U);
  EXPECT_TRUE(unselectedErrorDiagnostics.events().empty())
      << "an invalid clock on an unselected route must not disable CTF";

  CollectingDiagnosticSink allDiagnostics;
  const auto allPlan = planOutputs(outputRequest(true, true), "Multicore.SWO.raw", config, allDiagnostics);
  EXPECT_TRUE(allPlan.csv.has_value());
  EXPECT_FALSE(allPlan.ctf.has_value());
  EXPECT_EQ(allDiagnostics.singleEvent().message, "CTF output cannot use the configured timestamps.clock");
  EXPECT_TRUE(allDiagnostics.containsContext("stream", "2"));

  ctfRequest.selection.streams = {99U};
  config.setups[0].timestamps->clockHz = 0U;
  CollectingDiagnosticSink unknownInvalidDiagnostics;
  const auto unknownInvalidPlan = planOutputs(ctfRequest, "Multicore.SWO.raw", config, unknownInvalidDiagnostics);
  ASSERT_TRUE(unknownInvalidPlan.ctf.has_value());
  EXPECT_TRUE(unknownInvalidPlan.ctf->metadata.streams.empty());
  EXPECT_TRUE(unknownInvalidPlan.ctf->metadata.clockDomains.empty());
  EXPECT_TRUE(unknownInvalidDiagnostics.events().empty())
      << "unknown-only selection must not report malformed or zero clocks on filtered routes";
}

TEST(CtraceUnitTests, testOutputRequirementsIgnoreMissingLegacyClockForExcludedStream)
{
  TraceRunConfig config;
  config.path = "Legacy.ctrace-run.yml";
  config.setups.push_back(TraceRunTestSupport::makeTimestampSetup("core", 1000000U, 1U));
  config.setups.front().timestamps->clockHz.reset();

  auto request = outputRequest(false, true);
  request.selection.streams = {1U};
  CollectingDiagnosticSink diagnostics;
  const auto plan = planOutputs(request, "Legacy.SWO.raw", config, diagnostics);

  ASSERT_TRUE(plan.ctf.has_value());
  EXPECT_TRUE(plan.ctf->metadata.streams.empty());
  EXPECT_TRUE(plan.ctf->metadata.clockDomains.empty());
  EXPECT_TRUE(plan.ctf->metadata.sources.empty());
  EXPECT_TRUE(diagnostics.events().empty())
      << "an excluded legacy route must not require timestamps.clock or create CTF topology";
}

TEST(CtraceUnitTests, testOutputRequirementsRejectsInputWithoutArtifactName)
{
  CollectingDiagnosticSink diagnostics;
  EXPECT_THROW((void)planTraceOutputs(outputRequest(true, false), {}, CtraceRunMeta::fromConfig({}), diagnostics),
               std::runtime_error);
}
