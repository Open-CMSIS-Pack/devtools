/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdSessionTestSupport.h"
#include "CoreSightFormatter.h"
#include "FormattedTraceTestSupport.h"
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
#include <ios>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/** @brief Provides controlled fault injection without exposing descriptor stream mutation in production. */
class TraceRunInputDescriptorTestAccess final {
public:
  /** @brief Marks the retained stream bad so the next read exercises stable error normalization. */
  static void setBad(TraceRunInputDescriptor& input)
  {
    input.stream().setstate(std::ios::badbit);
  }
};

/** @brief Creates the default trace-run configuration used by directory tests. */
static TraceRunConfig defaultTraceRunConfig()
{
  TraceRunConfig config;
  config.setups.push_back(TraceRunTestSupport::makeTimestampSetup(std::nullopt, 100000000U));
  return config;
}

/** @brief Creates one direct decode-job input for focused control tests. */
static TraceRunInputDescriptor testInput(const std::filesystem::path& path, TraceRunConfig config = {})
{
  const auto filename = path.filename().string();
  constexpr std::string_view rawSuffix = ".raw";
  if (filename.size() <= rawSuffix.size() ||
      filename.compare(filename.size() - rawSuffix.size(), rawSuffix.size(), rawSuffix) != 0) {
    throw std::runtime_error("test input must be named <solution-set>.<channel>.raw");
  }
  const auto channelSeparator = filename.rfind('.', filename.size() - rawSuffix.size() - 1U);
  if (channelSeparator == std::string::npos) {
    throw std::runtime_error("test input must be named <solution-set>.<channel>.raw");
  }
  const auto solutionSet = filename.substr(0U, channelSeparator);
  const auto configFile = path.parent_path() / (solutionSet + ".ctrace-run.yml");
  config.path = configFile.string();
  return TraceRunDiscovery::resolveInput(std::move(config));
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
  writeTestFile(traceDir / "Alpha.SWO.raw", std::string{"\0\0\0\0\0\x80\x15\0", 8U});
  writeTestFile(traceDir / "Beta.TB_MTB.raw", "unselected");
  writeTestFile(traceDir / "Alpha.ER.raw", "unsupported");

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
  EXPECT_FALSE(diagnostics.containsContext("channel", "TB_MTB"));
  EXPECT_TRUE(diagnostics.containsContext("channel", "ER"));
  EXPECT_FALSE(std::filesystem::exists(traceDir / "Beta.TB_MTB.csv"));
  EXPECT_FALSE(std::filesystem::exists(traceDir / "Alpha.ER.csv"));
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

TEST(CtraceUnitTests, testTraceDirectoryDecodesFormattedInputThroughRawFrontend)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-formatted-frontend-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Formatted.ctrace-run.yml", "ctrace-run:\n");
  auto itm = FormattedTraceTestSupport::itmHardwareSync();
  const auto software = FormattedTraceTestSupport::itmSoftwarePacket(1U, 'A');
  itm.insert(itm.end(), software.begin(), software.end());
  const auto raw = FormattedTraceTestSupport::memoryAlignedFrames({{1U, std::move(itm)}});
  writeTestFile(traceDir / "Formatted.TB.raw", std::string(raw.begin(), raw.end()));

  TraceRunConfig config;
  config.traceFormat = TraceRunFormat::Formatted;
  config.setups.push_back(TraceRunTestSupport::makeTimestampSetup("core", 400000000U));
  config.references.push_back(TraceRunTestSupport::makeReference("itm", "core", 1U, {}, "core/itm"));

  CliOptions options;
  options.traceDir = traceDir.string();
  options.targetName = "Formatted";
  options.outputFormat = OutputFormat::All;

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_EQ(diagnostics.failureCount(), 0U);
  EXPECT_FALSE(diagnostics.containsMessage("formatted trace input is not enabled yet"));
  EXPECT_FALSE(diagnostics.containsMessage("CTF output requires timestamps.clock"));
  EXPECT_FALSE(diagnostics.containsMessage("skipping raw trace channel"));
  EXPECT_FALSE(diagnostics.containsMessage("formatted raw trace input size"));
  EXPECT_NE(readTestTextFile(traceDir / "Formatted.TB.csv").find(",1,itm,1,0x41,,,"), std::string::npos);
  EXPECT_TRUE(std::filesystem::is_regular_file(traceDir / "Formatted.ctf" / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(traceDir / "Formatted.ctf" / "stream_0"));
  EXPECT_FALSE(std::filesystem::exists(traceDir / "Formatted.TB.traceanalysis.xml"));
}

TEST(CtraceUnitTests, testTraceDirectoryPreflightsFormattedAlignmentBeforeDecoderAndArtifacts)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-formatted-preflight-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Partial.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Partial.TB.raw", std::string(15U, 'f'));
  writeTestFile(traceDir / "Partial.TB.csv", "csv sentinel");
  writeTestFile(traceDir / "Partial.ctf" / "sentinel", "ctf sentinel");
  writeTestFile(traceDir / "Partial.TB.traceanalysis.xml", "xml sentinel");

  TraceRunConfig config;
  config.traceFormat = TraceRunFormat::Formatted;
  config.references.push_back(TraceRunTestSupport::makeReference("itm", "core", 1U, {}, "core/itm"));

  CliOptions options;
  options.traceDir = traceDir.string();
  options.targetName = "Partial";
  options.outputFormat = OutputFormat::All;

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_TRUE(diagnostics.containsMessage("formatted raw trace input size must be a multiple of 16 bytes"));
  EXPECT_FALSE(diagnostics.containsMessage("formatted trace input is not enabled yet"));
  EXPECT_FALSE(diagnostics.containsMessage("CTF output requires timestamps.clock"));
  EXPECT_FALSE(diagnostics.containsMessage("applied ctrace-run meta"));
  EXPECT_EQ(readTestTextFile(traceDir / "Partial.TB.csv"), "csv sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Partial.ctf" / "sentinel"), "ctf sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Partial.TB.traceanalysis.xml"), "xml sentinel");
}

TEST(CtraceUnitTests, testTraceDirectoryRejectsDirectoryInputBeforeArtifacts)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-nonregular-input-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Directory.ctrace-run.yml", "ctrace-run:\n");
  std::filesystem::create_directory(traceDir / "Directory.SWO.raw");
  writeTestFile(traceDir / "Directory.SWO.csv", "csv sentinel");
  writeTestFile(traceDir / "Directory.ctf" / "sentinel", "ctf sentinel");
  writeTestFile(traceDir / "Directory.SWO.traceanalysis.xml", "xml sentinel");

  CliOptions options;
  options.traceDir = traceDir.string();
  options.outputFormat = OutputFormat::All;
  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader;
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_TRUE(diagnostics.containsMessage("raw trace input is not a regular file"));
  EXPECT_FALSE(diagnostics.containsMessage("applied ctrace-run meta"));
  EXPECT_EQ(readTestTextFile(traceDir / "Directory.SWO.csv"), "csv sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Directory.ctf" / "sentinel"), "ctf sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Directory.SWO.traceanalysis.xml"), "xml sentinel");
}

TEST(CtraceUnitTests, testTraceDirectoryRejectsDanglingSymlinkBeforeArtifacts)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-dangling-input-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Dangling.ctrace-run.yml", "ctrace-run:\n");
  std::error_code error;
  std::filesystem::create_symlink("missing.raw", traceDir / "Dangling.SWO.raw", error);
  if (error) {
    GTEST_SKIP() << error.message();
  }
  writeTestFile(traceDir / "Dangling.SWO.csv", "csv sentinel");
  writeTestFile(traceDir / "Dangling.ctf" / "sentinel", "ctf sentinel");
  writeTestFile(traceDir / "Dangling.SWO.traceanalysis.xml", "xml sentinel");

  CliOptions options;
  options.traceDir = traceDir.string();
  options.outputFormat = OutputFormat::All;
  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader;
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_TRUE(diagnostics.containsMessage("raw trace input is not a regular file"));
  EXPECT_FALSE(diagnostics.containsMessage("applied ctrace-run meta"));
  EXPECT_EQ(readTestTextFile(traceDir / "Dangling.SWO.csv"), "csv sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Dangling.ctf" / "sentinel"), "ctf sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Dangling.SWO.traceanalysis.xml"), "xml sentinel");
}

TEST(CtraceUnitTests, testTraceDirectoryDecodesExplicitUnformattedTraceBuffersAndSkipsEventRecorder)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-explicit-input-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Plain.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Plain.TB.raw");
  writeTestFile(traceDir / "Plain.ER.raw", "unsupported");
  writeTestFile(traceDir / "Named.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Named.TB_MTB.raw");
  writeTestFile(traceDir / "Swo.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Swo.SWO.raw");

  auto config = defaultTraceRunConfig();
  config.traceFormat = TraceRunFormat::Unformatted;
  CliOptions options;
  options.traceDir = traceDir.string();
  options.outputFormat = OutputFormat::Csv;

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_EQ(diagnostics.failureCount(), 0U);
  EXPECT_TRUE(diagnostics.containsMessage("skipping raw trace channel excluded from active input selection"));
  EXPECT_TRUE(diagnostics.containsContext("channel", "ER"));
  EXPECT_TRUE(std::filesystem::is_regular_file(traceDir / "Plain.TB.csv"));
  EXPECT_TRUE(std::filesystem::is_regular_file(traceDir / "Named.TB_MTB.csv"));
  EXPECT_TRUE(std::filesystem::is_regular_file(traceDir / "Swo.SWO.csv"));
  EXPECT_FALSE(std::filesystem::exists(traceDir / "Plain.ER.csv"));
}

TEST(CtraceUnitTests, testTraceDirectoryRejectsAmbiguousExplicitInputsWithoutTouchingArtifacts)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-ambiguous-input-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Ambiguous.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Ambiguous.SWO.raw");
  writeTestFile(traceDir / "Ambiguous.TB.raw");
  writeTestFile(traceDir / "Ambiguous.SWO.csv", "swo sentinel");
  writeTestFile(traceDir / "Ambiguous.TB.csv", "tb sentinel");
  writeTestFile(traceDir / "Ambiguous.ctf" / "sentinel", "ctf sentinel");
  writeTestFile(traceDir / "Ambiguous.SWO.traceanalysis.xml", "swo xml sentinel");
  writeTestFile(traceDir / "Ambiguous.TB.traceanalysis.xml", "tb xml sentinel");
  writeTestFile(traceDir / "TbNamed.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "TbNamed.TB.raw");
  writeTestFile(traceDir / "TbNamed.TB_MTB.raw");
  writeTestFile(traceDir / "TbNamed.TB.csv", "tb sentinel");
  writeTestFile(traceDir / "TbNamed.TB_MTB.csv", "named sentinel");
  writeTestFile(traceDir / "TbNamed.ctf" / "sentinel", "ctf sentinel");
  writeTestFile(traceDir / "TbNamed.TB.traceanalysis.xml", "tb xml sentinel");
  writeTestFile(traceDir / "TbNamed.TB_MTB.traceanalysis.xml", "named xml sentinel");
  writeTestFile(traceDir / "NamedPair.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "NamedPair.TB_MTB.raw");
  writeTestFile(traceDir / "NamedPair.TB_ETB.raw");
  writeTestFile(traceDir / "NamedPair.TB_MTB.csv", "mtb sentinel");
  writeTestFile(traceDir / "NamedPair.TB_ETB.csv", "etb sentinel");
  writeTestFile(traceDir / "NamedPair.ctf" / "sentinel", "ctf sentinel");
  writeTestFile(traceDir / "NamedPair.TB_MTB.traceanalysis.xml", "mtb xml sentinel");
  writeTestFile(traceDir / "NamedPair.TB_ETB.traceanalysis.xml", "etb xml sentinel");

  auto config = defaultTraceRunConfig();
  config.traceFormat = TraceRunFormat::Unformatted;
  CliOptions options;
  options.traceDir = traceDir.string();
  options.outputFormat = OutputFormat::All;

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_TRUE(diagnostics.containsMessage("multiple eligible raw trace inputs found"));
  EXPECT_FALSE(diagnostics.containsMessage("applied ctrace-run meta"));
  EXPECT_EQ(readTestTextFile(traceDir / "Ambiguous.SWO.csv"), "swo sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Ambiguous.TB.csv"), "tb sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Ambiguous.ctf" / "sentinel"), "ctf sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Ambiguous.SWO.traceanalysis.xml"), "swo xml sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Ambiguous.TB.traceanalysis.xml"), "tb xml sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "TbNamed.TB.csv"), "tb sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "TbNamed.TB_MTB.csv"), "named sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "TbNamed.ctf" / "sentinel"), "ctf sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "TbNamed.TB.traceanalysis.xml"), "tb xml sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "TbNamed.TB_MTB.traceanalysis.xml"), "named xml sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "NamedPair.TB_MTB.csv"), "mtb sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "NamedPair.TB_ETB.csv"), "etb sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "NamedPair.ctf" / "sentinel"), "ctf sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "NamedPair.TB_MTB.traceanalysis.xml"), "mtb xml sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "NamedPair.TB_ETB.traceanalysis.xml"), "etb xml sentinel");
  EXPECT_EQ(std::count_if(diagnostics.events().begin(), diagnostics.events().end(),
                          [](const auto& event) {
                            return event.message.find("multiple eligible raw trace inputs found") != std::string::npos;
                          }),
            3U);
}

TEST(CtraceUnitTests, testTraceDirectoryReportsNormalizedSetupWarningWithContext)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-setup-warning-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  const auto configPath = traceDir / "Conflicting.ctrace-run.yml";
  writeTestFile(configPath, "ctrace-run:\n");
  auto itm = FormattedTraceTestSupport::itmHardwareSync();
  for (const auto channel : {1U, 2U}) {
    const auto software = FormattedTraceTestSupport::itmSoftwarePacket(channel, 'A');
    itm.insert(itm.end(), software.begin(), software.end());
  }
  const auto raw = FormattedTraceTestSupport::memoryAlignedFrames({{1U, std::move(itm)}});
  writeTestFile(traceDir / "Conflicting.TB.raw", std::string(raw.begin(), raw.end()));

  TraceRunConfig config;
  config.traceFormat = TraceRunFormat::Formatted;
  config.references.push_back(TraceRunTestSupport::makeReference("itm", "core", 1U, {}, "core/itm"));
  config.setups = {
      TraceRunTestSupport::makeTimestampSetup("core", 100000000U, 1U, 3U),
      TraceRunTestSupport::makeTimestampSetup("core", 100000000U, 1U, 5U),
  };
  config.setups.back().line = 12U;

  CliOptions options;
  options.traceDir = traceDir.string();
  options.targetName = "Conflicting";
  options.outputFormat = OutputFormat::Csv;

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  const auto* warning = findDiagnostic(
      diagnostics, "ignoring conflicting ctrace-setup itm.enable assignment for one formatted ITM route");
  ASSERT_NE(warning, nullptr);
  EXPECT_EQ(warning->severity, DiagnosticSink::Severity::Warning);
  EXPECT_EQ(warning->impact, DiagnosticSink::Impact::NonFailing);
  const std::vector<std::pair<std::string, std::string>> expectedContext{
      {"config", configPath.string()}, {"pname", "core"}, {"line", "12"}};
  EXPECT_EQ(warning->context, expectedContext);
  EXPECT_EQ(diagnostics.failureCount(), 0U);
  EXPECT_FALSE(diagnostics.containsMessage("ITM data was received on a channel not enabled"));
  const auto csv = readTestTextFile(traceDir / "Conflicting.TB.csv");
  EXPECT_NE(csv.find(",1,itm,1,0x41,,,"), std::string::npos);
  EXPECT_NE(csv.find(",1,itm,2,0x41,,,"), std::string::npos);
}

TEST(CtraceUnitTests, testTraceDirectoryReportsGenerationDiagnosticsAndMissingSwo)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-diagnostics-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTestFile(traceDir / "Alpha.ctrace-run.yml", "ctrace-run:\n");
  writeTestFile(traceDir / "Alpha.ER.raw", "unsupported");
  writeTestFile(traceDir / "Alpha.ER.csv", "csv sentinel");
  writeTestFile(traceDir / "Alpha.ctf" / "sentinel", "ctf sentinel");
  writeTestFile(traceDir / "Alpha.ER.traceanalysis.xml", "xml sentinel");

  TraceRunConfig config;
  auto reported = TraceRunTestSupport::makeReference("event", "core", 3U, {}, "core/event");
  reported.info = {"producer note", "second producer note"};
  reported.warning = {"producer warning", "second producer warning"};
  reported.error = {"producer error", "second producer error"};
  TraceRunReference emptyError = reported;
  emptyError.ctraceRef = "core/pmu";
  emptyError.type = "pmu";
  emptyError.info.clear();
  emptyError.warning = {""};
  emptyError.error = {""};
  auto channelZero = TraceRunTestSupport::makeReference("itm", std::nullopt, std::nullopt, {0U}, "core/itm");
  channelZero.error = {"channel zero diagnostic"};
  TraceRunReference noStream = reported;
  noStream.ctraceRef = "core/no-stream";
  noStream.stream.reset();
  noStream.warning.clear();
  noStream.error.clear();
  auto inconsistent = TraceRunTestSupport::makeReference("itm", "other", 3U, {1U}, "other/itm");
  config.setups.push_back(TraceRunTestSupport::makeTimestampSetup("core"));
  config.references = {reported, emptyError, channelZero, noStream, inconsistent};

  CliOptions options;
  options.traceDir = traceDir.string();
  options.outputFormat = OutputFormat::All;
  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(config);
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_TRUE(diagnostics.containsMessage("producer note"));
  EXPECT_TRUE(diagnostics.containsMessage("second producer note"));
  EXPECT_TRUE(diagnostics.containsMessage("producer warning"));
  EXPECT_TRUE(diagnostics.containsMessage("second producer warning"));
  EXPECT_TRUE(diagnostics.containsMessage("producer error"));
  EXPECT_TRUE(diagnostics.containsMessage("second producer error"));
  EXPECT_TRUE(diagnostics.containsMessage("channel zero diagnostic"));
  EXPECT_TRUE(diagnostics.containsMessage("trace generation setup failed without a diagnostic message"));
  EXPECT_FALSE(diagnostics.containsMessage("does not match ctrace-setup pname"));
  EXPECT_TRUE(diagnostics.containsMessage("skipping raw trace channel"));
  EXPECT_TRUE(diagnostics.containsContext("channel", "ER"));
  EXPECT_TRUE(diagnostics.containsMessage("no eligible raw trace input found"));

  const auto skipped = std::find_if(diagnostics.events().begin(), diagnostics.events().end(), [](const auto& event) {
    return event.message == "skipping raw trace channel excluded from active input selection";
  });
  const auto missing = std::find_if(diagnostics.events().begin(), diagnostics.events().end(), [](const auto& event) {
    return event.message.find("no eligible raw trace input found") != std::string::npos;
  });
  ASSERT_NE(skipped, diagnostics.events().end());
  ASSERT_NE(missing, diagnostics.events().end());
  EXPECT_LT(skipped, missing);
  EXPECT_EQ(readTestTextFile(traceDir / "Alpha.ER.csv"), "csv sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Alpha.ctf" / "sentinel"), "ctf sentinel");
  EXPECT_EQ(readTestTextFile(traceDir / "Alpha.ER.traceanalysis.xml"), "xml sentinel");

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
  reference.error = {"producer could not configure event trace"};
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

TEST(CtraceUnitTests, testTraceDirectoryRejectsMalformedConsumedItmSetupBeforeOutput)
{
  const TemporaryTestPath temporaryPath("ctrace-trace-directory-itm-setup-error-test");
  const auto traceDir = temporaryPath.path() / ".trace";
  writeTraceInputs(traceDir, {"InvalidItm"});

  TraceRunSetup setup;
  setup.processorName = "core";
  setup.line = 7U;
  setup.itm = TraceRunItmSetup{};
  setup.itm->enableError = "'itm.enable' must be a scalar unsigned integer";
  TraceRunConfig config;
  config.setups.push_back(std::move(setup));

  CliOptions options;
  options.traceDir = traceDir.string();
  options.targetName = "InvalidItm";
  options.outputFormat = OutputFormat::Csv;

  CollectingDiagnosticSink diagnostics;
  TestTraceRunConfigReader reader(std::move(config));
  const auto checkpoint = diagnostics.failureCount();
  TraceDirectoryJob(options, diagnostics, reader).run();

  EXPECT_EQ(diagnostics.failureCount(), checkpoint + 1U);
  EXPECT_TRUE(diagnostics.containsMessage("'itm.enable' must be a scalar unsigned integer"));
  EXPECT_FALSE(std::filesystem::exists(traceDir / "InvalidItm.SWO.csv"));
}

TEST(CtraceUnitTests, testFileDecodeJobHandlesDisabledCtf)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-control-test");
  CollectingDiagnosticSink diagnostics;
  const auto rawPath = temporaryPath.path() / "empty.SWO.raw";
  writeTestFile(rawPath);
  CliOptions ctf;
  ctf.outputFormat = OutputFormat::Ctf;
  bool sessionCreated = false;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  const auto delegate = OpenCsdSessionTestSupport::scriptedFactory(script);
  OpenCsdItmSessionFactory factory = [&](OpenCsdPacketCollector& collector, OpenCsdErrorController& errors) {
    sessionCreated = true;
    return delegate(collector, errors);
  };
  FileDecodeJob disabled(ctf, testInput(rawPath), diagnostics, std::move(factory));
  EXPECT_NO_THROW(disabled.run());
  EXPECT_TRUE(diagnostics.containsMessage("CTF output requires timestamps.clock"));
  EXPECT_FALSE(sessionCreated);
  EXPECT_FALSE(std::filesystem::exists(temporaryPath.path() / "empty.ctf"));
}

TEST(CtraceUnitTests, testFileDecodeJobUsesInjectedSessionForFormattedInput)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-formatted-session-test");
  const auto rawPath = temporaryPath.path() / "formatted.TB.raw";
  writeTestFile(rawPath, std::string(CoreSightFormatter::kMemoryAlignedFrameSize, 'f'));

  bool sessionCreated = false;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  const auto delegate = OpenCsdSessionTestSupport::scriptedFactory(script);
  OpenCsdItmSessionFactory factory = [&](OpenCsdPacketCollector& collector, OpenCsdErrorController& errors) {
    sessionCreated = true;
    return delegate(collector, errors);
  };

  CliOptions options;
  options.outputFormat = OutputFormat::Csv;
  TraceRunConfig config;
  config.traceFormat = TraceRunFormat::Formatted;
  config.references.push_back(TraceRunTestSupport::makeReference("itm", "core", 1U, {}, "core/itm"));
  CollectingDiagnosticSink diagnostics;
  FileDecodeJob job(options, testInput(rawPath, config), diagnostics, std::move(factory));

  EXPECT_NO_THROW(job.run());
  EXPECT_TRUE(sessionCreated);
  EXPECT_FALSE(diagnostics.containsMessage("formatted trace input is not enabled yet"));
  EXPECT_EQ(readTestTextFile(temporaryPath.path() / "formatted.TB.csv"),
            "cycles,stream,type,source,value,pc,address,note\n");
}

TEST(CtraceUnitTests, testInputSelectionAndPreflightNeverConstructDecoder)
{
  const TemporaryTestPath temporaryPath("ctrace-input-before-decoder-test");
  const auto& root = temporaryPath.createDirectory();
  writeTestFile(root / "Ambiguous.SWO.raw");
  writeTestFile(root / "Ambiguous.TB.raw");
  writeTestFile(root / "Partial.TB.raw", std::string(15U, 'f'));

  bool sessionCreated = false;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  const auto delegate = OpenCsdSessionTestSupport::scriptedFactory(script);
  OpenCsdItmSessionFactory factory = [&](OpenCsdPacketCollector& collector, OpenCsdErrorController& errors) {
    sessionCreated = true;
    return delegate(collector, errors);
  };
  CollectingDiagnosticSink diagnostics;
  CliOptions options;
  options.outputFormat = OutputFormat::All;
  const auto run = [&](const std::filesystem::path& configFile, TraceRunConfig config) {
    config.path = configFile.string();
    auto input = TraceRunDiscovery::resolveInput(std::move(config));
    FileDecodeJob(options, std::move(input), diagnostics, factory).run();
  };

  EXPECT_TRUE(throwsWithMessage([&] { run(root / "Missing.ctrace-run.yml", {}); }, "no eligible raw trace input"));
  TraceRunConfig unformatted;
  unformatted.traceFormat = TraceRunFormat::Unformatted;
  EXPECT_TRUE(throwsWithMessage([&] { run(root / "Ambiguous.ctrace-run.yml", unformatted); },
                                "multiple eligible raw trace inputs"));
  TraceRunConfig formatted;
  formatted.traceFormat = TraceRunFormat::Formatted;
  formatted.references.push_back(TraceRunTestSupport::makeReference("itm", "core", 1U, {}, "core/itm"));
  EXPECT_TRUE(throwsWithMessage([&] { run(root / "Partial.ctrace-run.yml", formatted); }, "multiple of 16 bytes"));
  EXPECT_FALSE(sessionCreated);
  EXPECT_FALSE(std::filesystem::exists(root / "Missing.ctf"));
  EXPECT_FALSE(std::filesystem::exists(root / "Ambiguous.ctf"));
  EXPECT_FALSE(std::filesystem::exists(root / "Partial.ctf"));
}

/** @brief Checks the unfiltered final CSV row and matching route-independent CLI diagnostic. */
static void expectGlobalDecodeAbort(const std::filesystem::path& csvPath, const CollectingDiagnosticSink& diagnostics,
                                    std::uint64_t processed, std::string_view reason)
{
  const auto lines = readTestLines(csvPath);
  ASSERT_GE(lines.size(), 2U);
  EXPECT_EQ(lines.front(), "cycles,stream,type,source,value,pc,address,note");
  EXPECT_EQ(lines.back().find(",,error,,,,,"), 0U) << "abort must have no timestamp, stream, or source";
  const auto prefix = "decode aborted after processing " + std::to_string(processed) +
                      " input bytes; trace is incomplete: ";
  EXPECT_NE(lines.back().find(prefix), std::string::npos);
  EXPECT_NE(lines.back().find(reason), std::string::npos);
  EXPECT_EQ(std::count_if(lines.begin(), lines.end(), [](const auto& line) {
              return line.find("decode aborted after processing ") != std::string::npos;
            }), 1);
  std::size_t globalErrors = 0U;
  for (const auto& event : diagnostics.events()) {
    if (event.message.find(prefix) != 0U) {
      continue;
    }
    ++globalErrors;
    EXPECT_EQ(event.severity, DiagnosticSink::Severity::Error);
    EXPECT_NE(event.message.find(reason), std::string::npos);
    EXPECT_TRUE(std::none_of(event.context.begin(), event.context.end(), [](const auto& item) {
      return item.first == "stream";
    }));
  }
  EXPECT_EQ(globalErrors, 1U);
}

/** @brief Emits one valid payload and resolves its timestamp before the next root operation. */
static std::vector<OpenCsdSessionTestSupport::ScriptedObservation>
committedSoftwareObservations(const TraceRouteIdentity& route)
{
  OcsdTraceElement timestamp;
  timestamp.setType(OCSD_GEN_TRC_ELEM_ITMTRACE);
  swt_itm_info info{};
  info.pkt_type = TS_SYNC;
  timestamp.setSWT_ITMInfo(info);
  timestamp.setTS(42U, false);
  return {OpenCsdSessionTestSupport::syncCallback(route, 0U),
          OpenCsdSessionTestSupport::softwareCallback(route, 6U, 1U, 'A'),
          OpenCsdSessionTestSupport::genericCallback(route.traceBusId.value_or(0U), 8U, timestamp)};
}

TEST(CtraceUnitTests, testFileDecodeJobRetainsCsvAfterFatalDecoderError)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-fatal-test");
  const auto rawPath = temporaryPath.path() / "fatal.SWO.raw";
  writeTestFile(rawPath, "x");
  CliOptions options;
  options.outputFormat = OutputFormat::Csv;
  options.selection = TraceSelection{{"itm"}, {7U}};
  CollectingDiagnosticSink diagnostics;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes = {{OCSD_RESP_FATAL_SYS_ERR, 1U}};

  FileDecodeJob job(options, testInput(rawPath), diagnostics, OpenCsdSessionTestSupport::scriptedFactory(script));
  EXPECT_NO_THROW(job.run());
  EXPECT_GT(diagnostics.failureCount(), 0U);
  const auto csvPath = temporaryPath.path() / "fatal.SWO.csv";
  expectGlobalDecodeAbort(csvPath, diagnostics, 1U, "OpenCSD aborted decode: OpenCSD reported a system error");
  EXPECT_EQ(readTestLines(csvPath).size(), 2U) << "the excluded route error must remain filtered";
}

TEST(CtraceUnitTests, testFileDecodeJobRetainsCsvForDecoderInitializationFailure)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-initialization-fatal-test");
  const auto rawPath = temporaryPath.path() / "startup.SWO.raw";
  writeTestFile(rawPath);
  CliOptions options;
  options.outputFormat = OutputFormat::Csv;
  options.selection.types = {"itm"};
  CollectingDiagnosticSink diagnostics;
  OpenCsdItmSessionFactory factory = [](OpenCsdPacketCollector&, OpenCsdErrorController&)
      -> std::unique_ptr<OpenCsdItmSessionInterface> {
    throw OpenCsdItmSessionError("synthetic decoder initialization failure");
  };

  FileDecodeJob job(options, testInput(rawPath), diagnostics, std::move(factory));
  EXPECT_NO_THROW(job.run());
  const auto csvPath = temporaryPath.path() / "startup.SWO.csv";
  expectGlobalDecodeAbort(csvPath, diagnostics, 0U, "synthetic decoder initialization failure");
  EXPECT_EQ(readTestLines(csvPath).size(), 2U);
}

TEST(CtraceUnitTests, testFileDecodeJobKeepsSafePrefixAndRejectsFatalBatchForAllOutputs)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-fatal-prefix-test");
  const auto rawPath = temporaryPath.path() / "prefix.TB.raw";
  writeTestFile(rawPath, std::string(2U * CoreSightFormatter::kMemoryAlignedFrameSize, 'f'));
  TraceRunConfig config;
  config.traceFormat = TraceRunFormat::Formatted;
  config.setups.push_back(TraceRunTestSupport::makeTimestampSetup("core", 400000000U));
  config.references.push_back(TraceRunTestSupport::makeReference("itm", "core", 1U, {}, "core/itm"));
  CliOptions options;
  options.outputFormat = OutputFormat::All;
  options.selection.types = {"itm"};
  CollectingDiagnosticSink diagnostics;
  const TraceRouteIdentity route{TraceRouteId{0U}, 1U};
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes.emplace_back(OCSD_RESP_CONT, 16U, committedSoftwareObservations(route));
  script->pushes.emplace_back(
      OCSD_RESP_FATAL_SYS_ERR, 16U,
      std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
          OpenCsdSessionTestSupport::softwareCallback(route, 16U, 1U, 'X'),
          OpenCsdSessionTestSupport::errorObservation(OCSD_ERR_MEM, 20U, 1U, "synthetic fatal tail")});

  FileDecodeJob job(options, testInput(rawPath, config), diagnostics,
                    OpenCsdSessionTestSupport::scriptedFactory(script));
  EXPECT_NO_THROW(job.run());
  EXPECT_EQ(script->pushCalls, 2U);
  EXPECT_EQ(script->endCalls, 0U);
  const auto csvPath = temporaryPath.path() / "prefix.TB.csv";
  expectGlobalDecodeAbort(csvPath, diagnostics, 32U, "synthetic fatal tail");
  const auto lines = readTestLines(csvPath);
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[1], "42,1,itm,1,0x41,,,");
  EXPECT_EQ(readTestTextFile(csvPath).find("0x58"), std::string::npos)
      << "callbacks from the fatal root operation must never reach the retained CSV";
  EXPECT_TRUE(diagnostics.containsMessage("synthetic fatal tail"));
  EXPECT_FALSE(std::filesystem::exists(temporaryPath.path() / "prefix.ctf"));
  EXPECT_FALSE(std::filesystem::exists(temporaryPath.path() / "prefix.TB.traceanalysis.xml"));
}

TEST(CtraceUnitTests, testFileDecodeJobRetainsCsvAfterFatalEndOfTrace)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-fatal-eot-test");
  const auto rawPath = temporaryPath.path() / "end.SWO.raw";
  writeTestFile(rawPath, std::string(16U, 't'));
  CliOptions options;
  options.outputFormat = OutputFormat::Csv;
  options.selection.types = {"itm"};
  CollectingDiagnosticSink diagnostics;
  const TraceRouteIdentity route{};
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes.emplace_back(OCSD_RESP_CONT, 16U, committedSoftwareObservations(route));
  script->ends.emplace_back(
      OCSD_RESP_FATAL_SYS_ERR, std::nullopt,
      std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
          OpenCsdSessionTestSupport::softwareCallback(route, 14U, 1U, 'X'),
          OpenCsdSessionTestSupport::errorObservation(OCSD_ERR_FAIL, 14U, 0U, "synthetic end-of-trace failure")});

  FileDecodeJob job(options, testInput(rawPath), diagnostics, OpenCsdSessionTestSupport::scriptedFactory(script));
  EXPECT_NO_THROW(job.run());
  EXPECT_EQ(script->endCalls, 1U);
  const auto csvPath = temporaryPath.path() / "end.SWO.csv";
  expectGlobalDecodeAbort(csvPath, diagnostics, 16U, "OpenCSD aborted end-of-trace processing");
  EXPECT_TRUE(diagnostics.containsMessage("synthetic end-of-trace failure"));
  const auto lines = readTestLines(csvPath);
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lines[1], "42,,itm,1,0x41,,,");
  EXPECT_EQ(readTestTextFile(csvPath).find("0x58"), std::string::npos);
}

TEST(CtraceUnitTests, testFileDecodeJobReportsRawInputReadFailuresWithPath)
{
  const TemporaryTestPath temporaryPath("ctrace-file-decode-read-failure-test");
  const auto rawPath = temporaryPath.path() / "failure.SWO.raw";
  writeTestFile(rawPath, "trace");
  auto input = testInput(rawPath);
  TraceRunInputDescriptorTestAccess::setBad(input);

  CollectingDiagnosticSink diagnostics;
  FileDecodeJob job(CliOptions{}, std::move(input), diagnostics);
  const auto message = captureExceptionMessage([&] { job.run(); });
  ASSERT_TRUE(message.has_value());
  EXPECT_EQ(*message, "failed to read input file: " + rawPath.string());
}

TEST(CtraceUnitTests, testFileDecodeJobConsumesPreflightedHandleAfterPathReplacement)
{
  if (!TestPlatform::supports(TestPlatformCapability::PosixPermissions)) {
    GTEST_SKIP();
  }

  const TemporaryTestPath temporaryPath("ctrace-file-decode-open-handle-test");
  const auto rawPath = temporaryPath.path() / "retained.SWO.raw";
  const std::string raw{"\0\0\0\0\0\x80\x17\x34\x12\x00\x08\x09\x41", 13U};
  writeTestFile(rawPath, raw);
  auto input = testInput(rawPath);

  std::filesystem::rename(rawPath, temporaryPath.path() / "moved.raw");
  std::filesystem::create_directory(rawPath);

  CliOptions options;
  options.outputFormat = OutputFormat::Csv;
  CollectingDiagnosticSink diagnostics;
  FileDecodeJob job(options, std::move(input), diagnostics);
  EXPECT_NO_THROW(job.run());
  EXPECT_EQ(diagnostics.failureCount(), 0U);
  EXPECT_EQ(readTestTextFile(temporaryPath.path() / "retained.SWO.csv"),
            "cycles,stream,type,source,value,pc,address,note\n"
            "0,,pcsample,,,0x08001234,,\n"
            "0,,itm,1,0x41,,,\n");
}
