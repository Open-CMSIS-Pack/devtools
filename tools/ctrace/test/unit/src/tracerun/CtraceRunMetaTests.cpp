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
#include "TraceRunConfig.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using TraceRunTestSupport::makeReference;
using TraceRunTestSupport::makeTimestampSetup;

/** @brief Tests whether metadata normalization rejects a configuration. */
static bool metaRejects(const TraceRunConfig& config, std::string_view message)
{
  return throwsWithMessage([&config] { (void)CtraceRunMeta::fromConfig(config); }, message);
}

/** @brief Creates one explicitly formatted configuration for route-normalization tests. */
static TraceRunConfig formattedConfig(std::vector<TraceRunReference> references, std::vector<TraceRunSetup> setups = {})
{
  TraceRunConfig config;
  config.path = "trace.yml";
  config.traceFormat = TraceRunFormat::Formatted;
  config.references = std::move(references);
  config.setups = std::move(setups);
  return config;
}

/** @brief Creates a reference whose ctrace path is explicit at the call site. */
static TraceRunReference routeReference(std::string type, std::string path, std::optional<std::string> processorName,
                                        std::optional<std::uint32_t> stream, std::vector<std::uint32_t> sources = {})
{
  return makeReference(std::move(type), std::move(processorName), stream, std::move(sources), std::move(path));
}

TEST(CtraceUnitTests, testTimestampPrescalerMetadataDefaults)
{
  const auto ctraceRunMeta = CtraceRunMeta::fromConfig(TraceRunConfig{});
  ASSERT_TRUE(!ctraceRunMeta.timestampPrescaler().has_value())
      << "a missing timestamp setup must leave the metadata prescaler unspecified";

  TraceRunConfig incompleteTraceRun;
  TraceRunSetup incompleteSetup;
  incompleteSetup.timestamps = TraceRunTimestampSetup{};
  incompleteTraceRun.setups.push_back(incompleteSetup);
  const auto missingPrescalerMeta = CtraceRunMeta::fromConfig(incompleteTraceRun);
  ASSERT_TRUE(missingPrescalerMeta.timestampPrescaler() ==
              std::optional<std::uint32_t>(TraceRunSchema::kDefaultTimestampPrescaler))
      << "timestamp setup must resolve an omitted prescaler to the specified default";
  TraceRunConfig traceRun;
  TraceRunSetup setup;
  setup.timestamps = TraceRunTimestampSetup{std::nullopt, 4U};
  traceRun.setups.push_back(setup);
  const auto prescalerOnlyMeta = CtraceRunMeta::fromConfig(traceRun);
  ASSERT_TRUE(prescalerOnlyMeta.timestampPrescaler() == std::optional<std::uint32_t>(4U))
      << "explicit timestamp prescaler metadata mismatch";
}

TEST(CtraceUnitTests, testCtraceRunMetaRejectsInvalidReferences)
{
  /** @brief Describes one invalid reference and its expected diagnostic. */
  struct Case {
    TraceRunReference reference;
    const char* message;
  };
  std::vector<Case> cases;
  auto duplicateDwtSource = makeReference("dwt", std::nullopt, 1U, {1U, 1U});
  duplicateDwtSource.dataSetupIndex = 0U;
  cases.push_back({duplicateDwtSource, "duplicate value in source array"});
  cases.push_back(
      {makeReference("itm", std::nullopt, 0U, {1U}), "stream must be a CoreSight ATB trace ID between 1 and 111"});
  cases.push_back(
      {routeReference("event", "events#0", "core", 0U), "stream must be a CoreSight ATB trace ID between 1 and 111"});
  cases.push_back({makeReference("itm", std::nullopt, 1U, {32U}), "ITM source must be between 0 and 31"});

  for (const auto& testCase : cases) {
    TraceRunConfig config;
    config.path = "trace.yml";
    config.references = {testCase.reference};
    EXPECT_TRUE(metaRejects(config, testCase.message));

    config.references.front().line = 7U;
    EXPECT_TRUE(metaRejects(config, "trace.yml(7)"));
  }

  TraceRunConfig diagnosed;
  diagnosed.references.push_back(makeReference("itm", std::nullopt, 0U, {99U}));
  diagnosed.references.front().error = {"producer rejected this route"};
  EXPECT_TRUE(metaRejects(diagnosed, "stream must be a CoreSight ATB trace ID between 1 and 111"));

  diagnosed.references.front() = makeReference("itm", std::nullopt, 0U, {1U, 1U});
  diagnosed.references.front().error = {"producer rejected duplicate sources"};
  EXPECT_TRUE(metaRejects(diagnosed, "stream must be a CoreSight ATB trace ID between 1 and 111"));

  TraceRunConfig diagnosedBinding;
  diagnosedBinding.path = "trace.yml";
  diagnosedBinding.setups.push_back(makeTimestampSetup("core"));
  diagnosedBinding.references.push_back(makeReference("itm", "core", 1U, {}, "core/itm"));
  diagnosedBinding.references.front().error = {"producer rejected this route"};
  const auto meta = CtraceRunMeta::fromConfig(diagnosedBinding);
  EXPECT_EQ(meta.processorCount(), 1U);
  EXPECT_TRUE(meta.sources().empty());
  EXPECT_EQ(meta.referenceDiagnostics().size(), 1U);
}

TEST(CtraceUnitTests, testCtraceRunMetaExpandsItmAndDwtSourceArrays)
{
  TraceRunConfig config;
  config.references.push_back(makeReference("itm", std::nullopt, 1U, {1U, 2U}));
  auto dwtReference = makeReference("dwt", std::nullopt, 1U, {3U, 4U});
  dwtReference.dataSetupIndex = 0U;
  config.references.push_back(dwtReference);

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.sources().size(), 4U);
  EXPECT_EQ(meta.sources()[0].type, "itm");
  EXPECT_EQ(meta.sources()[0].source, 1U);
  EXPECT_EQ(meta.sources()[1].type, "itm");
  EXPECT_EQ(meta.sources()[1].source, 2U);
  EXPECT_EQ(meta.sources()[2].type, "dwt");
  EXPECT_EQ(meta.sources()[2].source, 3U);
  EXPECT_EQ(meta.sources()[3].type, "dwt");
  EXPECT_EQ(meta.sources()[3].source, 4U);
}

TEST(CtraceUnitTests, testCtraceRunMetaRejectsInvalidPrescalers)
{
  TraceRunConfig config;
  config.path = "trace.yml";
  config.setups.push_back(makeTimestampSetup(std::nullopt, 1U, 2U));
  EXPECT_TRUE(metaRejects(config, "ctrace-setup timestamps.itm-prescaler must be one of"));

  config.setups.front().timestamps->line = 12U;
  EXPECT_TRUE(metaRejects(config, "trace.yml(12): 'timestamps.itm-prescaler' must be one of"));
}

TEST(CtraceUnitTests, testCtraceRunMetaRejectsDuplicateSetups)
{
  TraceRunConfig duplicate;
  duplicate.path = "trace.yml";
  duplicate.setups = {makeTimestampSetup(std::nullopt, 100U, 4U), makeTimestampSetup(std::nullopt, 100U, 4U)};
  const auto merged = CtraceRunMeta::fromConfig(duplicate);
  EXPECT_EQ(merged.timestampClockHz(), std::optional<std::uint64_t>(100U));
  EXPECT_EQ(merged.timestampPrescaler(), std::optional<std::uint32_t>(4U));

  duplicate.setups.back().timestamps->timestampPrescaler = 16U;
  duplicate.setups.back().timestamps->line = 9U;
  EXPECT_TRUE(metaRejects(duplicate, "conflicting timestamps.itm-prescaler values for one processor"));

  TraceRunConfig unnamedMultiProcessor;
  unnamedMultiProcessor.path = "trace.yml";
  unnamedMultiProcessor.setups = {makeTimestampSetup("a"), makeTimestampSetup("b"), makeTimestampSetup(std::nullopt)};
  EXPECT_TRUE(metaRejects(unnamedMultiProcessor,
                          "pname is required for every ctrace-setup in a multi-processor configuration"));

  TraceRunConfig inferredFragment;
  inferredFragment.setups = {makeTimestampSetup("core", 100U, 4U), makeTimestampSetup(std::nullopt, 100U, 4U)};
  const auto inferred = CtraceRunMeta::fromConfig(inferredFragment);
  EXPECT_EQ(inferred.processorCount(), 1U);
  EXPECT_EQ(inferred.routes().front().processorName, std::optional<std::string>("core"));

  TraceRunConfig conflictingClock;
  conflictingClock.setups = {makeTimestampSetup("core", 100U, 4U), makeTimestampSetup("core", 200U, 4U)};
  const auto deferredClock = CtraceRunMeta::fromConfig(conflictingClock);
  EXPECT_FALSE(deferredClock.timestampClockHz().has_value());
  ASSERT_EQ(deferredClock.timestampClockErrors().size(), 1U);
  EXPECT_EQ(deferredClock.routes().front().timestampClockError,
            std::optional<std::string>("conflicting active ctrace-setup timestamps.clock values"));

  auto missingClock = makeTimestampSetup("core", std::nullopt, 4U);
  for (const auto reverse : {false, true}) {
    TraceRunConfig complementConfig;
    complementConfig.setups = reverse ? std::vector<TraceRunSetup>{makeTimestampSetup("core", 100U, 4U), missingClock}
                                      : std::vector<TraceRunSetup>{missingClock, makeTimestampSetup("core", 100U, 4U)};
    const auto complemented = CtraceRunMeta::fromConfig(complementConfig);
    EXPECT_EQ(complemented.timestampClockHz(), std::optional<std::uint64_t>(100U));
    EXPECT_FALSE(complemented.routes().front().timestampClockError.has_value());
  }

  auto firstError = makeTimestampSetup("core", std::nullopt, 4U);
  firstError.timestamps->clockError = "first clock error";
  auto secondError = firstError;
  secondError.timestamps->clockError = "second clock error";
  TraceRunConfig conflictingErrors;
  conflictingErrors.setups = {firstError, secondError};
  const auto deferredErrors = CtraceRunMeta::fromConfig(conflictingErrors);
  EXPECT_EQ(deferredErrors.routes().front().timestampClockError,
            std::optional<std::string>("conflicting active ctrace-setup timestamps.clock values"));
}

TEST(CtraceUnitTests, testCtraceRunMetaNormalizesAmbiguousUnformattedProcessorIdentities)
{
  TraceRunConfig multiUnnamed;
  multiUnnamed.path = "trace.yml";
  multiUnnamed.setups = {makeTimestampSetup("a"), makeTimestampSetup("b")};
  multiUnnamed.references.push_back(makeReference("itm", std::nullopt, 1U, {1U}));
  multiUnnamed.references.front().line = 17U;
  const auto unnamedMeta = CtraceRunMeta::fromConfig(multiUnnamed);
  EXPECT_EQ(unnamedMeta.processorCount(), 2U);
  EXPECT_TRUE(unnamedMeta.sources().empty());
  EXPECT_FALSE(unnamedMeta.routes().front().processorName.has_value());

  TraceRunConfig multiUnmatched = multiUnnamed;
  multiUnmatched.references.front().processorName = "c";
  const auto unmatchedMeta = CtraceRunMeta::fromConfig(multiUnmatched);
  EXPECT_EQ(unmatchedMeta.processorCount(), 2U);
  EXPECT_TRUE(unmatchedMeta.sources().empty());

  TraceRunConfig selected;
  selected.setups = {
      makeTimestampSetup("a", 100U, 4U),
      makeTimestampSetup("b", 200U, 16U),
  };
  selected.references = {makeReference("itm", "a", 5U, {1U}, "a/itm")};
  const auto selectedMeta = CtraceRunMeta::fromConfig(selected);
  EXPECT_EQ(selectedMeta.processorCount(), 1U);
  EXPECT_EQ(selectedMeta.routes().front().processorName, std::optional<std::string>("a"));
  EXPECT_EQ(selectedMeta.routes().front().timestampClockHz, std::optional<std::uint64_t>(100U));
  EXPECT_EQ(selectedMeta.routes().front().timestampPrescaler, 4U);
  EXPECT_EQ(selectedMeta.sources().front().traceBusId, 0U);

  selected.references.push_back(makeReference("itm", std::nullopt, 5U, {2U}, "messages"));
  const auto inferredReferenceMeta = CtraceRunMeta::fromConfig(selected);
  ASSERT_EQ(inferredReferenceMeta.sources().size(), 2U);
  EXPECT_EQ(inferredReferenceMeta.sources().back().processorName, std::optional<std::string>("a"));

  selected.references.back().stream = 6U;
  const auto ambiguousReferenceMeta = CtraceRunMeta::fromConfig(selected);
  ASSERT_EQ(ambiguousReferenceMeta.sources().size(), 1U);
  ASSERT_EQ(ambiguousReferenceMeta.warnings().size(), 1U);
  EXPECT_NE(ambiguousReferenceMeta.warnings().front().message.find("processor binding is ambiguous"),
            std::string::npos);

  selected.references.push_back(routeReference("event", "unused", std::nullopt, std::nullopt));
  const auto ignoredReferenceMeta = CtraceRunMeta::fromConfig(selected);
  EXPECT_EQ(ignoredReferenceMeta.sources().size(), 1U);

  TraceRunConfig eventBinding;
  eventBinding.setups = selected.setups;
  eventBinding.references = {routeReference("event", "a/events#0", "a", 5U)};
  const auto eventMeta = CtraceRunMeta::fromConfig(eventBinding);
  EXPECT_EQ(eventMeta.routes().front().processorName, std::optional<std::string>("a"));

  eventBinding.setups.clear();
  eventBinding.references.push_back(routeReference("exception", "b/exceptions", "b", 6U));
  const auto eventCandidates = CtraceRunMeta::fromConfig(eventBinding);
  EXPECT_EQ(eventCandidates.processorCount(), 2U);
  EXPECT_FALSE(eventCandidates.routes().front().processorName.has_value());

  TraceRunConfig pathOnly;
  pathOnly.path = "trace.yml";
  pathOnly.references = {
      routeReference("itm", "a/itm", std::nullopt, 1U, {1U}),
      routeReference("itm", "b/itm", std::nullopt, 2U, {2U}),
  };
  const auto pathOnlyMeta = CtraceRunMeta::fromConfig(pathOnly);
  EXPECT_EQ(pathOnlyMeta.processorCount(), 2U);
  ASSERT_EQ(pathOnlyMeta.sources().size(), 2U);
  EXPECT_FALSE(pathOnlyMeta.routes().front().processorName.has_value());

  TraceRunConfig pathMismatch;
  pathMismatch.path = "trace.yml";
  pathMismatch.setups = {makeTimestampSetup("a")};
  pathMismatch.references = {routeReference("itm", "b/itm", std::nullopt, 1U, {1U})};
  const auto mismatchMeta = CtraceRunMeta::fromConfig(pathMismatch);
  EXPECT_TRUE(mismatchMeta.sources().empty());

  pathMismatch.references.front().processorName = "a";
  EXPECT_TRUE(metaRejects(pathMismatch, "path processor conflicts with pname"));

  TraceRunConfig opaquePath;
  opaquePath.references = {routeReference("itm", "opaque/printf-route", std::nullopt, 1U, {1U})};
  const auto opaqueMeta = CtraceRunMeta::fromConfig(opaquePath);
  ASSERT_EQ(opaqueMeta.sources().size(), 1U);
  EXPECT_FALSE(opaqueMeta.sources().front().processorName.has_value());

  TraceRunConfig pathBoundData;
  TraceRunSetup foreignData;
  foreignData.processorName = "core";
  foreignData.data = {TraceRunDataSetup{2U}};
  pathBoundData.setups = {foreignData};
  auto otherData = routeReference("dwt", "other/data#0", std::nullopt, 1U, {0U});
  otherData.dataSetupIndex = 0U;
  pathBoundData.references = {otherData};
  const auto pathBoundMeta = CtraceRunMeta::fromConfig(pathBoundData);
  ASSERT_EQ(pathBoundMeta.sources().size(), 1U);
  EXPECT_EQ(pathBoundMeta.routes().front().processorName, std::optional<std::string>("other"));
  EXPECT_EQ(pathBoundMeta.sources().front().dataSize, TraceRunSchema::kDefaultDwtDataSize);
}

TEST(CtraceUnitTests, testCtraceRunMetaWarnsForSingleSetupIdentityConflicts)
{
  TraceRunConfig namedSetup;
  namedSetup.path = "trace.yml";
  namedSetup.setups.push_back(makeTimestampSetup("a"));
  namedSetup.references.push_back(makeReference("itm", "b", 1U, {1U}));
  const auto namedMeta = CtraceRunMeta::fromConfig(namedSetup);
  EXPECT_TRUE(namedMeta.sources().empty());
  ASSERT_EQ(namedMeta.warnings().size(), 1U);

  TraceRunConfig unnamedSetup;
  unnamedSetup.path = "trace.yml";
  unnamedSetup.setups.push_back(makeTimestampSetup(std::nullopt));
  unnamedSetup.references = {
      makeReference("itm", "a", 1U, {1U}),
      makeReference("itm", "b", 2U, {2U}),
  };
  EXPECT_TRUE(metaRejects(unnamedSetup, "unformatted SINGLE trace requires one unambiguous processor"));

  TraceRunConfig referencesOnly;
  referencesOnly.path = "trace.yml";
  referencesOnly.references = unnamedSetup.references;
  auto unnamedDwtReference = makeReference("dwt", std::nullopt, 3U, {0U});
  unnamedDwtReference.dataSetupIndex = 0U;
  referencesOnly.references.push_back(unnamedDwtReference);
  EXPECT_TRUE(metaRejects(referencesOnly, "pname is required for every ctrace-ref"));

  referencesOnly.references.pop_back();
  const auto mergedReferences = CtraceRunMeta::fromConfig(referencesOnly);
  EXPECT_EQ(mergedReferences.processorCount(), 2U);
  EXPECT_EQ(mergedReferences.sources().size(), 2U);
  EXPECT_FALSE(mergedReferences.routes().front().processorName.has_value());
}

TEST(CtraceUnitTests, testCtraceRunMetaBindsOneNamedReferenceToUnnamedSetup)
{
  TraceRunConfig config;
  config.setups.push_back(makeTimestampSetup(std::nullopt, 100U));
  config.references.push_back(makeReference("itm", "core", 1U, {1U}));

  const auto meta = CtraceRunMeta::fromConfig(config);

  ASSERT_EQ(meta.sources().size(), 1U);
  EXPECT_EQ(meta.sources().front().processorName, std::optional<std::string>("core"));
  ASSERT_EQ(meta.timestampsByTraceBusId().size(), 1U);
  EXPECT_EQ(meta.timestampsByTraceBusId().at(0U).processorName, std::optional<std::string>("core"));
  EXPECT_EQ(meta.timestampsByTraceBusId().at(0U).clockHz, std::optional<std::uint64_t>(100U));
}

TEST(CtraceUnitTests, testCtraceRunMetaBindsStreamlessTimestampToInternalRoute)
{
  TraceRunConfig config;
  config.setups.push_back(makeTimestampSetup("core", 100U));
  config.references.push_back(makeReference("itm", "core", std::nullopt, {}, "core/timestamps"));

  const auto meta = CtraceRunMeta::fromConfig(config);

  EXPECT_TRUE(meta.sources().empty());
  ASSERT_EQ(meta.timestampsByTraceBusId().size(), 1U);
  EXPECT_EQ(meta.timestampsByTraceBusId().at(0U).processorName, std::optional<std::string>("core"));
  EXPECT_EQ(meta.timestampsByTraceBusId().at(0U).clockHz, std::optional<std::uint64_t>(100U));
  ASSERT_EQ(meta.timestampPrescalersByTraceBusId().size(), 1U);
  EXPECT_EQ(meta.timestampPrescalersByTraceBusId().at(0U), 1U);
}

TEST(CtraceUnitTests, testCtraceRunMetaMergesCompatibleUnformattedProcessorSettings)
{
  TraceRunConfig config;
  config.path = "trace.yml";
  config.setups = {
      makeTimestampSetup("a", 100U, 4U, 1U),
      makeTimestampSetup("b", 200U, 4U, 2U),
  };
  config.references = {
      makeReference("itm", "a", 5U, {1U}),
      makeReference("itm", "b", 5U, {2U}),
  };

  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_EQ(meta.processorCount(), 2U);
  EXPECT_EQ(meta.timestampPrescaler(), std::optional<std::uint32_t>(4U));
  EXPECT_FALSE(meta.timestampClockHz().has_value());
  EXPECT_FALSE(meta.itmEnableMask().has_value());
  ASSERT_EQ(meta.routes().size(), 1U);
  EXPECT_FALSE(meta.routes().front().processorName.has_value());
  EXPECT_EQ(meta.routes().front().timestampClockError,
            std::optional<std::string>(
                "unformatted SINGLE trace has ambiguous timestamps.clock values across processor candidates"));
  ASSERT_EQ(meta.sources().size(), 2U);
  EXPECT_EQ(meta.sources()[0].traceBusId, 0U);
  EXPECT_EQ(meta.sources()[1].traceBusId, 0U);

  config.setups[1] = makeTimestampSetup("b", 100U, 4U, 1U);
  const auto equivalent = CtraceRunMeta::fromConfig(config);
  EXPECT_EQ(equivalent.timestampClockHz(), std::optional<std::uint64_t>(100U));
  EXPECT_EQ(equivalent.itmEnableMask(), std::optional<std::uint32_t>(1U));

  TraceRunSetup defaulted;
  defaulted.processorName = "b";
  defaulted.itm = TraceRunItmSetup{1U};
  config.setups[1] = defaulted;
  EXPECT_TRUE(metaRejects(config, "different timestamps.itm-prescaler values"));

  config.setups[0].timestamps->timestampPrescaler = 1U;
  const auto missingClock = CtraceRunMeta::fromConfig(config);
  EXPECT_EQ(missingClock.timestampPrescaler(), std::optional<std::uint32_t>(1U));
  EXPECT_FALSE(missingClock.timestampClockHz().has_value());
  EXPECT_EQ(missingClock.routes().front().timestampClockError,
            std::optional<std::string>(
                "unformatted SINGLE trace has ambiguous timestamps.clock values across processor candidates"));
}

TEST(CtraceUnitTests, testCtraceRunMetaMapsDistinctPrescalersPerStream)
{
  TraceRunConfig config;
  config.setups = {
      makeTimestampSetup("a", 100U, 4U),
      makeTimestampSetup("b", 100U, 16U),
  };
  config.references = {
      makeReference("itm", "a", 5U, {1U}),
      makeReference("itm", "b", 6U, {2U}),
  };
  EXPECT_TRUE(metaRejects(config, "different timestamps.itm-prescaler values"));

  config.traceFormat = TraceRunFormat::Formatted;
  config.references[0].ctraceRef = "a/itm";
  config.references[1].ctraceRef = "b/itm";
  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_TRUE(meta.hasDistinctProcessorPrescalers());
  EXPECT_FALSE(meta.timestampPrescaler().has_value());
  EXPECT_EQ(meta.timestampPrescalersByTraceBusId().at(5U), 4U);
  EXPECT_EQ(meta.timestampPrescalersByTraceBusId().at(6U), 16U);

  config.path = "trace.yml";
  config.references.back().stream = 5U;
  config.references.back().line = 23U;
  EXPECT_TRUE(metaRejects(config, "conflicting ITM processor bindings"));
}

TEST(CtraceUnitTests, testCtraceRunMetaResolvesDwtDataAndDefaults)
{
  TraceRunConfig config;
  config.path = "trace.yml";
  auto setup = makeTimestampSetup("core");
  setup.data.resize(2U);
  setup.data[0].size = 2U;
  setup.data[0].sizeError = "size warning";
  config.setups.push_back(setup);

  auto configured = makeReference("dwt", "core", 1U, {0U}, "core/data#0");
  configured.dataSetupIndex = 0U;
  configured.address = 0x20000000U;
  configured.dataType = "float";
  configured.dataSize = 4U;
  auto missingIndex = makeReference("dwt", "core", 1U, {1U}, "core/data");
  auto outOfRange = makeReference("dwt", "core", 1U, {2U}, "core/data#9");
  outOfRange.dataSetupIndex = 9U;
  config.references = {configured, missingIndex, outOfRange};

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.sources().size(), 2U);
  EXPECT_EQ(meta.configPath(), "trace.yml");
  EXPECT_EQ(meta.sources()[0].dataType, "float");
  EXPECT_EQ(meta.sources()[0].dataSize, 4U);
  EXPECT_EQ(meta.sources()[0].address, std::optional<std::uint64_t>(0x20000000U));
  EXPECT_FALSE(meta.sources()[0].dataTypeError.has_value());
  EXPECT_FALSE(meta.sources()[0].dataSizeError.has_value());
  EXPECT_EQ(meta.sources()[1].dataType, std::string(TraceRunSchema::kDefaultDwtDataType));
  EXPECT_EQ(meta.sources()[1].dataSize, TraceRunSchema::kDefaultDwtDataSize);
}

TEST(CtraceUnitTests, testCtraceRunMetaIgnoresInactiveSetups)
{
  TraceRunConfig config;
  TraceRunSetup inactive;
  inactive.processorName = "unused";
  inactive.data.resize(1U);
  config.setups.push_back(inactive);
  config.references.push_back(makeReference("dwt", "other", 1U, {0U}, "other/data#4"));
  config.references.front().dataSetupIndex = 4U;

  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_EQ(meta.processorCount(), 1U);
  EXPECT_EQ(meta.sources().front().dataType, std::string(TraceRunSchema::kDefaultDwtDataType));

  TraceRunConfig dataOnly;
  TraceRunSetup dataSetup;
  dataSetup.processorName = "core";
  dataSetup.data.resize(1U);
  dataOnly.setups.push_back(dataSetup);
  auto dataReference = makeReference("dwt", "core", 1U, {0U}, "core/data#0");
  dataReference.dataSetupIndex = 0U;
  dataOnly.references.push_back(dataReference);
  const auto dataMeta = CtraceRunMeta::fromConfig(dataOnly);
  EXPECT_EQ(dataMeta.processorCount(), 1U);
  ASSERT_EQ(dataMeta.sources().size(), 1U);

  TraceRunConfig unrelatedFeature;
  unrelatedFeature.setups.push_back(makeTimestampSetup("core", 100U));
  TraceRunSetup ignoredSetup;
  ignoredSetup.processorName = "other";
  ignoredSetup.featurePaths = {"other/instructions"};
  unrelatedFeature.setups.push_back(ignoredSetup);
  unrelatedFeature.references.push_back(makeReference("itm", "core", 1U, {1U}, "core/itm"));
  unrelatedFeature.references.push_back(routeReference("unsupported", "other/instructions", "other", 2U));
  const auto unrelatedMeta = CtraceRunMeta::fromConfig(unrelatedFeature);
  EXPECT_EQ(unrelatedMeta.processorCount(), 1U);
  EXPECT_EQ(unrelatedMeta.routes().front().processorName, std::optional<std::string>("core"));
}

TEST(CtraceUnitTests, testCtraceRunMetaDoesNotExposeDwtControlReferencesAsDataSources)
{
  TraceRunConfig config;
  auto data = makeReference("dwt", "core", 1U, {0U}, "core/data#0");
  data.dataSetupIndex = 0U;
  auto start = makeReference("dwt", "core", 1U, {1U}, "core/instructions/start");
  config.references = {data, start};

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.sources().size(), 1U);
  EXPECT_EQ(meta.sources().front().source, 0U);
  EXPECT_EQ(meta.processorCount(), 1U);
}

TEST(CtraceUnitTests, testCtraceRunMetaCreatesOneSyntheticUnformattedRoute)
{
  for (const auto format :
       {std::optional<TraceRunFormat>{}, std::optional<TraceRunFormat>{TraceRunFormat::Unformatted}}) {
    TraceRunConfig config;
    config.traceFormat = format;
    config.references = {routeReference("itm", "messages", "core", 7U, {1U})};

    const auto meta = CtraceRunMeta::fromConfig(config);

    ASSERT_EQ(meta.routes().size(), 1U);
    const auto& route = meta.routes().front();
    EXPECT_EQ(route.protocol, CtraceRunProtocol::Itm);
    EXPECT_FALSE(route.traceBusId.has_value());
    EXPECT_FALSE(route.timestampsConfigured);
    EXPECT_EQ(route.timestampPrescaler, TraceRunSchema::kDefaultTimestampPrescaler);
    ASSERT_EQ(route.sources.size(), 1U);
    EXPECT_EQ(route.sources.front().traceBusId, 0U);
    ASSERT_EQ(meta.sources().size(), 1U);
    EXPECT_EQ(meta.sources().front().traceBusId, 0U) << "SINGLE accessors must expose the transport channel";
  }

  const auto emptyMeta = CtraceRunMeta::fromConfig(TraceRunConfig{});
  ASSERT_EQ(emptyMeta.routes().size(), 1U);
  EXPECT_FALSE(emptyMeta.routes().front().traceBusId.has_value());
}

TEST(CtraceUnitTests, testCtraceRunMetaBuildsFormattedAnchorRoutesAndMetadata)
{
  auto firstSetup = makeTimestampSetup("first", 100U, 4U, 3U);
  firstSetup.data.resize(1U);
  firstSetup.data.front().size = 2U;
  auto secondSetup = makeTimestampSetup("second", 200U, 16U, 5U);

  auto firstAnchor = routeReference("itm", "first/itm", "first", 1U, {1U});
  firstAnchor.info = {"producer info"};
  firstAnchor.warning = {"producer warning"};
  firstAnchor.error = {"producer error"};
  auto data = routeReference("dwt", "first/data#0", "first", 1U, {2U});
  data.dataSetupIndex = 0U;
  data.address = 0x20000000U;
  auto secondAnchor = routeReference("itm", "second/itm", "second", 111U);
  const auto config = formattedConfig({firstAnchor, data, secondAnchor}, {firstSetup, secondSetup});

  const auto meta = CtraceRunMeta::fromConfig(config);

  ASSERT_EQ(meta.routes().size(), 2U);
  const auto& first = meta.routes()[0];
  EXPECT_EQ(first.traceBusId, std::optional<std::uint8_t>(1U));
  EXPECT_EQ(first.processorName, std::optional<std::string>("first"));
  EXPECT_TRUE(first.timestampsConfigured);
  EXPECT_EQ(first.timestampClockHz, std::optional<std::uint64_t>(100U));
  EXPECT_EQ(first.timestampPrescaler, 4U);
  EXPECT_EQ(first.itmEnableMask, std::optional<std::uint32_t>(3U));
  ASSERT_EQ(first.sources.size(), 2U);
  EXPECT_EQ(first.sources[0].type, "itm");
  EXPECT_EQ(first.sources[1].type, "dwt");
  EXPECT_EQ(first.sources[1].dataSize, 2U);
  ASSERT_EQ(first.referenceDiagnostics.size(), 3U);
  EXPECT_EQ(first.referenceDiagnostics[0].severity, CtraceRunReferenceDiagnostic::Severity::Info);
  EXPECT_EQ(first.referenceDiagnostics[1].severity, CtraceRunReferenceDiagnostic::Severity::Warning);
  EXPECT_EQ(first.referenceDiagnostics[2].severity, CtraceRunReferenceDiagnostic::Severity::Error);
  EXPECT_EQ(meta.referenceDiagnostics().size(), 3U);

  const auto& second = meta.routes()[1];
  EXPECT_EQ(second.traceBusId, std::optional<std::uint8_t>(111U));
  EXPECT_EQ(second.processorName, std::optional<std::string>("second"));
  EXPECT_EQ(second.timestampClockHz, std::optional<std::uint64_t>(200U));
  EXPECT_EQ(second.timestampPrescaler, 16U);
}

TEST(CtraceUnitTests, testCtraceRunMetaAcceptsOnlyConstrainedFormattedFallbacks)
{
  struct Fallback {
    const char* type;
    const char* path;
    bool data;
  };
  const std::vector<Fallback> accepted{
      {"dwt", "data#0", true},           {"itm", "timestamps", false},
      {"dwt", "timestamps", false},      {"exception", "exceptions", false},
      {"event", "events#0", false},      {"pmu", "events#0", false},
      {"pcsample", "pcsampling", false}, {"dwt", "synchronization", false},
  };
  for (const auto& fallback : accepted) {
    auto reference = routeReference(fallback.type, fallback.path, std::nullopt, 1U);
    if (fallback.data) {
      reference.dataSetupIndex = 0U;
    }
    const auto meta = CtraceRunMeta::fromConfig(formattedConfig({reference}));
    ASSERT_EQ(meta.routes().size(), 1U) << fallback.type << " / " << fallback.path;
    EXPECT_EQ(meta.routes().front().traceBusId, std::optional<std::uint8_t>(1U));
  }

  const std::vector<TraceRunReference> rejected{
      routeReference("overflow", "overflow", std::nullopt, 1U),
      routeReference("global_ts", "timesync", std::nullopt, 1U),
      routeReference("dwt", "instructions", std::nullopt, 1U),
      routeReference("itm", "messages", std::nullopt, 1U),
      routeReference("dwt", "timestamps", std::nullopt, std::nullopt),
  };
  for (const auto& reference : rejected) {
    EXPECT_TRUE(
        metaRejects(formattedConfig({reference}), "requires an ITM route anchor or supported feature fallback"));
  }

  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "nested/core/itm", "core", 1U)}),
                          "anchor path must use '[pname/]itm'"));
  auto nestedData = routeReference("dwt", "nested/core/data#0", "core", 1U);
  nestedData.dataSetupIndex = 0U;
  EXPECT_TRUE(metaRejects(formattedConfig({nestedData}), "requires an ITM route anchor or supported feature fallback"));
  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("event", "timestamps", std::nullopt, 1U)}),
                          "timestamps reference must use type 'itm' or transitional type 'dwt'"));
}

TEST(CtraceUnitTests, testCtraceRunMetaValidatesFormattedRouteBindingsAndIds)
{
  for (const auto id : {0U, 112U, 127U}) {
    auto invalid = routeReference("itm", "itm", std::nullopt, id);
    invalid.error = {"producer diagnostic must not hide the invalid ID"};
    EXPECT_TRUE(metaRejects(formattedConfig({invalid}), "between 1 and 111"));
  }

  const auto sameIdDifferentProcessors = formattedConfig({
      routeReference("itm", "first/itm", "first", 1U),
      routeReference("itm", "second/itm", "second", 1U),
  });
  EXPECT_TRUE(metaRejects(sameIdDifferentProcessors, "conflicting ITM processor bindings"));

  const auto conflictingContentProcessor = formattedConfig({
      routeReference("itm", "core/itm", "core", 1U),
      routeReference("itm", "other/messages", "other", 1U, {1U}),
  });
  EXPECT_TRUE(metaRejects(conflictingContentProcessor, "conflicting ITM processor bindings"));

  const auto sameProcessorDifferentIds = formattedConfig({
      routeReference("itm", "core/itm", "core", 1U),
      routeReference("itm", "core/itm", "core", 2U),
  });
  EXPECT_TRUE(metaRejects(sameProcessorDifferentIds, "bound to multiple CoreSight Trace Bus IDs"));

  auto fallback = routeReference("dwt", "core/data#0", "core", 1U);
  fallback.dataSetupIndex = 0U;
  const auto compatible =
      CtraceRunMeta::fromConfig(formattedConfig({routeReference("itm", "core/itm", "core", 1U), fallback}));
  EXPECT_EQ(compatible.routes().size(), 1U);

  const auto unbound = CtraceRunMeta::fromConfig(formattedConfig({
      routeReference("itm", "itm", std::nullopt, 1U),
      routeReference("itm", "itm", std::nullopt, 111U),
  }));
  ASSERT_EQ(unbound.routes().size(), 2U);
  EXPECT_FALSE(unbound.routes()[0].processorName.has_value());
  EXPECT_FALSE(unbound.routes()[1].processorName.has_value());

  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "itm", std::nullopt, std::nullopt)}),
                          "anchor requires a CoreSight Trace Bus ID"));
  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("dwt", "core/itm", "core", 1U)}),
                          "anchor must use reference type 'itm'"));

  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "core/itm", "core", 1U, {32U})}),
                          "ITM source must be between 0 and 31"));

  const auto adoptedProcessor = CtraceRunMeta::fromConfig(formattedConfig({
      routeReference("itm", "itm", std::nullopt, 1U),
      routeReference("itm", "core/itm", "core", 1U),
  }));
  EXPECT_EQ(adoptedProcessor.routes().front().processorName, std::optional<std::string>("core"));

  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "core/itm", "core", 1U),
                                           routeReference("itm", "other/itm", "core", 1U, {1U})}),
                          "path processor conflicts with pname"));
}

TEST(CtraceUnitTests, testCtraceRunMetaValidatesFormattedSetupInference)
{
  const std::vector<TraceRunSetup> processors{
      makeTimestampSetup("first"),
      makeTimestampSetup("second"),
  };
  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "itm", std::nullopt, 1U)}, processors),
                          "pname is required for a formatted ctrace-ref"));
  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "ghost/itm", "ghost", 1U)}, processors),
                          "has no matching active ctrace-setup processor"));

  auto unnamedFragment = makeTimestampSetup(std::nullopt);
  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "first/itm", "first", 1U)},
                                          {processors[0], processors[1], unnamedFragment}),
                          "pname is required for active ctrace-setup fragments"));

  const auto inferred = CtraceRunMeta::fromConfig(
      formattedConfig({routeReference("itm", "itm", std::nullopt, 1U)}, {makeTimestampSetup("core")}));
  ASSERT_EQ(inferred.routes().size(), 1U);
  EXPECT_EQ(inferred.routes().front().processorName, std::optional<std::string>("core"));

  EXPECT_TRUE(
      metaRejects(formattedConfig({routeReference("itm", "other/itm", "core", 1U)}, {makeTimestampSetup("core")}),
                  "path processor conflicts with pname"));
  EXPECT_TRUE(
      metaRejects(formattedConfig({routeReference("itm", "other/itm", std::nullopt, 1U)}, {makeTimestampSetup("core")}),
                  "has no matching active ctrace-setup processor"));

  const auto unnamedSetup = makeTimestampSetup(std::nullopt);
  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "first/itm", "first", 1U),
                                           routeReference("itm", "second/itm", "second", 2U)},
                                          {unnamedSetup}),
                          "one unnamed ctrace-setup processor cannot bind multiple formatted pnames"));
  EXPECT_TRUE(metaRejects(
      formattedConfig({routeReference("itm", "itm", std::nullopt, 1U), routeReference("itm", "itm", std::nullopt, 2U)},
                      {unnamedSetup}),
      "one unnamed ctrace-setup processor cannot bind multiple formatted ITM routes"));

  const auto unnamedWithContent = CtraceRunMeta::fromConfig(formattedConfig(
      {routeReference("itm", "core/itm", "core", 1U), routeReference("itm", "core/messages", "core", 1U, {1U})},
      {unnamedSetup}));
  EXPECT_EQ(unnamedWithContent.routes().front().processorName, std::optional<std::string>("core"));
}

TEST(CtraceUnitTests, testCtraceRunMetaMergesRepeatedFormattedSetupFragments)
{
  auto timestamps = makeTimestampSetup("core", 100U, 4U);
  timestamps.featurePaths = {"core/timestamps"};
  TraceRunSetup itm;
  itm.itm = TraceRunItmSetup{3U};
  itm.featurePaths = {"itm"};
  auto config = formattedConfig({routeReference("itm", "core/itm", "core", 1U)}, {timestamps, itm});

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.routes().size(), 1U);
  EXPECT_EQ(meta.routes().front().timestampClockHz, std::optional<std::uint64_t>(100U));
  EXPECT_EQ(meta.routes().front().timestampPrescaler, 4U);
  EXPECT_EQ(meta.routes().front().itmEnableMask, std::optional<std::uint32_t>(3U));

  auto conflictingPrescaler = timestamps;
  conflictingPrescaler.timestamps->timestampPrescaler = 16U;
  EXPECT_TRUE(metaRejects(formattedConfig(config.references, {timestamps, conflictingPrescaler}),
                          "conflicting timestamps.itm-prescaler"));

  auto invalidPrescaler = timestamps;
  invalidPrescaler.timestamps->timestampPrescaler = 2U;
  invalidPrescaler.timestamps->line = 12U;
  EXPECT_TRUE(metaRejects(formattedConfig(config.references, {invalidPrescaler}), "trace.yml(12)"));

  auto conflictingClock = timestamps;
  conflictingClock.timestamps->clockHz = 200U;
  const auto deferredClock =
      CtraceRunMeta::fromConfig(formattedConfig(config.references, {timestamps, conflictingClock}));
  EXPECT_TRUE(deferredClock.routes().front().timestampClockError.has_value());

  auto missingClock = timestamps;
  missingClock.timestamps->clockHz.reset();
  for (const auto reverse : {false, true}) {
    const auto complemented = CtraceRunMeta::fromConfig(
        formattedConfig(config.references, reverse ? std::vector<TraceRunSetup>{timestamps, missingClock}
                                                   : std::vector<TraceRunSetup>{missingClock, timestamps}));
    EXPECT_EQ(complemented.routes().front().timestampClockHz, std::optional<std::uint64_t>(100U));
    EXPECT_FALSE(complemented.routes().front().timestampClockError.has_value());
  }

  auto conflictingItm = itm;
  conflictingItm.itm->enableMask = 5U;
  const auto nonFatalItm = CtraceRunMeta::fromConfig(formattedConfig(config.references, {itm, conflictingItm}));
  EXPECT_FALSE(nonFatalItm.routes().front().itmEnableMask.has_value());
  EXPECT_TRUE(nonFatalItm.routes().front().itmEnableError.has_value());
  ASSERT_EQ(nonFatalItm.warnings().size(), 1U);
  EXPECT_NE(nonFatalItm.warnings().front().message.find("itm.enable"), std::string::npos);

  TraceRunSetup firstData;
  firstData.processorName = "core";
  firstData.data.resize(1U);
  firstData.data.front().size = 2U;
  firstData.featurePaths = {"core/data#0"};
  auto secondData = firstData;
  auto dataReference = routeReference("dwt", "core/data#0", "core", 1U, {0U});
  dataReference.dataSetupIndex = 0U;
  const auto compatibleData =
      CtraceRunMeta::fromConfig(formattedConfig({config.references.front(), dataReference}, {firstData, secondData}));
  ASSERT_EQ(compatibleData.routes().front().sources.size(), 1U);
  EXPECT_EQ(compatibleData.routes().front().sources.front().dataSize, 2U);
  EXPECT_FALSE(compatibleData.routes().front().sources.front().dataSizeError.has_value());

  secondData.data.front().size = 4U;
  const auto conflictingData =
      CtraceRunMeta::fromConfig(formattedConfig({config.references.front(), dataReference}, {firstData, secondData}));
  ASSERT_EQ(conflictingData.routes().front().sources.size(), 1U);
  EXPECT_EQ(conflictingData.routes().front().sources.front().dataSize, TraceRunSchema::kDefaultDwtDataSize);
  EXPECT_EQ(conflictingData.routes().front().sources.front().dataSizeError,
            std::optional<std::string>("conflicting active ctrace-setup data.size values"));

  auto defaultData = firstData;
  defaultData.data.front().size.reset();
  secondData.data.front().size = 2U;
  const auto defaultConflict =
      CtraceRunMeta::fromConfig(formattedConfig({config.references.front(), dataReference}, {defaultData, secondData}));
  EXPECT_EQ(defaultConflict.routes().front().sources.front().dataSizeError,
            std::optional<std::string>("conflicting active ctrace-setup data.size values"));

  secondData = firstData;
  secondData.data.front().sizeError = "invalid setup size";
  const auto adoptedDataError =
      CtraceRunMeta::fromConfig(formattedConfig({config.references.front(), dataReference}, {firstData, secondData}));
  EXPECT_EQ(adoptedDataError.routes().front().sources.front().dataSizeError,
            std::optional<std::string>("invalid setup size"));

  firstData.data.front().sizeError = "first setup error";
  secondData.data.front().sizeError = "second setup error";
  const auto conflictingDataErrors =
      CtraceRunMeta::fromConfig(formattedConfig({config.references.front(), dataReference}, {firstData, secondData}));
  EXPECT_EQ(conflictingDataErrors.routes().front().sources.front().dataSizeError,
            std::optional<std::string>("conflicting active ctrace-setup data.size values"));
}

TEST(CtraceUnitTests, testCtraceRunMetaRetainsItmSetupErrorsIndependentlyOfFragmentOrder)
{
  TraceRunSetup valid;
  valid.processorName = "core";
  valid.itm = TraceRunItmSetup{3U};
  TraceRunSetup malformed = valid;
  malformed.itm->enableMask.reset();
  malformed.itm->enableError = "invalid itm.enable";
  const auto anchor = routeReference("itm", "core/itm", "core", 1U);

  for (const auto format : {TraceRunFormat::Unformatted, TraceRunFormat::Formatted}) {
    for (const auto reverse : {false, true}) {
      auto setups =
          reverse ? std::vector<TraceRunSetup>{malformed, valid} : std::vector<TraceRunSetup>{valid, malformed};
      auto config = formattedConfig({anchor}, std::move(setups));
      config.traceFormat = format;
      const auto meta = CtraceRunMeta::fromConfig(config);
      ASSERT_EQ(meta.routes().size(), 1U);
      EXPECT_FALSE(meta.routes().front().itmEnableMask.has_value());
      EXPECT_EQ(meta.routes().front().itmEnableError, std::optional<std::string>("invalid itm.enable"));
    }
  }

  auto secondMalformed = malformed;
  secondMalformed.itm->enableError = "another itm.enable error";
  for (const auto format : {TraceRunFormat::Unformatted, TraceRunFormat::Formatted}) {
    auto config = formattedConfig({anchor}, {malformed, secondMalformed});
    config.traceFormat = format;
    const auto meta = CtraceRunMeta::fromConfig(config);
    EXPECT_EQ(meta.routes().front().itmEnableError,
              std::optional<std::string>("conflicting active ctrace-setup itm.enable values"));
  }

  auto secondValid = valid;
  secondValid.itm->enableMask = 5U;
  auto unformattedConflict = formattedConfig({anchor}, {valid, secondValid});
  unformattedConflict.traceFormat = TraceRunFormat::Unformatted;
  const auto conflictMeta = CtraceRunMeta::fromConfig(unformattedConflict);
  EXPECT_FALSE(conflictMeta.routes().front().itmEnableMask.has_value());
  EXPECT_EQ(conflictMeta.routes().front().itmEnableError,
            std::optional<std::string>("conflicting active ctrace-setup itm.enable values"));
  ASSERT_EQ(conflictMeta.warnings().size(), 1U);

  TraceRunSetup emptyItm;
  emptyItm.processorName = "core";
  emptyItm.itm = TraceRunItmSetup{};
  const auto emptyMeta = CtraceRunMeta::fromConfig(formattedConfig({anchor}, {emptyItm}));
  EXPECT_FALSE(emptyMeta.routes().front().itmEnableMask.has_value());
  EXPECT_FALSE(emptyMeta.routes().front().itmEnableError.has_value());
}

TEST(CtraceUnitTests, testCtraceRunMetaResolvesDisabledFragmentsLocally)
{
  TraceRunSetup disabled;
  disabled.processorName = "core";
  disabled.disabled = true;
  disabled.ordinal = 4U;
  disabled.featurePaths = {"core/itm"};
  const auto anchor = routeReference("itm", "core/itm", "core", 1U);
  EXPECT_TRUE(metaRejects(formattedConfig({anchor}, {disabled}), "disabled ctrace-setup fragment 4"));

  TraceRunSetup active;
  active.processorName = "core";
  active.itm = TraceRunItmSetup{1U};
  active.featurePaths = {"core/itm"};
  EXPECT_NO_THROW((void)CtraceRunMeta::fromConfig(formattedConfig({anchor}, {disabled, active})));

  const auto shortAnchor = routeReference("itm", "itm", "core", 1U);
  EXPECT_TRUE(metaRejects(formattedConfig({shortAnchor}, {disabled}), "disabled ctrace-setup fragment 4"));

  auto disabledData = disabled;
  disabledData.featurePaths = {"core/data#0"};
  EXPECT_NO_THROW((void)CtraceRunMeta::fromConfig(formattedConfig({anchor}, {disabledData, active})));

  auto disabledControl = disabled;
  disabledControl.featurePaths = {"core/instructions", "core/synchronization"};
  EXPECT_TRUE(
      metaRejects(formattedConfig({routeReference("dwt", "core/instructions/start", "core", 1U)}, {disabledControl}),
                  "resolves only to disabled ctrace-setup fragment"));
  EXPECT_TRUE(
      metaRejects(formattedConfig({routeReference("dwt", "core/synchronization#0", "core", 1U)}, {disabledControl}),
                  "resolves only to disabled ctrace-setup fragment"));
  disabledControl.processorName.reset();
  disabledControl.featurePaths = {"instructions"};
  EXPECT_TRUE(
      metaRejects(formattedConfig({routeReference("dwt", "core/instructions/start", "core", 1U)}, {disabledControl}),
                  "resolves only to disabled ctrace-setup fragment"));
  EXPECT_NO_THROW((void)CtraceRunMeta::fromConfig(formattedConfig(
      {anchor, routeReference("dwt", "other/instructions/start", "core", 1U)}, {disabledControl, active})));

  TraceRunSetup foreignDisabled;
  foreignDisabled.processorName = "other";
  foreignDisabled.disabled = true;
  foreignDisabled.featurePaths = {"other/data#0"};
  auto data = routeReference("dwt", "core/data#0", std::nullopt, 1U);
  data.dataSetupIndex = 0U;
  EXPECT_NO_THROW((void)CtraceRunMeta::fromConfig(formattedConfig({data}, {foreignDisabled})));

  TraceRunConfig unformatted;
  unformatted.references = {anchor};
  unformatted.setups = {disabled};
  EXPECT_TRUE(metaRejects(unformatted, "resolves only to disabled ctrace-setup fragment"));
}

TEST(CtraceUnitTests, testCtraceRunMetaRetainsDiagnosticsWithoutWeakeningRouting)
{
  auto diagnosedAnchor = routeReference("itm", "core/itm", "core", 1U, {32U});
  diagnosedAnchor.error = {"source setup failed"};
  const auto diagnosed = CtraceRunMeta::fromConfig(formattedConfig({diagnosedAnchor}));
  ASSERT_EQ(diagnosed.routes().size(), 1U);
  EXPECT_TRUE(diagnosed.routes().front().sources.empty());
  ASSERT_EQ(diagnosed.routes().front().referenceDiagnostics.size(), 1U);
  EXPECT_EQ(diagnosed.routes().front().referenceDiagnostics.front().message, "source setup failed");

  TraceRunConfig diagnosedSingle;
  diagnosedSingle.references = {diagnosedAnchor};
  const auto single = CtraceRunMeta::fromConfig(diagnosedSingle);
  EXPECT_EQ(single.routes().front().processorName, std::optional<std::string>("core"));
  EXPECT_TRUE(single.routes().front().sources.empty());

  auto duplicateSource = routeReference("itm", "core/itm", "core", 1U, {1U, 1U});
  duplicateSource.error = {"duplicate producer source"};
  const auto duplicate = CtraceRunMeta::fromConfig(formattedConfig({duplicateSource}));
  EXPECT_TRUE(duplicate.routes().front().sources.empty());

  auto streamlessDiagnostic = routeReference("global_ts", "core/timesync", "core", std::nullopt);
  streamlessDiagnostic.warning = {"time sync unavailable"};
  const auto routeDiagnostic =
      CtraceRunMeta::fromConfig(formattedConfig({routeReference("itm", "core/itm", "core", 1U), streamlessDiagnostic}));
  ASSERT_EQ(routeDiagnostic.routes().front().referenceDiagnostics.size(), 1U);
  EXPECT_EQ(routeDiagnostic.routes().front().referenceDiagnostics.front().message, "time sync unavailable");

  auto secondAnchor = routeReference("itm", "other/itm", "other", 2U);
  const auto routedDiagnostic = CtraceRunMeta::fromConfig(
      formattedConfig({routeReference("itm", "core/itm", "core", 1U), secondAnchor, streamlessDiagnostic}));
  ASSERT_EQ(routedDiagnostic.routes().size(), 2U);
  ASSERT_EQ(routedDiagnostic.routes()[0].referenceDiagnostics.size(), 1U);
  EXPECT_EQ(routedDiagnostic.routes()[0].referenceDiagnostics.front().message, "time sync unavailable");
  EXPECT_TRUE(routedDiagnostic.routes()[1].referenceDiagnostics.empty());

  const auto conflictingStreamless = formattedConfig({
      routeReference("itm", "itm", std::nullopt, 1U),
      routeReference("global_ts", "first/timesync", "first", std::nullopt),
      routeReference("overflow", "second/overflow", "second", std::nullopt),
  });
  EXPECT_TRUE(metaRejects(conflictingStreamless, "streamless ctrace-ref cannot be associated"));

  EXPECT_TRUE(metaRejects(formattedConfig({routeReference("itm", "core/itm", "core", 1U),
                                           routeReference("overflow", "core/overflow", "core", 2U)}),
                          "without an ITM route anchor or supported feature fallback"));
}

TEST(CtraceUnitTests, testCtraceRunMetaDefersFormattedOutputMetadataErrors)
{
  auto setup = makeTimestampSetup("core", std::nullopt, std::nullopt);
  setup.timestamps->clockError = "invalid timestamps.clock";
  auto data = routeReference("dwt", "core/data#0", "core", 1U, {0U});
  data.dataSetupIndex = 0U;
  data.dataTypeError = "invalid data-type";
  data.dataSizeError = "invalid size";
  const auto meta =
      CtraceRunMeta::fromConfig(formattedConfig({routeReference("itm", "core/itm", "core", 1U), data}, {setup}));

  ASSERT_EQ(meta.routes().size(), 1U);
  const auto& route = meta.routes().front();
  EXPECT_TRUE(route.timestampsConfigured);
  EXPECT_FALSE(route.timestampClockHz.has_value());
  EXPECT_EQ(route.timestampClockError, std::optional<std::string>("invalid timestamps.clock"));
  EXPECT_EQ(route.timestampPrescaler, TraceRunSchema::kDefaultTimestampPrescaler);
  ASSERT_EQ(route.sources.size(), 1U);
  EXPECT_EQ(route.sources.front().dataTypeError, std::optional<std::string>("invalid data-type"));
  EXPECT_EQ(route.sources.front().dataSizeError, std::optional<std::string>("invalid size"));
}
