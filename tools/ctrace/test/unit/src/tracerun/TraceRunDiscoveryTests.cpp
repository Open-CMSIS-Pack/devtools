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
#include <filesystem>
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
  writeTestFile(traceDir / "Alpha..raw");
  std::filesystem::create_directories(traceDir / "ignored.ctrace-run.yml");
  std::filesystem::create_directories(traceDir / "Alpha.SWO.raw.dir");

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
      "",
      ".",
      "..",
      "../Alpha",
      "..\\Alpha",
      "/Alpha",
      "C:Alpha",
      "C:/Alpha",
      "C:\\Alpha",
      "\\\\server\\share",
      "Alpha:stream",
      "Alpha?Beta",
      "Alpha.",
      "Alpha ",
      "NUL",
      "con.txt",
      "PRN",
      "AUX",
      "CONIN$",
      "CONOUT$",
      "COM1.log",
      "LPT9",
      "bad\x01name",
  };
  for (const auto& unsafeTarget : unsafeTargets) {
    require(throwsWithMessage([&] { (void)TraceRunDiscovery::selectConfigFiles(traceDir, unsafeTarget); },
                              "solution-set name"),
            "TraceRunDiscovery should reject unsafe target name: " + unsafeTarget);
  }

  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(traceDir, std::string("Missing")), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(traceDir, std::string("ABCD")), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(traceDir, std::string("COM0")), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(traceDir / "missing", std::nullopt), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::solutionSetName("wrong.yml"), std::runtime_error);
  EXPECT_THROW((void)TraceRunDiscovery::solutionSetName(".ctrace-run.yml"), std::runtime_error);

  const TemporaryTestPath emptyPath("ctrace-trace-run-discovery-empty-test");
  emptyPath.createDirectory();
  writeTestFile(emptyPath.path() / ".ctrace-run.yml");
  EXPECT_THROW((void)TraceRunDiscovery::selectConfigFiles(emptyPath.path(), std::nullopt), std::runtime_error);
  EXPECT_TRUE(TraceRunDiscovery::rawInputs("parentless.ctrace-run.yml").empty());
}
