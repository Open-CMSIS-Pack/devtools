/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "CtfTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "ctf/CtfEncoder.h"
#include "ctf/CtfSchema.h"
#include "TestPath.h"
#include "TraceEvent.h"
#include "TraceOutputConfig.h"
#include "TraceSelection.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using CtfTestSupport::CtfExceptionRecord;
using CtfTestSupport::CtfRecord;
using CtfTestSupport::kCtfEventOffset;
using CtfTestSupport::kCtfPacketContextSize;
using CtfTestSupport::kCtfPacketHeaderSize;
using CtfTestSupport::kCtfPacketSize;
using CtfTestSupport::readCtfExceptionRecords;
using CtfTestSupport::readCtfRecords;
using CtfTestSupport::readLe16;
using CtfTestSupport::readLe32;
using CtfTestSupport::requireFirstCtfRecord;
using CtfTestSupport::requireSingleItmEvent;
using CtfTestSupport::TimestampedCtfExceptionRecord;
using CtfTestSupport::timestampedCtfExceptionRecords;

/** @brief Formats an encoded CTF UUID for comparison. */
static std::string formatCtfUuid(const std::vector<unsigned char>& bytes, std::size_t offset)
{
  static constexpr char hexDigits[] = "0123456789abcdef";
  std::string result;
  result.reserve(36U);
  for (std::size_t index = 0; index < 16U; ++index) {
    if (index == 4U || index == 6U || index == 8U || index == 10U) {
      result.push_back('-');
    }
    const auto byte = bytes[offset + index];
    result.push_back(hexDigits[(byte >> 4U) & 0x0fU]);
    result.push_back(hexDigits[byte & 0x0fU]);
  }
  return result;
}

/** @brief Creates resolved DWT source metadata for one exact legacy route. */
static CtfSourceDescriptor resolvedDwtSource(std::uint32_t comparator, std::string type, std::uint8_t size,
                                             TraceRouteIdentity route = {})
{
  CtfSourceDescriptor source;
  source.type = "dwt";
  source.source = comparator;
  source.route = route;
  source.dataType = std::move(type);
  source.dataSize = size;
  return source;
}

/** @brief Creates an explicit legacy SINGLE-stream encoder configuration. */
static CtfEncoderConfig legacyEncoderConfig(std::uint64_t clockHz, TraceSelection selection = {},
                                            std::vector<CtfSourceDescriptor> sources = {},
                                            DiagnosticSink* diagnostics = nullptr,
                                            std::vector<TraceRouteIdentity> routes = {})
{
  const auto metadataRoute = routes.empty() ? TraceRouteIdentity{} : routes.front();
  return {
      CtfTestSupport::legacyTopology(clockHz, metadataRoute, std::move(sources)),
      std::move(selection),
      diagnostics,
      std::move(routes),
  };
}

/** @brief Creates a two-route formatted topology with boundary stream IDs. */
static CtfMetadataTopology formattedEncoderTopology(bool sharedClock = false)
{
  const TraceRouteIdentity first{TraceRouteId{4U}, 1U};
  const TraceRouteIdentity second{TraceRouteId{90U}, 111U};
  CtfMetadataTopology topology{
      {
          {CtfClockDomainId{3U}, "first_clock", CtfTestSupport::testUuid(3U), 240000000U, false},
          {CtfClockDomainId{9U}, "second_clock", CtfTestSupport::testUuid(9U), 480000000U, false},
      },
      {
          {CtfStreamClassId{1U}, first, std::string("first"), CtfClockDomainId{3U}},
          {CtfStreamClassId{111U}, second, std::string("second"),
           sharedClock ? CtfClockDomainId{3U} : CtfClockDomainId{9U}},
      },
      {
          {"dwt", 0U, first, std::string("First DWT"), 0x1000U, "unsigned", 4U},
          {"dwt", 0U, second, std::string("Second DWT"), 0x2000U, "signed", 2U},
          {"itm", 1U, first, std::string("First console"), std::nullopt, "unsigned", 4U},
          {"itm", 1U, second, std::string("Second console"), std::nullopt, "unsigned", 4U},
      },
  };
  if (sharedClock) {
    topology.clockDomains.pop_back();
  }
  return topology;
}

/** @brief Creates an encoder configuration for the formatted test topology. */
static CtfEncoderConfig formattedEncoderConfig(TraceSelection selection = {}, bool sharedClock = false)
{
  return {
      formattedEncoderTopology(sharedClock),
      std::move(selection),
      nullptr,
      {{TraceRouteId{4U}, 1U}, {TraceRouteId{90U}, 111U}},
  };
}

/** @brief Starts an encoder with a deterministic bundle UUID. */
static void startEncoder(CtfEncoder& encoder, const std::filesystem::path& outputDirectory)
{
  encoder.start(outputDirectory, CtfTestSupport::testUuid());
}

TEST(CtraceUnitTests, testCtfEncoderWritesOnlyIntoProvidedDirectory)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-boundary-test");
  const auto& root = temporaryPath.path();
  const auto missingDirectory = root / "missing";
  const auto outputDirectory = root / "provided";

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"itm"}, {}}));
  const auto rejectedMissingDirectory = throwsException([&] { startEncoder(encoder, missingDirectory); });
  ASSERT_TRUE(rejectedMissingDirectory && !std::filesystem::exists(missingDirectory))
      << "CtfEncoder must not create or own its output directory";

  std::filesystem::create_directories(outputDirectory);
  startEncoder(encoder, outputDirectory);
  encoder.writeEvent(atCycle(softwarePacket(1U, 1U, 'A'), 10U));
  encoder.stop();
  ASSERT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata") &&
              std::filesystem::is_regular_file(outputDirectory / "stream_0"))
      << "CtfEncoder must encode metadata and stream data into the provided directory";
  requireSingleItmEvent(outputDirectory / "stream_0", 1U, "CtfEncoder encoded an unexpected ITM event");

  encoder.abort();
  ASSERT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata"))
      << "CtfEncoder abort must not delete a directory owned by its caller";
}

TEST(CtraceUnitTests, testCtfEncoderPcSampleEncoding)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-pc-sample-test");
  const auto& outputDirectory = temporaryPath.createDirectory();

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"pcsample"}, {}}));
  startEncoder(encoder, outputDirectory);
  auto pc = atCycle(TraceEvent{PcSampleTraceEvent{0x08001234U, PcSampleKind::Pc}}, 10U);
  pc.quality = TraceQuality{false, true, 0U};
  encoder.writeEvent(pc);
  auto sleep = atCycle(TraceEvent{PcSampleTraceEvent{0x12345678U, PcSampleKind::Sleep}}, 11U);
  sleep.quality = TraceQuality{true, false, 7U};
  encoder.writeEvent(sleep);
  encoder.stop();

  const auto records = readCtfRecords(outputDirectory / "stream_0");
  ASSERT_EQ(records.size(), 2U);
  for (const auto& record : records) {
    EXPECT_EQ(record.id, CtfSchema::value(CtfSchema::EventId::PcSample));
    EXPECT_EQ(record.traceBusId, 0U);
  }
  ASSERT_EQ(records[0].payload.size(), 10U);
  EXPECT_EQ(records[0].timestamp, 10U);
  EXPECT_EQ(records[0].payload[0U], CtfSchema::value(CtfSchema::PcSampleState::Pc));
  EXPECT_EQ(readLe32(records[0].payload, 1U), 0x08001234U);
  EXPECT_EQ(records[0].payload[5U], CtfSchema::SampleFlagTimestampReliable);
  EXPECT_EQ(readLe32(records[0].payload, 6U), 0U);

  ASSERT_EQ(records[1].payload.size(), 6U);
  EXPECT_EQ(records[1].timestamp, 11U);
  EXPECT_EQ(records[1].payload[0U], CtfSchema::value(CtfSchema::PcSampleState::Sleep));
  EXPECT_EQ(records[1].payload[1U], CtfSchema::SampleFlagOverflow);
  EXPECT_EQ(readLe32(records[1].payload, 2U), 7U);

  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("name = \"PC_SAMPLE\""), std::string::npos);
  EXPECT_NE(metadata.find("uint8_t cmsis_pc_sample_state"), std::string::npos);
  EXPECT_EQ(metadata.find("cmsis_pc_sample_state_t"), std::string::npos);
  EXPECT_NE(metadata.find("uint32_t cmsis_pc[cmsis_pc_sample_state]"), std::string::npos);
}

TEST(CtraceUnitTests, testCtfEncoderPcSampleProhibitedPreservesQualityAndFollowingPc)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-pc-sample-prohibited-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"pcsample"}, {}}));
  startEncoder(encoder, outputDirectory);

  auto prohibited = atCycle(TraceEvent{PcSampleTraceEvent{0xffffffffU, PcSampleKind::TraceProhibited}}, 12U);
  prohibited.quality = TraceQuality{true, true, 7U};
  encoder.writeEvent(prohibited);
  auto pc = atCycle(TraceEvent{PcSampleTraceEvent{0x08005678U, PcSampleKind::Pc}}, 13U);
  pc.quality = TraceQuality{false, true, 7U};
  encoder.writeEvent(pc);
  encoder.stop();

  const auto records = readCtfRecords(outputDirectory / "stream_0");
  ASSERT_EQ(records.size(), 2U);
  EXPECT_EQ(records[0].id, CtfSchema::value(CtfSchema::EventId::PcSampleProhibited));
  EXPECT_EQ(records[0].timestamp, 12U);
  EXPECT_EQ(records[0].traceBusId, 0U);
  EXPECT_FALSE(records[0].routeLabelId.has_value());
  ASSERT_EQ(records[0].payload.size(), 5U);
  EXPECT_EQ(records[0].payload[0U],
            CtfSchema::SampleFlagOverflow | CtfSchema::SampleFlagTimestampReliable);
  EXPECT_EQ(readLe32(records[0].payload, 1U), 7U);

  EXPECT_EQ(records[1].id, CtfSchema::value(CtfSchema::EventId::PcSample));
  EXPECT_EQ(records[1].timestamp, 13U);
  ASSERT_EQ(records[1].payload.size(), 10U);
  EXPECT_EQ(records[1].payload[0U], CtfSchema::value(CtfSchema::PcSampleState::Pc));
  EXPECT_EQ(readLe32(records[1].payload, 1U), 0x08005678U);
  EXPECT_EQ(records[1].payload[5U], CtfSchema::SampleFlagTimestampReliable);
  EXPECT_EQ(readLe32(records[1].payload, 6U), 7U);
}

TEST(CtraceUnitTests, testCtfEncoderPcSampleProhibitedRespectsTypeAndRouteFilters)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-pc-sample-prohibited-filter-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(formattedEncoderConfig(TraceSelection{{"pcsample"}, {111U}}));
  startEncoder(encoder, outputDirectory);
  const TraceRouteIdentity firstRoute{TraceRouteId{4U}, 1U};
  const TraceRouteIdentity secondRoute{TraceRouteId{90U}, 111U};
  auto prohibited = atCycle(TraceEvent{PcSampleTraceEvent{0U, PcSampleKind::TraceProhibited}}, 15U);
  prohibited.quality = TraceQuality{false, false, 3U};
  encoder.writeEvent(onRoute(prohibited, firstRoute));
  encoder.writeEvent(onRoute(prohibited, secondRoute));
  encoder.writeEvent(onRoute(softwarePacket(1U, 1U, 'A'), secondRoute));
  encoder.stop();

  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
  const auto records =
      readCtfRecords(outputDirectory / "stream_111", CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_EQ(records.size(), 1U);
  EXPECT_EQ(records[0].id, CtfSchema::value(CtfSchema::EventId::PcSampleProhibited));
  EXPECT_EQ(records[0].timestamp, 15U);
  EXPECT_EQ(records[0].traceBusId, 111U);
  EXPECT_EQ(records[0].routeLabelId, std::optional<std::uint8_t>{111U});
  ASSERT_EQ(records[0].payload.size(), 5U);
  EXPECT_EQ(records[0].payload[0U], CtfSchema::SampleFlagBeforeFirstTimestamp);
  EXPECT_EQ(readLe32(records[0].payload, 1U), 3U);

  const TemporaryTestPath filteredPath("ctrace-ctf-pc-sample-prohibited-unselected-test");
  CtfEncoder filteredEncoder(legacyEncoderConfig(1000000U, TraceSelection{{"itm"}, {}}));
  startEncoder(filteredEncoder, filteredPath.createDirectory());
  filteredEncoder.writeEvent(prohibited);
  filteredEncoder.stop();
  EXPECT_TRUE(readCtfRecords(filteredPath.path() / "stream_0").empty());
}

TEST(CtraceUnitTests, testCtfEncoderExpandsDwtEventCounterMask)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-dwt-event-test");
  const auto& outputDirectory = temporaryPath.createDirectory();

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"event"}, {}}));
  startEncoder(encoder, outputDirectory);
  auto event = atCycle(TraceEvent{DwtEventTraceEvent{0x3fU}}, 123U);
  event.quality = TraceQuality{true, false, 7U};
  encoder.writeEvent(event);
  encoder.stop();

  const auto records = readCtfRecords(outputDirectory / "stream_0");
  ASSERT_EQ(records.size(), 6U);
  constexpr std::array<std::uint8_t, 6U> expectedCounters{{0U, 1U, 2U, 3U, 4U, 5U}};
  for (std::size_t index = 0U; index < records.size(); ++index) {
    const auto& record = records[index];
    EXPECT_EQ(record.id, CtfSchema::value(CtfSchema::EventId::DwtEvent));
    EXPECT_EQ(record.timestamp, 123U);
    EXPECT_EQ(record.traceBusId, 0U);
    ASSERT_EQ(record.payload.size(), 6U);
    EXPECT_EQ(record.payload[0U], expectedCounters[index]);
    EXPECT_EQ(record.payload[1U], CtfSchema::SampleFlagOverflow | CtfSchema::SampleFlagBeforeFirstTimestamp);
    EXPECT_EQ(readLe32(record.payload, 2U), 7U);
  }

  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("name = \"DWT_EVENT\""), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_dwt_event_counter_t cmsis_dwt_event_counter;"), std::string::npos);
}

TEST(CtraceUnitTests, testCtfEncoderWritesDwtMatch)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-dwt-match-test");
  const auto& outputDirectory = temporaryPath.createDirectory();

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"dwt"}, {}}));
  startEncoder(encoder, outputDirectory);
  auto match = atCycle(TraceEvent{DwtMatchTraceEvent{2U}}, 123U);
  match.quality = TraceQuality{true, false, 7U};
  encoder.writeEvent(match);
  encoder.stop();

  const auto records = readCtfRecords(outputDirectory / "stream_0");
  ASSERT_EQ(records.size(), 1U);
  const auto& record = records.front();
  EXPECT_EQ(record.id, CtfSchema::value(CtfSchema::EventId::DwtMatch));
  EXPECT_EQ(record.timestamp, 123U);
  EXPECT_EQ(record.traceBusId, 0U);
  ASSERT_EQ(record.payload.size(), 6U);
  EXPECT_EQ(record.payload[0U], 2U);
  EXPECT_EQ(record.payload[1U], CtfSchema::SampleFlagOverflow | CtfSchema::SampleFlagBeforeFirstTimestamp);
  EXPECT_EQ(readLe32(record.payload, 2U), 7U);

  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("name = \"DWT_MATCH\""), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_dwt_comparator_t cmsis_dwt_comparator;"), std::string::npos);
}

TEST(CtraceUnitTests, testCtfEncoderExpandsPmuEventCounterMask)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-pmu-event-test");
  const auto& outputDirectory = temporaryPath.createDirectory();

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"pmu"}, {}}));
  startEncoder(encoder, outputDirectory);
  auto event = atCycle(TraceEvent{PmuTraceEvent{0x81U}}, 124U);
  event.quality = TraceQuality{false, true, 9U};
  encoder.writeEvent(event);
  encoder.stop();

  const auto records = readCtfRecords(outputDirectory / "stream_0");
  ASSERT_EQ(records.size(), 2U);
  constexpr std::array<std::uint8_t, 2U> expectedCounters{{0U, 7U}};
  for (std::size_t index = 0U; index < records.size(); ++index) {
    const auto& record = records[index];
    EXPECT_EQ(record.id, CtfSchema::value(CtfSchema::EventId::PmuEvent));
    EXPECT_EQ(record.timestamp, 124U);
    EXPECT_EQ(record.traceBusId, 0U);
    ASSERT_EQ(record.payload.size(), 6U);
    EXPECT_EQ(record.payload[0U], expectedCounters[index]);
    EXPECT_EQ(record.payload[1U], CtfSchema::SampleFlagTimestampReliable);
    EXPECT_EQ(readLe32(record.payload, 2U), 9U);
  }

  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("name = \"PMU_EVENT\""), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_pmu_event_counter_t cmsis_pmu_event_counter;"), std::string::npos);
}

TEST(CtraceUnitTests, testCtfEncoderPacketBoundaryAndUuid)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-packet-boundary-test");
  const auto& outputDirectory = temporaryPath.createDirectory();

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"itm"}, {}}));
  startEncoder(encoder, outputDirectory);

  // A one-byte ITM event occupies 21 bytes. Exactly 3118 events fit after
  // the 56-byte header/context; event 3119 starts packet 2.
  constexpr std::size_t eventsInFirstPacket = 3118U;
  for (std::size_t index = 0; index <= eventsInFirstPacket; ++index) {
    encoder.writeEvent(atCycle(softwarePacket(1U, 1U, static_cast<std::uint32_t>(index)), index + 1U));
  }
  encoder.stop();

  const auto stream = readTestBinaryFile(outputDirectory / "stream_0");
  const auto metadata = readTestTextFile(outputDirectory / "metadata");

  constexpr std::size_t eventSize = 21U;
  constexpr auto firstContentSize = kCtfPacketHeaderSize + kCtfPacketContextSize + eventsInFirstPacket * eventSize;
  constexpr std::uint32_t ctfMagic = 0xc1fc1fc1U;
  ASSERT_TRUE(stream.size() == kCtfPacketSize * 2U) << "CTF packet rollover must emit two complete 64-KiB packets";
  ASSERT_TRUE(readLe32(stream, 0U) == ctfMagic && readLe32(stream, kCtfPacketSize) == ctfMagic)
      << "CTF packet rollover emitted an invalid packet header";
  ASSERT_TRUE(readLe32(stream, kCtfPacketHeaderSize) == kCtfPacketSize * 8U &&
              readLe32(stream, kCtfPacketHeaderSize + 4U) == firstContentSize * 8U)
      << "first CTF packet content size mismatch at rollover";
  ASSERT_TRUE(readLe32(stream, kCtfPacketSize + kCtfPacketHeaderSize + 4U) == (kCtfEventOffset + eventSize) * 8U)
      << "second CTF packet content size mismatch";
  ASSERT_TRUE(readLe32(stream, kCtfPacketHeaderSize + 28U) == 0U &&
              readLe32(stream, kCtfPacketSize + kCtfPacketHeaderSize + 28U) == 1U)
      << "CTF packet sequence must advance across a 64-KiB boundary";
  ASSERT_TRUE(std::equal(stream.begin() + 4U, stream.begin() + 20U, stream.begin() + kCtfPacketSize + 4U))
      << "CTF packet UUID must remain stable across packet rollover";
  const auto& injectedUuid = CtfTestSupport::testUuid();
  ASSERT_TRUE(
      std::equal(injectedUuid.bytes().begin(), injectedUuid.bytes().end(), stream.begin() + 4U) &&
      std::equal(injectedUuid.bytes().begin(), injectedUuid.bytes().end(), stream.begin() + kCtfPacketSize + 4U))
      << "every packet header must use the explicitly injected bundle UUID";

  const auto uuid = formatCtfUuid(stream, 4U);
  ASSERT_EQ(uuid, injectedUuid.toString());
  ASSERT_TRUE(metadata.find("uuid = \"" + injectedUuid.toString() + "\";") != std::string::npos)
      << "CTF metadata UUID must match the binary packet UUID";
  ASSERT_TRUE((stream[4U + 6U] & 0xf0U) == 0x40U && (stream[4U + 8U] & 0xc0U) == 0x80U)
      << "CTF UUID must use RFC 4122 version-4 and variant bits";

  encoder.abort();
}

TEST(CtraceUnitTests, testCtfEncoderDwtAddressEncoding)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-dwt-address-test");
  const auto& outputDirectory = temporaryPath.createDirectory();

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"dwt"}, {}}));
  startEncoder(encoder, outputDirectory);
  encoder.writeEvent(atCycle(TraceEvent{DwtAddressTraceEvent{
                                 3U,
                                 DwtPcAndDataAddressTraceLocation{{4U, 0x12345678U}, {2U, 0x0000abcdU}},
                             }},
                             99U));
  encoder.writeEvent(
      atCycle(TraceEvent{DwtAddressTraceEvent{1U, DwtDataAddressTraceLocation{{1U, 0x58U}}}}, 100U));
  encoder.writeEvent(
      atCycle(TraceEvent{DwtAddressTraceEvent{2U, DwtDataAddressTraceLocation{{4U, 0x20007858U}}}}, 101U));
  encoder.writeEvent(atCycle(TraceEvent{DwtAddressTraceEvent{0U, DwtPcTraceLocation{{4U, 0x08001234U}}}}, 102U));
  encoder.writeEvent(atCycle(TraceEvent{DwtAddressTraceEvent{1U, DwtPcTraceLocation{{1U, 0x58U}}}}, 103U));
  encoder.writeEvent(atCycle(TraceEvent{DwtAddressTraceEvent{2U, DwtPcTraceLocation{{2U, 0x7858U}}}}, 104U));
  encoder.stop();

  const auto records = readCtfRecords(outputDirectory / "stream_0");
  ASSERT_EQ(records.size(), 6U);
  const auto& record = records.front();
  ASSERT_TRUE(record.id == CtfSchema::value(CtfSchema::EventId::DwtAddress)) << "CTF DWT address event ID mismatch";
  ASSERT_TRUE(record.timestamp == 99U) << "CTF DWT address timestamp mismatch";
  ASSERT_TRUE(record.payload.size() == 14U) << "CTF DWT address event payload size mismatch";
  ASSERT_TRUE(record.payload[0U] == 3U &&
              record.payload[1U] == CtfSchema::value(CtfSchema::DwtAddressTag::U32) &&
              record.payload[6U] == CtfSchema::value(CtfSchema::DwtAddressTag::U16))
      << "CTF DWT address comparator, PC tag, or data-address tag mismatch";
  ASSERT_TRUE(readLe32(record.payload, 2U) == 0x12345678U && readLe16(record.payload, 7U) == 0xabcdU)
      << "CTF DWT PC/address payload mismatch";

  EXPECT_EQ(records[1].payload.size(), 10U);
  EXPECT_EQ(records[1].payload[3U], CtfSchema::value(CtfSchema::DwtAddressTag::U8));
  EXPECT_EQ(records[1].payload[4U], 0x58U);
  EXPECT_EQ(records[2].payload.size(), 13U);
  EXPECT_EQ(records[2].payload[3U], CtfSchema::value(CtfSchema::DwtAddressTag::U32));
  EXPECT_EQ(readLe32(records[2].payload, 4U), 0x20007858U);
  EXPECT_EQ(records[3].payload.size(), 13U);
  EXPECT_EQ(records[3].payload[6U], CtfSchema::value(CtfSchema::DwtAddressTag::None));
  EXPECT_EQ(records[3].payload[7U], 0U);
  EXPECT_EQ(records[4].payload.size(), 10U);
  EXPECT_EQ(records[4].payload[1U], CtfSchema::value(CtfSchema::DwtAddressTag::U8));
  EXPECT_EQ(records[4].payload[2U], 0x58U);
  EXPECT_EQ(records[5].payload.size(), 11U);
  EXPECT_EQ(records[5].payload[1U], CtfSchema::value(CtfSchema::DwtAddressTag::U16));
  EXPECT_EQ(readLe16(records[5].payload, 2U), 0x7858U);

  ASSERT_NE(encoder.completedMetadata(), nullptr);
  EXPECT_TRUE(encoder.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{0U},
                                                                   CtfGraphicalTopic::DwtAddress));

  encoder.abort();
}

TEST(CtraceUnitTests, testCtfEncoderRejectsInvalidClockAndPayloadMetadata)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-invalid-payload-test");
  temporaryPath.createDirectory();
  CtfEncoder zeroClock(legacyEncoderConfig(0U));
  EXPECT_THROW(startEncoder(zeroClock, temporaryPath.path()), std::invalid_argument);

  CtfEncoder invalidItm(legacyEncoderConfig(1000000U, TraceSelection{{"itm"}, {}}));
  invalidItm.stop();
  invalidItm.writeEvent(softwarePacket(1U));
  startEncoder(invalidItm, temporaryPath.path());
  EXPECT_THROW(invalidItm.writeEvent(softwarePacket(1U, 3U, 0U)), std::runtime_error);
  invalidItm.abort();

  CtfEncoder invalidDwt(
      legacyEncoderConfig(1000000U, TraceSelection{{"dwt"}, {}}, {resolvedDwtSource(0U, "unsupported", 3U)}));
  EXPECT_THROW(startEncoder(invalidDwt, temporaryPath.path()), std::invalid_argument);

  CtfEncoder invalidAddress(legacyEncoderConfig(1000000U, TraceSelection{{"dwt"}, {}}));
  startEncoder(invalidAddress, temporaryPath.path());
  EXPECT_THROW(invalidAddress.writeEvent(TraceEvent{DwtAddressTraceEvent{0U, DwtDataAddressTraceLocation{{3U, 0U}}}}),
               std::runtime_error);
  invalidAddress.abort();
}

TEST(CtraceUnitTests, testCtfEncoderKeepsFormattedStreamsLazyAndProjectsCompletedMetadata)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-formatted-lazy-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(formattedEncoderConfig());
  startEncoder(encoder, outputDirectory);

  EXPECT_EQ(encoder.completedMetadata(), nullptr);
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_111"));
  const TraceRouteIdentity firstRoute{TraceRouteId{4U}, 1U};
  encoder.writeEvent(atCycle(onRoute(TraceEvent{LocalTimestampTraceEvent{}}, firstRoute), 30U));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"))
      << "a control packet without CTF output must not create a formatted stream";

  encoder.writeEvent(atCycle(onRoute(softwarePacket(1U, 1U, 'A'), firstRoute), 40U));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_111"));
  encoder.stop();

  const auto records = readCtfRecords(outputDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_EQ(records.size(), 3U);
  EXPECT_EQ(records[0].id, CtfSchema::value(CtfSchema::EventId::TraceStatus));
  EXPECT_EQ(records[0].payload[0U], CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart));
  EXPECT_EQ(records[0].timestamp, 30U);
  EXPECT_EQ(records[1].id, CtfSchema::value(CtfSchema::EventId::Exception));
  EXPECT_EQ(records[2].id, CtfSchema::value(CtfSchema::EventId::Itm));
  EXPECT_EQ(records[2].timestamp, 40U);
  for (const auto& record : records) {
    EXPECT_EQ(record.traceBusId, 1U);
  }

  const auto* completed = encoder.completedMetadata();
  ASSERT_NE(completed, nullptr);
  ASSERT_EQ(completed->topology().streams.size(), 1U);
  ASSERT_EQ(completed->topology().clockDomains.size(), 1U);
  ASSERT_EQ(completed->topology().sources.size(), 2U);
  EXPECT_EQ(completed->topology().streams.front().streamClassId, CtfStreamClassId{1U});
  EXPECT_EQ(completed->topology().clockDomains.front().id, CtfClockDomainId{3U});
  for (const auto& source : completed->topology().sources) {
    EXPECT_EQ(source.route.traceBusId, 1U);
  }
  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("stream {\n    id = 1;"), std::string::npos);
  EXPECT_NE(metadata.find("name = first_clock;"), std::string::npos);
  EXPECT_EQ(metadata.find("stream {\n    id = 111;"), std::string::npos);
  EXPECT_EQ(metadata.find("name = second_clock;"), std::string::npos);
  EXPECT_EQ(metadata.find("Second DWT"), std::string::npos);

  encoder.abort();
  EXPECT_EQ(encoder.completedMetadata(), nullptr);
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata"))
      << "the non-owning encoder must not delete its caller's completed output";
}

TEST(CtraceUnitTests, testCtfEncoderWritesInterleavedNonContiguousStreamsWithIndependentState)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-formatted-multistream-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(formattedEncoderConfig());
  startEncoder(encoder, outputDirectory);

  const TraceRouteIdentity firstRoute{TraceRouteId{4U}, 1U};
  const TraceRouteIdentity secondRoute{TraceRouteId{90U}, 111U};
  encoder.writeEvent(atCycle(onRoute(softwarePacket(1U, 1U, 'A'), firstRoute), 100U));
  encoder.writeEvent(atCycle(onRoute(softwarePacket(1U, 1U, 'B'), secondRoute), 10U));
  encoder.writeEvent(atCycle(onRoute(softwarePacket(1U, 1U, 'C'), firstRoute), 50U));
  encoder.writeEvent(atCycle(onRoute(softwarePacket(1U, 1U, 'D'), secondRoute), 20U));
  encoder.stop();

  const auto firstRecords =
      readCtfRecords(outputDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled);
  const auto secondRecords =
      readCtfRecords(outputDirectory / "stream_111", CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_EQ(firstRecords.size(), 4U);
  ASSERT_EQ(secondRecords.size(), 4U);
  EXPECT_EQ(firstRecords[0].id, CtfSchema::value(CtfSchema::EventId::TraceStatus));
  EXPECT_EQ(secondRecords[0].id, CtfSchema::value(CtfSchema::EventId::TraceStatus));
  EXPECT_EQ(firstRecords[1].id, CtfSchema::value(CtfSchema::EventId::Exception));
  EXPECT_EQ(secondRecords[1].id, CtfSchema::value(CtfSchema::EventId::Exception));
  EXPECT_EQ(firstRecords[2].timestamp, 100U);
  EXPECT_EQ(firstRecords[3].timestamp, 100U);
  EXPECT_EQ(secondRecords[2].timestamp, 10U);
  EXPECT_EQ(secondRecords[3].timestamp, 20U);
  for (const auto& record : firstRecords) {
    EXPECT_EQ(record.traceBusId, 1U);
  }
  for (const auto& record : secondRecords) {
    EXPECT_EQ(record.traceBusId, 111U);
  }

  const auto firstBytes = readTestBinaryFile(outputDirectory / "stream_1");
  const auto secondBytes = readTestBinaryFile(outputDirectory / "stream_111");
  EXPECT_EQ(readLe32(firstBytes, 20U), 1U);
  EXPECT_EQ(readLe32(secondBytes, 20U), 111U);
  EXPECT_EQ(formatCtfUuid(firstBytes, 4U), CtfTestSupport::testUuid().toString());
  EXPECT_EQ(formatCtfUuid(secondBytes, 4U), CtfTestSupport::testUuid().toString());
  EXPECT_EQ(readLe32(firstBytes, kCtfPacketHeaderSize + 28U), 0U);
  EXPECT_EQ(readLe32(secondBytes, kCtfPacketHeaderSize + 28U), 0U);

  const auto* completed = encoder.completedMetadata();
  ASSERT_NE(completed, nullptr);
  EXPECT_EQ(completed->topology().streams.size(), 2U);
  EXPECT_EQ(completed->topology().clockDomains.size(), 2U);
  EXPECT_EQ(completed->topology().sources.size(), 4U);
  const auto metadata = readTestTextFile(outputDirectory / "metadata");
  EXPECT_NE(metadata.find("stream {\n    id = 1;"), std::string::npos);
  EXPECT_NE(metadata.find("stream {\n    id = 111;"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_stream_1_dwt0_value_type = \"unsigned\";"), std::string::npos);
  EXPECT_NE(metadata.find("cmsis_stream_111_dwt0_value_type = \"signed\";"), std::string::npos);
}

TEST(CtraceUnitTests, testCtfEncoderAppliesFormattedStreamFilterBeforeLazyCreation)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-formatted-stream-filter-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(formattedEncoderConfig(TraceSelection{{}, {111U}}));
  startEncoder(encoder, outputDirectory);
  const TraceRouteIdentity firstRoute{TraceRouteId{4U}, 1U};
  const TraceRouteIdentity secondRoute{TraceRouteId{90U}, 111U};

  encoder.writeEvent(onRoute(softwarePacket(1U, 1U, 'A'), firstRoute));
  encoder.writeEvent(onRoute(TraceEvent{OverflowTraceEvent{}}, firstRoute));
  encoder.writeEvent(onRoute(TraceEvent{SyncTraceEvent{}}, firstRoute));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));

  encoder.writeEvent(onRoute(softwarePacket(1U, 1U, 'B'), secondRoute));
  encoder.stop();
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_111"));
  ASSERT_NE(encoder.completedMetadata(), nullptr);
  ASSERT_EQ(encoder.completedMetadata()->topology().streams.size(), 1U);
  EXPECT_EQ(encoder.completedMetadata()->topology().streams.front().streamClassId, CtfStreamClassId{111U});
  ASSERT_EQ(encoder.completedMetadata()->topology().clockDomains.size(), 1U);
  EXPECT_EQ(encoder.completedMetadata()->topology().clockDomains.front().id, CtfClockDomainId{9U});
}

TEST(CtraceUnitTests, testCtfEncoderKeepsOverflowQualityIndependentAcrossFormattedRoutes)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-formatted-overflow-isolation-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(formattedEncoderConfig());
  startEncoder(encoder, outputDirectory);
  const TraceRouteIdentity firstRoute{TraceRouteId{4U}, 1U};
  const TraceRouteIdentity secondRoute{TraceRouteId{90U}, 111U};

  auto firstOverflow = onRoute(TraceEvent{OverflowTraceEvent{}}, firstRoute);
  firstOverflow.quality = TraceQuality{true, false, 7U};
  encoder.writeEvent(firstOverflow);
  auto secondSample = onRoute(softwarePacket(1U, 1U, 'B'), secondRoute);
  secondSample.quality = TraceQuality{false, true, 0U};
  encoder.writeEvent(secondSample);
  encoder.writeEvent(onRoute(softwarePacket(1U, 1U, 'A'), firstRoute));
  encoder.stop();

  const auto firstRecords =
      readCtfRecords(outputDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled);
  const auto secondRecords =
      readCtfRecords(outputDirectory / "stream_111", CtfStreamWriter::EventContextLayout::RouteLabeled);
  const auto& firstSample =
      requireFirstCtfRecord(firstRecords, CtfSchema::EventId::Itm, "first route's CTF ITM sample is missing");
  const auto& secondRouteSample =
      requireFirstCtfRecord(secondRecords, CtfSchema::EventId::Itm, "second route's CTF ITM sample is missing");
  EXPECT_EQ(readLe32(firstSample.payload, 4U), 7U);
  EXPECT_EQ(readLe32(secondRouteSample.payload, 4U), 0U);
  EXPECT_EQ(secondRouteSample.payload[3U] & CtfSchema::SampleFlagOverflow, 0U);
  const auto firstOverflowStatus = std::find_if(firstRecords.begin(), firstRecords.end(), [](const CtfRecord& record) {
    return record.id == CtfSchema::value(CtfSchema::EventId::TraceStatus) &&
           record.payload[0U] == CtfSchema::value(CtfSchema::TraceStatusReason::Overflow);
  });
  ASSERT_NE(firstOverflowStatus, firstRecords.end());
  EXPECT_EQ(readLe32(firstOverflowStatus->payload, 1U), 7U);
  EXPECT_EQ(std::count_if(secondRecords.begin(), secondRecords.end(),
                          [](const CtfRecord& record) {
                            return record.id == CtfSchema::value(CtfSchema::EventId::TraceStatus) &&
                                   record.payload[0U] == CtfSchema::value(CtfSchema::TraceStatusReason::Overflow);
                          }),
            0);
}

TEST(CtraceUnitTests, testCtfEncoderKeepsExceptionLanesIndependentAcrossFormattedRoutes)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-formatted-exception-isolation-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(formattedEncoderConfig());
  startEncoder(encoder, outputDirectory);
  const TraceRouteIdentity firstRoute{TraceRouteId{4U}, 1U};
  const TraceRouteIdentity secondRoute{TraceRouteId{90U}, 111U};

  encoder.writeEvent(onRoute(exceptionPacket(15U, ExceptionAction::Entered, 10U), firstRoute));
  encoder.writeEvent(onRoute(exceptionPacket(54U, ExceptionAction::Entered, 20U), secondRoute));
  encoder.writeEvent(onRoute(exceptionPacket(16U, ExceptionAction::Entered, 30U), firstRoute));
  encoder.writeEvent(onRoute(exceptionPacket(0U, ExceptionAction::Returned, 40U), secondRoute));
  encoder.stop();

  EXPECT_EQ(readCtfExceptionRecords(outputDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled),
            (std::vector<CtfExceptionRecord>({
                {0U, 0U, 1U},
                {0U, 1U, 1U},
                {15U, 0U, 0U},
                {15U, 1U, 1U},
                {16U, 0U, 0U},
            })));
  EXPECT_EQ(readCtfExceptionRecords(outputDirectory / "stream_111", CtfStreamWriter::EventContextLayout::RouteLabeled),
            (std::vector<CtfExceptionRecord>({
                {0U, 0U, 1U},
                {0U, 1U, 1U},
                {54U, 0U, 0U},
                {54U, 1U, 1U},
                {0U, 2U, 0U},
            })));
  ASSERT_NE(encoder.completedMetadata(), nullptr);
  EXPECT_EQ(encoder.completedMetadata()->observedExceptions(CtfStreamClassId{1U}),
            (std::vector<ExceptionNumber>{0U, 15U, 16U}));
  EXPECT_EQ(encoder.completedMetadata()->observedExceptions(CtfStreamClassId{111U}),
            (std::vector<ExceptionNumber>{0U, 54U}));
  EXPECT_TRUE(encoder.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{1U},
                                                                   CtfGraphicalTopic::Exception));
  EXPECT_TRUE(encoder.completedMetadata()->observedGraphicalTopic(CtfStreamClassId{111U},
                                                                   CtfGraphicalTopic::Exception));
}

TEST(CtraceUnitTests, testCtfEncoderWritesMetadataOnlyForEmptyFormattedTopology)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-formatted-empty-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(CtfEncoderConfig{});
  startEncoder(encoder, outputDirectory);
  encoder.stop();

  ASSERT_NE(encoder.completedMetadata(), nullptr);
  EXPECT_TRUE(encoder.completedMetadata()->topology().streams.empty());
  EXPECT_TRUE(encoder.completedMetadata()->topology().clockDomains.empty());
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "metadata"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_0"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
}

TEST(CtraceUnitTests, testCtfEncoderDoesNotCreateFormattedArtifactsForEventsWithoutSelectedRecords)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-formatted-filter-test");
  const auto& outputDirectory = temporaryPath.createDirectory();
  CtfEncoder encoder(formattedEncoderConfig(TraceSelection{{"itm"}, {}}));
  startEncoder(encoder, outputDirectory);
  const TraceRouteIdentity firstRoute{TraceRouteId{4U}, 1U};
  const TraceRouteIdentity secondRoute{TraceRouteId{90U}, 111U};

  encoder.writeEvent(onRoute(exceptionPacket(15U, ExceptionAction::Entered, 1U), firstRoute));
  encoder.writeEvent(onRoute(exceptionPacket(15U, ExceptionAction::Unknown, 2U), firstRoute));
  encoder.writeEvent(onRoute(TraceEvent{OverflowTraceEvent{}}, firstRoute));
  encoder.writeEvent(onRoute(TraceEvent{SyncTraceEvent{}}, firstRoute));
  encoder.writeEvent(onRoute(TraceEvent{DwtDataTraceEvent{0U, 1U, 1U, AccessType::Read}}, firstRoute));
  encoder.writeEvent(onRoute(TraceEvent{DwtEventTraceEvent{0U}}, firstRoute));
  encoder.writeEvent(onRoute(TraceEvent{PmuTraceEvent{0U}}, firstRoute));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_111"));

  encoder.writeEvent(onRoute(exceptionPacket(75U, ExceptionAction::Entered, 3U), secondRoute));
  encoder.writeEvent(onRoute(softwarePacket(1U, 1U, 'A'), secondRoute));
  encoder.stop();
  EXPECT_FALSE(std::filesystem::exists(outputDirectory / "stream_1"));
  EXPECT_TRUE(std::filesystem::is_regular_file(outputDirectory / "stream_111"));
  ASSERT_NE(encoder.completedMetadata(), nullptr);
  ASSERT_EQ(encoder.completedMetadata()->topology().streams.size(), 1U);
  EXPECT_EQ(encoder.completedMetadata()->topology().streams.front().streamClassId, CtfStreamClassId{111U});
  EXPECT_EQ(encoder.completedMetadata()->topology().clockDomains.front().id, CtfClockDomainId{9U});
  EXPECT_TRUE(encoder.completedMetadata()->observedExceptions(CtfStreamClassId{111U}).empty());
  EXPECT_EQ(readTestTextFile(outputDirectory / "metadata").find("\"External IRQ 59\" = 75"), std::string::npos)
      << "a filtered exception must not leak into metadata when another event later emits the stream";

  const auto zeroCounterDirectory = outputDirectory / "zero-counters";
  std::filesystem::create_directory(zeroCounterDirectory);
  CtfEncoder zeroCounters(formattedEncoderConfig());
  startEncoder(zeroCounters, zeroCounterDirectory);
  zeroCounters.writeEvent(onRoute(TraceEvent{DwtEventTraceEvent{0U}}, firstRoute));
  zeroCounters.writeEvent(onRoute(TraceEvent{PmuTraceEvent{0U}}, firstRoute));
  zeroCounters.stop();
  EXPECT_FALSE(std::filesystem::exists(zeroCounterDirectory / "stream_1"));
  ASSERT_NE(zeroCounters.completedMetadata(), nullptr);
  EXPECT_TRUE(zeroCounters.completedMetadata()->topology().streams.empty());
}

TEST(CtraceUnitTests, testCtfEncoderCreatesFormattedWritersForStatusOnlyOutput)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-formatted-status-only-test");
  const auto& root = temporaryPath.createDirectory();
  const TraceRouteIdentity route{TraceRouteId{4U}, 1U};

  const auto syncDirectory = root / "sync";
  std::filesystem::create_directory(syncDirectory);
  CtfEncoder syncEncoder(formattedEncoderConfig());
  startEncoder(syncEncoder, syncDirectory);
  syncEncoder.writeEvent(onRoute(TraceEvent{SyncTraceEvent{}}, route));
  syncEncoder.stop();
  const auto syncRecords =
      readCtfRecords(syncDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_EQ(syncRecords.size(), 3U);
  EXPECT_EQ(syncRecords[0].payload[0U], CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart));
  EXPECT_EQ(syncRecords[1].id, CtfSchema::value(CtfSchema::EventId::Exception));
  EXPECT_EQ(syncRecords[2].payload[0U], CtfSchema::value(CtfSchema::TraceStatusReason::Resync));

  const auto overflowDirectory = root / "overflow";
  std::filesystem::create_directory(overflowDirectory);
  CtfEncoder overflowEncoder(formattedEncoderConfig(TraceSelection{{"overflow"}, {}}));
  startEncoder(overflowEncoder, overflowDirectory);
  overflowEncoder.writeEvent(onRoute(TraceEvent{OverflowTraceEvent{}}, route));
  overflowEncoder.stop();
  const auto overflowRecords =
      readCtfRecords(overflowDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_EQ(overflowRecords.size(), 1U);
  EXPECT_EQ(overflowRecords.front().payload[0U], CtfSchema::value(CtfSchema::TraceStatusReason::Overflow));

  const auto issueDirectory = root / "issue";
  std::filesystem::create_directory(issueDirectory);
  CtfEncoder issueEncoder(formattedEncoderConfig(TraceSelection{{"error"}, {}}));
  startEncoder(issueEncoder, issueDirectory);
  issueEncoder.writeEvent(onRoute(issuePacket(TraceIssueCode::OpenCsdDecodeError), route));
  issueEncoder.stop();
  const auto issueRecords =
      readCtfRecords(issueDirectory / "stream_1", CtfStreamWriter::EventContextLayout::RouteLabeled);
  ASSERT_EQ(issueRecords.size(), 1U);
  EXPECT_EQ(issueRecords.front().payload[0U], CtfSchema::value(CtfSchema::TraceStatusReason::DecodeError));
}

TEST(CtraceUnitTests, testCtfEncoderWritesConfiguredDwtValueVariantsAndDefault)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-value-variants-test");
  temporaryPath.createDirectory();
  std::vector<CtfSourceDescriptor> sources{
      resolvedDwtSource(0U, "signed", 2U),
      resolvedDwtSource(1U, "float", 4U),
      resolvedDwtSource(2U, "signed", 4U),
  };

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"dwt"}, {}}, sources));
  startEncoder(encoder, temporaryPath.path());

  auto signed16 =
      atCycle(TraceEvent{DwtDataTraceEvent{0U, 2U, 0xff80U, AccessType::Write, DwtAddressFragment{2U, 0x1234U},
                                           DwtAddressFragment{2U, 0x5678U}}},
              10U);
  signed16.quality = TraceQuality{false, true, 0U};
  encoder.writeEvent(signed16);

  encoder.writeEvent(atCycle(TraceEvent{DwtDataTraceEvent{1U, 4U, 0x3f800000U, AccessType::Read}}, 11U));
  encoder.writeEvent(atCycle(TraceEvent{DwtDataTraceEvent{2U, 4U, 0xffffffffU, AccessType::Read}}, 12U));
  encoder.writeEvent(atCycle(TraceEvent{DwtDataTraceEvent{3U, 4U, 0x12345678U, AccessType::Read}}, 13U));

  encoder.stop();
  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  ASSERT_EQ(records.size(), 4U);
  for (const auto& record : records) {
    EXPECT_EQ(record.id, CtfSchema::value(CtfSchema::EventId::DwtValue));
  }
  EXPECT_EQ(records[0].timestamp, 10U);
  EXPECT_EQ(records[0].traceBusId, 0U);
  EXPECT_EQ(records[0].payload[0U], 0U);
  EXPECT_EQ(records[0].payload[1U], CtfSchema::value(CtfSchema::DwtAccess::Write));
  EXPECT_EQ(records[0].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Signed16));
  EXPECT_EQ(readLe16(records[0].payload, 3U), 0xff80U);
  EXPECT_EQ(records[0].payload[5U], CtfSchema::value(CtfSchema::DwtAddressTag::U16));
  EXPECT_EQ(readLe16(records[0].payload, 6U), 0x5678U);
  EXPECT_EQ(records[0].payload[8U], CtfSchema::value(CtfSchema::DwtAddressTag::U16));
  EXPECT_EQ(readLe16(records[0].payload, 9U), 0x1234U);

  EXPECT_EQ(records[1].timestamp, 11U);
  EXPECT_EQ(records[1].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Float32));
  EXPECT_EQ(readLe32(records[1].payload, 3U), 0x3f800000U);
  EXPECT_EQ(records[2].timestamp, 12U);
  EXPECT_EQ(records[2].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Signed32));
  EXPECT_EQ(readLe32(records[2].payload, 3U), 0xffffffffU);
  EXPECT_EQ(records[3].timestamp, 13U);
  EXPECT_EQ(records[3].traceBusId, 0U);
  EXPECT_EQ(records[3].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Unsigned32));
  EXPECT_EQ(readLe32(records[3].payload, 3U), 0x12345678U);
}

TEST(CtraceUnitTests, testCtfEncoderDoesNotBorrowDwtMetadataFromAnotherRoute)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-exact-dwt-route-test");
  temporaryPath.createDirectory();
  const TraceRouteIdentity configured{TraceRouteId{0U}, std::nullopt};
  const TraceRouteIdentity other{TraceRouteId{9U}, std::nullopt};
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"dwt"}, {}},
                                         {resolvedDwtSource(0U, "signed", 1U, configured)}, nullptr,
                                         {configured, other}));
  startEncoder(encoder, temporaryPath.path());
  EXPECT_TRUE(throwsWithMessage(
      [&] { encoder.writeEvent(onRoute(TraceEvent{DwtDataTraceEvent{0U, 1U, 0xffU, AccessType::Read}}, other)); },
      "without an exact runtime stream descriptor"));
  encoder.abort();
}

TEST(CtraceUnitTests, testCtfEncoderReportsRoutedDwtSizeMismatchContext)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-routed-size-warning-test");
  temporaryPath.createDirectory();
  const TraceRouteIdentity route{TraceRouteId{9U}, std::nullopt};
  auto source = resolvedDwtSource(0U, "unsigned", 4U, route);
  CollectingDiagnosticSink diagnostics;
  CtfEncoder encoder(
      legacyEncoderConfig(1000000U, TraceSelection{{"dwt"}, {}}, {source}, &diagnostics, {route}));
  startEncoder(encoder, temporaryPath.path());
  encoder.writeEvent(onRoute(TraceEvent{DwtDataTraceEvent{0U, 1U, 0U, AccessType::Read}}, route));
  encoder.stop();

  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_TRUE(diagnostics.containsContext("channel", "DWT0"));
}

TEST(CtraceUnitTests, testCtfEncoderIgnoresUnselectedStreamTimeAndQuality)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-filtered-stream-state-test");
  temporaryPath.createDirectory();
  const TraceRouteIdentity selectedRoute{TraceRouteId{1U}, std::nullopt};
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{"itm"}, {0U}}, {}, nullptr, {selectedRoute}));
  startEncoder(encoder, temporaryPath.path());

  auto excludedTimestamp = atCycle(onStream(TraceEvent{LocalTimestampTraceEvent{}}, 2U), 900U);
  excludedTimestamp.quality = TraceQuality{false, true, 0U};
  encoder.writeEvent(excludedTimestamp);
  auto excludedOverflow = atCycle(onStream(TraceEvent{OverflowTraceEvent{}}, 2U), 1000U);
  excludedOverflow.quality = TraceQuality{true, false, 99U};
  encoder.writeEvent(excludedOverflow);

  auto selected = atCycle(onRoute(softwarePacket(1U, 1U, 'A'), selectedRoute), 10U);
  selected.quality = TraceQuality{false, true, 0U};
  encoder.writeEvent(selected);
  encoder.stop();

  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  ASSERT_EQ(records.size(), 1U);
  EXPECT_EQ(records[0].id, CtfSchema::value(CtfSchema::EventId::Itm));
  EXPECT_EQ(records[0].timestamp, 10U);
  EXPECT_EQ(records[0].traceBusId, 0U);
  EXPECT_EQ(records[0].payload[3U], CtfSchema::SampleFlagTimestampReliable);
  EXPECT_EQ(readLe32(records[0].payload, 4U), 0U);
}

TEST(CtraceUnitTests, testCtfEncoderDoesNotBootstrapNoBusRouteForAnotherExplicitStream)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-no-unknown-route-bootstrap-test");
  temporaryPath.createDirectory();
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{}, {2U}}));
  startEncoder(encoder, temporaryPath.path());
  encoder.stop();

  EXPECT_TRUE(readCtfRecords(temporaryPath.path() / "stream_0").empty());
}

TEST(CtraceUnitTests, testCtfEncoderDoesNotBootstrapNoBusRouteWhenKnownSourceIsFilteredOut)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-no-source-route-bootstrap-test");
  temporaryPath.createDirectory();
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{}, {1U}}, {resolvedDwtSource(0U, "unsigned", 4U)}));
  startEncoder(encoder, temporaryPath.path());
  encoder.stop();

  EXPECT_TRUE(readCtfRecords(temporaryPath.path() / "stream_0").empty());
}

TEST(CtraceUnitTests, testCtfEncoderStreamSelectionKeepsStartAndResyncContext)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-selected-stream-status-test");
  temporaryPath.createDirectory();
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{}, {0U}}));
  startEncoder(encoder, temporaryPath.path());
  encoder.writeEvent(exceptionPacket(15U, ExceptionAction::Entered, 10U));
  encoder.writeEvent(atCycle(TraceEvent{SyncTraceEvent{}}, 11U));
  encoder.writeEvent(exceptionPacket(54U, ExceptionAction::Entered, 20U));
  encoder.stop();

  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  std::vector<std::uint8_t> statusReasons;
  for (const auto& record : records) {
    EXPECT_EQ(record.traceBusId, 0U);
    if (record.id == CtfSchema::value(CtfSchema::EventId::TraceStatus)) {
      statusReasons.push_back(record.payload[0U]);
    }
  }
  const auto exceptionRecords = timestampedCtfExceptionRecords(records);
  EXPECT_EQ(statusReasons, std::vector<std::uint8_t>({
                               CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart),
                               CtfSchema::value(CtfSchema::TraceStatusReason::Resync),
                           }));
  EXPECT_EQ(exceptionRecords, (std::vector<TimestampedCtfExceptionRecord>({
                                  {0U, {0U, 0U, 1U}},
                                  {10U, {0U, 1U, 1U}},
                                  {10U, {15U, 0U, 0U}},
                                  {20U, {15U, 1U, 1U}},
                                  {20U, {54U, 0U, 0U}},
                              })));
}

TEST(CtraceUnitTests, testCtfEncoderTracksLocalTimeAndUnqualifiedOverflow)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-time-quality-test");
  temporaryPath.createDirectory();
  CtfEncoder encoder(legacyEncoderConfig(1000000U));
  startEncoder(encoder, temporaryPath.path());

  encoder.writeEvent(atCycle(TraceEvent{LocalTimestampTraceEvent{}}, 20U));
  encoder.writeEvent(TraceEvent{OverflowTraceEvent{}});

  auto saturated = atCycle(softwarePacket(1U, 1U, 0U), 21U);
  saturated.quality = TraceQuality{true, true, std::numeric_limits<std::uint64_t>::max()};
  encoder.writeEvent(saturated);
  encoder.stop();
  encoder.stop();

  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  const auto& itm = requireFirstCtfRecord(records, CtfSchema::EventId::Itm, "saturated CTF ITM sample missing");
  EXPECT_EQ(itm.timestamp, 21U);
  EXPECT_EQ(itm.traceBusId, 0U);
  EXPECT_EQ(itm.payload[3U], CtfSchema::SampleFlagOverflow | CtfSchema::SampleFlagTimestampReliable);
  EXPECT_EQ(readLe32(itm.payload, 4U), std::numeric_limits<std::uint32_t>::max());

  const auto overflowStatus = std::find_if(records.begin(), records.end(), [](const CtfRecord& record) {
    return record.id == CtfSchema::value(CtfSchema::EventId::TraceStatus) && record.traceBusId == 0U &&
           record.payload[0U] == CtfSchema::value(CtfSchema::TraceStatusReason::Overflow);
  });
  ASSERT_NE(overflowStatus, records.end());
  EXPECT_EQ(readLe32(overflowStatus->payload, 1U), 1U);
}

TEST(CtraceUnitTests, testCtfEncoderLazilyBootstrapsExactSelectedRoute)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-lazy-route-bootstrap-test");
  temporaryPath.createDirectory();
  const TraceRouteIdentity route{TraceRouteId{9U}, std::nullopt};
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{{}, {0U}}, {}, nullptr, {route}));
  startEncoder(encoder, temporaryPath.path());
  encoder.writeEvent(onRoute(softwarePacket(1U, 1U, 0x5aU), route));
  encoder.stop();

  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  ASSERT_EQ(records.size(), 3U);
  EXPECT_EQ(records[0].id, CtfSchema::value(CtfSchema::EventId::TraceStatus));
  EXPECT_EQ(records[0].traceBusId, 0U);
  EXPECT_EQ(records[0].payload[0U], CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart));
  EXPECT_EQ(records[1].id, CtfSchema::value(CtfSchema::EventId::Exception));
  EXPECT_EQ(records[1].traceBusId, 0U);
  EXPECT_EQ(records[2].id, CtfSchema::value(CtfSchema::EventId::Itm));
  EXPECT_EQ(records[2].traceBusId, 0U);
}

TEST(CtraceUnitTests, testCtfEncoderRejectsConflictingIdentityForSameRouteId)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-route-mismatch-test");
  temporaryPath.createDirectory();
  const TraceRouteIdentity configured{TraceRouteId{4U}, std::nullopt};
  CtfEncoder invalidConfig(
      legacyEncoderConfig(1000000U, TraceSelection{}, {}, nullptr, {configured, {TraceRouteId{4U}, 2U}}));
  EXPECT_THROW(startEncoder(invalidConfig, temporaryPath.path()), std::runtime_error);

  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{}, {}, nullptr, {configured}));
  startEncoder(encoder, temporaryPath.path());
  EXPECT_THROW(encoder.writeEvent(onRoute(softwarePacket(1U), {TraceRouteId{4U}, 2U})), std::runtime_error);
  EXPECT_THROW(encoder.writeEvent(onRoute(softwarePacket(1U), {TraceRouteId{9U}, std::nullopt})), std::runtime_error);
  encoder.abort();

  CtfEncoder lazyEncoder(legacyEncoderConfig(1000000U));
  startEncoder(lazyEncoder, temporaryPath.path());
  EXPECT_TRUE(throwsWithMessage([&] { lazyEncoder.writeEvent(onRoute(softwarePacket(1U), configured)); },
                                "without an exact runtime stream descriptor"));
  lazyEncoder.abort();
}

TEST(CtraceUnitTests, testCtfEncoderBootstrapsOnlyTheMetadataStreamRoute)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-metadata-route-bootstrap-test");
  temporaryPath.createDirectory();
  const TraceRouteIdentity first{TraceRouteId{4U}, std::nullopt};
  const TraceRouteIdentity second{TraceRouteId{9U}, std::nullopt};
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{}, {}, nullptr, {first, second}));
  startEncoder(encoder, temporaryPath.path());
  encoder.stop();

  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  ASSERT_EQ(records.size(), 2U);
  for (const auto& record : records) {
    EXPECT_EQ(record.traceBusId, 0U) << "opaque route ID must not leak into the CTF Trace Bus ID field";
  }
  EXPECT_EQ(records[0].id, CtfSchema::value(CtfSchema::EventId::TraceStatus));
  EXPECT_EQ(records[1].id, CtfSchema::value(CtfSchema::EventId::Exception));
}

TEST(CtraceUnitTests, testCtfEncoderRejectsCataloguedRouteWithoutRuntimeStreamDescriptor)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-no-runtime-stream-test");
  temporaryPath.createDirectory();
  const TraceRouteIdentity first{TraceRouteId{4U}, std::nullopt};
  const TraceRouteIdentity second{TraceRouteId{9U}, std::nullopt};
  CtfEncoder encoder(legacyEncoderConfig(1000000U, TraceSelection{}, {}, nullptr, {first, second}));
  startEncoder(encoder, temporaryPath.path());

  encoder.writeEvent(onRoute(exceptionPacket(15U, ExceptionAction::Entered, 10U), first));
  EXPECT_TRUE(throwsWithMessage(
      [&] { encoder.writeEvent(onRoute(exceptionPacket(54U, ExceptionAction::Entered, 20U), second)); },
      "without an exact runtime stream descriptor"));
  encoder.abort();
}
