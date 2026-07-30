/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.hpp"
#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "TraceRunDiscovery.hpp"
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

TEST(CtraceUnitTests, testTraceRunDiscovery)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-run-discovery-test");
  const auto& traceDir = temporaryPath.path();
  writeTestFile(traceDir / "Beta.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.SWO.raw");
  writeTestFile(traceDir / "Alpha.TB.raw");
  writeTestFile(traceDir / "Alpha.ER.raw");
  writeTestFile(traceDir / "Alpha.swo.raw");
  writeTestFile(traceDir / "Alpha.custom.raw");
  writeTestFile(traceDir / "unrelated.raw");

  const auto batch = TraceRunDiscovery::selectConfigFiles(traceDir, std::nullopt);
  require(batch.size() == 2U, "TraceRunDiscovery batch configuration count mismatch");
  require(batch[0].filename() == "Alpha.ctrace-run.yml", "TraceRunDiscovery batch sort mismatch");
  require(batch[1].filename() == "Beta.ctrace-run.yml", "TraceRunDiscovery second batch item mismatch");

  const auto selected = TraceRunDiscovery::selectConfigFiles(traceDir, std::string("Alpha"));
  require(selected.size() == 1U, "TraceRunDiscovery target selection count mismatch");
  require(selected[0].filename() == "Alpha.ctrace-run.yml", "TraceRunDiscovery target path mismatch");
  require(TraceRunDiscovery::solutionSetName(selected[0]) == "Alpha", "TraceRunDiscovery solution-set mismatch");

  const auto rawInputs = TraceRunDiscovery::rawInputs(selected[0]);
  require(rawInputs.size() == 3U, "TraceRunDiscovery must accept only the specified trace channels");
  require(rawInputs[0].channel == "ER", "TraceRunDiscovery ER channel mismatch");
  require(rawInputs[1].channel == "SWO", "TraceRunDiscovery SWO channel mismatch");
  require(rawInputs[2].channel == "TB", "TraceRunDiscovery TB channel mismatch");

  const std::vector<std::string> unsafeTargets{
      "../Alpha",          "..\\Alpha",    "/Alpha",     "C:Alpha", "C:/Alpha", "C:\\Alpha",
      "\\\\server\\share", "Alpha:stream", "Alpha?Beta", "Alpha.",  "NUL",      "COM1.log",
  };
  for (const auto& unsafeTarget : unsafeTargets) {
    bool rejected = false;
    try {
      (void)TraceRunDiscovery::selectConfigFiles(traceDir, unsafeTarget);
    } catch (const std::runtime_error& error) {
      rejected = std::string(error.what()).find("solution-set name") != std::string::npos;
    }
    require(rejected, "TraceRunDiscovery should reject unsafe target name: " + unsafeTarget);
  }
}
