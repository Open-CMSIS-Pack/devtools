/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

// CTF bundle metadata, event handling, and direct-output lifecycle tests.
#include "CtfTestSupport.hpp"
#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "ctf/CtfBundleOutput.hpp"
#include "CtraceRunMeta.hpp"
#include "TestPath.hpp"
#include "TraceEvent.hpp"
#include "TraceOutputConfig.hpp"
#include "TraceRunConfig.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using namespace CtfTestSupport;

constexpr std::size_t kCtfStatusOrContextPayloadSize = 5U;
constexpr std::size_t kCtfStatusOrContextEventSize = kCtfEventHeaderSize + kCtfStatusOrContextPayloadSize;
constexpr std::uint32_t kCtfDwtValueEventId = 4U;
constexpr std::uint32_t kCtfTraceStatusEventId = 5U;
constexpr std::uint32_t kCtfExceptionEventId = 7U;

std::filesystem::path testTraceCompassXmlPath(const std::filesystem::path& outputDirectory)
{
  auto basePath = outputDirectory;
  if (basePath.extension() == ".ctf") {
    basePath.replace_extension();
  }
  basePath += ".SWO.traceanalysis.xml";
  return basePath;
}

CtfOutputConfig makeCtfBundleConfig(const std::filesystem::path& outputDirectory, std::uint64_t coreClockHz)
{
  return CtfOutputConfig(outputDirectory, testTraceCompassXmlPath(outputDirectory), coreClockHz, {}, {});
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

std::vector<ResolvedTraceSource> resolvedSources(const CtraceRunMeta& meta)
{
  std::set<std::pair<std::string, std::uint32_t>> keys;
  std::vector<ResolvedTraceSource> sources;
  for (const auto& route : meta.sources()) {
    if ((route.type != "itm" && route.type != "dwt") || (route.type == "itm" && route.source == 0U) ||
        !keys.emplace(route.type, route.source).second) {
      continue;
    }
    const auto* source = meta.resolveSource(route.type, std::nullopt, route.source);
    require(source != nullptr, "test trace route resolution failed");
    sources.push_back({
        source->type,
        source->source,
        source->traceBusId,
        source->label,
        source->symbolAddress,
        source->valueType,
        static_cast<std::uint8_t>(source->valueSize),
    });
  }
  return sources;
}

void requireSingleCtfItmEvent(const std::filesystem::path& streamPath, std::uint8_t expectedChannel,
                              const std::string& message)
{
  const auto bytes = readTestBinaryFile(streamPath);

  constexpr std::size_t itmPayloadSize = 8U;
  constexpr auto contentSize = kCtfEventOffset + kCtfEventHeaderSize + itmPayloadSize;
  require(bytes.size() >= contentSize, message);
  require(readLe32(bytes, kCtfPacketHeaderSize + 4U) == contentSize * 8U, message);
  require(readLe32(bytes, kCtfEventOffset) == 0U, message);
  require(bytes[kCtfEventOffset + kCtfEventHeaderSize] == expectedChannel, message);
}

std::vector<std::string> readCtfExceptionRecords(const std::filesystem::path& streamPath)
{
  const auto bytes = readTestBinaryFile(streamPath);

  constexpr std::size_t exceptionPayloadSize = 5U;

  std::vector<std::string> records;
  for (std::size_t packetStart = 0; packetStart + kCtfEventOffset <= bytes.size(); packetStart += kCtfPacketSize) {
    const auto contentBits = readLe32(bytes, packetStart + kCtfPacketHeaderSize + 4U);
    const auto contentEnd = packetStart + static_cast<std::size_t>(contentBits / 8U);
    std::size_t offset = packetStart + kCtfEventOffset;
    while (offset + kCtfEventHeaderSize <= contentEnd) {
      const auto id = readLe32(bytes, offset);
      if (id == kCtfTraceStatusEventId) {
        offset += kCtfStatusOrContextEventSize;
        continue;
      }
      if (id == kCtfExceptionEventId) {
        const auto number = readLe16(bytes, offset + kCtfEventHeaderSize);
        const auto action = bytes[offset + kCtfEventHeaderSize + 2U];
        records.push_back(std::to_string(number) + ":" + std::to_string(static_cast<unsigned>(action)));
        offset += kCtfEventHeaderSize + exceptionPayloadSize;
        continue;
      }
      break;
    }
  }
  return records;
}

std::vector<std::uint8_t> readOnlyCtfTraceStatusReasons(const std::filesystem::path& streamPath)
{
  const auto bytes = readTestBinaryFile(streamPath);

  std::vector<std::uint8_t> reasons;
  for (std::size_t packetStart = 0; packetStart + kCtfEventOffset <= bytes.size(); packetStart += kCtfPacketSize) {
    const auto contentBits = readLe32(bytes, packetStart + kCtfPacketHeaderSize + 4U);
    const auto contentEnd = packetStart + static_cast<std::size_t>(contentBits / 8U);
    std::size_t offset = packetStart + kCtfEventOffset;
    while (offset + kCtfStatusOrContextEventSize <= contentEnd) {
      require(readLe32(bytes, offset) == kCtfTraceStatusEventId, "expected only CTF trace-status events");
      reasons.push_back(bytes[offset + kCtfEventHeaderSize]);
      offset += kCtfStatusOrContextEventSize;
    }
    require(offset == contentEnd, "malformed CTF trace-status content size");
  }
  return reasons;
}

std::uint8_t readFirstCtfDwtValueTag(const std::filesystem::path& streamPath)
{
  const auto bytes = readTestBinaryFile(streamPath);

  const auto contentEnd = static_cast<std::size_t>(readLe32(bytes, kCtfPacketHeaderSize + 4U) / 8U);
  std::size_t offset = kCtfEventOffset;
  while (offset + kCtfEventHeaderSize <= contentEnd) {
    const auto eventId = readLe32(bytes, offset);
    if (eventId == kCtfDwtValueEventId) {
      return bytes[offset + kCtfEventHeaderSize + 2U];
    }
    require(eventId == kCtfTraceStatusEventId || eventId == kCtfExceptionEventId,
            "unexpected CTF event before DWT value");
    offset += kCtfStatusOrContextEventSize;
  }
  throw std::runtime_error("CTF DWT value event missing");
}

} // namespace

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
  TraceRunReference signedByteReference;
  signedByteReference.ctraceRef = "opaque/signed-byte";
  signedByteReference.type = "dwt";
  signedByteReference.sources = {0U};
  signedByteReference.dataSetupIndex = 0U;
  signedByteReference.label = "Sine";
  signedByteReference.stream = 7U;
  traceRun.references.push_back(signedByteReference);

  TraceRunReference reference;
  reference.ctraceRef = "opaque/current";
  reference.type = "dwt";
  reference.sources = {2U};
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
  traceRun.setups.push_back(std::move(setup));

  auto options = makeCtfBundleConfig(outputDir, 280000000U);
  options.sources = resolvedSources(CtraceRunMeta::fromConfig(traceRun));
  require(!options.sources.empty(), "resolved CTF source missing");
  require(options.sources.front().traceBusId == 7U, "resolved CTF source must retain its Trace Bus ID");
  require(options.sources.front().valueType == "signed int", "resolved CTF source must retain its value type");
  CtfBundleOutput output(std::move(options));
  output.start();
  TraceEvent signedByte{DwtDataTraceEvent{
      0U,
      1U,
      0xffU,
      AccessType::Write,
  }};
  signedByte.tcyc = 100U;
  signedByte.traceBusId = 7U;
  output.writeEvent(signedByte);
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

  auto dwtEventOffset = kCtfEventOffset;
  require(readLe32(stream, kCtfEventOffset) == kCtfTraceStatusEventId, "CTF trace-start event missing");
  while (dwtEventOffset + kCtfEventHeaderSize <= stream.size() &&
         readLe32(stream, dwtEventOffset) != kCtfDwtValueEventId) {
    const auto eventId = readLe32(stream, dwtEventOffset);
    require(eventId == kCtfTraceStatusEventId || eventId == kCtfExceptionEventId,
            "unexpected CTF event before DWT value");
    dwtEventOffset += kCtfStatusOrContextEventSize;
  }
  require(readLe32(stream, dwtEventOffset) == kCtfDwtValueEventId, "CTF DWT value event missing");
  require(stream[dwtEventOffset + 12U] == 7U, "CTF event context must preserve the CoreSight Trace Bus ID");
  require(stream[dwtEventOffset + kCtfEventHeaderSize + 2U] == 1U,
          "CTF one-byte int payload must select the i8 variant");
  require(stream[dwtEventOffset + kCtfEventHeaderSize + 3U] == 0xffU, "CTF signed-byte payload mismatch");
}

TEST(CtraceUnitTests, testCtfBundleOutputDefaultsDwtValueType)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-default-dwt-type-test");
  const auto& root = temporaryPath.path();

  TraceRunConfig defaultTraceRun;
  TraceRunReference defaultReference;
  defaultReference.ctraceRef = "opaque/default-dwt";
  defaultReference.type = "dwt";
  defaultReference.sources = {0U};
  defaultReference.dataSetupIndex = 0U;
  defaultTraceRun.references.push_back(defaultReference);
  const auto defaultMeta = CtraceRunMeta::fromConfig(defaultTraceRun);
  const auto* defaultSource = defaultMeta.resolveSource("dwt", std::nullopt, 0U);
  require(defaultSource != nullptr && defaultSource->valueType == "unsigned int" && defaultSource->valueSize == 4U,
          "missing DWT data.symbol-type/data.symbol-size must default to unsigned int/4");

  const auto defaultOutputDir = root / "default";
  auto defaultOptions = makeCtfBundleConfig(defaultOutputDir, 1000000U);
  defaultOptions.sources = resolvedSources(defaultMeta);
  CollectingDiagnosticSink diagnostics;
  CtfBundleOutput defaultOutput(std::move(defaultOptions), &diagnostics);
  defaultOutput.start();
  defaultOutput.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 1U, 0xffU, AccessType::Write}});
  defaultOutput.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 2U, 0xffffU, AccessType::Write}});
  defaultOutput.stop();
  require(readFirstCtfDwtValueTag(defaultOutputDir / "stream_0") == 6U,
          "default DWT metadata must select the unsigned 32-bit CTF variant");
  require(diagnostics.events().size() == 1U && diagnostics.events()[0].severity == DiagnosticSink::Severity::Warning &&
              diagnostics.events()[0].code == "dwt-symbol-size-mismatch" && diagnostics.fatalCount() == 0U,
          "DWT size mismatch must be reported once per channel");

  TraceRunConfig signedTraceRun;
  TraceRunSetup signedSetup;
  signedSetup.data.push_back(TraceRunDataSetup{"signed int", 1U});
  signedTraceRun.setups.push_back(std::move(signedSetup));
  signedTraceRun.references.push_back(defaultReference);
  const auto signedOutputDir = root / "signed";
  auto signedOptions = makeCtfBundleConfig(signedOutputDir, 1000000U);
  signedOptions.sources = resolvedSources(CtraceRunMeta::fromConfig(signedTraceRun));
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
  TraceRunReference first;
  first.ctraceRef = "opaque/dwt-route";
  first.type = "dwt";
  first.stream = 1U;
  first.sources = {0U};
  first.dataSetupIndex = 0U;
  first.label = "core-one";
  TraceRunReference second = first;
  second.stream = 2U;
  second.label = "core-two";
  traceRun.references = {first, second};
  const auto meta = CtraceRunMeta::fromConfig(traceRun);
  const auto* selected = meta.resolveSource("dwt", std::optional<std::uint8_t>(1U), 0U);
  require(selected != nullptr && selected->label == std::optional<std::string>("core-one"),
          "selected stream must resolve its exact DWT metadata");

  bool rejectedAmbiguous = false;
  try {
    (void)meta.resolveSource("dwt", std::nullopt, 0U);
  } catch (const std::runtime_error& error) {
    rejectedAmbiguous = std::string(error.what()).find("ambiguous metadata") != std::string::npos;
  }
  require(rejectedAmbiguous, "unformatted CTF must reject conflicting metadata from multiple ATB streams");
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

  TraceEvent software = softwarePacket(1U, 1U, 'A');
  software.tcyc = 100;
  output.writeEvent(software);
  output.writeEvent(exceptionPacket(15, ExceptionAction::Entered, 200));
  output.writeEvent(overflowPacket(300));
  TraceEvent error = issuePacket("opencsd-bad-packet-sequence");
  error.tcyc = 400;
  error.quality = TraceQuality{true, false, 1U};
  output.writeEvent(error);
  output.stop();

  requireSingleCtfItmEvent(outputDir / "stream_0", 1U,
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

  TraceEvent globalTimestamp{GlobalTimestampTraceEvent{1234U, false}};
  globalTimestamp.tcyc = 42U;
  output.writeEvent(globalTimestamp);
  output.writeEvent(softwarePacket(1U, 1U, 'A'));
  output.stop();

  const auto stream = readTestBinaryFile(outputDir / "stream_0");

  constexpr std::size_t qualityFlagsOffset = kCtfEventOffset + kCtfEventHeaderSize + 3U;
  constexpr std::uint8_t timestampReliable = 1U << 1U;
  constexpr std::uint8_t beforeFirstLocalTimestamp = 1U << 2U;
  require(readLe32(stream, kCtfEventOffset) == 0U, "CTF ITM event missing after global timestamp");
  require(stream[qualityFlagsOffset] == (timestampReliable | beforeFirstLocalTimestamp),
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
  TraceEvent first = softwarePacket(1U, 1U, 'A');
  first.tcyc = 100U;
  output.writeEvent(first);
  TraceEvent second = softwarePacket(1U, 1U, 'B');
  second.tcyc = 10U;
  output.writeEvent(second);
  output.stop();

  const auto stream = readTestBinaryFile(outputDir / "stream_0");

  constexpr std::size_t itmEventSize = kCtfEventHeaderSize + 8U;
  require(readLe64(stream, kCtfEventOffset + 4U) == 100U, "first CTF event timestamp mismatch");
  require(readLe64(stream, kCtfEventOffset + itmEventSize + 4U) == 100U, "CTF event timestamps must not regress");
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

  const auto stream = readTestBinaryFile(outputDir / "stream_0");
  const auto metadata = readTestTextFile(outputDir / "metadata");

  constexpr std::size_t payloadSize = 9U;
  constexpr std::size_t contentSize = kCtfEventOffset + kCtfEventHeaderSize + payloadSize;
  require(stream.size() >= contentSize, "CTF global timestamp event missing");
  require(readLe32(stream, kCtfPacketHeaderSize + 4U) == contentSize * 8U,
          "CTF global timestamp filter emitted unrelated events");
  require(readLe32(stream, kCtfEventOffset) == 8U, "CTF global timestamp event ID mismatch");
  require(readLe64(stream, kCtfEventOffset + kCtfEventHeaderSize) == timestampValue,
          "CTF global timestamp value mismatch");
  require(stream[kCtfEventOffset + kCtfEventHeaderSize + 8U] == 1U, "CTF global timestamp clock-change flag mismatch");
  require(metadata.find("name = \"GLOBAL_TIMESTAMP\"") != std::string::npos, "CTF global timestamp metadata missing");
}

TEST(CtraceUnitTests, testCtfBundleOutputExcludesSoftwareChannelZero)
{
  const TemporaryCtfOutput temporaryOutput("ctrace-ctf-channel-zero-test");
  const auto& outputDir = temporaryOutput.outputDirectory();
  auto options = makeCtfBundleConfig(outputDir, 1000000U);
  TraceRunConfig traceRun;
  TraceRunReference channelZero;
  channelZero.ctraceRef = "opaque/channel-zero";
  channelZero.type = "itm";
  channelZero.label = "Console";
  channelZero.sources = {0U};
  traceRun.references.push_back(std::move(channelZero));
  options.sources = resolvedSources(CtraceRunMeta::fromConfig(traceRun));
  options.selection.types.push_back("itm");
  options.selection.types.push_back("error");
  CtfBundleOutput output(std::move(options));
  output.start();
  TraceEvent software = softwarePacket(0U, 1U, 'A');
  software.tcyc = 100U;
  output.writeEvent(software);
  TraceEvent softwareError = issuePacket("opencsd-incomplete-tail");
  softwareError.tcyc = 101U;
  output.writeEvent(softwareError);
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

  TraceEvent software = softwarePacket(1U, 1U, 'A');
  software.tcyc = 10U;
  output.writeEvent(software);
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

  bool rejected = false;
  try {
    CtfBundleOutput output(CtfOutputConfig(ctfDirectory, nestedXml, 1000000U, {}, {}));
  } catch (const std::invalid_argument&) {
    rejected = true;
  }
  require(rejected && readTestTextFile(ctfDirectory / "old-marker") == "old-ctf" &&
              readTestTextFile(nestedXml) == "old-xml",
          "overlapping CTF targets must be rejected before either existing target is deleted");

  const auto wrongTypeCtf = root / "WrongType.ctf";
  const auto wrongTypeXml = root / "WrongType.SWO.traceanalysis.xml";
  writeTestFile(wrongTypeCtf, "not-a-directory");
  std::filesystem::create_directory(wrongTypeXml);
  bool rejectedWrongTypes = false;
  try {
    CtfBundleOutput output(CtfOutputConfig(wrongTypeCtf, wrongTypeXml, 1000000U, {}, {}));
    output.start();
  } catch (const std::runtime_error&) {
    rejectedWrongTypes = true;
  }
  require(rejectedWrongTypes && readTestTextFile(wrongTypeCtf) == "not-a-directory" &&
              std::filesystem::is_directory(wrongTypeXml),
          "CTF start must reject unexpected target types before deleting either target");

#ifdef _WIN32
  bool rejectedCaseInsensitiveOverlap = false;
  try {
    CtfBundleOutput output(
        CtfOutputConfig(root / "Bundle.ctf", root / "BUNDLE.CTF" / "Bundle.SWO.traceanalysis.xml", 1000000U, {}, {}));
  } catch (const std::invalid_argument&) {
    rejectedCaseInsensitiveOverlap = true;
  }
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
  writeTestFile(ctfDirectory / "old-marker", "old-ctf");
  writeTestFile(xmlPath, "old-xml");

  auto config = makeCtfBundleConfig(ctfDirectory, 1000000U);
  CtfBundleOutput output(std::move(config));
  output.start();
  TraceEvent software = softwarePacket(1U, 1U, 'A');
  software.tcyc = 10U;
  output.writeEvent(software);
  output.stop();

  require(!std::filesystem::exists(ctfDirectory / "old-marker") &&
              std::filesystem::is_regular_file(ctfDirectory / "metadata") &&
              std::filesystem::is_regular_file(ctfDirectory / "stream_0") && std::filesystem::is_regular_file(xmlPath),
          "CTF bundle must complete directly in its final output paths");
  const auto completedXml = readTestTextFile(xmlPath);
  require(completedXml != "old-xml" && !completedXml.empty(),
          "CTF bundle did not replace the previous Trace Compass XML");

  writeTestFile(ctfDirectory / "stale-marker", "stale");
  writeTestFile(xmlPath, "stale-xml");
  output.start();
  require(!std::filesystem::exists(ctfDirectory / "stale-marker") &&
              !std::filesystem::exists(ctfDirectory / "metadata") && readTestTextFile(xmlPath) != "stale-xml",
          "restarting CTF output must delete the previous bundle before writing");
  output.writeEvent(software);
  output.abort();
  require(!std::filesystem::exists(ctfDirectory) && !std::filesystem::exists(xmlPath),
          "aborted direct CTF output must not leave an incomplete bundle");

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
  std::filesystem::create_directories(root.path());
  const auto blockedParent = root.path() / "not-a-directory";
  writeTestFile(blockedParent, "file");
  const auto outputDirectory = root.path() / "trace.ctf";
  const auto xmlPath = blockedParent / "trace.xml";

  CtfBundleOutput output(CtfOutputConfig(outputDirectory, xmlPath, 1000000U, {}, {}));
  EXPECT_THROW(output.start(), std::runtime_error);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory));
}
