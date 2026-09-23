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

/** @brief Creates a CTF bundle configuration with test defaults. */
static CtfOutputConfig makeCtfBundleConfig(const std::filesystem::path& outputDirectory, std::uint64_t coreClockHz)
{
  return CtfOutputConfig(outputDirectory, {},
                         CtfTestSupport::legacyTopology(coreClockHz));
}

/** @brief Creates a formatted CTF bundle configuration from an explicit topology. */
static CtfOutputConfig makeFormattedCtfBundleConfig(const std::filesystem::path& outputDirectory,
                                                    CtfMetadataTopology topology, TraceSelection selection = {})
{
  std::vector<TraceRouteIdentity> routes;
  for (const auto& stream : topology.streams) {
    routes.push_back(stream.route);
  }
  return CtfOutputConfig(outputDirectory, std::move(selection),
                         std::move(topology), std::move(routes));
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
  ASSERT_NE(output.completedMetadata(), nullptr);
  EXPECT_TRUE(output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{0U}, CtfGraphicalTopic::Exception));
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
  EXPECT_FALSE(std::filesystem::exists(plan.ctf->outputDirectory));
}

TEST(CtraceUnitTests, testCtfBundleOutputOmitsExcludedLegacyStream)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-excluded-legacy-stream-test");
  const auto rawPath = temporaryOutput.outputDirectory().parent_path() / "output.SWO.raw";
  TraceRunConfig traceRun;
  traceRun.path = "Legacy.ctrace-run.yml";
  traceRun.setups.push_back(TraceRunTestSupport::makeTimestampSetup(std::nullopt, 1000000U));
  traceRun.setups.front().timestamps->clockHz.reset();
  traceRun.references.push_back(TraceRunTestSupport::makeReference("itm", std::nullopt, std::nullopt, {1U}, "itm"));
  TraceSelection selection;
  selection.streams = {1U};
  CollectingDiagnosticSink diagnostics;
  auto plan = planTraceOutputs({false, true, selection}, rawPath, CtraceRunMeta::fromConfig(traceRun), diagnostics);

  ASSERT_TRUE(plan.ctf.has_value() && diagnostics.events().empty());
  EXPECT_TRUE(plan.ctf->metadata.streams.empty());
  EXPECT_TRUE(plan.ctf->metadata.clockDomains.empty());
  const auto outputDir = plan.ctf->outputDirectory;
  CtfBundleOutput output(std::move(*plan.ctf));
  output.start();
  output.stop();

  EXPECT_FALSE(std::filesystem::exists(outputDir / "stream_0"));
  const auto metadata = readTestTextFile(outputDir / "metadata");
  EXPECT_EQ(metadata.find("\nclock {"), std::string::npos);
  EXPECT_EQ(metadata.find("\nstream {"), std::string::npos);
  EXPECT_EQ(metadata.find("\nevent {"), std::string::npos);
  ASSERT_NE(output.completedMetadata(), nullptr);
  EXPECT_TRUE(output.completedMetadata()->topology().streams.empty());
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
  ASSERT_TRUE(defaultMeta.routes().size() == 1U && defaultMeta.routes().front().sources.size() == 1U &&
              defaultMeta.routes().front().sources.front().dataType == "unsigned" &&
              defaultMeta.routes().front().sources.front().dataSize == 4U)
      << "missing DWT data-type/size must default to unsigned/4";

  const auto defaultOutputDir = root / "default";
  auto defaultOptions = makeCtfBundleConfig(defaultOutputDir, 1000000U);
  defaultOptions.metadata.sources = {resolvedSource(defaultMeta.routes().front().sources.front())};
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
  ASSERT_TRUE(signedMeta.routes().size() == 1U && signedMeta.routes().front().sources.size() == 1U)
      << "signed DWT source missing";
  const auto signedOutputDir = root / "signed";
  auto signedOptions = makeCtfBundleConfig(signedOutputDir, 1000000U);
  signedOptions.metadata.sources = {resolvedSource(signedMeta.routes().front().sources.front())};
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
  ASSERT_TRUE(meta.routes().size() == 2U && meta.routes().front().sources.size() == 1U &&
              meta.routes().front().sources.front().route.traceBusId == 1U &&
              meta.routes().front().sources.front().label == std::optional<std::string>("core-one"))
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
  const auto rawPath = temporaryOutput.outputDirectory().parent_path() / "output.SWO.raw";
  TraceRunConfig traceRun;
  traceRun.setups.push_back(TraceRunTestSupport::makeTimestampSetup(std::nullopt, 1000000U));
  auto channelZero = TraceRunTestSupport::makeReference("itm", std::nullopt, std::nullopt, {0U}, "opaque/channel-zero");
  channelZero.label = "Console";
  traceRun.references.push_back(std::move(channelZero));
  TraceSelection selection{{"itm", "error"}, {}};
  CollectingDiagnosticSink preflightDiagnostics;
  auto outputPlan = planTraceOutputs({false, true, selection}, rawPath,
                                     CtraceRunMeta::fromConfig(traceRun), preflightDiagnostics);
  ASSERT_TRUE(outputPlan.ctf.has_value() && outputPlan.ctf->metadata.sources.empty() &&
              preflightDiagnostics.events().empty())
      << "CTF preflight must exclude software channel zero metadata";
  const auto outputDir = outputPlan.ctf->outputDirectory;
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

TEST(CtraceUnitTests, testCtfBundleOutputAbortRemovesOnlyItsPartialBundle)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-direct-abort-test");
  const auto& outputDir = temporaryOutput.outputDirectory();
  const auto xmlPath = outputDir.parent_path() / "target.traceanalysis.xml";
  writeTestFile(outputDir / "old-marker", "old");
  writeTestFile(xmlPath, "independent-xml");

  CtfBundleOutput output(makeCtfBundleConfig(outputDir, 1000000U));
  EXPECT_EQ(output.completedMetadata(), nullptr);
  output.start();
  EXPECT_FALSE(std::filesystem::exists(outputDir / "old-marker"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDir / "stream_0"));
  EXPECT_EQ(output.completedMetadata(), nullptr);
  output.writeEvent(softwarePacket(1U, 1U, 'A'));
  output.abort();
  EXPECT_FALSE(std::filesystem::exists(outputDir));
  EXPECT_EQ(output.completedMetadata(), nullptr);
  EXPECT_EQ(readTestTextFile(xmlPath), "independent-xml");
}

TEST(CtraceUnitTests, testCtfBundleOutputObservesNoGraphicalTopicsForSoftwareOnly)
{
  const TemporaryTestPath root("ctrace-ctf-legacy-xml-completion-test");
  const auto outputDirectory = root.path() / "output.ctf";

  CtfBundleOutput output(makeCtfBundleConfig(outputDirectory, 1000000U));
  output.start();
  output.writeEvent(atCycle(softwarePacket(1U, 1U, 'A'), 10U));
  output.stop();

  ASSERT_NE(output.completedMetadata(), nullptr);
  for (const auto topic : kCtfGraphicalTopics) {
    EXPECT_FALSE(output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{0U}, topic));
  }
  const auto records = readCtfRecords(outputDirectory / "stream_0");
  ASSERT_FALSE(records.empty());
  EXPECT_FALSE(records.front().routeLabelId.has_value());
  EXPECT_EQ(readTestTextFile(outputDirectory / "metadata").find("ctrace_route"), std::string::npos);
}

TEST(CtraceUnitTests, testCtfBundleOutputObservesProcessorStateOnlyForSleep)
{
  const TemporaryTestPath root("ctrace-ctf-processor-state-view-test");
  for (const auto kind : {PcSampleKind::Pc, PcSampleKind::TraceProhibited, PcSampleKind::Sleep}) {
    const auto directory = root.path() / std::to_string(static_cast<int>(kind));
    CtfBundleOutput output(makeCtfBundleConfig(directory, 1000000U));
    output.start();
    output.writeEvent(atCycle(TraceEvent{PcSampleTraceEvent{0x08001234U, kind}}, 10U));
    output.writeEvent(atCycle(TraceEvent{PcSampleTraceEvent{0U, PcSampleKind::TraceProhibited}}, 11U));
    output.stop();
    ASSERT_NE(output.completedMetadata(), nullptr);
    EXPECT_EQ(output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{0U}, CtfGraphicalTopic::ProcessorState),
              kind == PcSampleKind::Sleep);
    EXPECT_FALSE(output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{0U}, CtfGraphicalTopic::Exception));
  }
}

TEST(CtraceUnitTests, testCtfBundleOutputObservesDwtAddressOnlyForDataAddresses)
{
  const TemporaryTestPath root("ctrace-ctf-dwt-address-view-test");
  const auto observesAddress = [&](const std::string& name, const DwtAddressTraceLocation& location) {
    CtfBundleOutput output(makeCtfBundleConfig(root.path() / name, 1000000U));
    output.start();
    output.writeEvent(atCycle(TraceEvent{DwtAddressTraceEvent{0U, location}}, 10U));
    output.stop();
    return output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{0U}, CtfGraphicalTopic::DwtAddress);
  };
  EXPECT_FALSE(observesAddress("pc-only.ctf", DwtPcTraceLocation{{4U, 0x08001234U}}));
  EXPECT_TRUE(observesAddress("data-only.ctf", DwtDataAddressTraceLocation{{4U, 0x20000000U}}));
  EXPECT_TRUE(observesAddress("pc-and-data.ctf", DwtPcAndDataAddressTraceLocation{{4U, 0x08001234U}, {4U, 0x20000000U}}));
}

TEST(CtraceUnitTests, testCtfBundleOutputObservesNoViewsForMarkerOnlyLegacyAndRoutedTraces)
{
  const TemporaryTestPath root("ctrace-ctf-marker-only-no-xml-test");
  for (const auto routed : {false, true}) {
    const auto outputDirectory = root.path() / (routed ? "routed.ctf" : "legacy.ctf");
    const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
    const TraceRouteIdentity second{TraceRouteId{20U}, 111U};
    auto config = makeCtfBundleConfig(outputDirectory, 1000000U);
    if (routed) {
      config = makeFormattedCtfBundleConfig(outputDirectory, CtfMetadataTopology{
          {{CtfClockDomainId{7U}, "shared_clock", CtfTestSupport::testUuid(7U), 1000000U, false}},
          {
              {CtfStreamClassId{1U}, first, "core-one", CtfClockDomainId{7U}},
              {CtfStreamClassId{111U}, second, "core-two", CtfClockDomainId{7U}},
          },
          {},
      });
    }
    CollectingDiagnosticSink diagnostics;
    CtfBundleOutput output(std::move(config), &diagnostics);
    output.start();
    const auto marker = atCycle(TraceEvent{PcSampleTraceEvent{0U, PcSampleKind::TraceProhibited}}, 15U);
    output.writeEvent(routed ? onRoute(marker, first) : marker);
    if (routed) {
      output.writeEvent(onRoute(marker, second));
    }
    output.stop();

    EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata"));
    ASSERT_NE(output.completedMetadata(), nullptr);
    for (const auto& stream : output.completedMetadata()->topology().streams) {
      for (const auto topic : kCtfGraphicalTopics) {
        EXPECT_FALSE(output.completedMetadata()->observedGraphicalTopic(stream.streamClassId, topic));
      }
    }
    EXPECT_TRUE(diagnostics.events().empty()) << "missing graphical events must not produce a warning";
    const auto contextLayout = routed ? CtfStreamWriter::EventContextLayout::RouteLabeled
                                      : CtfStreamWriter::EventContextLayout::Legacy;
    const auto streamNames = routed ? std::vector<std::string>{"stream_1", "stream_111"}
                                    : std::vector<std::string>{"stream_0"};
    for (const auto& name : streamNames) {
      const auto records = readCtfRecords(outputDirectory / name, contextLayout);
      EXPECT_EQ(std::count_if(records.begin(), records.end(), [](const auto& record) {
                  return record.id == CtfSchema::value(CtfSchema::EventId::PcSampleProhibited);
                }),
                1);
    }
  }
}

TEST(CtraceUnitTests, testCtfBundleOutputPublishesRouteViewsForSharedClockStreams)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-shared-clock-xml-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity last{TraceRouteId{20U}, 111U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{7U}, "shared_clock", CtfTestSupport::testUuid(7U), 1000000U, false}},
      {
          {CtfStreamClassId{1U}, first, "core-one", CtfClockDomainId{7U}},
          {CtfStreamClassId{111U}, last, std::nullopt, CtfClockDomainId{7U}},
      },
      {},
  };
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)), &diagnostics);

  output.start();
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_111"));
  output.writeEvent(
      onRoute(atCycle(TraceEvent{DwtDataTraceEvent{0U, 4U, 0x11U, AccessType::Write}}, 10U), last));
  output.writeEvent(
      onRoute(atCycle(TraceEvent{DwtDataTraceEvent{0U, 4U, 0x22U, AccessType::Write}}, 20U), first));
  output.stop();

  ASSERT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  ASSERT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_111"));
  const auto firstRecords =
      readCtfRecords(outputDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled);
  const auto lastRecords =
      readCtfRecords(outputDirectory / "stream_111", CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_FALSE(firstRecords.empty());
  ASSERT_FALSE(lastRecords.empty());
  EXPECT_EQ(firstRecords.front().traceBusId, 1U);
  EXPECT_EQ(firstRecords.front().routeLabelId, std::optional<std::uint8_t>{1U});
  EXPECT_EQ(lastRecords.front().traceBusId, 111U);
  EXPECT_EQ(lastRecords.front().routeLabelId, std::optional<std::uint8_t>{111U});
  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("cmsis_stream_1_processor_name = \"core-one\";"), std::string::npos);
  EXPECT_NE(metadata.find("\"core-one\" = 1,\n} := cmsis_stream_1_route_t;"), std::string::npos);
  EXPECT_NE(metadata.find("\"111\" = 111,\n} := cmsis_stream_111_route_t;"), std::string::npos);
  EXPECT_EQ(metadata.find("cmsis_stream_111_processor_name"), std::string::npos);
  ASSERT_NE(output.completedMetadata(), nullptr);
  EXPECT_TRUE(output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtValue));
  EXPECT_TRUE(output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{111U}, CtfGraphicalTopic::DwtValue));
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testCtfBundleOutputPublishesIndependentClockMetadata)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-independent-clock-xml-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity second{TraceRouteId{20U}, 2U};
  CtfMetadataTopology topology{
      {
          {CtfClockDomainId{1U}, "first_clock", CtfTestSupport::testUuid(1U), 1000000U, false},
          {CtfClockDomainId{2U}, "second_clock", CtfTestSupport::testUuid(2U), 1000000U, false},
      },
      {
          {CtfStreamClassId{1U}, first, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{2U}, second, "core-two", CtfClockDomainId{2U}},
      },
      {},
  };
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)), &diagnostics);

  output.start();
  output.writeEvent(onRoute(softwarePacket(1U, 1U, 'A'), first));
  output.writeEvent(onRoute(softwarePacket(2U, 1U, 'B'), second));
  output.stop();
  output.stop();

  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_2"));
  EXPECT_TRUE(diagnostics.events().empty());
  ASSERT_NE(output.completedMetadata(), nullptr);
  EXPECT_EQ(output.completedMetadata()->topology().clockDomains.size(), 2U);
}

TEST(CtraceUnitTests, testCtfBundleOutputCompletesIndependentClocksWithoutDiagnosticSink)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-independent-clock-no-diagnostics-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity second{TraceRouteId{20U}, 2U};
  CtfMetadataTopology topology{
      {
          {CtfClockDomainId{1U}, "first_clock", CtfTestSupport::testUuid(1U), 1000000U, false},
          {CtfClockDomainId{2U}, "second_clock", CtfTestSupport::testUuid(2U), 1000000U, false},
      },
      {
          {CtfStreamClassId{1U}, first, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{2U}, second, "core-two", CtfClockDomainId{2U}},
      },
      {},
  };
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)));

  output.start();
  output.writeEvent(onRoute(softwarePacket(1U, 1U, 'A'), first));
  output.writeEvent(onRoute(softwarePacket(2U, 1U, 'B'), second));
  EXPECT_NO_THROW(output.stop());

  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_2"));
}

TEST(CtraceUnitTests, testCtfBundleOutputPublishesOnlyEmittedStreamsAndViews)
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
          {CtfStreamClassId{1U}, first, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{111U}, last, "core-last", CtfClockDomainId{2U}},
      },
      {},
  };
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)), &diagnostics);

  output.start();
  output.writeEvent(onRoute(TraceEvent{DwtDataTraceEvent{0U, 4U, 0x11U, AccessType::Write}}, first));
  output.stop();

  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("name = emitted_clock;"), std::string::npos);
  EXPECT_EQ(metadata.find("name = silent_clock;"), std::string::npos);
  EXPECT_NE(metadata.find("stream {\n    id = 1;"), std::string::npos);
  EXPECT_EQ(metadata.find("stream {\n    id = 111;"), std::string::npos);
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_111"));
  ASSERT_NE(output.completedMetadata(), nullptr);
  EXPECT_EQ(output.completedMetadata()->topology().streams.size(), 1U);
  EXPECT_TRUE(output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{1U}, CtfGraphicalTopic::DwtValue));
  EXPECT_FALSE(output.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{111U}, CtfGraphicalTopic::DwtValue));
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testCtfBundleOutputWritesMetadataOnlyForEmptyFormattedTrace)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-empty-formatted-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{1U}, "configured_clock", CtfTestSupport::testUuid(1U), 1000000U, false}},
      {{CtfStreamClassId{1U}, route, "core-one", CtfClockDomainId{1U}}},
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
  EXPECT_TRUE(diagnostics.events().empty());
}

TEST(CtraceUnitTests, testCtfBundleOutputCleansUpDirectWriteFailures)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-direct-write-failure-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity last{TraceRouteId{20U}, 111U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{1U}, "formatted_clock", CtfTestSupport::testUuid(1U), 1000000U, false}},
      {
          {CtfStreamClassId{1U}, first, "core-one", CtfClockDomainId{1U}},
          {CtfStreamClassId{111U}, last, "core-last", CtfClockDomainId{1U}},
      },
      {},
  };
  CtfBundleOutput output(makeFormattedCtfBundleConfig(outputDirectory, std::move(topology)));

  output.start();
  output.writeEvent(onRoute(softwarePacket(1U), first));
  ASSERT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  std::filesystem::create_directory(outputDirectory / "stream_111");
  EXPECT_THROW(output.writeEvent(onRoute(softwarePacket(2U), last)), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
  EXPECT_EQ(output.completedMetadata(), nullptr);
}

TEST(CtraceUnitTests, testCtfBundleOutputPublishesMetadataOnlyForEmptyTopology)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-empty-topology-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  CtfBundleOutput output(CtfOutputConfig(outputDirectory, {}, CtfMetadataTopology{}));
  EXPECT_NO_THROW(output.start());
  EXPECT_NO_THROW(output.stop());
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata"));
  ASSERT_NE(output.completedMetadata(), nullptr);
  EXPECT_TRUE(output.completedMetadata()->topology().streams.empty());
}

TEST(CtraceUnitTests, testCtfBundleOutputCleansUpAfterEncoderStartFailure)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-encoder-start-failure-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  const TraceRouteIdentity streamRoute{TraceRouteId{10U}, 1U};
  CtfMetadataTopology topology{
      {{CtfClockDomainId{1U}, "configured_clock", CtfTestSupport::testUuid(1U), 1000000U, false}},
      {{CtfStreamClassId{1U}, streamRoute, "core-one", CtfClockDomainId{1U}}},
      {},
  };
  const std::vector<TraceRouteIdentity> inconsistentRoutes{{TraceRouteId{10U}, 2U}};
  CtfBundleOutput output(CtfOutputConfig(outputDirectory, {}, std::move(topology), inconsistentRoutes));

  EXPECT_THROW(output.start(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
  EXPECT_EQ(output.completedMetadata(), nullptr);
}

TEST(CtraceUnitTests, testCtfBundleOutputReplacesExistingBundleAtStart)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-direct-replace-test");
  const auto& outputDir = temporaryOutput.outputDirectory();
  writeTestFile(outputDir / "old-marker", "old");
  CtfBundleOutput output(makeCtfBundleConfig(outputDir, 1000000U));
  output.start();
  EXPECT_FALSE(std::filesystem::exists(outputDir / "old-marker"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDir / "stream_0"));
  EXPECT_FALSE(std::filesystem::exists(outputDir / "metadata"));
  output.writeEvent(atCycle(TraceEvent{PcSampleTraceEvent{0U, PcSampleKind::Sleep}}, 10U));
  output.stop();
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDir / "metadata"));
  EXPECT_GT(std::filesystem::file_size(outputDir / "stream_0"), 0U);
}

TEST(CtraceUnitTests, testCtfBundleOutputRejectsExistingFileBeforeDeletion)
{
  const TemporaryTestPath root("ctrace-ctf-wrong-type-test");
  const auto ctfDirectory = root.path() / "WrongType.ctf";
  writeTestFile(ctfDirectory, "not-a-directory");
  CtfBundleOutput output(makeCtfBundleConfig(ctfDirectory, 1000000U));
  EXPECT_THROW(output.start(), std::runtime_error);
  EXPECT_EQ(readTestTextFile(ctfDirectory), "not-a-directory");
}

TEST(CtraceUnitTests, testCtfBundleOutputOwnsDirectLifecycle)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-bundle-lifecycle-test");
  const auto& testRoot = temporaryPath.path();
  const auto root = testRoot / std::filesystem::u8path(u8"Gr\u00f6\u00dfe");
  const auto ctfDirectory = root / "Bundle.ctf";

  auto config = makeCtfBundleConfig(ctfDirectory, 1000000U);
  CtfBundleOutput output(std::move(config));
  output.start();
  const auto sleep = atCycle(TraceEvent{PcSampleTraceEvent{0U, PcSampleKind::Sleep}}, 10U);
  output.writeEvent(sleep);
  output.stop();

  EXPECT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "metadata"));
  EXPECT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "stream_0"));
  ASSERT_NE(output.completedMetadata(), nullptr);

  writeTestFile(ctfDirectory / "stale-marker", "stale");
  output.start();
  ASSERT_TRUE(!std::filesystem::exists(ctfDirectory / "stale-marker") &&
              !std::filesystem::exists(ctfDirectory / "metadata"))
      << "restarting CTF output must delete the previous bundle before writing";
  output.writeEvent(sleep);
  output.stop();
  EXPECT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "metadata"));
  EXPECT_TRUE(std::filesystem::is_regular_file(ctfDirectory / "stream_0"));

  std::filesystem::create_directories(testRoot / "working");
  std::filesystem::create_directories(testRoot / "captures");
  const auto relativeRoot = testRoot / "working" / ".." / "captures";
  const auto relativeCtf = relativeRoot / "Relative.ctf";
  CtfBundleOutput relativeOutput(makeCtfBundleConfig(relativeCtf, 1000000U));
  relativeOutput.start();
  relativeOutput.writeEvent(sleep);
  relativeOutput.stop();
  ASSERT_TRUE(std::filesystem::is_regular_file(testRoot / "captures" / "Relative.ctf" / "metadata"))
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
  for (const auto& unsafe : {std::filesystem::path{}, std::filesystem::path("."), std::filesystem::path(".."),
                            std::filesystem::temp_directory_path().root_path()}) {
    EXPECT_THROW((void)CtfBundleOutput(makeCtfBundleConfig(unsafe, 1000000U)), std::invalid_argument);
  }
}

TEST(CtraceUnitTests, testCtfBundleOutputRejectsLongPaths)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-long-path-test");
  CtfBundleOutput output(makeCtfBundleConfig(temporaryPath.path() / std::string(1024U, 'x'), 1000000U));
  EXPECT_THROW(output.start(), std::runtime_error);
  temporaryPath.createDirectory();
  EXPECT_TRUE(throwsWithMessage([&] { output.start(); }, "Failed to inspect existing CTF output"));
}

TEST(CtraceUnitTests, testCtfBundleOutputCleansUpAfterMetadataFailure)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-metadata-failure-test");
  const auto& outputDirectory = temporaryOutput.outputDirectory();
  CtfBundleOutput output(makeCtfBundleConfig(outputDirectory, 1000000U));
  output.start();
  std::filesystem::create_directory(outputDirectory / "metadata");
  EXPECT_THROW(output.stop(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
  EXPECT_EQ(output.completedMetadata(), nullptr);
}

TEST(CtraceUnitTests, testCtfBundleOutputRejectsNonDirectoryParent)
{
  const TemporaryTestPath root("ctrace-ctf-parent-failure-test");
  const auto blockedParent = root.path() / "not-a-directory";
  writeTestFile(blockedParent, "file");
  CtfBundleOutput output(makeCtfBundleConfig(blockedParent / "trace.ctf", 1000000U));
  EXPECT_THROW(output.start(), std::runtime_error);
  EXPECT_EQ(readTestTextFile(blockedParent), "file");
}

TEST(CtraceUnitTests, testCtfBundleOutputReportsPseudoFilesystemCreationFailure)
{
  if (!TestPlatform::supports(TestPlatformCapability::LinuxSpecialFiles)) {
    GTEST_SKIP();
  }
  CtfBundleOutput output(makeCtfBundleConfig(TestPlatform::creationFailurePath("ctrace-output.ctf"), 1000000U));
  EXPECT_THROW(output.start(), std::runtime_error);
}

TEST(CtraceUnitTests, testCtfBundleOutputReportsPermissionFailures)
{
  if (!TestPlatform::supports(TestPlatformCapability::PosixPermissions)) {
    GTEST_SKIP();
  }
  const TemporaryTestPath temporaryPath("ctrace-ctf-permission-failure-test");
  const auto& root = temporaryPath.createDirectory();
  const auto readOnly = std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec;

  const auto destructorCtf = root / "destructor.ctf";
  {
    CtfBundleOutput output(makeCtfBundleConfig(destructorCtf, 1000000U));
    output.start();
    std::filesystem::permissions(root, readOnly);
  }
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);
  EXPECT_TRUE(std::filesystem::exists(destructorCtf));
  std::filesystem::remove_all(destructorCtf);

  const auto existingCtf = root / "existing.ctf";
  writeTestFile(existingCtf / "marker", "existing");
  std::filesystem::permissions(root, readOnly);
  CtfBundleOutput removeFailure(makeCtfBundleConfig(existingCtf, 1000000U));
  EXPECT_THROW(removeFailure.start(), std::runtime_error);
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);

  const auto cleanupCtf = root / "cleanup.ctf";
  CtfBundleOutput cleanup(makeCtfBundleConfig(cleanupCtf, 1000000U));
  cleanup.start();
  std::filesystem::permissions(root, readOnly);
  EXPECT_THROW(cleanup.abort(), std::runtime_error);
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);
  EXPECT_TRUE(std::filesystem::exists(cleanupCtf));
  EXPECT_NO_THROW(cleanup.abort());
  EXPECT_FALSE(std::filesystem::exists(cleanupCtf));

  const auto blockedCtf = root / "new.ctf";
  std::filesystem::permissions(root, readOnly);
  CtfBundleOutput creationFailure(makeCtfBundleConfig(blockedCtf, 1000000U));
  EXPECT_THROW(creationFailure.start(), std::runtime_error);
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);
}
