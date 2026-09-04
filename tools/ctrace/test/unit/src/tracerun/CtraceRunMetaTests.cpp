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
  diagnosed.references.front().error = "producer rejected this route";
  EXPECT_NO_THROW((void)CtraceRunMeta::fromConfig(diagnosed));

  TraceRunConfig diagnosedBinding;
  diagnosedBinding.path = "trace.yml";
  diagnosedBinding.setups.push_back(makeTimestampSetup("core"));
  diagnosedBinding.references.push_back(makeReference("itm", "unusable", 0U, {99U}));
  diagnosedBinding.references.front().error = "producer rejected this route";
  const auto meta = CtraceRunMeta::fromConfig(diagnosedBinding);
  EXPECT_EQ(meta.processorCount(), 1U);
  EXPECT_TRUE(meta.sources().empty());
  EXPECT_TRUE(meta.timestampsByTraceBusId().empty());
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

TEST(CtraceUnitTests, testCtraceRunMetaResolvesDwtComparatorRegistersPerStream)
{
  TraceRunConfig config;
  auto instructionRange = makeReference("dwt", std::nullopt, 1U, {0U, 1U}, "instructions:start#0");
  instructionRange.registers = {
      {"DWT_COMP0", 0x08001234U},
      {"DWT_COMP1", 0x00005678U, 0x0000ffffU},
      {"DWT_COMP1", 0x08010000U, 0xffff0000U},
      {"DWT_FUNCTION0", 0x0000000fU},
  };
  auto second = makeReference("dwt", std::nullopt, 2U, {2U}, "data#1");
  second.dataSetupIndex = 1U;
  second.registers = {{"DWT_COMP2", 0x20001200U}};
  auto incomplete = makeReference("dwt", std::nullopt, 2U, {3U}, "data#2");
  incomplete.dataSetupIndex = 2U;
  incomplete.registers = {{"DWT_COMP3", 0x00003400U, 0x0000ff00U}};
  config.references = {instructionRange, second, incomplete};

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.sources().size(), 2U);
  EXPECT_EQ(meta.sources()[0].source, 2U);
  EXPECT_EQ(meta.sources()[1].source, 3U);
  ASSERT_EQ(meta.dwtComparatorValuesByTraceBusId().size(), 2U);
  const auto& streamOne = meta.dwtComparatorValuesByTraceBusId().at(1U);
  EXPECT_EQ(streamOne.at(0U), 0x08001234U);
  EXPECT_EQ(streamOne.at(1U), 0x08015678U);
  const auto& streamTwo = meta.dwtComparatorValuesByTraceBusId().at(2U);
  EXPECT_EQ(streamTwo.at(2U), 0x20001200U);
  EXPECT_EQ(streamTwo.count(3U), 0U) << "a partially specified comparator must not be used for reconstruction";
}

TEST(CtraceUnitTests, testCtraceRunMetaRejectsConflictingDwtComparatorRegisters)
{
  TraceRunConfig config;
  config.path = "trace.yml";
  auto first = makeReference("dwt", std::nullopt, 1U, {0U}, "data#0");
  first.dataSetupIndex = 0U;
  first.registers = {{"DWT_COMP0", 0x08001200U}};
  auto second = first;
  second.ctraceRef = "data#1";
  second.dataSetupIndex = 1U;
  second.line = 17U;
  second.registers = {{"DWT_COMP0", 0x20001200U}};
  config.references = {first, second};

  EXPECT_TRUE(metaRejects(config, "trace.yml(17): DWT comparator 0 has conflicting values for Trace Bus ID 1"));
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
  duplicate.setups = {makeTimestampSetup(std::nullopt), makeTimestampSetup(std::nullopt)};
  EXPECT_TRUE(metaRejects(duplicate, "duplicate active ctrace-setup for pname '<unnamed>'"));
  duplicate.setups.back().line = 9U;
  EXPECT_TRUE(metaRejects(duplicate, "duplicate active 'ctrace-setup' for pname '<unnamed>'"));

  TraceRunConfig unnamedMultiProcessor;
  unnamedMultiProcessor.path = "trace.yml";
  unnamedMultiProcessor.setups = {makeTimestampSetup("a"), makeTimestampSetup(std::nullopt)};
  EXPECT_TRUE(metaRejects(unnamedMultiProcessor,
                          "pname is required for every ctrace-setup in a multi-processor configuration"));
}

TEST(CtraceUnitTests, testCtraceRunMetaWarnsForCrossRootProcessorIdentityConflicts)
{
  TraceRunConfig multiUnnamed;
  multiUnnamed.path = "trace.yml";
  multiUnnamed.setups = {makeTimestampSetup("a"), makeTimestampSetup("b")};
  multiUnnamed.references.push_back(makeReference("itm", std::nullopt, 1U, {1U}));
  multiUnnamed.references.front().line = 17U;
  auto diagnosedReference = makeReference("itm", "a", 0U, {99U});
  diagnosedReference.error = "producer rejected this route";
  multiUnnamed.references.push_back(diagnosedReference);
  const auto unnamedMeta = CtraceRunMeta::fromConfig(multiUnnamed);
  EXPECT_EQ(unnamedMeta.processorCount(), 2U);
  EXPECT_TRUE(unnamedMeta.sources().empty());
  ASSERT_EQ(unnamedMeta.warnings().size(), 1U);
  EXPECT_NE(unnamedMeta.warnings().front().message.find("without pname"), std::string::npos);
  ASSERT_EQ(unnamedMeta.warnings().front().context.size(), 4U);
  EXPECT_EQ(unnamedMeta.warnings().front().context[2], (std::pair<std::string, std::string>{"line", "17"}));

  TraceRunConfig multiUnmatched = multiUnnamed;
  multiUnmatched.references.front().processorName = "c";
  const auto unmatchedMeta = CtraceRunMeta::fromConfig(multiUnmatched);
  EXPECT_TRUE(unmatchedMeta.sources().empty());
  ASSERT_EQ(unmatchedMeta.warnings().size(), 1U);
  EXPECT_NE(unmatchedMeta.warnings().front().message.find("no matching ctrace-setup"), std::string::npos);
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
  const auto unnamedMeta = CtraceRunMeta::fromConfig(unnamedSetup);
  EXPECT_EQ(unnamedMeta.processorCount(), 1U);
  EXPECT_EQ(unnamedMeta.sources().size(), 2U);
  ASSERT_EQ(unnamedMeta.warnings().size(), 1U);

  TraceRunConfig referencesOnly;
  referencesOnly.path = "trace.yml";
  referencesOnly.references = unnamedSetup.references;
  auto unnamedDwtReference = makeReference("dwt", std::nullopt, 3U, {0U});
  unnamedDwtReference.dataSetupIndex = 0U;
  referencesOnly.references.push_back(unnamedDwtReference);
  EXPECT_TRUE(metaRejects(referencesOnly, "pname is required for every ctrace-ref"));

  referencesOnly.references.pop_back();
  const auto meta = CtraceRunMeta::fromConfig(referencesOnly);
  EXPECT_EQ(meta.processorCount(), 2U);
  EXPECT_FALSE(meta.timestampClockHz().has_value());
}

TEST(CtraceUnitTests, testCtraceRunMetaMapsDistinctProcessorSettings)
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
  EXPECT_FALSE(meta.timestampClockHz().has_value());
  EXPECT_EQ(meta.timestampPrescaler(), std::optional<std::uint32_t>(4U));
  EXPECT_FALSE(meta.itmEnableMask().has_value());
  ASSERT_EQ(meta.itmEnableMasksByTraceBusId().size(), 1U);
  EXPECT_EQ(meta.itmEnableMasksByTraceBusId().at(5U), 1U);
  ASSERT_EQ(meta.timestampsByTraceBusId().size(), 1U);
  EXPECT_EQ(meta.timestampsByTraceBusId().at(5U).clockHz, std::optional<std::uint64_t>(100U));
  EXPECT_FALSE(meta.timestampsByTraceBusId().at(5U).clockError.has_value());
  EXPECT_EQ(meta.warnings().size(), 2U);
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
  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_TRUE(meta.hasDistinctProcessorPrescalers());
  EXPECT_FALSE(meta.timestampPrescaler().has_value());
  EXPECT_EQ(meta.timestampPrescalersByTraceBusId().at(5U), 4U);
  EXPECT_EQ(meta.timestampPrescalersByTraceBusId().at(6U), 16U);

  config.path = "trace.yml";
  config.references.back().stream = 5U;
  config.references.back().line = 23U;
  const auto conflictingMeta = CtraceRunMeta::fromConfig(config);
  EXPECT_EQ(conflictingMeta.timestampPrescalersByTraceBusId().at(5U), 4U);
  EXPECT_EQ(conflictingMeta.warnings().size(), 2U);
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
