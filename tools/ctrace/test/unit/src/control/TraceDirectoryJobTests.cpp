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
#include "CtraceRunMeta.hpp"
#include "FileDecodeJob.hpp"
#include "OpenCsdErrorController.hpp"
#include "OpenCsdItmSession.hpp"
#include "OpenCsdPacketCollector.hpp"
#include "TraceDirectoryJob.hpp"
#include "TraceRunConfig.hpp"
#include "TraceRunConfigReader.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
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

class StaticTraceRunConfigReader final : public TraceRunConfigReader {
public:
  explicit StaticTraceRunConfigReader(TraceRunConfig config, bool fail = false)
    : config_(std::move(config)), fail_(fail)
  {
  }

  TraceRunConfig read(const std::string& path) const override
  {
    if (fail_) {
      throw std::runtime_error("synthetic config failure");
    }
    auto config = config_;
    config.path = path;
    return config;
  }

private:
  TraceRunConfig config_;
  bool fail_ = false;
};

class FatalOpenCsdSession final : public OpenCsdItmSessionInterface {
public:
  ocsd_datapath_resp_t pushData(ocsd_trc_index_t, std::uint32_t, const std::uint8_t*, std::uint32_t& processed) override
  {
    processed = 1U;
    return OCSD_RESP_FATAL_SYS_ERR;
  }

  ocsd_datapath_resp_t flush() override
  {
    return OCSD_RESP_CONT;
  }
  ocsd_datapath_resp_t reset() override
  {
    return OCSD_RESP_CONT;
  }
  ocsd_datapath_resp_t endOfTrace() override
  {
    return OCSD_RESP_CONT;
  }
};

bool hasDiagnostic(const CollectingDiagnosticSink& diagnostics, const std::string& code)
{
  for (const auto& event : diagnostics.events()) {
    if (event.code == code) {
      return true;
    }
  }
  return false;
}

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

TEST(CtraceUnitTests, testTraceDirectoryReportsGenerationDiagnosticsAndMissingSwo)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-diagnostics-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.TB.raw", "unsupported");

  TraceRunConfig config;
  TraceRunReference reported;
  reported.ctraceRef = "core/event";
  reported.type = "event";
  reported.processorName = "core";
  reported.stream = 3U;
  reported.info = "producer note";
  reported.warning = "producer warning";
  reported.error = "producer error";
  TraceRunReference emptyError = reported;
  emptyError.ctraceRef = "core/pmu";
  emptyError.type = "pmu";
  emptyError.info.reset();
  emptyError.warning = "";
  emptyError.error = "";
  TraceRunReference channelZero;
  channelZero.ctraceRef = "core/itm";
  channelZero.type = "itm";
  channelZero.sources = {0U};
  channelZero.error = "ignored channel zero";
  TraceRunReference noStream = reported;
  noStream.ctraceRef = "core/no-stream";
  noStream.stream.reset();
  noStream.warning.reset();
  noStream.error.reset();
  config.references = {reported, emptyError, channelZero, noStream};

  CliOptions options;
  options.traceDir = traceDir.string();
  CollectingDiagnosticSink diagnostics;
  StaticTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_TRUE(hasDiagnostic(diagnostics, "trace-run-generation-info"));
  EXPECT_TRUE(hasDiagnostic(diagnostics, "trace-run-generation-warning"));
  EXPECT_TRUE(hasDiagnostic(diagnostics, "trace-run-generation-error"));
  EXPECT_TRUE(hasDiagnostic(diagnostics, "unsupported-trace-channel"));
  EXPECT_TRUE(hasDiagnostic(diagnostics, "missing-swo-raw-input"));
}

TEST(CtraceUnitTests, testTraceDirectoryReportsConfigFailureAndRequiresDirectory)
{
  CollectingDiagnosticSink diagnostics;
  StaticTraceRunConfigReader reader({}, true);
  EXPECT_THROW(TraceDirectoryJob(CliOptions{}, diagnostics, reader).run(), std::runtime_error);

  const TemporaryTestPath temporaryPath("ctrace-trace-directory-config-failure-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "invalid");
  CliOptions options;
  options.traceDir = traceDir.string();
  TraceDirectoryJob(options, diagnostics, reader).run();
  EXPECT_TRUE(hasDiagnostic(diagnostics, "solution-set-failed"));
}

TEST(CtraceUnitTests, testFileDecodeJobHandlesMissingInputAndDisabledCtf)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-control-test");
  CollectingDiagnosticSink diagnostics;
  CliOptions checkOnly;
  FileDecodeJob missing(checkOnly, temporaryPath.path() / "missing.raw", diagnostics, CtraceRunMeta::fromConfig({}));
  EXPECT_THROW(missing.run(), std::runtime_error);

  const auto rawPath = temporaryPath.path() / "empty.SWO.raw";
  writeTestFile(rawPath);
  CliOptions ctf;
  ctf.outputFormat = OutputFormat::Ctf;
  FileDecodeJob disabled(ctf, rawPath, diagnostics, CtraceRunMeta::fromConfig({}));
  EXPECT_NO_THROW(disabled.run());
  EXPECT_TRUE(hasDiagnostic(diagnostics, "ctf-timestamp-clock-missing"));
}

TEST(CtraceUnitTests, testFileDecodeJobUsesPerStreamPrescalers)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-prescalers-test");
  const auto rawPath = temporaryPath.path() / "empty.SWO.raw";
  writeTestFile(rawPath);

  TraceRunConfig config;
  TraceRunSetup first;
  first.processorName = "first";
  first.timestamps = TraceRunTimestampSetup{100U, 4U};
  TraceRunSetup second;
  second.processorName = "second";
  second.timestamps = TraceRunTimestampSetup{100U, 16U};
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

  CollectingDiagnosticSink diagnostics;
  FileDecodeJob job(CliOptions{}, rawPath, diagnostics, CtraceRunMeta::fromConfig(config));
  EXPECT_NO_THROW(job.run());
  EXPECT_TRUE(hasDiagnostic(diagnostics, "timestamp-prescaler"));
}

TEST(CtraceUnitTests, testFileDecodeJobAbortsOutputsAfterFatalDecoderError)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-fatal-test");
  const auto rawPath = temporaryPath.path() / "fatal.SWO.raw";
  writeTestFile(rawPath, "x");
  CliOptions options;
  options.outputFormat = OutputFormat::Csv;
  CollectingDiagnosticSink diagnostics;
  const OpenCsdItmSessionFactory factory = [](OpenCsdPacketCollector&, OpenCsdErrorController&) {
    return std::make_unique<FatalOpenCsdSession>();
  };

  FileDecodeJob job(options, rawPath, diagnostics, CtraceRunMeta::fromConfig({}), factory);
  EXPECT_NO_THROW(job.run());
  EXPECT_GT(diagnostics.fatalCount(), 0U);
  EXPECT_FALSE(std::filesystem::exists(temporaryPath.path() / "fatal.SWO.csv"));
}

#if defined(__linux__)
TEST(CtraceUnitTests, testFileDecodeJobReportsRawInputReadFailures)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-read-failure-test");
  std::filesystem::create_directories(temporaryPath.path());
  CollectingDiagnosticSink diagnostics;
  FileDecodeJob job(CliOptions{}, temporaryPath.path(), diagnostics, CtraceRunMeta::fromConfig({}));
  EXPECT_THROW(job.run(), std::runtime_error);
}
#endif
