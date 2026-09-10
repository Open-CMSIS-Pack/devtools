/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

// CTF bundle metadata, event handling, and direct-output lifecycle tests.
#include "CtfTestSupport.h"
#include "TestPlatform.h"
#include "TestSupport.h"
#include "TraceRunTestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfBundleOutput.h"
#include "ctf/CtfMetadataWriter.h"
#include "ctf/CtfSchema.h"
#include "ctf/TraceCompassXmlWriter.h"
#include "CtraceRunMeta.h"
#include "OutputRequirements.h"
#include "TestPath.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"
#include "TraceRunConfig.h"
#include "TraceSelection.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using CtfTestSupport::CtfExceptionRecord;
using CtfTestSupport::parseCtfRecords;
using CtfTestSupport::readCtfExceptionRecords;
using CtfTestSupport::readCtfRecords;
using CtfTestSupport::readLe64;
using CtfTestSupport::requireFirstCtfRecord;
using CtfTestSupport::requireSingleItmEvent;

/** @brief Derives the Trace Compass XML path used by bundle tests. */
static std::filesystem::path testTraceCompassXmlPath(const std::filesystem::path& outputDirectory)
{
  auto basePath = outputDirectory;
  if (basePath.extension() == ".ctf") {
    basePath.replace_extension();
  }
  basePath += ".SWO.traceanalysis.xml";
  return basePath;
}

/** @brief Creates a CTF bundle configuration with test defaults. */
static CtfOutputConfig makeCtfBundleConfig(const std::filesystem::path& outputDirectory, std::uint64_t coreClockHz)
{
  return CtfOutputConfig(outputDirectory, testTraceCompassXmlPath(outputDirectory), {},
                         CtfTestSupport::legacyTopology(coreClockHz));
}

/** @brief Creates a legacy CTF bundle configuration with explicit target paths. */
static CtfOutputConfig makeCtfBundleConfig(const std::filesystem::path& outputDirectory,
                                           const std::filesystem::path& traceCompassXmlPath, std::uint64_t coreClockHz)
{
  return CtfOutputConfig(outputDirectory, traceCompassXmlPath, {}, CtfTestSupport::legacyTopology(coreClockHz));
}

/** @brief Creates a formatted CTF bundle configuration from an explicit topology. */
static CtfOutputConfig makeFormattedCtfBundleConfig(const std::filesystem::path& outputDirectory,
                                                    CtfMetadataTopology topology, TraceSelection selection = {})
{
  std::vector<TraceRouteIdentity> routes;
  for (const auto& stream : topology.streams) {
    routes.push_back(stream.route);
  }
  return CtfOutputConfig(outputDirectory, testTraceCompassXmlPath(outputDirectory), std::move(selection),
                         std::move(topology), std::move(routes), true);
}

/** @brief Requires all files of a completed CTF bundle. */
static void requireCompleteCtfBundle(const std::filesystem::path& ctfDirectory, const std::filesystem::path& xmlPath,
                                     const std::string& message)
{
  ASSERT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "metadata") &&
              std::filesystem::is_regular_file(ctfDirectory / "stream_0") && std::filesystem::is_regular_file(xmlPath))
      << message;
}

/** @brief Owns the temporary directory used by one CTF output test. */
class TemporaryCtfOutput {
public:
  /** @brief Creates a temporary CTF output path. */
  explicit TemporaryCtfOutput(const std::string& name)
    : m_root(name),
      m_outputDirectory(m_root.path() / "output.ctf")
  {
  }

  /** @brief Returns the temporary output directory. */
  const std::filesystem::path& outputDirectory() const
  {
    return m_outputDirectory;
  }

private:
  TemporaryTestPath m_root;
  std::filesystem::path m_outputDirectory;
};

/** @brief Converts normalized trace-run source metadata for output tests. */
static CtfSourceDescriptor resolvedSource(const CtraceRunSourceMeta& source)
{
  return {
      source.type,
      source.source,
      source.route,
      source.label,
      source.address,
      source.dataType,
      static_cast<std::uint8_t>(source.dataSize),
  };
}

/** @brief Reads trace-status reasons while requiring no other event types. */
static std::vector<std::uint8_t> readOnlyCtfTraceStatusReasons(const std::filesystem::path& streamPath)
{
  std::vector<std::uint8_t> reasons;
  for (const auto& record : readCtfRecords(streamPath)) {
    if (record.id != CtfSchema::value(CtfSchema::EventId::TraceStatus)) {
      throw std::runtime_error("expected only CTF trace-status events");
    }
    reasons.push_back(record.payload.front());
  }
  return reasons;
}

/** @brief Reads the first encoded DWT value tag from a test stream. */
static std::uint8_t readFirstCtfDwtValueTag(const std::filesystem::path& streamPath)
{
  const auto records = readCtfRecords(streamPath);
  const auto& record = requireFirstCtfRecord(records, CtfSchema::EventId::DwtValue, "expected CTF DWT event missing");
  return record.payload[2U];
}

/** @brief Requires every generated state change to begin its output path with the normalized route. */
static void requireRoutePrefixedStateChanges(const std::string& xml)
{
  const std::string stateChange = "<stateChange>";
  const std::string stateChangeEnd = "</stateChange>";
  const std::string stateAttribute = "<stateAttribute";
  const std::string routeAttribute = "<stateAttribute type=\"eventField\" value=\"cmsis_trace_bus_id\" />";
  std::size_t offset = 0U;
  std::size_t stateChangeCount = 0U;
  while ((offset = xml.find(stateChange, offset)) != std::string::npos) {
    const auto end = xml.find(stateChangeEnd, offset);
    const auto firstAttribute = xml.find(stateAttribute, offset + stateChange.size());
    ASSERT_NE(end, std::string::npos);
    ASSERT_NE(firstAttribute, std::string::npos);
    ASSERT_LT(firstAttribute, end);
    EXPECT_EQ(xml.compare(firstAttribute, routeAttribute.size(), routeAttribute), 0);
    ++stateChangeCount;
    offset = end + stateChangeEnd.size();
  }
  EXPECT_GT(stateChangeCount, 0U);
}

/** @brief Requires every generated view entry to select all normalized trace routes. */
static void requireRoutePrefixedViewEntries(const std::string& xml)
{
  const std::string entryPath = "<entry path=\"";
  std::size_t offset = 0U;
  std::size_t entryCount = 0U;
  while ((offset = xml.find(entryPath, offset)) != std::string::npos) {
    const auto path = offset + entryPath.size();
    EXPECT_EQ(xml.compare(path, 2U, "*/"), 0);
    ++entryCount;
    offset = path + 2U;
  }
  EXPECT_GT(entryCount, 0U);
}

TEST(CtraceUnitTests, testCtfBundleOutputExceptionContext)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-exception-test");
  const auto& outputDir = temporaryOutput.outputDirectory();

  auto options = makeCtfBundleConfig(outputDir, 1000000U);
  CtfBundleOutput output(std::move(options));
  output.start();
  output.writeEvent(exceptionPacket(15, ExceptionAction::Entered, 100));
  output.writeEvent(exceptionPacket(54, ExceptionAction::Entered, 200));
  output.writeEvent(exceptionPacket(54, ExceptionAction::Exited, 300));
  output.writeEvent(exceptionPacket(15, ExceptionAction::Returned, 400));
  output.writeEvent(exceptionPacket(15, ExceptionAction::Exited, 500));
  output.writeEvent(exceptionPacket(0, ExceptionAction::Returned, 600));
  output.stop();

  const auto records = readCtfExceptionRecords(outputDir / "stream_0");
  const auto metadata = readTestTextFile(outputDir / "metadata");

  ASSERT_TRUE(metadata.find("\"entered\" = 0") != std::string::npos) << "CTF exception entered label mismatch";
  ASSERT_TRUE(metadata.find("\"exited\" = 1") != std::string::npos) << "CTF exception exited label mismatch";
  ASSERT_TRUE(metadata.find("\"returned\" = 2") != std::string::npos) << "CTF exception returned label mismatch";
  ASSERT_TRUE(metadata.find("\"trace\" = 0") != std::string::npos) << "CTF exception trace origin mismatch";
  ASSERT_TRUE(metadata.find("\"synthetic\" = 1") != std::string::npos) << "CTF exception synthetic origin mismatch";

  ASSERT_TRUE(records == std::vector<CtfExceptionRecord>({
                             {0U, 0U, 1U},
                             {0U, 1U, 1U},
                             {15U, 0U, 0U},
                             {15U, 1U, 1U},
                             {54U, 0U, 0U},
                             {54U, 1U, 0U},
                             {15U, 2U, 0U},
                             {15U, 1U, 0U},
                             {0U, 2U, 0U},
                         }))
      << "CtfBundleOutput exception active-context records mismatch";
}

TEST(CtraceUnitTests, testCtfBundleOutputOverflowClosesExceptionUntilReturn)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-exception-overflow-test");
  const auto& outputDir = temporaryOutput.outputDirectory();

  CtfBundleOutput output(makeCtfBundleConfig(outputDir, 1000000U));
  output.start();
  output.writeEvent(exceptionPacket(15U, ExceptionAction::Entered, 100U));
  output.writeEvent(overflowPacket(200U));
  output.writeEvent(exceptionPacket(0U, ExceptionAction::Returned, 300U));
  output.stop();

  ASSERT_TRUE(readCtfExceptionRecords(outputDir / "stream_0") ==
              std::vector<CtfExceptionRecord>({{0U, 0U, 1U}, {0U, 1U, 1U}, {15U, 0U, 0U}, {15U, 1U, 1U}, {0U, 2U, 0U}}))
      << "CTF overflow must close the active exception without inventing Thread Mode before its return";
}

TEST(CtraceUnitTests, testCtfBundleOutputUsesCtraceRunMeta)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-ctrace-run-meta-test");
  const auto& outputDir = temporaryOutput.outputDirectory();

  TraceRunConfig traceRun;
  traceRun.path = "Board.ctrace-run.yml";
  traceRun.traceFormat = TraceRunFormat::Formatted;
  auto signedByteReference = TraceRunTestSupport::makeReference("dwt", "core", 7U, {0U}, "core/data#0");
  signedByteReference.dataSetupIndex = 0U;
  signedByteReference.label = "Sine";
  signedByteReference.dataType = "signed";
  signedByteReference.dataSize = 1U;
  traceRun.references.push_back(signedByteReference);

  auto reference = TraceRunTestSupport::makeReference("dwt", "core", std::nullopt, {2U}, "core/data#2");
  reference.dataSetupIndex = 2U;
  reference.label = "Current\n\t\"\\\x01";
  reference.address = 0x24000e88U;
  reference.dataType = "signed";
  reference.dataSize = 4U;
  traceRun.references.push_back(reference);
  traceRun.references.push_back(TraceRunTestSupport::makeReference("itm", "core", 7U, {}, "core/itm"));
  TraceRunSetup setup;
  setup.processorName = "core";
  setup.timestamps = TraceRunTimestampSetup{280000000U, 1U};
  traceRun.setups.push_back(std::move(setup));

  CollectingDiagnosticSink preflightDiagnostics;
  auto outputPlan = planTraceOutputs({false, true, {}}, outputDir.parent_path() / "output.SWO.raw",
                                     CtraceRunMeta::fromConfig(traceRun), preflightDiagnostics);
  ASSERT_TRUE(outputPlan.ctf.has_value() && preflightDiagnostics.events().empty()) << "resolved CTF source missing";
  auto options = std::move(*outputPlan.ctf);
  ASSERT_TRUE(!options.metadata.sources.empty()) << "resolved CTF source missing";
  ASSERT_TRUE(options.metadata.sources.front().route.traceBusId == 7U)
      << "resolved CTF source must retain its Trace Bus ID";
  ASSERT_TRUE(options.metadata.sources.front().dataType == "signed") << "resolved CTF source must retain its data type";
  ASSERT_EQ(options.routes.size(), 1U);
  ASSERT_EQ(options.metadata.streams.size(), 1U);
  EXPECT_EQ(options.metadata.streams.front().streamClassId, CtfStreamClassId{7U});
  std::filesystem::create_directories(outputDir);
  CtfMetadataModel model(CtfTestSupport::testUuid(), std::move(options.metadata));
  CtfMetadataWriter::write(outputDir, model);

  const auto metadata = readTestTextFile(outputDir / "metadata");

  ASSERT_TRUE(metadata.find("freq = 280000000") != std::string::npos) << "CTF trace-run clock mismatch";
  ASSERT_TRUE(metadata.find("cmsis_stream_7_dwt0_value_type = \"signed\"") != std::string::npos)
      << "CTF signed-byte source type mismatch";
  ASSERT_TRUE(metadata.find("cmsis_stream_7_dwt2_value_type = \"signed\"") != std::string::npos)
      << "CTF trace-run type mismatch";
  ASSERT_TRUE(metadata.find("cmsis_stream_7_dwt2_address_start = \"0x24000E88\"") != std::string::npos)
      << "CTF trace-run start address mismatch";
  ASSERT_TRUE(metadata.find("cmsis_stream_7_dwt2_address_end = \"0x24000E8B\"") != std::string::npos)
      << "CTF trace-run end address mismatch";
  ASSERT_TRUE(metadata.find("\"Current\\n\\t\\\"\\\\\\x01\" = 2") != std::string::npos)
      << "CTF trace-run label escaping mismatch";
  ASSERT_TRUE(metadata.find("stream_id = 7") != std::string::npos)
      << "generalized metadata must bind events to the configured stream class";
}

TEST(CtraceUnitTests, testCtfOutputPlanningKeepsUnknownFilterWithoutLegacyBootstrap)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-unknown-selected-route-test");
  const auto& outputDir = temporaryOutput.outputDirectory();
  TraceRunConfig traceRun;
  traceRun.path = "SelectedRoute.ctrace-run.yml";
  traceRun.traceFormat = TraceRunFormat::Formatted;
  traceRun.setups.push_back(TraceRunTestSupport::makeTimestampSetup("core", 1000000U, 1U));
  traceRun.references.push_back(TraceRunTestSupport::makeReference("itm", "core", 2U, {1U}, "core/itm"));
  const auto meta = CtraceRunMeta::fromConfig(traceRun);

  TraceSelection unknownSelection;
  unknownSelection.streams = {99U};
  CollectingDiagnosticSink unknownDiagnostics;
  auto plan = planTraceOutputs({false, true, unknownSelection}, outputDir.parent_path() / "output.SWO.raw", meta,
                               unknownDiagnostics);
  ASSERT_TRUE(plan.ctf.has_value()) << "an unmatched stream filter must retain the requested CTF plan";
  EXPECT_TRUE(plan.ctf->metadata.streams.empty())
      << "an unmatched formatted stream filter must not invent a CTF topology";
  EXPECT_TRUE(unknownDiagnostics.events().empty());
  EXPECT_FALSE(std::filesystem::exists(outputDir));
}

TEST(CtraceUnitTests, testCtfBundleOutputPreservesLegacyUnknownStreamFilter)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-legacy-unknown-stream-test");
  const auto& outputDir = temporaryOutput.outputDirectory();
  TraceRunConfig traceRun;
  traceRun.path = "Legacy.ctrace-run.yml";
  traceRun.setups.push_back(TraceRunTestSupport::makeTimestampSetup(std::nullopt, 1000000U));
  traceRun.references.push_back(TraceRunTestSupport::makeReference("itm", std::nullopt, std::nullopt, {1U}, "itm"));
  TraceSelection selection;
  selection.streams = {99U};
  CollectingDiagnosticSink diagnostics;
  auto plan = planTraceOutputs({false, true, selection}, outputDir.parent_path() / "output.SWO.raw",
                               CtraceRunMeta::fromConfig(traceRun), diagnostics);

  ASSERT_TRUE(plan.ctf.has_value() && diagnostics.events().empty());
  ASSERT_TRUE(CtfMetadataModel(CtfTestSupport::testUuid(), plan.ctf->metadata).isLegacySingleStreamLayout());
  CtfBundleOutput output(std::move(*plan.ctf));
  output.start();
  output.stop();

  ASSERT_TRUE(std::filesystem::is_regular_file(outputDir / "stream_0"));
  EXPECT_EQ(std::filesystem::file_size(outputDir / "stream_0"), 0U);
  EXPECT_TRUE(readCtfRecords(outputDir / "stream_0").empty());
  const auto metadata = readTestTextFile(outputDir / "metadata");
  EXPECT_NE(metadata.find("name = swo_clock;"), std::string::npos);
  EXPECT_NE(metadata.find("stream {\n    id = 0;"), std::string::npos);
  EXPECT_TRUE(std::filesystem::is_regular_file(testTraceCompassXmlPath(outputDir)));
}

TEST(CtraceUnitTests, testCtfBundleOutputDefaultsDwtValueType)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-default-dwt-type-test");
  const auto& root = temporaryPath.path();

  TraceRunConfig defaultTraceRun;
  auto defaultReference =
      TraceRunTestSupport::makeReference("dwt", std::nullopt, std::nullopt, {0U}, "opaque/default-dwt");
  defaultReference.dataSetupIndex = 0U;
  defaultTraceRun.references.push_back(defaultReference);
  const auto defaultMeta = CtraceRunMeta::fromConfig(defaultTraceRun);
  ASSERT_TRUE(defaultMeta.sources().size() == 1U && defaultMeta.sources().front().dataType == "unsigned" &&
              defaultMeta.sources().front().dataSize == 4U)
      << "missing DWT data-type/size must default to unsigned/4";

  const auto defaultOutputDir = root / "default";
  auto defaultOptions = makeCtfBundleConfig(defaultOutputDir, 1000000U);
  defaultOptions.metadata.sources = {resolvedSource(defaultMeta.sources().front())};
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput defaultOutput(std::move(defaultOptions), &diagnostics);
  defaultOutput.start();
  defaultOutput.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 1U, 0xffU, AccessType::Write}});
  defaultOutput.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 2U, 0xffffU, AccessType::Write}});
  defaultOutput.stop();
  ASSERT_TRUE(readFirstCtfDwtValueTag(defaultOutputDir / "stream_0") == 5U)
      << "default DWT metadata must select the unsigned 32-bit CTF variant";
  const auto& sizeWarning = diagnostics.singleEvent();
  ASSERT_TRUE(sizeWarning.severity == DiagnosticSink::Severity::Warning && diagnostics.failureCount() == 0U)
      << "DWT size mismatch must be reported once per channel";

  TraceRunConfig signedTraceRun;
  auto signedReference = defaultReference;
  signedReference.dataType = "signed";
  signedReference.dataSize = 1U;
  signedTraceRun.references.push_back(signedReference);
  const auto signedMeta = CtraceRunMeta::fromConfig(signedTraceRun);
  ASSERT_TRUE(signedMeta.sources().size() == 1U) << "signed DWT source missing";
  const auto signedOutputDir = root / "signed";
  auto signedOptions = makeCtfBundleConfig(signedOutputDir, 1000000U);
  signedOptions.metadata.sources = {resolvedSource(signedMeta.sources().front())};
  CollectingDiagnosticSink signedDiagnostics;
  CtfBundleOutput signedOutput(std::move(signedOptions), &signedDiagnostics);
  signedOutput.start();
  signedOutput.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 1U, 0xffU, AccessType::Write}});
  signedOutput.stop();
  ASSERT_TRUE(readFirstCtfDwtValueTag(signedOutputDir / "stream_0") == 0U)
      << "explicit signed/1 metadata must select the signed 8-bit CTF variant";
  ASSERT_TRUE(signedDiagnostics.events().empty()) << "matching DWT sizes must not produce a warning";

  TraceRunConfig traceRun;
  traceRun.path = "ambiguous-streams.ctrace-run.yml";
  traceRun.traceFormat = TraceRunFormat::Formatted;
  auto first = TraceRunTestSupport::makeReference("dwt", "core-one", 1U, {0U}, "core-one/data#0");
  first.dataSetupIndex = 0U;
  first.label = "core-one";
  first.dataType = "signed";
  first.dataSize = 4U;
  TraceRunReference second = first;
  second.processorName = "core-two";
  second.ctraceRef = "core-two/data#0";
  second.stream = 2U;
  second.label = "core-two";
  traceRun.references = {
      first,
      second,
      TraceRunTestSupport::makeReference("itm", "core-one", 1U, {}, "core-one/itm"),
      TraceRunTestSupport::makeReference("itm", "core-two", 2U, {}, "core-two/itm"),
  };
  const auto meta = CtraceRunMeta::fromConfig(traceRun);
  ASSERT_TRUE(meta.sources().size() == 2U && meta.sources().front().route.traceBusId == 1U &&
              meta.sources().front().label == std::optional<std::string>("core-one"))
      << "trace-run metadata must preserve the exact DWT stream route";
}

TEST(CtraceUnitTests, testCtfWarningsRemainVisibleWithoutResettingContext)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-warning-test");
  const auto& root = temporaryPath.path();
  const auto filteredDir = root / "filtered.ctf";
  const auto contextDir = root / "context.ctf";
  const auto dataLossDir = root / "data-loss.ctf";

  auto filteredOptions = makeCtfBundleConfig(filteredDir, 1000000U);
  filteredOptions.selection.types.push_back("error");
  CtfBundleOutput filtered(std::move(filteredOptions));
  filtered.start();
  TraceEvent warning = issuePacket(TraceIssueCode::OpenCsdDecodeError, "decoder warning", TraceIssueSeverity::Warning);
  filtered.writeEvent(warning);
  filtered.stop();
  ASSERT_TRUE(readOnlyCtfTraceStatusReasons(filteredDir / "stream_0") ==
              std::vector<std::uint8_t>({CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError)}))
      << "--type error must retain decoder warnings in CTF";

  CtfBundleOutput context(makeCtfBundleConfig(contextDir, 1000000U));
  context.start();
  context.writeEvent(exceptionPacket(15U, ExceptionAction::Entered, 10U));
  context.writeEvent(warning);
  context.writeEvent(exceptionPacket(54U, ExceptionAction::Entered, 20U));
  context.stop();
  ASSERT_TRUE(
      readCtfExceptionRecords(contextDir / "stream_0") ==
      std::vector<CtfExceptionRecord>({{0U, 0U, 1U}, {0U, 1U, 1U}, {15U, 0U, 0U}, {15U, 1U, 1U}, {54U, 0U, 0U}}))
      << "a decoder warning must not reset the active CTF exception context";

  auto dataLossOptions = makeCtfBundleConfig(dataLossDir, 1000000U);
  dataLossOptions.selection.types.push_back("exception");
  CtfBundleOutput dataLoss(std::move(dataLossOptions));
  dataLoss.start();
  dataLoss.writeEvent(exceptionPacket(15U, ExceptionAction::Entered, 10U));
  dataLoss.writeEvent(issuePacket(TraceIssueCode::DataLoss, "decoder data loss"));
  dataLoss.writeEvent(exceptionPacket(15U, ExceptionAction::Returned, 20U));
  dataLoss.stop();
  ASSERT_TRUE(
      readCtfExceptionRecords(dataLossDir / "stream_0") ==
      std::vector<CtfExceptionRecord>({{0U, 0U, 1U}, {0U, 1U, 1U}, {15U, 0U, 0U}, {15U, 1U, 1U}, {15U, 2U, 0U}}))
      << "filtered data-loss must still reset the CTF exception context";
}

TEST(CtraceUnitTests, testCtfBundleOutputTypeFilterExcludesSyntheticEvents)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-filter-test");
  const auto& outputDir = temporaryOutput.outputDirectory();

  auto options = makeCtfBundleConfig(outputDir, 1000000U);
  options.selection.types.push_back("itm");
  CtfBundleOutput output(std::move(options));
  output.start();

  output.writeEvent(atCycle(softwarePacket(1U, 1U, 'A'), 100U));
  output.writeEvent(exceptionPacket(15, ExceptionAction::Entered, 200));
  output.writeEvent(overflowPacket(300));
  auto error = atCycle(issuePacket(TraceIssueCode::OpenCsdBadPacketSequence), 400U);
  error.quality = TraceQuality{true, false, 1U};
  output.writeEvent(error);
  output.stop();

  requireSingleItmEvent(outputDir / "stream_0", 1U,
                        "CTF itm filter should exclude synthetic events and errors of other packet types");
}

TEST(CtraceUnitTests, testCtfGlobalTimestampDoesNotEstablishLocalTimeQuality)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-global-timestamp-quality-test");
  const auto& outputDir = temporaryOutput.outputDirectory();

  auto options = makeCtfBundleConfig(outputDir, 1000000U);
  options.selection.types.push_back("itm");
  CtfBundleOutput output(std::move(options));
  output.start();

  output.writeEvent(atCycle(TraceEvent{GlobalTimestampTraceEvent{1234U, false}}, 42U));
  output.writeEvent(softwarePacket(1U, 1U, 'A'));
  output.stop();

  const auto records = readCtfRecords(outputDir / "stream_0");
  constexpr std::uint8_t timestampReliable = 1U << 1U;
  constexpr std::uint8_t beforeFirstLocalTimestamp = 1U << 2U;
  ASSERT_TRUE(records.size() == 1U && records.front().id == CtfSchema::value(CtfSchema::EventId::Itm))
      << "CTF ITM event missing after global timestamp";
  ASSERT_TRUE(records.front().payload[3U] == (timestampReliable | beforeFirstLocalTimestamp))
      << "global timestamp must not mark following samples as locally timestamped";
}

TEST(CtraceUnitTests, testCtfHoldsRegressingEventTimestamps)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-regressing-timestamp-test");
  const auto& outputDir = temporaryOutput.outputDirectory();

  auto options = makeCtfBundleConfig(outputDir, 1000000U);
  options.selection.types.push_back("itm");
  CtfBundleOutput output(std::move(options));
  output.start();
  output.writeEvent(atCycle(softwarePacket(1U, 1U, 'A'), 100U));
  output.writeEvent(atCycle(softwarePacket(1U, 1U, 'B'), 10U));
  output.stop();

  const auto records = readCtfRecords(outputDir / "stream_0");
  ASSERT_TRUE(records.size() == 2U && records[0].timestamp == 100U) << "first CTF event timestamp mismatch";
  ASSERT_TRUE(records[1].timestamp == 100U) << "CTF event timestamps must not regress";
}

TEST(CtraceUnitTests, testCtfGlobalTimestampEvent)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-global-timestamp-event-test");
  const auto& outputDir = temporaryOutput.outputDirectory();

  auto options = makeCtfBundleConfig(outputDir, 1000000U);
  options.selection.types.push_back("global_ts");
  CtfBundleOutput output(std::move(options));
  output.start();
  constexpr std::uint64_t timestampValue = 0x123456789abcdef0ULL;
  output.writeEvent(TraceEvent{GlobalTimestampTraceEvent{timestampValue, true}});
  output.stop();

  const auto records = readCtfRecords(outputDir / "stream_0");
  const auto metadata = readTestTextFile(outputDir / "metadata");

  ASSERT_TRUE(records.size() == 1U) << "CTF global timestamp filter emitted unrelated events";
  const auto& record = records.front();
  ASSERT_TRUE(record.id == CtfSchema::value(CtfSchema::EventId::GlobalTimestamp))
      << "CTF global timestamp event ID mismatch";
  ASSERT_TRUE(readLe64(record.payload, 0U) == timestampValue) << "CTF global timestamp value mismatch";
  ASSERT_TRUE(record.payload[8U] == 1U) << "CTF global timestamp clock-change flag mismatch";
  ASSERT_TRUE(metadata.find("name = \"GLOBAL_TIMESTAMP\"") != std::string::npos)
      << "CTF global timestamp metadata missing";
}

TEST(CtraceUnitTests, testCtfBundleOutputExcludesSoftwareChannelZero)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-channel-zero-test");
  const auto& outputDir = temporaryOutput.outputDirectory();
  TraceRunConfig traceRun;
  traceRun.setups.push_back(TraceRunTestSupport::makeTimestampSetup(std::nullopt, 1000000U));
  auto channelZero = TraceRunTestSupport::makeReference("itm", std::nullopt, std::nullopt, {0U}, "opaque/channel-zero");
  channelZero.label = "Console";
  traceRun.references.push_back(std::move(channelZero));
  TraceSelection selection{{"itm", "error"}, {}};
  CollectingDiagnosticSink preflightDiagnostics;
  auto outputPlan = planTraceOutputs({false, true, selection}, outputDir.parent_path() / "output.SWO.raw",
                                     CtraceRunMeta::fromConfig(traceRun), preflightDiagnostics);
  ASSERT_TRUE(outputPlan.ctf.has_value() && outputPlan.ctf->metadata.sources.empty() &&
              preflightDiagnostics.events().empty())
      << "CTF preflight must exclude software channel zero metadata";
  auto options = std::move(*outputPlan.ctf);
  CtfBundleOutput output(std::move(options));
  output.start();
  output.writeEvent(atCycle(softwarePacket(0U, 1U, 'A'), 100U));
  output.writeEvent(atCycle(issuePacket(TraceIssueCode::OpenCsdIncompleteTail), 101U));
  output.stop();
  ASSERT_TRUE(readOnlyCtfTraceStatusReasons(outputDir / "stream_0") ==
              std::vector<std::uint8_t>({CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError)}))
      << "CTF must exclude software channel zero payload but retain its decoder errors";
  std::vector<std::string> bundleEntries;
  for (const auto& entry : std::filesystem::directory_iterator(outputDir)) {
    bundleEntries.push_back(entry.path().filename().string());
  }
  std::sort(bundleEntries.begin(), bundleEntries.end());
  ASSERT_TRUE(bundleEntries == std::vector<std::string>({"metadata", "stream_0"}))
      << "CTF bundle must contain only metadata and the primary stream";

  const auto metadata = readTestTextFile(outputDir / "metadata");
  ASSERT_TRUE(metadata.find("ITM" + std::to_string(0)) == std::string::npos)
      << "CTF metadata must not register software channel zero";
  ASSERT_TRUE(metadata.find("cmsis_itm0_") == std::string::npos && metadata.find("Console") == std::string::npos)
      << "CTF analysis metadata must not expose configured software channel zero";
}

TEST(CtraceUnitTests, testCtfBundleOutputAbortRemovesPartialBundle)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-direct-abort-test");
  const auto& outputDir = temporaryOutput.outputDirectory();
  const auto xmlPath = testTraceCompassXmlPath(outputDir);
  writeTestFile(outputDir / "old-marker", "old");
  writeTestFile(xmlPath, "old-xml");

  auto options = makeCtfBundleConfig(outputDir, 1000000U);
  CtfBundleOutput output(std::move(options));
  output.start();
  ASSERT_TRUE(!std::filesystem::exists(outputDir / "old-marker") &&
              std::filesystem::is_regular_file(outputDir / "stream_0") && !std::filesystem::exists(xmlPath))
      << "CTF start must remove both previous targets while deferring companion XML";

  TraceEvent software = softwarePacket(1U, 1U, 'A');
  output.writeEvent(software);
  output.abort();
  ASSERT_TRUE(!std::filesystem::exists(outputDir) && !std::filesystem::exists(xmlPath))
      << "CTF abort must remove the incomplete direct output";
}

TEST(CtraceUnitTests, testCtfBundleOutputPreservesLegacyTraceCompassXmlAfterCompletion)
{
  const TemporaryTestPath root("ctrace-ctf-legacy-xml-completion-test");
  const auto outputDirectory = root.path() / "output.ctf";
  const auto xmlPath = testTraceCompassXmlPath(outputDirectory);
  const auto expectedXmlPath = root.path() / "expected.xml";
  TraceCompassXmlWriter::writeFile(expectedXmlPath);

  CtfBundleOutput output(makeCtfBundleConfig(outputDirectory, 1000000U));
  output.start();
  EXPECT_FALSE(std::filesystem::exists(xmlPath));
  output.writeEvent(atCycle(softwarePacket(1U, 1U, 'A'), 10U));
  output.stop();

  ASSERT_TRUE(std::filesystem::is_regular_file(xmlPath));
  EXPECT_EQ(readTestTextFile(xmlPath), readTestTextFile(expectedXmlPath));
}

TEST(CtraceUnitTests, testCtfBundleOutputGeneratesRoutePrefixedXmlForSharedClockStreams)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-shared-clock-xml-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity last{TraceRouteId{20U}, 111U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{7U}, "shared_clock", CtfTestSupport::testUuid(7U), 1000000U, false}},
      {
          {CtfStreamClassId{1U}, first, CtfSourceKind::Itm, "core-one", CtfClockDomainId{7U}},
          {CtfStreamClassId{111U}, last, CtfSourceKind::Itm, "core-last", CtfClockDomainId{7U}},
      },
      {},
  };
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)), &diagnostics);

  output.start();
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_111"));
  output.writeEvent(onRoute(atCycle(softwarePacket(1U, 1U, 'A'), 10U), last));
  output.writeEvent(onRoute(atCycle(softwarePacket(2U, 1U, 'B'), 20U), first));
  output.stop();

  ASSERT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  ASSERT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_111"));
  const auto xml = readTestTextFile(testTraceCompassXmlPath(outputDirectory));
  EXPECT_NE(xml.find("<stateAttribute type=\"eventField\" value=\"cmsis_trace_bus_id\" />"), std::string::npos);
  EXPECT_NE(xml.find("<entry path=\"*/ITM/*\""), std::string::npos);
  requireRoutePrefixedStateChanges(xml);
  requireRoutePrefixedViewEntries(xml);
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testCtfBundleOutputKeepsIndependentClockCtfAndOmitsStaleXml)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-independent-clock-xml-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const auto xmlPath = testTraceCompassXmlPath(outputDirectory);
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity second{TraceRouteId{20U}, 2U};
  CtfMetadataTopology topology{
      {
          {CtfClockDomainId{1U}, "first_clock", CtfTestSupport::testUuid(1U), 1000000U, false},
          {CtfClockDomainId{2U}, "second_clock", CtfTestSupport::testUuid(2U), 1000000U, false},
      },
      {
          {CtfStreamClassId{1U}, first, CtfSourceKind::Itm, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{2U}, second, CtfSourceKind::Itm, "core-two", CtfClockDomainId{2U}},
      },
      {},
  };
  writeTestFile(xmlPath, "stale-xml");
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)), &diagnostics);

  output.start();
  EXPECT_FALSE(std::filesystem::exists(xmlPath));
  output.writeEvent(onRoute(softwarePacket(1U, 1U, 'A'), first));
  output.writeEvent(onRoute(softwarePacket(2U, 1U, 'B'), second));
  output.stop();
  output.stop();

  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_2"));
  EXPECT_FALSE(std::filesystem::exists(xmlPath));
  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_EQ(diagnostics.events().front().severity, DiagnosticSink::Severity::Warning);
  EXPECT_EQ(diagnostics.failureCount(), 0U);
  EXPECT_TRUE(diagnostics.containsContext("clockDomains", "2"));
}

TEST(CtraceUnitTests, testCtfBundleOutputBasesXmlAndMetadataOnEmittedStreams)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-emitted-clock-xml-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity last{TraceRouteId{20U}, 111U};
  CtfMetadataTopology topology{
      {
          {CtfClockDomainId{1U}, "emitted_clock", CtfTestSupport::testUuid(1U), 1000000U, false},
          {CtfClockDomainId{2U}, "silent_clock", CtfTestSupport::testUuid(2U), 2000000U, false},
      },
      {
          {CtfStreamClassId{1U}, first, CtfSourceKind::Itm, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{111U}, last, CtfSourceKind::Itm, "core-last", CtfClockDomainId{2U}},
      },
      {},
  };
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)), &diagnostics);

  output.start();
  output.writeEvent(onRoute(softwarePacket(1U, 1U, 'A'), first));
  output.stop();

  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("name = emitted_clock;"), std::string::npos);
  EXPECT_EQ(metadata.find("name = silent_clock;"), std::string::npos);
  EXPECT_NE(metadata.find("stream {\n    id = 1;"), std::string::npos);
  EXPECT_EQ(metadata.find("stream {\n    id = 111;"), std::string::npos);
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_111"));
  EXPECT_TRUE(std::filesystem::is_regular_file(testTraceCompassXmlPath(outputDirectory)));
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testCtfBundleOutputWritesMetadataOnlyForEmptyFormattedTrace)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-empty-formatted-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{1U}, "configured_clock", CtfTestSupport::testUuid(1U), 1000000U, false}},
      {{CtfStreamClassId{1U}, route, CtfSourceKind::Itm, "core-one", CtfClockDomainId{1U}}},
      {},
  };
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)), &diagnostics);

  output.start();
  output.stop();

  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_EQ(metadata.find("\nclock {"), std::string::npos);
  EXPECT_EQ(metadata.find("\nstream {"), std::string::npos);
  EXPECT_EQ(metadata.find("\nevent {"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(testTraceCompassXmlPath(outputDirectory)));
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testCtfBundleOutputCleansUpDirectWriteFailures)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-direct-write-failure-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const auto xmlPath = testTraceCompassXmlPath(outputDirectory);
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity last{TraceRouteId{20U}, 111U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{1U}, "formatted_clock", CtfTestSupport::testUuid(1U), 1000000U, false}},
      {
          {CtfStreamClassId{1U}, first, CtfSourceKind::Itm, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{111U}, last, CtfSourceKind::Itm, "core-last", CtfClockDomainId{1U}},
      },
      {},
  };
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)));

  output.start();
  output.writeEvent(onRoute(softwarePacket(1U), first));
  ASSERT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  std::filesystem::create_directory(outputDirectory / "stream_111");
  writeTestFile(xmlPath, "partial-xml");
  EXPECT_THROW(output.writeEvent(onRoute(softwarePacket(2U), last)), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
  EXPECT_FALSE(std::filesystem::exists(xmlPath));
}

TEST(CtraceUnitTests, testCtfBundleOutputCleansUpAfterEncoderStartFailure)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-encoder-start-failure-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const auto xmlPath = testTraceCompassXmlPath(outputDirectory);
  writeTestFile(xmlPath, "stale-xml");
  CtfBundleOutput output(CtfOutputConfig(outputDirectory, xmlPath, {}, CtfMetadataTopology{}));

  EXPECT_THROW(output.start(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
  EXPECT_FALSE(std::filesystem::exists(xmlPath));
}

TEST(CtraceUnitTests, testCtfBundleOutputReplacesExistingBundleAtStart)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-direct-replace-test");
  const auto& outputDir = temporaryOutput.outputDirectory();
  const auto xmlPath = testTraceCompassXmlPath(outputDir);
  writeTestFile(outputDir / "old-marker", "old");
  writeTestFile(xmlPath, "old-xml");

  auto options = makeCtfBundleConfig(outputDir, 1000000U);
  CtfBundleOutput output(std::move(options));
  output.start();
  ASSERT_TRUE(!std::filesystem::exists(outputDir / "old-marker") &&
              std::filesystem::is_regular_file(outputDir / "stream_0") &&
              !std::filesystem::exists(outputDir / "metadata") && !std::filesystem::exists(xmlPath))
      << "CTF start must replace existing output before decoding begins";

  output.writeEvent(atCycle(softwarePacket(1U, 1U, 'A'), 10U));
  output.stop();
  ASSERT_TRUE(std::filesystem::is_regular_file(outputDir / "metadata") &&
              std::filesystem::file_size(outputDir / "stream_0") > 0U && std::filesystem::is_regular_file(xmlPath))
      << "CTF stop must complete the directly written bundle";
}

TEST(CtraceUnitTests, testCtfBundleOutputRejectsOverlappingTargetsBeforeDeletion)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-overlapping-targets-test");
  const auto& root = temporaryPath.path();
  const auto ctfDirectory = root / "Bundle.ctf";
  const auto nestedXml = ctfDirectory / "Bundle.SWO.traceanalysis.xml";
  writeTestFile(ctfDirectory / "old-marker", "old-ctf");
  writeTestFile(nestedXml, "old-xml");

  const auto rejected = throwsException<std::invalid_argument>(
      [&] { CtfBundleOutput output(makeCtfBundleConfig(ctfDirectory, nestedXml, 1000000U)); });
  ASSERT_TRUE(rejected && readTestTextFile(ctfDirectory / "old-marker") == "old-ctf" &&
              readTestTextFile(nestedXml) == "old-xml")
      << "overlapping CTF targets must be rejected before either existing target is deleted";

  const auto wrongTypeCtf = root / "WrongType.ctf";
  const auto wrongTypeXml = root / "WrongType.SWO.traceanalysis.xml";
  writeTestFile(wrongTypeCtf, "not-a-directory");
  std::filesystem::create_directory(wrongTypeXml);
  const auto rejectedWrongTypes = throwsException([&] {
    CtfBundleOutput output(makeCtfBundleConfig(wrongTypeCtf, wrongTypeXml, 1000000U));
    output.start();
  });
  ASSERT_TRUE(rejectedWrongTypes && readTestTextFile(wrongTypeCtf) == "not-a-directory" &&
              std::filesystem::is_directory(wrongTypeXml))
      << "CTF start must reject unexpected target types before deleting either target";

  const auto rejectedCaseInsensitiveOverlap = throwsException<std::invalid_argument>([&] {
    CtfBundleOutput output(
        makeCtfBundleConfig(root / "Bundle.ctf", root / "BUNDLE.CTF" / "Bundle.SWO.traceanalysis.xml", 1000000U));
  });
  ASSERT_TRUE(rejectedCaseInsensitiveOverlap) << "CTF target overlap checks must conservatively ignore ASCII case";
}

TEST(CtraceUnitTests, testCtfBundleOutputOwnsDirectLifecycle)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-bundle-lifecycle-test");
  const auto& testRoot = temporaryPath.path();
  const auto root = testRoot / std::filesystem::u8path(u8"Gr\u00f6\u00dfe");
  const auto ctfDirectory = root / "Bundle.ctf";
  const auto xmlPath = root / "Bundle.SWO.traceanalysis.xml";

  auto config = makeCtfBundleConfig(ctfDirectory, 1000000U);
  CtfBundleOutput output(std::move(config));
  output.start();
  const auto software = atCycle(softwarePacket(1U, 1U, 'A'), 10U);
  output.writeEvent(software);
  output.stop();

  requireCompleteCtfBundle(ctfDirectory, xmlPath, "CTF bundle must support Unicode output paths");
  ASSERT_TRUE(!readTestTextFile(xmlPath).empty()) << "CTF bundle did not write the Trace Compass XML";

  writeTestFile(ctfDirectory / "stale-marker", "stale");
  writeTestFile(xmlPath, "stale-xml");
  output.start();
  ASSERT_TRUE(!std::filesystem::exists(ctfDirectory / "stale-marker") &&
              !std::filesystem::exists(ctfDirectory / "metadata") && !std::filesystem::exists(xmlPath))
      << "restarting CTF output must delete the previous bundle before writing";
  output.writeEvent(software);
  output.stop();
  requireCompleteCtfBundle(ctfDirectory, xmlPath, "restarted CTF bundle must complete normally");

  std::filesystem::create_directories(testRoot / "working");
  std::filesystem::create_directories(testRoot / "captures");
  const auto relativeRoot = testRoot / "working" / ".." / "captures";
  const auto relativeCtf = relativeRoot / "Relative.ctf";
  CtfBundleOutput relativeOutput(makeCtfBundleConfig(relativeCtf, 1000000U));
  relativeOutput.start();
  relativeOutput.writeEvent(software);
  relativeOutput.stop();
  ASSERT_TRUE(std::filesystem::is_regular_file(testRoot / "captures" / "Relative.ctf" / "metadata") &&
              std::filesystem::is_regular_file(testRoot / "captures" / "Relative.SWO.traceanalysis.xml"))
      << "CTF output must accept a legitimate parent-relative trace path";
}

TEST(CtraceUnitTests, testCtfBundleOutputReportsIdentityAndSupportsInactiveStop)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-identity-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  CtfBundleOutput output(makeCtfBundleConfig(outputDirectory, 1000000U));
  EXPECT_EQ(output.backendName(), "ctf");
  EXPECT_EQ(output.targetPath(), outputDirectory.string());
  output.stop();
  output.writeEvent(softwarePacket(1U));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
}

TEST(CtraceUnitTests, testCtfBundleOutputRejectsUnsafeTargets)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-unsafe-targets-test");
  const auto safeCtf = temporaryPath.path() / "safe.ctf";
  const auto safeXml = temporaryPath.path() / "safe.xml";
  for (const auto& unsafe : {std::filesystem::path{}, std::filesystem::path("."), std::filesystem::path("..")}) {
    EXPECT_THROW((void)CtfBundleOutput(makeCtfBundleConfig(unsafe, safeXml, 1000000U)), std::invalid_argument);
    EXPECT_THROW((void)CtfBundleOutput(makeCtfBundleConfig(safeCtf, unsafe, 1000000U)), std::invalid_argument);
  }
}

TEST(CtraceUnitTests, testCtfBundleOutputRejectsInvalidExistingXmlAndLongPaths)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-invalid-existing-output-test");
  const auto ctfDirectory = temporaryPath.path() / "output.ctf";
  const auto xmlDirectory = temporaryPath.path() / "output.xml";
  std::filesystem::create_directories(xmlDirectory);
  CtfBundleOutput directoryXml(makeCtfBundleConfig(ctfDirectory, xmlDirectory, 1000000U));
  EXPECT_THROW(directoryXml.start(), std::runtime_error);

  const auto longName = std::string(1024U, 'x');
  CtfBundleOutput longCtf(
      makeCtfBundleConfig(temporaryPath.path() / longName, temporaryPath.path() / "long-ctf.xml", 1000000U));
  EXPECT_THROW(longCtf.start(), std::runtime_error);
  CtfBundleOutput longXml(
      makeCtfBundleConfig(temporaryPath.path() / "long-xml.ctf", temporaryPath.path() / longName, 1000000U));
  EXPECT_THROW(longXml.start(), std::runtime_error);
}

TEST(CtraceUnitTests, testCtfBundleOutputCleansUpAfterMetadataFailure)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-metadata-failure-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const auto xmlPath = testTraceCompassXmlPath(outputDirectory);
  CtfBundleOutput output(makeCtfBundleConfig(outputDirectory, 1000000U));
  output.start();
  std::filesystem::create_directory(outputDirectory / "metadata");
  EXPECT_THROW(output.stop(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
  EXPECT_FALSE(std::filesystem::exists(xmlPath));
}

TEST(CtraceUnitTests, testCtfBundleOutputCleansUpAfterTraceCompassStartFailure)
{
  const TemporaryTestPath root("ctrace-ctf-start-failure-test");
  root.createDirectory();
  const auto blockedParent = root.path() / "not-a-directory";
  writeTestFile(blockedParent, "file");
  const auto outputDirectory = root.path() / "trace.ctf";
  const auto xmlPath = blockedParent / "trace.xml";

  CtfBundleOutput output(makeCtfBundleConfig(outputDirectory, xmlPath, 1000000U));
  EXPECT_THROW(output.start(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
}

TEST(CtraceUnitTests, testCtfBundleOutputReportsPseudoFilesystemFinishFailure)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  const TemporaryTestPath temporaryPath("ctrace-ctf-pseudo-filesystem-test");
  const auto outputDirectory = temporaryPath.path() / "output.ctf";
  CtfBundleOutput output(
      makeCtfBundleConfig(outputDirectory, TestPlatform::creationFailurePath("ctrace-coverage-output.xml"), 1000000U));
  output.start();
  EXPECT_THROW(output.stop(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
}

TEST(CtraceUnitTests, testCtfBundleOutputReportsPermissionFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::PosixPermissions)) {
    GTEST_SKIP();
  }
  const TemporaryTestPath temporaryPath("ctrace-ctf-permission-failure-test");
  const auto& root = temporaryPath.createDirectory();

  const auto destructorCtf = root / "destructor.ctf";
  const auto destructorXml = root / "destructor.xml";
  {
    CtfBundleOutput output(makeCtfBundleConfig(destructorCtf, destructorXml, 1000000U));
    output.start();
    std::filesystem::permissions(root, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  }
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);
  EXPECT_TRUE(std::filesystem::exists(destructorCtf));
  EXPECT_FALSE(std::filesystem::exists(destructorXml));
  std::filesystem::remove_all(destructorCtf);
  std::filesystem::remove(destructorXml);

  const auto existingCtf = root / "existing.ctf";
  const auto existingXml = root / "existing.xml";
  writeTestFile(existingCtf / "marker", "existing");
  std::filesystem::permissions(root, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  CtfBundleOutput removeDirectoryFailure(makeCtfBundleConfig(existingCtf, existingXml, 1000000U));
  EXPECT_THROW(removeDirectoryFailure.start(), std::runtime_error);
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);
  std::filesystem::remove_all(existingCtf);

  const auto writableParent = root / "writable";
  const auto blockedParent = root / "blocked";
  std::filesystem::create_directories(writableParent);
  std::filesystem::create_directories(blockedParent);
  const auto blockedXml = blockedParent / "existing.xml";
  writeTestFile(blockedXml, "existing");
  std::filesystem::permissions(blockedParent, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  CtfBundleOutput removeFileFailure(makeCtfBundleConfig(writableParent / "output.ctf", blockedXml, 1000000U));
  EXPECT_THROW(removeFileFailure.start(), std::runtime_error);
  std::filesystem::permissions(blockedParent, std::filesystem::perms::owner_all);

  const auto cleanupCtf = writableParent / "cleanup.ctf";
  std::filesystem::permissions(blockedParent, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  CtfBundleOutput finishCleanupFailure(makeCtfBundleConfig(cleanupCtf, blockedParent / "new.xml", 1000000U));
  finishCleanupFailure.start();
  EXPECT_THROW(finishCleanupFailure.stop(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(cleanupCtf));
  std::filesystem::permissions(blockedParent, std::filesystem::perms::owner_all);

  const auto blockedCleanupParent = root / "blocked-cleanup";
  std::filesystem::create_directories(blockedCleanupParent);
  const auto blockedCleanupCtf = blockedCleanupParent / "output.ctf";
  const auto blockedCleanupXml = blockedCleanupParent / "output.xml";
  CtfBundleOutput blockedCleanup(makeCtfBundleConfig(blockedCleanupCtf, blockedCleanupXml, 1000000U));
  blockedCleanup.start();
  writeTestFile(blockedCleanupXml, "partial-xml");
  std::filesystem::permissions(blockedCleanupParent,
                               std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  std::string cleanupError;
  const auto abortBlockedOutputs = [&] {
    try {
      blockedCleanup.abort();
    } catch (const std::runtime_error& error) {
      cleanupError = error.what();
      throw;
    }
  };
  EXPECT_THROW(abortBlockedOutputs(), std::runtime_error);
  std::filesystem::permissions(blockedCleanupParent, std::filesystem::perms::owner_all);
  EXPECT_NE(cleanupError.find("CTF directory"), std::string::npos);
  EXPECT_NE(cleanupError.find("Trace Compass XML"), std::string::npos);
  EXPECT_TRUE(std::filesystem::exists(blockedCleanupCtf));
  EXPECT_TRUE(std::filesystem::exists(blockedCleanupXml));
  EXPECT_NO_THROW(blockedCleanup.abort());
  EXPECT_FALSE(std::filesystem::exists(blockedCleanupCtf));
  EXPECT_FALSE(std::filesystem::exists(blockedCleanupXml));

  const auto blockedCtf = blockedParent / "new.ctf";
  std::filesystem::permissions(blockedParent, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
  CtfBundleOutput createDirectoryFailure(makeCtfBundleConfig(blockedCtf, writableParent / "new.xml", 1000000U));
  EXPECT_THROW(createDirectoryFailure.start(), std::runtime_error);
  std::filesystem::permissions(blockedParent, std::filesystem::perms::owner_all);
}
