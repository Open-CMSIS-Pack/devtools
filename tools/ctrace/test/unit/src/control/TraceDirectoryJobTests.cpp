/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestPath.hpp"
#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "CliOptions.hpp"
#include "TraceDirectoryJob.hpp"
#include "TraceRunConfig.hpp"
#include "TraceRunConfigReader.hpp"
#include <filesystem>
#include <string>
#include <vector>

class RecordingTraceRunConfigReader final : public TraceRunConfigReader {
public:
  TraceRunConfig read(const std::string& path) const override
  {
    paths.push_back(path);
    TraceRunConfig config;
    config.path = path;
    TraceRunSetup setup;
    setup.timestamps = TraceRunTimestampSetup{100000000U, 1U};
    config.setups.push_back(setup);
    return config;
  }

  mutable std::vector<std::string> paths;
};

TEST(CtraceUnitTests, testTraceDirectoryTargetAndOutputNames)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-job-test");
  const auto& root = temporaryPath.path();
  const auto traceDir = root / ".trace";
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.SWO.raw");
  writeTestFile(traceDir / "Beta.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Beta.SWO.raw");

  CliOptions options;
  options.traceDir = traceDir.string();
  options.targetName = "Alpha";
  options.outputFormat = OutputFormat::All;

  CollectingDiagnosticSink diagnostics;
  RecordingTraceRunConfigReader reader;
  TraceDirectoryJob job(options, diagnostics, reader);
  const auto checkpoint = diagnostics.fatalCount();
  job.run();

  require(diagnostics.fatalCount() == checkpoint, "TraceDirectoryJob target run failed");
  require(reader.paths.size() == 1U, "TraceDirectoryJob should read one selected YAML file");
  require(std::filesystem::path(reader.paths[0]).filename() == "Alpha.ctrace-run.yml",
          "TraceDirectoryJob reader path mismatch");
  require(std::filesystem::is_regular_file(traceDir / "Alpha.SWO.csv"), "TraceDirectoryJob CSV output name mismatch");
  require(std::filesystem::is_regular_file(traceDir / "Alpha.ctf" / "metadata"),
          "TraceDirectoryJob CTF output name mismatch");
  require(std::filesystem::is_regular_file(traceDir / "Alpha.SWO.traceanalysis.xml"),
          "TraceDirectoryJob XML output name mismatch");
  require(!std::filesystem::exists(traceDir / "Beta.SWO.csv"),
          "TraceDirectoryJob should not process unselected target");
}

TEST(CtraceUnitTests, testTraceDirectoryBatchCheckAndExplicitConfig)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-check-test");
  const auto& root = temporaryPath.path();
  const auto traceDir = root / ".trace";
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.SWO.raw");
  writeTestFile(traceDir / "Beta.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Beta.SWO.raw");

  CliOptions batchOptions;
  batchOptions.traceDir = traceDir.string();

  CollectingDiagnosticSink diagnostics;
  RecordingTraceRunConfigReader batchReader;
  TraceDirectoryJob batchJob(batchOptions, diagnostics, batchReader);
  const auto batchCheckpoint = diagnostics.fatalCount();
  batchJob.run();

  require(diagnostics.fatalCount() == batchCheckpoint, "TraceDirectoryJob batch check failed");
  require(batchReader.paths.size() == 2U, "TraceDirectoryJob batch should read every trace-run file");
  require(!std::filesystem::exists(traceDir / "Alpha.SWO.csv"), "check-only batch should not create CSV output");
  require(!std::filesystem::exists(traceDir / "Alpha.ctf"), "check-only batch should not create CTF output");

  writeTestFile(traceDir / "Broken.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Broken.SWO.raw", std::string{static_cast<char>(0x01), 'A'});

  CliOptions brokenOptions;
  brokenOptions.traceDir = traceDir.string();
  brokenOptions.targetName = "Broken";

  RecordingTraceRunConfigReader brokenReader;
  TraceDirectoryJob brokenJob(brokenOptions, diagnostics, brokenReader);
  const auto brokenCheckpoint = diagnostics.fatalCount();
  brokenJob.run();
  require(diagnostics.fatalCount() > brokenCheckpoint,
          "check-only trace directory should fail on decoder error packets");
}
