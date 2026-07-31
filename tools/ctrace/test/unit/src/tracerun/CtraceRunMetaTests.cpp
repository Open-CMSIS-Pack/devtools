/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "CtraceRunMeta.hpp"
#include "TraceRunConfig.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

TraceRunReference makeReference(std::string type, std::optional<std::string> processorName,
                                std::optional<std::uint32_t> stream, std::vector<std::uint32_t> sources,
                                std::string ctraceRef = "route")
{
  TraceRunReference reference;
  reference.type = std::move(type);
  reference.processorName = std::move(processorName);
  reference.stream = stream;
  reference.sources = std::move(sources);
  reference.ctraceRef = std::move(ctraceRef);
  return reference;
}

TraceRunSetup makeSetup(std::optional<std::string> processorName, std::optional<std::uint64_t> clock = 1U,
                        std::optional<std::uint32_t> prescaler = 1U,
                        std::optional<std::uint32_t> enableMask = std::nullopt)
{
  TraceRunSetup setup;
  setup.processorName = std::move(processorName);
  setup.timestamps = TraceRunTimestampSetup{clock, prescaler};
  if (enableMask.has_value()) {
    setup.itm = TraceRunItmSetup{*enableMask};
  }
  return setup;
}

std::string metaError(const TraceRunConfig& config)
{
  try {
    (void)CtraceRunMeta::fromConfig(config);
  } catch (const std::runtime_error& error) {
    return error.what();
  }
  return {};
}

} // namespace

TEST(CtraceUnitTests, testTimestampPrescalerMetadataDefaults)
{
  const auto ctraceRunMeta = CtraceRunMeta::fromConfig(TraceRunConfig{});
  require(!ctraceRunMeta.timestampPrescaler().has_value(),
          "a missing timestamp setup must leave the metadata prescaler unspecified");

  TraceRunConfig incompleteTraceRun;
  TraceRunSetup incompleteSetup;
  incompleteSetup.timestamps = TraceRunTimestampSetup{};
  incompleteTraceRun.setups.push_back(incompleteSetup);
  const auto missingPrescalerMeta = CtraceRunMeta::fromConfig(incompleteTraceRun);
  require(missingPrescalerMeta.timestampPrescaler() ==
              std::optional<std::uint32_t>(TraceRunSchema::kDefaultTimestampPrescaler),
          "timestamp setup must resolve an omitted prescaler to the specified default");
  TraceRunConfig traceRun;
  TraceRunSetup setup;
  setup.timestamps = TraceRunTimestampSetup{std::nullopt, 4U};
  traceRun.setups.push_back(setup);
  const auto prescalerOnlyMeta = CtraceRunMeta::fromConfig(traceRun);
  require(prescalerOnlyMeta.timestampPrescaler() == std::optional<std::uint32_t>(4U),
          "explicit timestamp prescaler metadata mismatch");
}

TEST(CtraceUnitTests, testCtraceRunMetaRejectsInvalidReferences)
{
  struct Case {
    TraceRunReference reference;
    const char* message;
  };
  std::vector<Case> cases;
  cases.push_back({makeReference("dwt", std::nullopt, 1U, {1U, 1U}), "duplicate value in source array"});
  cases.push_back(
      {makeReference("itm", std::nullopt, 0U, {1U}), "stream must be a CoreSight ATB trace ID between 1 and 111"});
  cases.push_back(
      {makeReference("itm", std::nullopt, 1U, {1U, 2U}), "source arrays are only supported for DWT references"});
  cases.push_back({makeReference("itm", std::nullopt, 1U, {32U}), "ITM source must be between 0 and 31"});

  for (const auto& testCase : cases) {
    TraceRunConfig config;
    config.path = "trace.yml";
    config.references = {testCase.reference};
    EXPECT_NE(metaError(config).find(testCase.message), std::string::npos);

    config.references.front().line = 7U;
    EXPECT_NE(metaError(config).find("trace.yml(7)"), std::string::npos);
  }

  TraceRunConfig diagnosed;
  diagnosed.references.push_back(makeReference("itm", std::nullopt, 0U, {99U}));
  diagnosed.references.front().error = "producer rejected this route";
  EXPECT_NO_THROW((void)CtraceRunMeta::fromConfig(diagnosed));
}

TEST(CtraceUnitTests, testCtraceRunMetaRejectsInvalidPrescalers)
{
  TraceRunConfig config;
  config.path = "trace.yml";
  config.setups.push_back(makeSetup(std::nullopt, 1U, 2U));
  EXPECT_NE(metaError(config).find("ctrace-setup timestamps.itm-prescaler must be one of"), std::string::npos);

  config.setups.front().timestamps->line = 12U;
  EXPECT_NE(metaError(config).find("trace.yml(12): 'timestamps.itm-prescaler' must be one of"), std::string::npos);
}

TEST(CtraceUnitTests, testCtraceRunMetaRejectsAmbiguousProcessorIdentity)
{
  TraceRunConfig duplicate;
  duplicate.path = "trace.yml";
  duplicate.setups = {makeSetup(std::nullopt), makeSetup(std::nullopt)};
  EXPECT_NE(metaError(duplicate).find("duplicate active ctrace-setup for pname '<unnamed>'"), std::string::npos);
  duplicate.setups.back().line = 9U;
  EXPECT_NE(metaError(duplicate).find("duplicate active 'ctrace-setup' for pname '<unnamed>'"), std::string::npos);

  TraceRunConfig multiUnnamed;
  multiUnnamed.path = "trace.yml";
  multiUnnamed.setups = {makeSetup("a"), makeSetup("b")};
  multiUnnamed.references.push_back(makeReference("itm", std::nullopt, 1U, {1U}));
  EXPECT_NE(metaError(multiUnnamed).find("pname is required for every ctrace-setup and ctrace-ref"), std::string::npos);

  TraceRunConfig multiUnmatched = multiUnnamed;
  multiUnmatched.references.front().processorName = "c";
  EXPECT_NE(metaError(multiUnmatched).find("pname 'c' has no matching ctrace-setup"), std::string::npos);
}

TEST(CtraceUnitTests, testCtraceRunMetaRejectsSingleAndReferenceOnlyIdentityConflicts)
{
  TraceRunConfig namedSetup;
  namedSetup.path = "trace.yml";
  namedSetup.setups.push_back(makeSetup("a"));
  namedSetup.references.push_back(makeReference("itm", "b", 1U, {1U}));
  EXPECT_NE(metaError(namedSetup).find("no matching ctrace-setup pname 'a'"), std::string::npos);

  TraceRunConfig unnamedSetup;
  unnamedSetup.path = "trace.yml";
  unnamedSetup.setups.push_back(makeSetup(std::nullopt));
  unnamedSetup.references = {
      makeReference("itm", "a", 1U, {1U}),
      makeReference("itm", "b", 2U, {2U}),
  };
  EXPECT_NE(metaError(unnamedSetup).find("pname is required for ctrace-setup"), std::string::npos);

  TraceRunConfig referencesOnly;
  referencesOnly.path = "trace.yml";
  referencesOnly.references = unnamedSetup.references;
  referencesOnly.references.push_back(makeReference("dwt", std::nullopt, 3U, {0U}));
  EXPECT_NE(metaError(referencesOnly).find("pname is required for every ctrace-ref"), std::string::npos);

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
      makeSetup("a", 100U, 4U, 1U),
      makeSetup("b", 200U, 4U, 2U),
  };
  config.references = {
      makeReference("itm", "a", 5U, {1U}),
      makeReference("itm", "b", 5U, {2U}),
  };

  const auto meta = CtraceRunMeta::fromConfig(config);
  EXPECT_FALSE(meta.timestampClockHz().has_value());
  EXPECT_EQ(meta.timestampPrescaler(), std::optional<std::uint32_t>(4U));
  EXPECT_FALSE(meta.itmEnableMask().has_value());
  EXPECT_TRUE(meta.itmEnableMasksByTraceBusId().empty());
  ASSERT_EQ(meta.timestampsByTraceBusId().size(), 1U);
  EXPECT_FALSE(meta.timestampsByTraceBusId().at(5U).clockHz.has_value());
  EXPECT_NE(meta.timestampsByTraceBusId().at(5U).clockError->find("multiple processors"), std::string::npos);
}

TEST(CtraceUnitTests, testCtraceRunMetaMapsDistinctPrescalersPerStream)
{
  TraceRunConfig config;
  config.setups = {
      makeSetup("a", 100U, 4U),
      makeSetup("b", 100U, 16U),
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
  EXPECT_NE(metaError(config).find("trace.yml(23): CoreSight Trace Bus ID 5"), std::string::npos);
}

TEST(CtraceUnitTests, testCtraceRunMetaResolvesDwtDataAndDefaults)
{
  TraceRunConfig config;
  config.path = "trace.yml";
  auto setup = makeSetup("core");
  setup.data.resize(2U);
  setup.data[0].symbolType = "signed int";
  setup.data[0].symbolSize = 2U;
  setup.data[0].symbolTypeError = "type warning";
  setup.data[0].symbolSizeError = "size warning";
  config.setups.push_back(setup);

  auto configured = makeReference("dwt", "core", 1U, {0U}, "core/data#0");
  configured.dataSetupIndex = 0U;
  configured.symbolAddress = 0x20000000U;
  auto missingIndex = makeReference("dwt", "core", 1U, {1U}, "core/data");
  auto outOfRange = makeReference("dwt", "core", 1U, {2U}, "core/data#9");
  outOfRange.dataSetupIndex = 9U;
  config.references = {configured, missingIndex, outOfRange};

  const auto meta = CtraceRunMeta::fromConfig(config);
  ASSERT_EQ(meta.sources().size(), 3U);
  EXPECT_EQ(meta.configPath(), "trace.yml");
  EXPECT_EQ(meta.sources()[0].valueType, "signed int");
  EXPECT_EQ(meta.sources()[0].valueSize, 2U);
  EXPECT_EQ(meta.sources()[0].symbolAddress, std::optional<std::uint64_t>(0x20000000U));
  EXPECT_EQ(meta.sources()[0].symbolTypeError, std::optional<std::string>("type warning"));
  EXPECT_EQ(meta.sources()[0].symbolSizeError, std::optional<std::string>("size warning"));
  EXPECT_EQ(meta.sources()[1].valueType, std::string(TraceRunSchema::kDefaultDwtDataType));
  EXPECT_EQ(meta.sources()[2].valueSize, TraceRunSchema::kDefaultDwtDataSize);
  EXPECT_EQ(meta.resolveSource("dwt", 1U, 0U), &meta.sources()[0]);
  EXPECT_EQ(meta.resolveSource("itm", std::nullopt, 0U), nullptr);
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
  EXPECT_EQ(meta.sources().front().valueType, std::string(TraceRunSchema::kDefaultDwtDataType));
}
