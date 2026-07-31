/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

// DWT packet decoding and explicitly deferred DWT feature tests.
#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "csv/CsvRowMapper.hpp"
#include "DwtPacketDecoder.hpp"
#include "TraceEvent.hpp"
#include "TraceSelection.hpp"

#include <cstdint>
#include <optional>

namespace {

DwtPayloadPacket dwtPayload(std::uint8_t discriminator, std::uint8_t size = 0U, std::uint32_t value = 0U,
                            std::uint64_t index = 0U, std::uint8_t traceBusId = 0U, std::uint64_t tcyc = 0U)
{
  return {index, traceBusId, discriminator, size, value, tcyc, {}};
}

} // namespace

TEST(CtraceUnitTests, testDwtPcSampleIsSuppressedUntilDedicatedEventExists)
{
  DwtPacketDecoder decoder;
  auto payload = dwtPayload(2U, 4U, 0x08001234U, 19U, 3U, 949339000U);
  payload.status.timestampReliable = true;

  const auto packets = decoder.decode(payload);
  require(packets.empty(), "DWT PC samples must remain suppressed until their output event is defined");
}

TEST(CtraceUnitTests, testDwtCounterPacketsArePreservedUntilOutputSemanticsExist)
{
  const auto verify = [](std::uint8_t discriminator, const char* selector) {
    DwtPacketDecoder decoder;
    auto payload = dwtPayload(discriminator, 1U, 0x21U, 23U, 4U, 949339100U);
    payload.status.overflow = true;
    payload.status.timestampReliable = false;
    payload.status.overflowCount = 7U;

    const auto packets = decoder.decode(payload);
    require(packets.size() == 1U, "DWT counter packet must be preserved internally");
    const auto& packet = packets.front();
    const auto* event = traceEventPayload<DwtEventTraceEvent>(packet);
    const auto* pmu = traceEventPayload<PmuTraceEvent>(packet);
    require((discriminator == 0U && event != nullptr && pmu == nullptr) ||
                (discriminator == 3U && event == nullptr && pmu != nullptr),
            "DWT counter semantic event type mismatch");
    const auto actualDiscriminator = event != nullptr ? event->discriminator : pmu->discriminator;
    const auto actualSize = event != nullptr ? event->size : pmu->size;
    const auto actualValue = event != nullptr ? event->value : pmu->value;
    require(actualDiscriminator == discriminator, "DWT counter discriminator mismatch");
    require(actualSize == 1U && actualValue == 0x21U, "DWT counter payload mismatch");
    require(packet.index == 23U && packet.traceBusId == 4U, "DWT counter identity mismatch");
    require(packet.tcyc == 949339100U, "DWT counter timestamp mismatch");
    require(packet.quality.has_value() && packet.quality->overflow, "DWT counter overflow status mismatch");
    require(packet.quality->overflowCount == 7U, "DWT counter overflow count mismatch");
    require(!traceEventType(packet).has_value(), "unimplemented DWT counter must not expose an output type");
    require(!traceEventSelectedForOutput(packet, TraceSelection{}),
            "DWT counter packet must remain disabled by default");
    require(!traceEventSelectedForOutput(packet, TraceSelection{{selector}, {}}),
            "DWT counter selector must remain disabled until output semantics exist");
  };

  verify(0U, "event");
  verify(3U, "pmu");
}

TEST(CtraceUnitTests, testDwtPacketDecoderRejectsReservedExceptionAction)
{
  DwtPacketDecoder decoder;
  auto payload = dwtPayload(1U, 2U, 11U, 17U, 3U, 1234U);
  payload.status.timestampReliable = true;

  const auto packets = decoder.decode(payload);
  require(packets.size() == 1U, "DwtPacketDecoder reserved exception action packet count mismatch");
  const auto* issue = traceEventPayload<TraceIssueEvent>(packets[0]);
  require(issue != nullptr, "DwtPacketDecoder reserved exception action should emit only an error");
  require(issue->code == "invalid-exception-action", "DwtPacketDecoder reserved exception action code mismatch");
  require(issue->message == "invalid exception action 0x0 for exception 11",
          "DwtPacketDecoder reserved exception action message mismatch");
  require(packets[0].index == 17U && packets[0].traceBusId == 3U,
          "DwtPacketDecoder reserved exception action identity mismatch");
  require(packets[0].tcyc.has_value() && *packets[0].tcyc == 1234U,
          "DwtPacketDecoder reserved exception action timestamp mismatch");
  require(CsvRowMapper::row(packets[0]) == "1234,3,error,,,,,invalid exception action 0x0 for exception 11",
          "DwtPacketDecoder reserved exception error CSV mismatch");
}

TEST(CtraceUnitTests, testDwtPacketDecoderFlushesPendingEventsInRawOrder)
{
  DwtPacketDecoder decoder;

  const auto comparatorThree = dwtPayload(14U, 4U, 0x08003000U, 10U, 3U);
  require(decoder.decode(comparatorThree).empty(), "an incomplete DWT comparator-three event must remain pending");

  const auto comparatorOne = dwtPayload(10U, 4U, 0x08001000U, 10U, 3U);
  require(decoder.decode(comparatorOne).empty(), "an incomplete DWT comparator-one event must remain pending");

  const auto comparatorZero = dwtPayload(8U, 4U, 0x08000000U, 20U, 4U);
  require(decoder.decode(comparatorZero).empty(), "an incomplete DWT comparator-zero event must remain pending");

  const auto packets = decoder.flush({}, 123U);
  require(packets.size() == 3U, "DWT flush must emit all pending comparator events");
  require(packets[0].index == 10U && packets[1].index == 10U && packets[2].index == 20U,
          "DWT flush must preserve raw-stream order across comparators");
  require(packets[0].traceBusId == 3U && packets[1].traceBusId == 3U && packets[2].traceBusId == 4U,
          "DWT flush must preserve the identity of each pending event");
  const auto* first = traceEventPayload<DwtAddressTraceEvent>(packets[0]);
  const auto* second = traceEventPayload<DwtAddressTraceEvent>(packets[1]);
  const auto* third = traceEventPayload<DwtAddressTraceEvent>(packets[2]);
  require(first != nullptr && first->comparator == 1U && second != nullptr && second->comparator == 3U &&
              third != nullptr && third->comparator == 0U,
          "DWT raw-order regression must not rewrite comparator identities");
}

TEST(CtraceUnitTests, testDwtPacketDecoderPreservesRepeatedAddressFragments)
{
  const auto verify = [](std::uint8_t discriminator, std::uint32_t firstValue, std::uint32_t secondValue) {
    DwtPacketDecoder decoder;
    const auto size = static_cast<std::uint8_t>((discriminator & 1U) == 0U ? 4U : 2U);
    const auto first = dwtPayload(discriminator, size, firstValue, 10U, 3U);
    require(decoder.decode(first).empty(), "first DWT address fragment must remain pending");

    auto packets = decoder.decode(dwtPayload(discriminator, size, secondValue, 20U, 4U, 200U));
    require(packets.size() == 1U, "a repeated DWT address fragment must flush its predecessor");
    const auto* firstAddress = traceEventPayload<DwtAddressTraceEvent>(packets.front());
    require(firstAddress != nullptr && packets.front().index == 10U && packets.front().traceBusId == 3U,
            "the first repeated DWT address fragment lost its identity");
    const auto firstPc = dwtAddressPc(*firstAddress);
    const auto firstOffset = dwtAddressOffset(*firstAddress);

    packets = decoder.flush({}, 300U);
    require(packets.size() == 1U, "the second DWT address fragment must remain available");
    const auto* secondAddress = traceEventPayload<DwtAddressTraceEvent>(packets.front());
    require(secondAddress != nullptr && packets.front().index == 20U && packets.front().traceBusId == 4U,
            "the second repeated DWT address fragment lost its identity");

    if (discriminator == 8U) {
      require(firstPc == std::optional<std::uint32_t>(firstValue) &&
                  dwtAddressPc(*secondAddress) == std::optional<std::uint32_t>(secondValue),
              "repeated DWT PC fragments were overwritten");
    } else {
      require(firstOffset == std::optional<std::uint32_t>(firstValue) &&
                  dwtAddressOffset(*secondAddress) == std::optional<std::uint32_t>(secondValue),
              "repeated DWT offset fragments were overwritten");
    }
  };

  verify(8U, 0x08001000U, 0x08002000U);
  verify(9U, 0x1000U, 0x2000U);
}

TEST(CtraceUnitTests, testDwtPacketDecoderRejectsUnsupportedAddressWidths)
{
  const auto verify = [](std::uint8_t discriminator, std::uint8_t size) {
    DwtPacketDecoder decoder;
    auto payload = dwtPayload(discriminator, size, 0x12345678U, 17U, 3U, 99U);
    payload.status.timestampReliable = true;

    const auto packets = decoder.decode(payload);
    require(packets.size() == 1U, "unsupported DWT address width must emit one error");
    const auto* issue = traceEventPayload<TraceIssueEvent>(packets.front());
    require(issue != nullptr && issue->code == "unsupported-dwt-address-payload" &&
                issue->severity == TraceIssueSeverity::Error,
            "unsupported DWT address width diagnostic mismatch");
    require(packets.front().index == 17U && packets.front().traceBusId == 3U &&
                packets.front().tcyc == std::optional<std::uint64_t>(99U),
            "unsupported DWT address width diagnostic lost packet identity");
  };

  verify(8U, 1U);
  verify(9U, 4U);
}

TEST(CtraceUnitTests, testDwtPacketDecoderMapsAllExceptionActions)
{
  const auto verify = [](std::uint32_t actionCode, ExceptionAction expected) {
    DwtPacketDecoder decoder;
    auto payload = dwtPayload(1U, 2U, (actionCode << 12U) | 11U, 17U, 3U, 1234U);
    payload.status.timestampReliable = true;

    const auto packets = decoder.decode(payload);
    ASSERT_EQ(packets.size(), 1U);
    const auto* exception = traceEventPayload<ExceptionTraceEvent>(packets.front());
    ASSERT_NE(exception, nullptr);
    EXPECT_EQ(exception->number, 11U);
    EXPECT_EQ(exception->action, expected);
  };

  verify(1U, ExceptionAction::Entered);
  verify(2U, ExceptionAction::Exited);
  verify(3U, ExceptionAction::Returned);
}

TEST(CtraceUnitTests, testDwtPacketDecoderFlushesPendingTraceForUnknownSource)
{
  DwtPacketDecoder decoder;
  const auto address = dwtPayload(8U, 4U, 0x08001234U, 10U);
  EXPECT_TRUE(decoder.decode(address).empty());

  auto unknown = dwtPayload(7U, 0U, 0U, 11U, 0U, 42U);
  const auto packets = decoder.decode(unknown);
  ASSERT_EQ(packets.size(), 1U);
  EXPECT_NE(traceEventPayload<DwtAddressTraceEvent>(packets.front()), nullptr);
  EXPECT_EQ(packets.front().tcyc, std::optional<std::uint64_t>(42U));

  decoder.reset();
  EXPECT_TRUE(decoder.flush({}, 43U).empty());

  unknown.discriminator = 24U;
  EXPECT_TRUE(decoder.decode(unknown).empty());
}

TEST(CtraceUnitTests, testDwtPacketDecoderCombinesPcOffsetAndValue)
{
  DwtPacketDecoder decoder;

  auto pc = dwtPayload(8U, 4U, 0x08001234U, 10U);
  pc.status.timestampReliable = true;
  EXPECT_TRUE(decoder.decode(pc).empty());

  auto offset = pc;
  offset.index = 11U;
  offset.discriminator = 9U;
  offset.size = 2U;
  offset.value = 0x20U;
  EXPECT_TRUE(decoder.decode(offset).empty());

  auto value = pc;
  value.index = 12U;
  value.discriminator = 16U;
  value.size = 4U;
  value.value = 0xabcdef01U;
  const auto packets = decoder.decode(value);
  ASSERT_EQ(packets.size(), 1U);
  const auto* data = traceEventPayload<DwtDataTraceEvent>(packets.front());
  ASSERT_NE(data, nullptr);
  EXPECT_EQ(data->access, AccessType::Read);
  EXPECT_EQ(data->pc, std::optional<std::uint32_t>(0x08001234U));
  EXPECT_EQ(data->addressLo16, std::optional<std::uint32_t>(0x20U));

  DwtPacketDecoder addressDecoder;
  EXPECT_TRUE(addressDecoder.decode(pc).empty());
  EXPECT_TRUE(addressDecoder.decode(offset).empty());
  const auto addresses = addressDecoder.flush({}, 100U);
  ASSERT_EQ(addresses.size(), 1U);
  const auto* address = traceEventPayload<DwtAddressTraceEvent>(addresses.front());
  ASSERT_NE(address, nullptr);
  EXPECT_EQ(dwtAddressPc(*address), std::optional<std::uint32_t>(0x08001234U));
  EXPECT_EQ(dwtAddressOffset(*address), std::optional<std::uint32_t>(0x20U));
}
