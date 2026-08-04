/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdSessionTestSupport.h"
#include "TestPath.h"
#include "TestSupport.h"
#include "TraceRunTestSupport.h"
#include <gtest/gtest.h>
#include "CliOptions.h"
#include "CtraceRunMeta.h"
#include "FileDecodeJob.h"
#include "TraceDirectoryJob.h"
#include "TraceRunConfig.h"
#include "TraceRunConfigReader.h"
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** @brief Creates the default trace-run configuration used by directory tests. */
static TraceRunConfig defaultTraceRunConfig()
{
  TraceRunConfig config;
  config.setups.push_back(TraceRunTestSupport::makeTimestampSetup(std::nullopt, 100000000U));
  return config;
}

/** @brief Supplies deterministic trace-run configurations to directory-job tests. */
class TestTraceRunConfigReader final : public TraceRunConfigReader {
public:
  /** @brief Creates a reader with fixed data and an optional failure mode. */
  explicit TestTraceRunConfigReader(TraceRunConfig config = defaultTraceRunConfig(), bool fail = false)
    : m_config(std::move(config)), m_fail(fail)
  {
  }

  /** @brief Records the path and returns or rejects the configured data. */
  TraceRunConfig read(const std::string& path) const override
  {
    m_paths.push_back(path);
    if (m_fail) {
      throw std::runtime_error("synthetic config failure");
    }
    auto config = m_config;
    config.path = path;
    return config;
  }

  /** @brief Returns every configuration path requested from this reader. */
  const std::vector<std::string>& paths() const
  {
    return m_paths;
  }

private:
  mutable std::vector<std::string> m_paths;
  TraceRunConfig m_config;
  bool m_fail = false;
};

/** @brief Writes trace-run and raw-input pairs for selected test targets. */
static void writeTraceInputs(const std::filesystem::path& traceDirectory,
                             std::initializer_list<std::string_view> targetNames)
{
  for (const auto targetName : targetNames) {
    const auto target = std::string(targetName);
    writeTestFile(traceDirectory / (target + ".ctrace-run.yml"), "ctrace-run:\n");
    writeTestFile(traceDirectory / (target + ".SWO.raw"));
  }
}

TEST(CtraceUnitTests, testTraceDirectoryTargetAndOutputNames)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-job-test");
  const auto& root = temporaryPath.path();
  const auto traceDir = root / ".trace";
  writeTraceInputs(traceDir, {"Alpha", "Beta"});

  CliOptions options;
  options.traceDir = traceDir.string();
  options.targetName = "Alpha";
  options.outputFormat = OutputFormat::All;

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader;
  TraceDirectoryJob job(options, diagnostics, reader);
  const auto checkpoint = diagnostics.failureCount();
  job.run();

  require(diagnostics.failureCount() == checkpoint, "TraceDirectoryJob target run failed");
  require(reader.paths().size() == 1U, "TraceDirectoryJob should read one selected YAML file");
  require(std::filesystem::path(reader.paths()[0]).filename() == "Alpha.ctrace-run.yml",
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
  writeTraceInputs(traceDir, {"Alpha", "Beta"});

  CliOptions batchOptions;
  batchOptions.traceDir = traceDir.string();

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader batchReader;
  TraceDirectoryJob batchJob(batchOptions, diagnostics, batchReader);
  const auto batchCheckpoint = diagnostics.failureCount();
  batchJob.run();

  require(diagnostics.failureCount() == batchCheckpoint, "TraceDirectoryJob batch check failed");
  require(batchReader.paths().size() == 2U, "TraceDirectoryJob batch should read every trace-run file");
  require(!std::filesystem::exists(traceDir / "Alpha.SWO.csv"), "check-only batch should not create CSV output");
  require(!std::filesystem::exists(traceDir / "Alpha.ctf"), "check-only batch should not create CTF output");

  writeTestFile(traceDir / "Broken.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Broken.SWO.raw", std::string{static_cast<char>(0x01), 'A'});

  CliOptions brokenOptions;
  brokenOptions.traceDir = traceDir.string();
  brokenOptions.targetName = "Broken";

  TestTraceRunConfigReader brokenReader;
  TraceDirectoryJob brokenJob(brokenOptions, diagnostics, brokenReader);
  const auto brokenCheckpoint = diagnostics.failureCount();
  brokenJob.run();
  require(diagnostics.failureCount() > brokenCheckpoint,
          "check-only trace directory should fail on decoder error packets");
}

TEST(CtraceUnitTests, testTraceDirectoryReportsGenerationDiagnosticsAndMissingSwo)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-diagnostics-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.TB.raw", "unsupported");

  TraceRunConfig config;
  auto reported = TraceRunTestSupport::makeReference("event", "core", 3U, {}, "core/event");
  reported.info = "producer note";
  reported.warning = "producer warning";
  reported.error = "producer error";
  TraceRunReference emptyError = reported;
  emptyError.ctraceRef = "core/pmu";
  emptyError.type = "pmu";
  emptyError.info.reset();
  emptyError.warning = "";
  emptyError.error = "";
  auto channelZero = TraceRunTestSupport::makeReference("itm", std::nullopt, std::nullopt, {0U}, "core/itm");
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
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_TRUE(diagnostics.contains("trace-run-generation-info"));
  EXPECT_TRUE(diagnostics.contains("trace-run-generation-warning"));
  EXPECT_TRUE(diagnostics.contains("trace-run-generation-error"));
  EXPECT_TRUE(diagnostics.contains("unsupported-trace-channel"));
  EXPECT_TRUE(diagnostics.contains("missing-swo-raw-input"));
}

TEST(CtraceUnitTests, testTraceDirectoryReportsConfigFailureAndRequiresDirectory)
{
  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader({}, true);
  EXPECT_THROW(TraceDirectoryJob(CliOptions{}, diagnostics, reader).run(), std::runtime_error);

  const TemporaryTestPath temporaryPath("ctrace-trace-directory-config-failure-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "invalid");
  CliOptions options;
  options.traceDir = traceDir.string();
  TraceDirectoryJob(options, diagnostics, reader).run();
  EXPECT_TRUE(diagnostics.contains("solution-set-failed"));
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
  EXPECT_TRUE(diagnostics.contains("ctf-timestamp-clock-missing"));
}

TEST(CtraceUnitTests, testFileDecodeJobUsesPerStreamPrescalers)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-prescalers-test");
  const auto rawPath = temporaryPath.path() / "empty.SWO.raw";
  writeTestFile(rawPath);

  TraceRunConfig config;
  config.setups = {
      TraceRunTestSupport::makeTimestampSetup("first", 100U, 4U),
      TraceRunTestSupport::makeTimestampSetup("second", 100U, 16U),
  };
  config.references = {
      TraceRunTestSupport::makeReference("itm", "first", 1U, {1U}, "first/itm"),
      TraceRunTestSupport::makeReference("itm", "second", 2U, {1U}, "second/itm"),
  };

  CollectingDiagnosticSink diagnostics;
  FileDecodeJob job(CliOptions{}, rawPath, diagnostics, CtraceRunMeta::fromConfig(config));
  EXPECT_NO_THROW(job.run());
  EXPECT_TRUE(diagnostics.contains("timestamp-prescaler"));
}

TEST(CtraceUnitTests, testFileDecodeJobAbortsOutputsAfterFatalDecoderError)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-fatal-test");
  const auto rawPath = temporaryPath.path() / "fatal.SWO.raw";
  writeTestFile(rawPath, "x");
  CliOptions options;
  options.outputFormat = OutputFormat::Csv;
  CollectingDiagnosticSink diagnostics;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes = {{OCSD_RESP_FATAL_SYS_ERR, 1U}};

  FileDecodeJob job(options, rawPath, diagnostics, CtraceRunMeta::fromConfig({}),
                    OpenCsdSessionTestSupport::scriptedFactory(script));
  EXPECT_NO_THROW(job.run());
  EXPECT_GT(diagnostics.failureCount(), 0U);
  EXPECT_FALSE(std::filesystem::exists(temporaryPath.path() / "fatal.SWO.csv"));
}

#if defined(__linux__)
TEST(CtraceUnitTests, testFileDecodeJobReportsRawInputReadFailures)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-read-failure-test");
  temporaryPath.createDirectory();
  CollectingDiagnosticSink diagnostics;
  FileDecodeJob job(CliOptions{}, temporaryPath.path(), diagnostics, CtraceRunMeta::fromConfig({}));
  EXPECT_THROW(job.run(), std::runtime_error);
}
#endif
