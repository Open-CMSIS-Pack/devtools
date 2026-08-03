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
#include "TestPath.h"
#include "TraceEvent.h"
#include "TraceSelection.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using CtfTestSupport::CtfRecord;
using CtfTestSupport::kCtfEventOffset;
using CtfTestSupport::kCtfPacketContextSize;
using CtfTestSupport::kCtfPacketHeaderSize;
using CtfTestSupport::kCtfPacketSize;
using CtfTestSupport::readCtfRecords;
using CtfTestSupport::readLe16;
using CtfTestSupport::readLe32;
using CtfTestSupport::requireFirstCtfRecord;
using CtfTestSupport::requireSingleItmEvent;

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

static ResolvedTraceSource resolvedDwtSource(std::uint32_t comparator, std::uint8_t traceBusId, std::string type,
                                             std::uint8_t size)
{
  ResolvedTraceSource source;
  source.type = "dwt";
  source.source = comparator;
  source.traceBusId = traceBusId;
  source.valueType = std::move(type);
  source.valueSize = size;
  return source;
}

TEST(CtraceUnitTests, testCtfEncoderWritesOnlyIntoProvidedDirectory)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-boundary-test");
  const auto& root = temporaryPath.path();
  const auto missingDirectory = root / "missing";
  const auto outputDirectory = root / "provided";

  CtfEncoder encoder(CtfEncoderConfig{
      1000000U,
      TraceSelection{{"itm"}, {}},
      {},
  });
  const auto rejectedMissingDirectory = throwsException([&] { encoder.start(missingDirectory); });
  require(rejectedMissingDirectory && !std::filesystem::exists(missingDirectory),
          "CtfEncoder must not create or own its output directory");

  std::filesystem::create_directories(outputDirectory);
  encoder.start(outputDirectory);
  encoder.writeEvent(atCycle(softwarePacket(1U, 1U, 'A'), 10U));
  encoder.stop();
  require(std::filesystem::is_regular_file(outputDirectory / "metadata") &&
              std::filesystem::is_regular_file(outputDirectory / "stream_0"),
          "CtfEncoder must encode metadata and stream data into the provided directory");
  requireSingleItmEvent(outputDirectory / "stream_0", 1U, "CtfEncoder encoded an unexpected ITM event");

  encoder.abort();
  require(std::filesystem::is_regular_file(outputDirectory / "metadata"),
          "CtfEncoder abort must not delete a directory owned by its caller");
}

TEST(CtraceUnitTests, testCtfEncoderPacketBoundaryAndUuid)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-packet-boundary-test");
  const auto& outputDirectory = temporaryPath.createDirectory();

  CtfEncoder encoder(CtfEncoderConfig{
      1000000U,
      TraceSelection{{"itm"}, {}},
      {},
  });
  encoder.start(outputDirectory);

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
  require(stream.size() == kCtfPacketSize * 2U, "CTF packet rollover must emit two complete 64-KiB packets");
  require(readLe32(stream, 0U) == ctfMagic && readLe32(stream, kCtfPacketSize) == ctfMagic,
          "CTF packet rollover emitted an invalid packet header");
  require(readLe32(stream, kCtfPacketHeaderSize) == kCtfPacketSize * 8U &&
              readLe32(stream, kCtfPacketHeaderSize + 4U) == firstContentSize * 8U,
          "first CTF packet content size mismatch at rollover");
  require(readLe32(stream, kCtfPacketSize + kCtfPacketHeaderSize + 4U) == (kCtfEventOffset + eventSize) * 8U,
          "second CTF packet content size mismatch");
  require(readLe32(stream, kCtfPacketHeaderSize + 28U) == 0U &&
              readLe32(stream, kCtfPacketSize + kCtfPacketHeaderSize + 28U) == 1U,
          "CTF packet sequence must advance across a 64-KiB boundary");
  require(std::equal(stream.begin() + 4U, stream.begin() + 20U, stream.begin() + kCtfPacketSize + 4U),
          "CTF packet UUID must remain stable across packet rollover");

  const auto uuid = formatCtfUuid(stream, 4U);
  require(metadata.find("uuid = \"" + uuid + "\";") != std::string::npos,
          "CTF metadata UUID must match the binary packet UUID");
  require((stream[4U + 6U] & 0xf0U) == 0x40U && (stream[4U + 8U] & 0xc0U) == 0x80U,
          "CTF UUID must use RFC 4122 version-4 and variant bits");

  encoder.abort();
}

TEST(CtraceUnitTests, testCtfEncoderDwtAddressEncoding)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-encoder-dwt-address-test");
  const auto& outputDirectory = temporaryPath.createDirectory();

  CtfEncoder encoder(CtfEncoderConfig{
      1000000U,
      TraceSelection{{"dwt"}, {}},
      {},
  });
  encoder.start(outputDirectory);
  encoder.writeEvent(atCycle(TraceEvent{DwtAddressTraceEvent{
                                 3U,
                                 DwtPcAndOffsetTraceLocation{0x12345678U, 0x0000abcdU},
                             }},
                             99U));
  encoder.stop();

  const auto records = readCtfRecords(outputDirectory / "stream_0");
  ASSERT_EQ(records.size(), 1U);
  const auto& record = records.front();
  require(record.id == CtfSchema::value(CtfSchema::EventId::DwtAddress), "CTF DWT address event ID mismatch");
  require(record.timestamp == 99U, "CTF DWT address timestamp mismatch");
  require(record.payload.size() == 14U, "CTF DWT address event payload size mismatch");
  require(record.payload[0U] == 3U && record.payload[1U] == 1U && record.payload[2U] == 1U,
          "CTF DWT address comparator or presence flags mismatch");
  require(readLe32(record.payload, 3U) == 0x12345678U && readLe16(record.payload, 7U) == 0xabcdU,
          "CTF DWT PC/address payload mismatch");

  encoder.abort();
}

TEST(CtraceUnitTests, testCtfEncoderRejectsInvalidClockAndPayloadMetadata)
{
  EXPECT_THROW((void)CtfEncoder(CtfEncoderConfig{}), std::invalid_argument);

  const TemporaryTestPath temporaryPath("ctrace-ctf-invalid-payload-test");
  temporaryPath.createDirectory();
  CtfEncoder invalidItm(CtfEncoderConfig{1000000U, TraceSelection{{"itm"}, {}}, {}});
  invalidItm.stop();
  invalidItm.writeEvent(softwarePacket(1U));
  invalidItm.start(temporaryPath.path());
  EXPECT_THROW(invalidItm.writeEvent(softwarePacket(1U, 3U, 0U)), std::runtime_error);
  invalidItm.abort();

  CtfEncoder invalidDwt(CtfEncoderConfig{
      1000000U,
      TraceSelection{{"dwt"}, {}},
      {resolvedDwtSource(0U, 1U, "unsupported", 3U)},
  });
  invalidDwt.start(temporaryPath.path());
  EXPECT_THROW(invalidDwt.writeEvent(onStream(TraceEvent{DwtDataTraceEvent{0U, 1U, 0U, AccessType::Read}}, 1U)),
               std::runtime_error);
  invalidDwt.abort();
}

TEST(CtraceUnitTests, testCtfEncoderWritesAllDwtValueVariants)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-value-variants-test");
  temporaryPath.createDirectory();
  std::vector<ResolvedTraceSource> sources{
      resolvedDwtSource(0U, 1U, "signed int", 2U), resolvedDwtSource(1U, 1U, "float", 4U),
      resolvedDwtSource(2U, 1U, "signed int", 4U), resolvedDwtSource(3U, 7U, "unsigned int", 1U),
      resolvedDwtSource(4U, 1U, "signed int", 1U), resolvedDwtSource(4U, 2U, "signed int", 1U),
  };
  sources.push_back(resolvedDwtSource(99U, 7U, "unsigned int", 1U));

  CtfEncoder encoder(CtfEncoderConfig{1000000U, TraceSelection{{"dwt"}, {}}, sources});
  encoder.start(temporaryPath.path());

  auto signed16 = atCycle(
      onStream(TraceEvent{DwtDataTraceEvent{0U, 2U, 0xff80U, AccessType::Write, 0x1234U, 0x08000000U}}, 1U), 10U);
  signed16.quality = TraceQuality{false, true, 0U};
  encoder.writeEvent(signed16);

  encoder.writeEvent(atCycle(onStream(TraceEvent{DwtDataTraceEvent{1U, 4U, 0x3f800000U, AccessType::Read}}, 1U), 11U));
  encoder.writeEvent(atCycle(onStream(TraceEvent{DwtDataTraceEvent{2U, 4U, 0xffffffffU, AccessType::Read}}, 1U), 12U));
  encoder.writeEvent(atCycle(TraceEvent{DwtDataTraceEvent{3U, 1U, 0x12U, AccessType::Read}}, 13U));
  encoder.writeEvent(atCycle(TraceEvent{DwtDataTraceEvent{4U, 1U, 0xffU, AccessType::Read}}, 14U));
  encoder.writeEvent(atCycle(TraceEvent{DwtDataTraceEvent{6U, 4U, 0x12345678U, AccessType::Read}}, 15U));

  encoder.stop();
  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  ASSERT_EQ(records.size(), 6U);
  for (const auto& record : records) {
    EXPECT_EQ(record.id, CtfSchema::value(CtfSchema::EventId::DwtValue));
  }
  EXPECT_EQ(records[0].timestamp, 10U);
  EXPECT_EQ(records[0].traceBusId, 1U);
  EXPECT_EQ(records[0].payload[0U], 0U);
  EXPECT_EQ(records[0].payload[1U], CtfSchema::value(CtfSchema::DwtAccess::Write));
  EXPECT_EQ(records[0].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Signed16));
  EXPECT_EQ(readLe16(records[0].payload, 3U), 0xff80U);
  EXPECT_EQ(records[0].payload[5U], 1U);
  EXPECT_EQ(readLe32(records[0].payload, 6U), 0x08000000U);
  EXPECT_EQ(records[0].payload[10U], 1U);
  EXPECT_EQ(readLe16(records[0].payload, 11U), 0x1234U);

  EXPECT_EQ(records[1].timestamp, 11U);
  EXPECT_EQ(records[1].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Float32));
  EXPECT_EQ(readLe32(records[1].payload, 3U), 0x3f800000U);
  EXPECT_EQ(records[2].timestamp, 12U);
  EXPECT_EQ(records[2].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Signed32));
  EXPECT_EQ(readLe32(records[2].payload, 3U), 0xffffffffU);
  EXPECT_EQ(records[3].timestamp, 13U);
  EXPECT_EQ(records[3].traceBusId, 0U);
  EXPECT_EQ(records[3].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Unsigned8));
  EXPECT_EQ(records[3].payload[3U], 0x12U);
  // The unformatted stream must retain equivalent metadata from both configured routes.
  EXPECT_EQ(records[4].timestamp, 14U);
  EXPECT_EQ(records[4].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Signed8));
  EXPECT_EQ(records[4].payload[3U], 0xffU);
  EXPECT_EQ(records[5].timestamp, 15U);
  EXPECT_EQ(records[5].payload[2U], CtfSchema::value(CtfSchema::ValueTag::Unsigned32));
  EXPECT_EQ(readLe32(records[5].payload, 3U), 0x12345678U);
}

TEST(CtraceUnitTests, testCtfEncoderRejectsConflictingUnformattedDwtRoutes)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-conflicting-dwt-routes-test");
  temporaryPath.createDirectory();
  const std::vector<ResolvedTraceSource> sources{
      resolvedDwtSource(0U, 1U, "signed int", 1U),
      resolvedDwtSource(0U, 2U, "float", 4U),
  };
  CtfEncoder encoder(CtfEncoderConfig{1000000U, TraceSelection{{"dwt"}, {}}, sources});
  encoder.start(temporaryPath.path());
  EXPECT_TRUE(
      throwsWithMessage([&] { encoder.writeEvent(TraceEvent{DwtDataTraceEvent{0U, 1U, 0xffU, AccessType::Read}}); },
                        "conflicting metadata for unformatted dwt source 0"));
  encoder.abort();
}

TEST(CtraceUnitTests, testCtfEncoderIgnoresUnselectedStreamTimeAndQuality)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-filtered-stream-state-test");
  temporaryPath.createDirectory();
  CtfEncoder encoder(CtfEncoderConfig{1000000U, TraceSelection{{"itm"}, {1U}}, {}});
  encoder.start(temporaryPath.path());

  auto excludedTimestamp = atCycle(onStream(TraceEvent{LocalTimestampTraceEvent{}}, 2U), 900U);
  excludedTimestamp.quality = TraceQuality{false, true, 0U};
  encoder.writeEvent(excludedTimestamp);
  auto excludedOverflow = atCycle(onStream(TraceEvent{OverflowTraceEvent{}}, 2U), 1000U);
  excludedOverflow.quality = TraceQuality{true, false, 99U};
  encoder.writeEvent(excludedOverflow);

  auto selected = atCycle(onStream(softwarePacket(1U, 1U, 'A'), 1U), 10U);
  selected.quality = TraceQuality{false, true, 0U};
  encoder.writeEvent(selected);
  encoder.stop();

  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  ASSERT_EQ(records.size(), 1U);
  EXPECT_EQ(records[0].id, CtfSchema::value(CtfSchema::EventId::Itm));
  EXPECT_EQ(records[0].timestamp, 10U);
  EXPECT_EQ(records[0].traceBusId, 1U);
  EXPECT_EQ(records[0].payload[3U], CtfSchema::SampleFlagTimestampReliable);
  EXPECT_EQ(readLe32(records[0].payload, 4U), 0U);
}

TEST(CtraceUnitTests, testCtfEncoderStreamSelectionKeepsStartAndResyncContext)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-selected-stream-status-test");
  temporaryPath.createDirectory();
  CtfEncoder encoder(CtfEncoderConfig{1000000U, TraceSelection{{}, {3U}}, {}});
  encoder.start(temporaryPath.path());
  encoder.writeEvent(onStream(exceptionPacket(15U, ExceptionAction::Entered, 10U), 3U));
  encoder.writeEvent(atCycle(onStream(TraceEvent{SyncTraceEvent{}}, 3U), 11U));
  encoder.writeEvent(onStream(exceptionPacket(54U, ExceptionAction::Entered, 20U), 3U));
  encoder.stop();

  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  std::vector<std::uint8_t> statusReasons;
  std::vector<std::pair<std::uint64_t, std::string>> exceptionRecords;
  for (const auto& record : records) {
    EXPECT_EQ(record.traceBusId, 3U);
    if (record.id == CtfSchema::value(CtfSchema::EventId::TraceStatus)) {
      statusReasons.push_back(record.payload[0U]);
    } else if (record.id == CtfSchema::value(CtfSchema::EventId::Exception)) {
      exceptionRecords.emplace_back(record.timestamp, std::to_string(readLe16(record.payload, 0U)) + ":" +
                                                          std::to_string(record.payload[2U]));
    }
  }
  EXPECT_EQ(statusReasons, std::vector<std::uint8_t>({
                               CtfSchema::value(CtfSchema::TraceStatusReason::TraceStart),
                               CtfSchema::value(CtfSchema::TraceStatusReason::Resync),
                           }));
  EXPECT_EQ(exceptionRecords, (std::vector<std::pair<std::uint64_t, std::string>>({
                                  {0U, "0:1"},
                                  {10U, "0:2"},
                                  {10U, "15:1"},
                                  {20U, "15:2"},
                                  {20U, "54:1"},
                              })));
}

TEST(CtraceUnitTests, testCtfEncoderTracksLocalTimeAndUnqualifiedOverflow)
{
  const TemporaryTestPath temporaryPath("ctrace-ctf-time-quality-test");
  temporaryPath.createDirectory();
  CtfEncoder encoder(CtfEncoderConfig{1000000U, TraceSelection{}, {}});
  encoder.start(temporaryPath.path());

  encoder.writeEvent(atCycle(onStream(TraceEvent{LocalTimestampTraceEvent{}}, 3U), 20U));
  encoder.writeEvent(onStream(TraceEvent{OverflowTraceEvent{}}, 3U));

  auto saturated = atCycle(onStream(softwarePacket(1U, 1U, 0U), 3U), 21U);
  saturated.quality = TraceQuality{true, true, std::numeric_limits<std::uint64_t>::max()};
  encoder.writeEvent(saturated);
  encoder.stop();
  encoder.stop();

  const auto records = readCtfRecords(temporaryPath.path() / "stream_0");
  const auto& itm = requireFirstCtfRecord(records, CtfSchema::EventId::Itm, "saturated CTF ITM sample missing");
  EXPECT_EQ(itm.timestamp, 21U);
  EXPECT_EQ(itm.traceBusId, 3U);
  EXPECT_EQ(itm.payload[3U], CtfSchema::SampleFlagOverflow | CtfSchema::SampleFlagTimestampReliable);
  EXPECT_EQ(readLe32(itm.payload, 4U), std::numeric_limits<std::uint32_t>::max());

  const auto overflowStatus = std::find_if(records.begin(), records.end(), [](const CtfRecord& record) {
    return record.id == CtfSchema::value(CtfSchema::EventId::TraceStatus) && record.traceBusId == 3U &&
           record.payload[0U] == CtfSchema::value(CtfSchema::TraceStatusReason::Overflow);
  });
  ASSERT_NE(overflowStatus, records.end());
  EXPECT_EQ(readLe32(overflowStatus->payload, 1U), 1U);
}
