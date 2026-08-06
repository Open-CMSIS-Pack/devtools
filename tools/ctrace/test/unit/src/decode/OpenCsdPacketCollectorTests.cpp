/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "csv/CsvRowMapper.h"
#include "OpenCsdPacketCollector.h"
#include "OpenCsdTraceElement.h"
#include "TraceEvent.h"
#include "common/trc_gen_elem.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"
#include "opencsd/trc_gen_elem_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

using OpenCsdTestSupport::CollectingOpenCsdElementSink;

/** @brief Collects one element and then simulates a downstream failure. */
class ThrowingTraceElementSink final : public CollectingOpenCsdElementSink {
public:
  /** @brief Stores the element before throwing the synthetic failure. */
  void append(OpenCsdTraceElement element) override
  {
    CollectingOpenCsdElementSink::append(std::move(element));
    throw std::runtime_error("synthetic output failure");
  }
};

/** @brief Creates a generic OpenCSD ITM element for callback tests. */
static OcsdTraceElement itmElement(swt_itm_type type, std::uint8_t source = 0U, std::uint8_t size = 0U,
                                   std::uint32_t value = 0U, bool overflow = false)
{
  OcsdTraceElement element;
  element.setType(OCSD_GEN_TRC_ELEM_ITMTRACE);
  swt_itm_info info{};
  info.pkt_type = type;
  info.payload_src_id = source;
  info.payload_size = size;
  info.value = value;
  info.overflow = overflow;
  element.setSWT_ITMInfo(info);
  return element;
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorUsesReconstructedGlobalTimestamp)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(sink);

  auto globalTimestamp = itmElement(TS_GLOBAL);
  globalTimestamp.setTS(0xfedcba9876543210ULL, true);

  require(collector.TraceElemIn(42U, 7U, globalTimestamp) == OCSD_RESP_CONT,
          "OpenCSD global timestamp collection failed");
  require(sink.elements().size() == 1U, "reconstructed global timestamp element missing");
  const auto element = sink.elements().front();
  require(element.kind == OpenCsdTraceElement::Kind::GlobalTimestamp, "reconstructed global timestamp kind mismatch");
  require(element.sourceIndex == 42U, "reconstructed global timestamp source index mismatch");
  require(element.traceBusId == 7U, "OpenCSD Trace Bus ID was not preserved");
  require(element.timestampValue == 0xfedcba9876543210ULL, "reconstructed global timestamp value mismatch");
  require(element.clockChange, "reconstructed global timestamp clock-change flag missing");

  require(collector.TraceElemIn(43U, 0xffU, globalTimestamp) == OCSD_RESP_CONT,
          "OpenCSD global timestamp collection with missing source ID failed");
  require(sink.elements().back().traceBusId == 0U, "an unavailable OpenCSD Trace Bus ID must fall back to zero");

  ItmTrcPacket rawGts1;
  rawGts1.setPktType(ITM_PKT_TS_GLOBAL_1);
  rawGts1.setValue(0x123456U, 4U);
  collector.RawPacketDataMon(OCSD_OP_DATA, 43U, &rawGts1, 0U, nullptr);

  ItmTrcPacket rawGts2;
  rawGts2.setPktType(ITM_PKT_TS_GLOBAL_2);
  rawGts2.setExtValue(0x123456789ULL);
  collector.RawPacketDataMon(OCSD_OP_DATA, 44U, &rawGts2, 0U, nullptr);
  require(sink.elements().size() == 2U, "raw GTS fragments must not be published as independent global timestamps");

  TraceEvent packet{GlobalTimestampTraceEvent{element.timestampValue, false}};
  require(CsvRowMapper::row(packet) == std::to_string(element.timestampValue) + ",,global_ts,,,,,",
          "CSV must preserve all 64 global timestamp bits in the cycles column");
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorMapsLocalTimestampRelations)
{
  const std::array<std::pair<swt_itm_type, LocalTimestampRelation>, 4> cases{{
      {TS_SYNC, LocalTimestampRelation::Synchronous},
      {TS_DELAY, LocalTimestampRelation::TimestampDelayed},
      {TS_PKT_DELAY, LocalTimestampRelation::PayloadDelayed},
      {TS_PKT_TS_DELAY, LocalTimestampRelation::TimestampAndPayloadDelayed},
  }};

  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(sink);
  for (std::size_t index = 0; index < cases.size(); ++index) {
    auto timestamp = itmElement(cases[index].first);
    timestamp.setTS(100U + index, false);

    require(collector.TraceElemIn(index, 0U, timestamp) == OCSD_RESP_CONT, "OpenCSD local timestamp collection failed");
  }

  require(sink.elements().size() == cases.size(), "local timestamp relation count mismatch");
  for (std::size_t index = 0; index < cases.size(); ++index) {
    require(sink.elements()[index].kind == OpenCsdTraceElement::Kind::LocalTimestamp, "local timestamp kind mismatch");
    require(sink.elements()[index].timestampRelation == cases[index].second, "local timestamp relation mismatch");
  }
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorMapsPayloadAndRawPacketKinds)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(sink);

  const auto software = itmElement(SWIT_PAYLOAD, 7U, 4U, 0x12345678U, true);
  EXPECT_EQ(collector.TraceElemIn(10U, 3U, software), OCSD_RESP_CONT);
  ASSERT_EQ(sink.elements().size(), 1U);
  EXPECT_EQ(sink.elements().back().kind, OpenCsdTraceElement::Kind::Software);
  EXPECT_EQ(sink.elements().back().channel, 7U);
  EXPECT_EQ(sink.elements().back().size, 4U);
  EXPECT_EQ(sink.elements().back().value, 0x12345678U);
  EXPECT_TRUE(sink.elements().back().overflow);

  const auto hardware = itmElement(DWT_PAYLOAD, 9U, 2U, 0x1234U);
  EXPECT_EQ(collector.TraceElemIn(11U, 4U, hardware), OCSD_RESP_CONT);
  ASSERT_EQ(sink.elements().size(), 2U);
  EXPECT_EQ(sink.elements().back().kind, OpenCsdTraceElement::Kind::Hardware);
  EXPECT_EQ(sink.elements().back().discriminator, 9U);

  OcsdTraceElement unrelated;
  unrelated.setType(OCSD_GEN_TRC_ELEM_NO_SYNC);
  EXPECT_EQ(collector.TraceElemIn(12U, 5U, unrelated), OCSD_RESP_CONT);
  EXPECT_EQ(sink.elements().size(), 2U);

  const auto unknown = itmElement(static_cast<swt_itm_type>(7));
  EXPECT_EQ(collector.TraceElemIn(12U, 5U, unknown), OCSD_RESP_CONT);
  EXPECT_EQ(sink.elements().size(), 2U);

  collector.RawPacketDataMon(OCSD_OP_DATA, 13U, nullptr, 0U, nullptr);
  ItmTrcPacket packet;
  packet.setPktType(ITM_PKT_ASYNC);
  collector.RawPacketDataMon(OCSD_OP_RESET, 14U, &packet, 0U, nullptr);
  EXPECT_EQ(sink.elements().size(), 2U);

  /** @brief Describes one raw-packet callback mapping test case. */
  struct RawPacketCase {
    ocsd_itm_pkt_type type;
    ocsd_datapath_op_t operation;
    OpenCsdTraceElement::Kind kind;
    const char* errorMessage;
    std::optional<TraceIssueCode> issueCode;
  };
  constexpr RawPacketCase rawPacketCases[]{
      {ITM_PKT_OVERFLOW, OCSD_OP_DATA, OpenCsdTraceElement::Kind::Overflow, "", std::nullopt},
      {ITM_PKT_RESERVED, OCSD_OP_DATA, OpenCsdTraceElement::Kind::Error, "Reserved ITM packet",
       TraceIssueCode::OpenCsdDecodeError},
      {ITM_PKT_BAD_SEQUENCE, OCSD_OP_DATA, OpenCsdTraceElement::Kind::Error, "Bad ITM packet sequence",
       TraceIssueCode::OpenCsdDecodeError},
      {ITM_PKT_INCOMPLETE_EOT, OCSD_OP_EOT, OpenCsdTraceElement::Kind::Error, "incomplete ITM packet at end of input",
       TraceIssueCode::OpenCsdIncompleteTail},
  };
  for (std::size_t index = 0U; index < std::size(rawPacketCases); ++index) {
    SCOPED_TRACE(index);
    const auto& testCase = rawPacketCases[index];
    packet.setPktType(testCase.type);
    collector.RawPacketDataMon(testCase.operation, 15U + index, &packet, 0U, nullptr);
    ASSERT_EQ(sink.elements().size(), 3U + index);
    EXPECT_EQ(sink.elements().back().kind, testCase.kind);
    EXPECT_EQ(sink.elements().back().errorMessage, testCase.errorMessage);
    EXPECT_EQ(sink.elements().back().issueCode, testCase.issueCode);
  }

  packet.setPktType(ITM_PKT_SWIT);
  collector.RawPacketDataMon(OCSD_OP_DATA, 19U, &packet, 0U, nullptr);
  EXPECT_EQ(sink.elements().size(), 6U);
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorTransactionsPreserveOnlyCommittedElements)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(sink);
  EXPECT_NO_THROW(collector.rethrowOutputError());
  EXPECT_FALSE(collector.transactionFirstSourceOffset().has_value());

  collector.beginTransaction();
  collector.appendDecodeError(5U, "decode", TraceIssueCode::DecodeError);
  collector.prependDiscontinuity(4U, "gap", TraceIssueCode::DataLoss, 2U);
  collector.prependDataLossError(3U, "loss", 3U);
  EXPECT_EQ(collector.transactionElementCount(), 3U);
  EXPECT_EQ(collector.transactionFirstSourceOffset(), 3U);
  collector.commitTransactionBefore(5U);
  ASSERT_EQ(sink.elements().size(), 1U);
  EXPECT_EQ(sink.elements().front().kind, OpenCsdTraceElement::Kind::Discontinuity);

  collector.beginTransaction();
  collector.appendDecodeError(7U, "rolled back");
  collector.rollbackTransaction();
  EXPECT_EQ(sink.elements().size(), 1U);

  collector.prependDiscontinuity(8U, "outside", TraceIssueCode::DataLoss);
  collector.prependDataLossError(9U, "outside loss", 4U);
  ASSERT_EQ(sink.elements().size(), 3U);
  EXPECT_EQ(sink.elements().back().rawBytesConsumed, 4U);

  collector.beginTransaction();
  collector.appendDecodeError(10U, "committed");
  collector.commitTransaction();
  ASSERT_EQ(sink.elements().size(), 4U);
  EXPECT_EQ(sink.elements().back().sourceIndex, 10U);
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorDefersOutputFailures)
{
  ThrowingTraceElementSink sink;
  OpenCsdPacketCollector collector(sink);
  const auto software = itmElement(SWIT_PAYLOAD, 1U, 1U, 42U);

  EXPECT_EQ(collector.TraceElemIn(1U, 1U, software), OCSD_RESP_FATAL_SYS_ERR);
  EXPECT_EQ(collector.TraceElemIn(2U, 1U, software), OCSD_RESP_FATAL_SYS_ERR);
  EXPECT_EQ(sink.elements().size(), 2U);
  EXPECT_THROW(collector.rethrowOutputError(), std::runtime_error);
  EXPECT_NO_THROW(collector.rethrowOutputError());

  ItmTrcPacket packet;
  packet.setPktType(ITM_PKT_ASYNC);
  collector.RawPacketDataMon(OCSD_OP_DATA, 3U, &packet, 0U, nullptr);
  collector.RawPacketDataMon(OCSD_OP_DATA, 4U, &packet, 0U, nullptr);
  EXPECT_EQ(sink.elements().size(), 4U);
  EXPECT_THROW(collector.rethrowOutputError(), std::runtime_error);
}
