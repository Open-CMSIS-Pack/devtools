/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdSessionTestSupport.h"
#include "TestPath.h"
#include "TestPlatform.h"
#include "TestSupport.h"
#include "TraceRunTestSupport.h"
#include <gtest/gtest.h>
#include "CliOptions.h"
#include "CtraceRunMeta.h"
#include "DiagnosticSink.h"
#include "FileDecodeJob.h"
#include "TraceDirectoryJob.h"
#include "TraceRunConfig.h"
#include "TraceRunConfigReader.h"
#include "opencsd/ocsd_if_types.h"
#include <algorithm>
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
    : m_config(std::move(config)),
      m_fail(fail)
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

/** @brief Finds one diagnostic with an exact message. */
static const DiagnosticSink::Event* findDiagnostic(const CollectingDiagnosticSink& diagnostics,
                                                   const std::string_view message)
{
  const auto found = std::find_if(diagnostics.events().begin(), diagnostics.events().end(),
                                  [&](const auto& event) { return event.message == message; });
  return found == diagnostics.events().end() ? nullptr : &*found;
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

  ASSERT_TRUE(diagnostics.failureCount() == checkpoint) << "TraceDirectoryJob target run failed";
  ASSERT_TRUE(reader.paths().size() == 1U) << "TraceDirectoryJob should read one selected YAML file";
  ASSERT_TRUE(std::filesystem::path(reader.paths()[0]).filename() == "Alpha.ctrace-run.yml")
      << "TraceDirectoryJob reader path mismatch";
  ASSERT_TRUE(std::filesystem::is_regular_file(traceDir / "Alpha.SWO.csv"))
      << "TraceDirectoryJob CSV output name mismatch";
  ASSERT_TRUE(std::filesystem::is_regular_file(traceDir / "Alpha.ctf" / "metadata"))
      << "TraceDirectoryJob CTF output name mismatch";
  ASSERT_TRUE(std::filesystem::is_regular_file(traceDir / "Alpha.SWO.traceanalysis.xml"))
      << "TraceDirectoryJob XML output name mismatch";
  ASSERT_TRUE(!std::filesystem::exists(traceDir / "Beta.SWO.csv"))
      << "TraceDirectoryJob should not process unselected target";
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

  ASSERT_TRUE(diagnostics.failureCount() == batchCheckpoint) << "TraceDirectoryJob batch check failed";
  ASSERT_TRUE(batchReader.paths().size() == 2U) << "TraceDirectoryJob batch should read every trace-run file";
  ASSERT_TRUE(!std::filesystem::exists(traceDir / "Alpha.SWO.csv")) << "check-only batch should not create CSV output";
  ASSERT_TRUE(!std::filesystem::exists(traceDir / "Alpha.ctf")) << "check-only batch should not create CTF output";

  writeTestFile(traceDir / "Broken.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Broken.SWO.raw", std::string{static_cast<char>(0x01), 'A'});

  CliOptions brokenOptions;
  brokenOptions.traceDir = traceDir.string();
  brokenOptions.targetName = "Broken";

  TestTraceRunConfigReader brokenReader;
  TraceDirectoryJob brokenJob(brokenOptions, diagnostics, brokenReader);
  const auto brokenCheckpoint = diagnostics.failureCount();
  brokenJob.run();
  ASSERT_TRUE(diagnostics.failureCount() > brokenCheckpoint)
      << "check-only trace directory should fail on decoder error packets";
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
  channelZero.error = "channel zero diagnostic";
  TraceRunReference noStream = reported;
  noStream.ctraceRef = "core/no-stream";
  noStream.stream.reset();
  noStream.warning.reset();
  noStream.error.reset();
  auto inconsistent = TraceRunTestSupport::makeReference("itm", "other", 3U, {1U}, "other/itm");
  config.setups.push_back(TraceRunTestSupport::makeTimestampSetup("core"));
  config.references = {reported, emptyError, channelZero, noStream, inconsistent};

  CliOptions options;
  options.traceDir = traceDir.string();
  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_TRUE(diagnostics.containsMessage("producer note"));
  EXPECT_TRUE(diagnostics.containsMessage("producer warning"));
  EXPECT_TRUE(diagnostics.containsMessage("producer error"));
  EXPECT_TRUE(diagnostics.containsMessage("channel zero diagnostic"));
  EXPECT_TRUE(diagnostics.containsMessage("trace generation setup failed without a diagnostic message"));
  EXPECT_TRUE(diagnostics.containsMessage("does not match ctrace-setup pname"));
  EXPECT_TRUE(diagnostics.containsMessage("skipping raw trace channel"));
  EXPECT_TRUE(diagnostics.containsMessage("no supported <solution-set>.SWO.raw input found"));

  const auto* info = findDiagnostic(diagnostics, "producer note");
  const auto* warning = findDiagnostic(diagnostics, "producer warning");
  const auto* error = findDiagnostic(diagnostics, "producer error");
  const auto* channelZeroError = findDiagnostic(diagnostics, "channel zero diagnostic");
  ASSERT_NE(info, nullptr);
  ASSERT_NE(warning, nullptr);
  ASSERT_NE(error, nullptr);
  ASSERT_NE(channelZeroError, nullptr);
  EXPECT_EQ(info->severity, DiagnosticSink::Severity::Info);
  EXPECT_EQ(warning->severity, DiagnosticSink::Severity::Warning);
  EXPECT_EQ(error->severity, DiagnosticSink::Severity::Error);
  EXPECT_EQ(channelZeroError->severity, DiagnosticSink::Severity::Error);
  EXPECT_EQ(error->impact, DiagnosticSink::Impact::NonFailing);
  EXPECT_EQ(channelZeroError->impact, DiagnosticSink::Impact::NonFailing);
}

TEST(CtraceUnitTests, testTraceDirectoryChecksOutputRequirementsAfterReferenceError)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-reference-error-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTraceInputs(traceDir, {"MissingClock"});

  TraceRunConfig config;
  auto reference = TraceRunTestSupport::makeReference("event", "core", 3U, {}, "core/event");
  reference.error = "producer could not configure event trace";
  config.references.push_back(std::move(reference));

  CliOptions options;
  options.traceDir = traceDir.string();
  options.targetName = "MissingClock";
  options.outputFormat = OutputFormat::All;

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  const auto* referenceError = findDiagnostic(diagnostics, "producer could not configure event trace");
  ASSERT_NE(referenceError, nullptr);
  EXPECT_EQ(referenceError->severity, DiagnosticSink::Severity::Error);
  EXPECT_EQ(referenceError->impact, DiagnosticSink::Impact::NonFailing);
  EXPECT_TRUE(diagnostics.containsMessage("CTF output requires timestamps.clock"));
  EXPECT_GT(diagnostics.failureCount(), 0U);
  EXPECT_TRUE(std::filesystem::is_regular_file(traceDir / "MissingClock.SWO.csv"));
  EXPECT_FALSE(std::filesystem::exists(traceDir / "MissingClock.ctf"));
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
  EXPECT_TRUE(diagnostics.containsMessage("synthetic config failure"));
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
  EXPECT_TRUE(diagnostics.containsMessage("CTF output requires timestamps.clock"));
}

TEST(CtraceUnitTests, testFileDecodeJobReportsPerStreamPrescalers)
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
  const auto diagnostic = std::find_if(diagnostics.events().begin(), diagnostics.events().end(), [](const auto& event) {
    return event.message == "using Trace-Bus-ID-specific timestamp prescalers";
  });
  ASSERT_NE(diagnostic, diagnostics.events().end());
  EXPECT_EQ(diagnostic->severity, DiagnosticSink::Severity::Info);
  EXPECT_EQ(diagnostic->context, (std::vector<std::pair<std::string, std::string>>{{"traceBusIds", "2"}}));
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

TEST(CtraceUnitTests, testFileDecodeJobReportsRawInputReadFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::DirectoryReadFailure)) {
    GTEST_SKIP();
  }
  const TemporaryTestPath temporaryPath("ctrace-file-decode-read-failure-test");
  temporaryPath.createDirectory();
  CollectingDiagnosticSink diagnostics;
  FileDecodeJob job(CliOptions{}, temporaryPath.path(), diagnostics, CtraceRunMeta::fromConfig({}));
  EXPECT_THROW(job.run(), std::runtime_error);
}
