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
