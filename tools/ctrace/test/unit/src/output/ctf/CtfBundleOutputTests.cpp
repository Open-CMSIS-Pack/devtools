/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

// CTF bundle metadata, event handling, and direct-output lifecycle tests.
#include "CtfTestSupport.h"
#include "TestSupport.h"
#include "TraceRunTestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfBundleOutput.h"
#include "CtraceRunMeta.h"
#include "OutputRequirements.h"
#include "TestPath.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"
#include "TraceRunConfig.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using CtfTestSupport::parseCtfRecords;
using CtfTestSupport::readCtfRecords;
using CtfTestSupport::readLe16;
using CtfTestSupport::readLe64;
using CtfTestSupport::requireFirstCtfRecord;
using CtfTestSupport::requireSingleItmEvent;

static std::filesystem::path testTraceCompassXmlPath(const std::filesystem::path& outputDirectory)
{
  auto basePath = outputDirectory;
  if (basePath.extension() == ".ctf") {
    basePath.replace_extension();
  }
  basePath += ".SWO.traceanalysis.xml";
  return basePath;
}

static CtfOutputConfig makeCtfBundleConfig(const std::filesystem::path& outputDirectory, std::uint64_t coreClockHz)
{
  return CtfOutputConfig(outputDirectory, testTraceCompassXmlPath(outputDirectory), coreClockHz, {}, {});
}

static void requireCompleteCtfBundle(const std::filesystem::path& ctfDirectory, const std::filesystem::path& xmlPath,
                                     const std::string& message)
{
  require(std::filesystem::is_regular_file(ctfDirectory / "metadata") &&
              std::filesystem::is_regular_file(ctfDirectory / "stream_0") && std::filesystem::is_regular_file(xmlPath),
          message);
}

class TemporaryCtfOutput {
public:
  explicit TemporaryCtfOutput(const std::string& name) : root_(name), outputDirectory_(root_.path() / "output.ctf") {}

  const std::filesystem::path& outputDirectory() const
  {
    return outputDirectory_;
  }

private:
  TemporaryTestPath root_;
  std::filesystem::path outputDirectory_;
};

static ResolvedTraceSource resolvedSource(const CtraceRunSourceMeta& source)
{
  return {
      source.type,
      source.source,
      source.traceBusId,
      source.label,
      source.symbolAddress,
      source.valueType,
      static_cast<std::uint8_t>(source.valueSize),
  };
}

static std::vector<std::string> readCtfExceptionRecords(const std::filesystem::path& streamPath)
{
  std::vector<std::string> records;
  for (const auto& record : readCtfRecords(streamPath)) {
    if (record.id == CtfSchema::value(CtfSchema::EventId::Exception)) {
      const auto number = readLe16(record.payload, 0U);
      const auto action = record.payload[2U];
      records.push_back(std::to_string(number) + ":" + std::to_string(static_cast<unsigned>(action)));
    }
  }
  return records;
}

static std::vector<std::uint8_t> readOnlyCtfTraceStatusReasons(const std::filesystem::path& streamPath)
{
  std::vector<std::uint8_t> reasons;
  for (const auto& record : readCtfRecords(streamPath)) {
    require(record.id == CtfSchema::value(CtfSchema::EventId::TraceStatus), "expected only CTF trace-status events");
    reasons.push_back(record.payload.front());
  }
  return reasons;
}

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
  output.stop();

  const auto records = readCtfExceptionRecords(outputDir / "stream_0");
  const auto metadata = readTestTextFile(outputDir / "metadata");

  require(metadata.find("\"entered\" = 1") != std::string::npos, "CTF exception entered label mismatch");
  require(metadata.find("\"exited\" = 2") != std::string::npos, "CTF exception exited label mismatch");

  require(records == std::vector<std::string>({
                         "0:1",
                         "0:2",
                         "15:1",
                         "15:2",
                         "54:1",
                         "54:2",
                         "15:1",
                         "15:2",
                         "0:1",
                     }),
          "CtfBundleOutput exception active-context records mismatch");
}

TEST(CtraceUnitTests, testCtfBundleOutputUsesCtraceRunMeta)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-ctrace-run-meta-test");
  const auto& outputDir = temporaryOutput.outputDirectory();

  TraceRunConfig traceRun;
  traceRun.path = "Board.ctrace-run.yml";
  auto signedByteReference = TraceRunTestSupport::makeReference("dwt", std::nullopt, 7U, {0U}, "opaque/signed-byte");
  signedByteReference.dataSetupIndex = 0U;
  signedByteReference.label = "Sine";
  traceRun.references.push_back(signedByteReference);

  auto reference = TraceRunTestSupport::makeReference("dwt", std::nullopt, std::nullopt, {2U}, "opaque/current");
  reference.dataSetupIndex = 2U;
  reference.label = "Current\n\t\"\\\x01";
  reference.symbolAddress = 0x24000e88U;
  traceRun.references.push_back(reference);
  TraceRunSetup setup;
  setup.data.resize(3U);
  setup.data[0].symbolType = "signed int";
  setup.data[0].symbolSize = 1U;
  setup.data[2].symbolType = "signed int";
  setup.data[2].symbolSize = 4U;
  setup.timestamps = TraceRunTimestampSetup{280000000U, 1U};
  traceRun.setups.push_back(std::move(setup));

  CollectingDiagnosticSink preflightDiagnostics;
  auto outputPlan = planTraceOutputs({false, true, {}}, outputDir.parent_path() / "output.SWO.raw",
                                     CtraceRunMeta::fromConfig(traceRun), preflightDiagnostics);
  require(outputPlan.ctf.has_value() && preflightDiagnostics.events().empty(), "resolved CTF source missing");
  auto options = std::move(*outputPlan.ctf);
  require(!options.sources.empty(), "resolved CTF source missing");
  require(options.sources.front().traceBusId == 7U, "resolved CTF source must retain its Trace Bus ID");
  require(options.sources.front().valueType == "signed int", "resolved CTF source must retain its value type");
  CtfBundleOutput output(std::move(options));
  output.start();
  output.writeEvent(atCycle(onStream(TraceEvent{DwtDataTraceEvent{0U, 1U, 0xffU, AccessType::Write}}, 7U), 100U));
  output.stop();

  const auto metadata = readTestTextFile(outputDir / "metadata");
  const auto stream = readTestBinaryFile(outputDir / "stream_0");

  require(metadata.find("freq = 280000000") != std::string::npos, "CTF trace-run clock mismatch");
  require(metadata.find("cmsis_dwt0_value_type = \"signed int\"") != std::string::npos,
          "CTF signed-byte source type mismatch");
  require(metadata.find("cmsis_dwt2_value_type = \"signed int\"") != std::string::npos, "CTF trace-run type mismatch");
  require(metadata.find("cmsis_dwt2_address_start = \"0x24000E88\"") != std::string::npos,
          "CTF trace-run start address mismatch");
  require(metadata.find("cmsis_dwt2_address_end = \"0x24000E8B\"") != std::string::npos,
          "CTF trace-run end address mismatch");
  require(metadata.find("\"Current\\n\\t\\\"\\\\\\x01\" = 2") != std::string::npos,
          "CTF trace-run label escaping mismatch");

  const auto records = parseCtfRecords(stream);
  require(records.front().id == CtfSchema::value(CtfSchema::EventId::TraceStatus), "CTF trace-start event missing");
  const auto& dwtRecord =
      requireFirstCtfRecord(records, CtfSchema::EventId::DwtValue, "expected CTF DWT value event missing");
  require(dwtRecord.traceBusId == 7U, "CTF event context must preserve the CoreSight Trace Bus ID");
  require(dwtRecord.payload[2U] == 1U, "CTF one-byte int payload must select the i8 variant");
  require(dwtRecord.payload[3U] == 0xffU, "CTF signed-byte payload mismatch");
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
  require(defaultMeta.sources().size() == 1U && defaultMeta.sources().front().valueType == "unsigned int" &&
              defaultMeta.sources().front().valueSize == 4U,
          "missing DWT data.symbol-type/data.symbol-size must default to unsigned int/4");

  const auto defaultOutputDir = root / "default";
  auto defaultOptions = makeCtfBundleConfig(defaultOutputDir, 1000000U);
  defaultOptions.sources = {resolvedSource(defaultMeta.sources().front())};
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput defaultOutput(std::move(defaultOptions), &diagnostics);
  defaultOutput.start();
  defaultOutput.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 1U, 0xffU, AccessType::Write}});
  defaultOutput.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 2U, 0xffffU, AccessType::Write}});
  defaultOutput.stop();
  require(readFirstCtfDwtValueTag(defaultOutputDir / "stream_0") == 6U,
          "default DWT metadata must select the unsigned 32-bit CTF variant");
  const auto& sizeWarning = diagnostics.singleEvent("dwt-symbol-size-mismatch");
  require(sizeWarning.severity == DiagnosticSink::Severity::Warning && diagnostics.failureCount() == 0U,
          "DWT size mismatch must be reported once per channel");

  TraceRunConfig signedTraceRun;
  TraceRunSetup signedSetup;
  signedSetup.data.push_back(TraceRunDataSetup{"signed int", 1U});
  signedTraceRun.setups.push_back(std::move(signedSetup));
  signedTraceRun.references.push_back(defaultReference);
  const auto signedMeta = CtraceRunMeta::fromConfig(signedTraceRun);
  require(signedMeta.sources().size() == 1U, "signed DWT source missing");
  const auto signedOutputDir = root / "signed";
  auto signedOptions = makeCtfBundleConfig(signedOutputDir, 1000000U);
  signedOptions.sources = {resolvedSource(signedMeta.sources().front())};
  CollectingDiagnosticSink signedDiagnostics;
  CtfBundleOutput signedOutput(std::move(signedOptions), &signedDiagnostics);
  signedOutput.start();
  signedOutput.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 1U, 0xffU, AccessType::Write}});
  signedOutput.stop();
  require(readFirstCtfDwtValueTag(signedOutputDir / "stream_0") == 1U,
          "explicit signed int/1 metadata must select the signed 8-bit CTF variant");
  require(signedDiagnostics.events().empty(), "matching DWT sizes must not produce a warning");

  TraceRunConfig traceRun;
  traceRun.path = "ambiguous-streams.ctrace-run.yml";
  TraceRunSetup setup;
  setup.data.push_back(TraceRunDataSetup{"signed int", 4U});
  traceRun.setups.push_back(std::move(setup));
  auto first = TraceRunTestSupport::makeReference("dwt", std::nullopt, 1U, {0U}, "opaque/dwt-route");
  first.dataSetupIndex = 0U;
  first.label = "core-one";
  TraceRunReference second = first;
  second.stream = 2U;
  second.label = "core-two";
  traceRun.references = {first, second};
  const auto meta = CtraceRunMeta::fromConfig(traceRun);
  require(meta.sources().size() == 2U && meta.sources().front().traceBusId == 1U &&
              meta.sources().front().label == std::optional<std::string>("core-one"),
          "trace-run metadata must preserve the exact DWT stream route");
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
  TraceEvent warning = issuePacket("opencsd-warning", "decoder warning", TraceIssueSeverity::Warning);
  filtered.writeEvent(warning);
  filtered.stop();
  require(readOnlyCtfTraceStatusReasons(filteredDir / "stream_0") == std::vector<std::uint8_t>({4U}),
          "--type error must retain decoder warnings in CTF");

  CtfBundleOutput context(makeCtfBundleConfig(contextDir, 1000000U));
  context.start();
  context.writeEvent(exceptionPacket(15U, ExceptionAction::Entered, 10U));
  context.writeEvent(warning);
  context.writeEvent(exceptionPacket(54U, ExceptionAction::Entered, 20U));
  context.stop();
  require(readCtfExceptionRecords(contextDir / "stream_0") ==
              std::vector<std::string>({"0:1", "0:2", "15:1", "15:2", "54:1"}),
          "a decoder warning must not reset the active CTF exception context");

  auto dataLossOptions = makeCtfBundleConfig(dataLossDir, 1000000U);
  dataLossOptions.selection.types.push_back("exception");
  CtfBundleOutput dataLoss(std::move(dataLossOptions));
  dataLoss.start();
  dataLoss.writeEvent(exceptionPacket(15U, ExceptionAction::Entered, 10U));
  dataLoss.writeEvent(issuePacket("data-loss", "decoder data loss"));
  dataLoss.writeEvent(exceptionPacket(15U, ExceptionAction::Returned, 20U));
  dataLoss.stop();
  require(readCtfExceptionRecords(dataLossDir / "stream_0") ==
              std::vector<std::string>({"0:1", "0:2", "15:1", "15:2", "15:1"}),
          "filtered data-loss must still reset the CTF exception context");
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
  auto error = atCycle(issuePacket("opencsd-bad-packet-sequence"), 400U);
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
  require(records.size() == 1U && records.front().id == CtfSchema::value(CtfSchema::EventId::Itm),
          "CTF ITM event missing after global timestamp");
  require(records.front().payload[3U] == (timestampReliable | beforeFirstLocalTimestamp),
          "global timestamp must not mark following samples as locally timestamped");
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
  require(records.size() == 2U && records[0].timestamp == 100U, "first CTF event timestamp mismatch");
  require(records[1].timestamp == 100U, "CTF event timestamps must not regress");
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

  require(records.size() == 1U, "CTF global timestamp filter emitted unrelated events");
  const auto& record = records.front();
  require(record.id == CtfSchema::value(CtfSchema::EventId::GlobalTimestamp), "CTF global timestamp event ID mismatch");
  require(readLe64(record.payload, 0U) == timestampValue, "CTF global timestamp value mismatch");
  require(record.payload[8U] == 1U, "CTF global timestamp clock-change flag mismatch");
  require(metadata.find("name = \"GLOBAL_TIMESTAMP\"") != std::string::npos, "CTF global timestamp metadata missing");
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
  require(outputPlan.ctf.has_value() && outputPlan.ctf->sources.empty() && preflightDiagnostics.events().empty(),
          "CTF preflight must exclude software channel zero metadata");
  auto options = std::move(*outputPlan.ctf);
  CtfBundleOutput output(std::move(options));
  output.start();
  output.writeEvent(atCycle(softwarePacket(0U, 1U, 'A'), 100U));
  output.writeEvent(atCycle(issuePacket("opencsd-incomplete-tail"), 101U));
  output.stop();
  require(readOnlyCtfTraceStatusReasons(outputDir / "stream_0") == std::vector<std::uint8_t>({4U}),
          "CTF must exclude software channel zero payload but retain its decoder errors");
  require(!std::filesystem::exists(outputDir / ("stream_" + std::to_string(1))), "CTF must not create a second stream");

  const auto metadata = readTestTextFile(outputDir / "metadata");
  require(metadata.find("ITM" + std::to_string(0)) == std::string::npos,
          "CTF metadata must not register software channel zero");
  require(metadata.find("cmsis_itm0_") == std::string::npos && metadata.find("Console") == std::string::npos,
          "CTF analysis metadata must not expose configured software channel zero");
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
  require(!std::filesystem::exists(outputDir / "old-marker") &&
              std::filesystem::is_regular_file(outputDir / "stream_0") && readTestTextFile(xmlPath) != "old-xml",
          "CTF start must remove both previous targets and write directly to their final paths");

  TraceEvent software = softwarePacket(1U, 1U, 'A');
  output.writeEvent(software);
  output.abort();
  require(!std::filesystem::exists(outputDir) && !std::filesystem::exists(xmlPath),
          "CTF abort must remove the incomplete direct output");
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
  const auto xml = readTestTextFile(xmlPath);
  require(!std::filesystem::exists(outputDir / "old-marker") &&
              std::filesystem::is_regular_file(outputDir / "stream_0") &&
              !std::filesystem::exists(outputDir / "metadata") && xml != "old-xml" && !xml.empty(),
          "CTF start must replace existing output before decoding begins");

  output.writeEvent(atCycle(softwarePacket(1U, 1U, 'A'), 10U));
  output.stop();
  require(std::filesystem::is_regular_file(outputDir / "metadata") &&
              std::filesystem::file_size(outputDir / "stream_0") > 0U && std::filesystem::is_regular_file(xmlPath),
          "CTF stop must complete the directly written bundle");
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
      [&] { CtfBundleOutput output(CtfOutputConfig(ctfDirectory, nestedXml, 1000000U, {}, {})); });
  require(rejected && readTestTextFile(ctfDirectory / "old-marker") == "old-ctf" &&
              readTestTextFile(nestedXml) == "old-xml",
          "overlapping CTF targets must be rejected before either existing target is deleted");

  const auto wrongTypeCtf = root / "WrongType.ctf";
  const auto wrongTypeXml = root / "WrongType.SWO.traceanalysis.xml";
  writeTestFile(wrongTypeCtf, "not-a-directory");
  std::filesystem::create_directory(wrongTypeXml);
  const auto rejectedWrongTypes = throwsException([&] {
    CtfBundleOutput output(CtfOutputConfig(wrongTypeCtf, wrongTypeXml, 1000000U, {}, {}));
    output.start();
  });
  require(rejectedWrongTypes && readTestTextFile(wrongTypeCtf) == "not-a-directory" &&
              std::filesystem::is_directory(wrongTypeXml),
          "CTF start must reject unexpected target types before deleting either target");

#ifdef _WIN32
  const auto rejectedCaseInsensitiveOverlap = throwsException<std::invalid_argument>([&] {
    CtfBundleOutput output(
        CtfOutputConfig(root / "Bundle.ctf", root / "BUNDLE.CTF" / "Bundle.SWO.traceanalysis.xml", 1000000U, {}, {}));
  });
  require(rejectedCaseInsensitiveOverlap, "Windows CTF target overlap checks must be case-insensitive");
#endif
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
  require(!readTestTextFile(xmlPath).empty(), "CTF bundle did not write the Trace Compass XML");

  writeTestFile(ctfDirectory / "stale-marker", "stale");
  writeTestFile(xmlPath, "stale-xml");
  output.start();
  require(!std::filesystem::exists(ctfDirectory / "stale-marker") &&
              !std::filesystem::exists(ctfDirectory / "metadata") && readTestTextFile(xmlPath) != "stale-xml",
          "restarting CTF output must delete the previous bundle before writing");
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
  require(std::filesystem::is_regular_file(testRoot / "captures" / "Relative.ctf" / "metadata") &&
              std::filesystem::is_regular_file(testRoot / "captures" / "Relative.SWO.traceanalysis.xml"),
          "CTF output must accept a legitimate parent-relative trace path");
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
    EXPECT_THROW((void)CtfBundleOutput(CtfOutputConfig(unsafe, safeXml, 1000000U, {}, {})), std::invalid_argument);
    EXPECT_THROW((void)CtfBundleOutput(CtfOutputConfig(safeCtf, unsafe, 1000000U, {}, {})), std::invalid_argument);
  }
}

TEST(CtraceUnitTests, testCtfBundleOutputRejectsInvalidExistingXmlAndLongPaths)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-invalid-existing-output-test");
  const auto ctfDirectory = temporaryPath.path() / "output.ctf";
  const auto xmlDirectory = temporaryPath.path() / "output.xml";
  std::filesystem::create_directories(xmlDirectory);
  CtfBundleOutput directoryXml(CtfOutputConfig(ctfDirectory, xmlDirectory, 1000000U, {}, {}));
  EXPECT_THROW(directoryXml.start(), std::runtime_error);

  const auto longName = std::string(1024U, 'x');
  CtfBundleOutput longCtf(
      CtfOutputConfig(temporaryPath.path() / longName, temporaryPath.path() / "long-ctf.xml", 1000000U, {}, {}));
  EXPECT_THROW(longCtf.start(), std::runtime_error);
  CtfBundleOutput longXml(
      CtfOutputConfig(temporaryPath.path() / "long-xml.ctf", temporaryPath.path() / longName, 1000000U, {}, {}));
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

  CtfBundleOutput output(CtfOutputConfig(outputDirectory, xmlPath, 1000000U, {}, {}));
  EXPECT_THROW(output.start(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
}
