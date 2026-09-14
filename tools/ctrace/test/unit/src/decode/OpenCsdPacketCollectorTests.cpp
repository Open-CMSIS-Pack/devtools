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
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
  const TraceRouteIdentity route{TraceRouteId{9U}, 7U};
  OpenCsdPacketCollector collector(route, sink);

  auto globalTimestamp = itmElement(TS_GLOBAL);
  globalTimestamp.setTS(0xfedcba9876543210ULL, true);

  ASSERT_TRUE(collector.TraceElemIn(42U, 7U, globalTimestamp) == OCSD_RESP_CONT)
      << "OpenCSD global timestamp collection failed";
  ASSERT_TRUE(sink.elements().size() == 1U) << "reconstructed global timestamp element missing";
  const auto element = sink.elements().front();
  ASSERT_TRUE(element.kind == OpenCsdTraceElement::Kind::GlobalTimestamp)
      << "reconstructed global timestamp kind mismatch";
  ASSERT_TRUE(element.sourceIndex == 42U) << "reconstructed global timestamp source index mismatch";
  ASSERT_EQ(element.route, route) << "bound normalized route was not preserved";
  ASSERT_TRUE(element.timestampValue == 0xfedcba9876543210ULL) << "reconstructed global timestamp value mismatch";
  ASSERT_TRUE(element.clockChange) << "reconstructed global timestamp clock-change flag missing";

  ASSERT_TRUE(collector.TraceElemIn(43U, 0xffU, globalTimestamp) == OCSD_RESP_CONT)
      << "OpenCSD global timestamp collection with missing source ID failed";
  ASSERT_EQ(sink.elements().back().route, route) << "OpenCSD callback channel must not override the bound route";

  ItmTrcPacket rawGts1;
  rawGts1.setPktType(ITM_PKT_TS_GLOBAL_1);
  rawGts1.setValue(0x123456U, 4U);
  collector.RawPacketDataMon(OCSD_OP_DATA, 43U, &rawGts1, 0U, nullptr);

  ItmTrcPacket rawGts2;
  rawGts2.setPktType(ITM_PKT_TS_GLOBAL_2);
  rawGts2.setExtValue(0x123456789ULL);
  collector.RawPacketDataMon(OCSD_OP_DATA, 44U, &rawGts2, 0U, nullptr);
  ASSERT_TRUE(sink.elements().size() == 2U)
      << "raw GTS fragments must not be published as independent global timestamps";

  TraceEvent packet{GlobalTimestampTraceEvent{element.timestampValue, false}};
  ASSERT_TRUE(CsvRowMapper::row(packet) == std::to_string(element.timestampValue) + ",,global_ts,,,,,")
      << "CSV must preserve all 64 global timestamp bits in the cycles column";
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
  OpenCsdPacketCollector collector(TraceRouteIdentity{}, sink);
  for (std::size_t index = 0; index < cases.size(); ++index) {
    auto timestamp = itmElement(cases[index].first);
    timestamp.setTS(100U + index, false);

    ASSERT_TRUE(collector.TraceElemIn(index, 0U, timestamp) == OCSD_RESP_CONT)
        << "OpenCSD local timestamp collection failed";
  }

  ASSERT_TRUE(sink.elements().size() == cases.size()) << "local timestamp relation count mismatch";
  for (std::size_t index = 0; index < cases.size(); ++index) {
    ASSERT_TRUE(sink.elements()[index].kind == OpenCsdTraceElement::Kind::LocalTimestamp)
        << "local timestamp kind mismatch";
    ASSERT_TRUE(sink.elements()[index].timestampRelation == cases[index].second) << "local timestamp relation mismatch";
  }
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorMapsPayloadAndRawPacketKinds)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(TraceRouteIdentity{}, sink);

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

TEST(CtraceUnitTests, testOpenCsdPacketCollectorNeverDerivesRouteFromCallbackChannelOrPacketKind)
{
  CollectingOpenCsdElementSink sink;
  const TraceRouteIdentity route{TraceRouteId{23U}, 11U};
  OpenCsdPacketCollector collector(route, sink);

  auto localTimestamp = itmElement(TS_SYNC);
  localTimestamp.setTS(7U, false);
  EXPECT_EQ(collector.TraceElemIn(1U, 0U, itmElement(SWIT_PAYLOAD, 1U, 1U, 42U)), OCSD_RESP_CONT);
  EXPECT_EQ(collector.TraceElemIn(2U, 7U, itmElement(DWT_PAYLOAD, 2U, 2U, 0x1234U)), OCSD_RESP_CONT);
  EXPECT_EQ(collector.TraceElemIn(3U, 0xffU, localTimestamp), OCSD_RESP_CONT);

  ItmTrcPacket packet;
  packet.setPktType(ITM_PKT_ASYNC);
  collector.RawPacketDataMon(OCSD_OP_DATA, 4U, &packet, 0U, nullptr);
  packet.setPktType(ITM_PKT_OVERFLOW);
  collector.RawPacketDataMon(OCSD_OP_DATA, 5U, &packet, 0U, nullptr);
  packet.setPktType(ITM_PKT_RESERVED);
  collector.RawPacketDataMon(OCSD_OP_DATA, 6U, &packet, 0U, nullptr);

  collector.appendDecodeError(7U, "decode");
  collector.prependDiscontinuity(8U, "recovered", TraceIssueCode::DataLoss);
  collector.prependDataLossError(9U, "lost", 1U);

  ASSERT_EQ(sink.elements().size(), 9U);
  for (const auto& element : sink.elements()) {
    EXPECT_EQ(element.route, route) << "callback channel or packet kind replaced the collector's bound route";
  }
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorRoutesFormattedCallbacksInTheirOriginalOrder)
{
  CollectingOpenCsdElementSink sink;
  const TraceRouteIdentity route1{TraceRouteId{3U}, 1U};
  const TraceRouteIdentity route111{TraceRouteId{8U}, 111U};
  OpenCsdPacketCollector collector(std::vector<TraceRouteIdentity>{route1, route111}, sink);

  ItmTrcPacket packet;
  collector.beginTransaction();
  EXPECT_EQ(collector.TraceElemIn(10U, 1U, itmElement(SWIT_PAYLOAD, 2U, 1U, 0x11U)), OCSD_RESP_CONT);
  packet.setPktType(ITM_PKT_ASYNC);
  collector.rawPacketForRoute(route111, OCSD_OP_DATA, 11U, &packet, 0U, nullptr);
  EXPECT_EQ(collector.TraceElemIn(12U, 111U, itmElement(DWT_PAYLOAD, 9U, 2U, 0x2222U)), OCSD_RESP_CONT);
  packet.setPktType(ITM_PKT_OVERFLOW);
  collector.rawPacketForRoute(route1, OCSD_OP_DATA, 13U, &packet, 0U, nullptr);

  for (const auto unknownChannel : {0U, 2U, 112U, 255U}) {
    EXPECT_EQ(
        collector.TraceElemIn(14U, static_cast<std::uint8_t>(unknownChannel), itmElement(SWIT_PAYLOAD, 1U, 1U, 0xffU)),
        OCSD_RESP_CONT);
  }
  EXPECT_NO_THROW(collector.rethrowOutputError());
  EXPECT_TRUE(sink.elements().empty());
  EXPECT_EQ(collector.transactionElementCount(), 4U);

  collector.commitTransaction();
  ASSERT_EQ(sink.elements().size(), 4U);
  EXPECT_EQ(sink.elements()[0].sourceIndex, 10U);
  EXPECT_EQ(sink.elements()[0].kind, OpenCsdTraceElement::Kind::Software);
  EXPECT_EQ(sink.elements()[0].route, route1);
  EXPECT_EQ(sink.elements()[1].sourceIndex, 11U);
  EXPECT_EQ(sink.elements()[1].kind, OpenCsdTraceElement::Kind::Sync);
  EXPECT_EQ(sink.elements()[1].route, route111);
  EXPECT_EQ(sink.elements()[2].sourceIndex, 12U);
  EXPECT_EQ(sink.elements()[2].kind, OpenCsdTraceElement::Kind::Hardware);
  EXPECT_EQ(sink.elements()[2].route, route111);
  EXPECT_EQ(sink.elements()[3].sourceIndex, 13U);
  EXPECT_EQ(sink.elements()[3].kind, OpenCsdTraceElement::Kind::Overflow);
  EXPECT_EQ(sink.elements()[3].route, route1);

  collector.beginTransaction();
  packet.setPktType(ITM_PKT_RESERVED);
  collector.rawPacketForRoute(route111, OCSD_OP_DATA, 14U, &packet, 0U, nullptr);
  EXPECT_EQ(collector.transactionElementCount(), 1U);
  collector.rollbackTransaction();
  EXPECT_EQ(collector.transactionElementCount(), 0U);
  EXPECT_EQ(sink.elements().size(), 4U);
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorSelectivelyCommitsSeveralFailingRoutesInCallbackOrder)
{
  CollectingOpenCsdElementSink sink;
  const TraceRouteIdentity route1{TraceRouteId{3U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{8U}, 2U};
  const TraceRouteIdentity route111{TraceRouteId{9U}, 111U};
  OpenCsdPacketCollector collector(std::vector<TraceRouteIdentity>{route1, route2, route111}, sink);
  ItmTrcPacket packet;

  collector.beginTransaction();
  EXPECT_EQ(collector.TraceElemIn(1U, 1U, itmElement(SWIT_PAYLOAD, 1U, 1U, 0x11U)), OCSD_RESP_CONT);
  EXPECT_EQ(collector.TraceElemIn(2U, 2U, itmElement(SWIT_PAYLOAD, 2U, 1U, 0x22U)), OCSD_RESP_CONT);
  EXPECT_EQ(collector.TraceElemIn(3U, 111U, itmElement(SWIT_PAYLOAD, 3U, 1U, 0x33U)), OCSD_RESP_CONT);
  collector.appendDecodeError(route1, 4U, "replaced route diagnostic", TraceIssueCode::DecodeError, false,
                              TraceIssueSeverity::Warning);
  EXPECT_EQ(collector.TraceElemIn(5U, 1U, itmElement(SWIT_PAYLOAD, 4U, 1U, 0x44U)), OCSD_RESP_CONT);
  EXPECT_EQ(collector.TraceElemIn(6U, 111U, itmElement(DWT_PAYLOAD, 5U, 1U, 0x55U)), OCSD_RESP_CONT);
  EXPECT_EQ(collector.TraceElemIn(7U, 2U, itmElement(SWIT_PAYLOAD, 6U, 1U, 0x66U)), OCSD_RESP_CONT);
  packet.setPktType(ITM_PKT_RESERVED);
  collector.rawPacketForRoute(route2, OCSD_OP_DATA, 8U, &packet, 0U, nullptr);
  EXPECT_EQ(collector.TraceElemIn(9U, 1U, itmElement(DWT_PAYLOAD, 7U, 1U, 0x77U)), OCSD_RESP_CONT);
  packet.setPktType(ITM_PKT_ASYNC);
  collector.rawPacketForRoute(route111, OCSD_OP_DATA, 10U, &packet, 0U, nullptr);

  collector.commitTransactionForRouteFailures({{route1.id, 5U}, {route2.id, 8U}});

  ASSERT_EQ(sink.elements().size(), 7U);
  const std::array<std::uint64_t, 7U> expectedOffsets{1U, 2U, 3U, 4U, 6U, 7U, 10U};
  const std::array<TraceRouteIdentity, 7U> expectedRoutes{route1, route2, route111, route1, route111, route2, route111};
  for (std::size_t index = 0U; index < expectedOffsets.size(); ++index) {
    EXPECT_EQ(sink.elements()[index].sourceIndex, expectedOffsets[index]);
    EXPECT_EQ(sink.elements()[index].route, expectedRoutes[index]);
  }
  EXPECT_EQ(collector.transactionElementCount(), 0U);

  collector.beginTransaction();
  EXPECT_THROW(collector.commitTransactionForRouteFailures({{TraceRouteId{99U}, 1U}}), std::invalid_argument);
  collector.rollbackTransaction();
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorResolvesTransportChannelsWithoutChangingSingleBinding)
{
  CollectingOpenCsdElementSink sink;
  const TraceRouteIdentity singleRoute{TraceRouteId{4U}, std::nullopt};
  OpenCsdPacketCollector single(singleRoute, sink);
  ASSERT_NE(single.routeForChannel(0U), nullptr);
  EXPECT_EQ(*single.routeForChannel(0U), singleRoute);
  EXPECT_EQ(single.routeForChannel(1U), nullptr);
  EXPECT_EQ(single.routeForChannel(255U), nullptr);

  EXPECT_EQ(single.TraceElemIn(1U, 17U, itmElement(SWIT_PAYLOAD, 1U, 1U, 0x42U)), OCSD_RESP_CONT);
  ASSERT_EQ(sink.elements().size(), 1U);
  EXPECT_EQ(sink.elements().front().route, singleRoute);

  const TraceRouteIdentity route1{TraceRouteId{7U}, 1U};
  const TraceRouteIdentity route111{TraceRouteId{8U}, 111U};
  OpenCsdPacketCollector formatted(std::vector<TraceRouteIdentity>{route1, route111}, sink);
  ASSERT_NE(formatted.routeForChannel(1U), nullptr);
  ASSERT_NE(formatted.routeForChannel(111U), nullptr);
  EXPECT_EQ(*formatted.routeForChannel(1U), route1);
  EXPECT_EQ(*formatted.routeForChannel(111U), route111);
  EXPECT_EQ(formatted.routeForChannel(0U), nullptr);
  EXPECT_EQ(formatted.routeForChannel(2U), nullptr);
  EXPECT_EQ(formatted.routeForChannel(112U), nullptr);
  EXPECT_EQ(formatted.routeForChannel(255U), nullptr);
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorInsertsRouteDataLossImmediatelyBeforeRetainedSync)
{
  CollectingOpenCsdElementSink sink;
  const TraceRouteIdentity route1{TraceRouteId{3U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{8U}, 2U};
  const TraceRouteIdentity unknownRoute{TraceRouteId{9U}, 3U};
  OpenCsdPacketCollector collector(std::vector<TraceRouteIdentity>{route1, route2}, sink);
  ItmTrcPacket packet;

  EXPECT_FALSE(collector.insertDataLossBeforeSync(route1, 1U, "outside transaction", 1U));
  collector.beginTransaction();
  EXPECT_EQ(collector.TraceElemIn(5U, 1U, itmElement(SWIT_PAYLOAD, 1U, 1U, 0x11U)), OCSD_RESP_CONT);
  EXPECT_EQ(collector.TraceElemIn(7U, 2U, itmElement(SWIT_PAYLOAD, 2U, 1U, 0x22U)), OCSD_RESP_CONT);
  packet.setPktType(ITM_PKT_ASYNC);
  collector.rawPacketForRoute(route1, OCSD_OP_DATA, 10U, &packet, 0U, nullptr);
  collector.rawPacketForRoute(route2, OCSD_OP_DATA, 12U, &packet, 0U, nullptr);

  EXPECT_EQ(collector.transactionFirstSyncOffset(route1), 10U);
  EXPECT_FALSE(collector.transactionFirstSyncOffset(route1, 10U).has_value());
  EXPECT_EQ(collector.transactionFirstSyncOffset(route1, 11U), 10U);
  EXPECT_FALSE(collector.insertDataLossBeforeSync(route1, 2U, "sync is unsafe", 8U, 10U));
  EXPECT_TRUE(collector.insertDataLossBeforeSync(route2, 3U, "route two recovered", 9U));

  collector.appendDataLossError(route1, 2U, "route one unresolved", 11U);
  collector.commitTransactionForRouteFailures({{route1.id, 20U}});

  ASSERT_EQ(sink.elements().size(), 6U);
  EXPECT_EQ(sink.elements()[0].sourceIndex, 5U);
  EXPECT_EQ(sink.elements()[1].sourceIndex, 7U);
  EXPECT_EQ(sink.elements()[2].kind, OpenCsdTraceElement::Kind::Sync);
  EXPECT_EQ(sink.elements()[2].route, route1);
  EXPECT_EQ(sink.elements()[3].issueCode, TraceIssueCode::DataLoss);
  EXPECT_EQ(sink.elements()[3].route, route2);
  EXPECT_EQ(sink.elements()[3].sourceIndex, 3U);
  EXPECT_EQ(sink.elements()[3].rawBytesConsumed, 9U);
  EXPECT_TRUE(sink.elements()[3].awaitingResumeTimestamp);
  EXPECT_EQ(sink.elements()[4].kind, OpenCsdTraceElement::Kind::Sync);
  EXPECT_EQ(sink.elements()[4].route, route2);
  EXPECT_EQ(sink.elements()[5].issueCode, TraceIssueCode::DataLoss);
  EXPECT_EQ(sink.elements()[5].route, route1);
  EXPECT_EQ(sink.elements()[5].sourceIndex, 2U);
  EXPECT_TRUE(sink.elements()[5].awaitingResumeTimestamp);

  EXPECT_THROW(collector.appendDataLossError(unknownRoute, 1U, "unknown", 1U), std::invalid_argument);
  collector.beginTransaction();
  EXPECT_THROW(collector.insertDataLossBeforeSync(unknownRoute, 1U, "unknown", 1U), std::invalid_argument);
  collector.rollbackTransaction();
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorDetectsErrorsUnmatchedByRouteRecovery)
{
  CollectingOpenCsdElementSink sink;
  const TraceRouteIdentity route1{TraceRouteId{3U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{8U}, 2U};
  OpenCsdPacketCollector collector(std::vector<TraceRouteIdentity>{route1, route2}, sink);
  ItmTrcPacket packet;

  collector.beginTransaction();
  packet.setPktType(ITM_PKT_RESERVED);
  collector.rawPacketForRoute(route1, OCSD_OP_DATA, 5U, &packet, 0U, nullptr);
  EXPECT_FALSE(collector.transactionHasUnmatchedError({{route1.id, 5U}}));
  collector.rawPacketForRoute(route2, OCSD_OP_DATA, 6U, &packet, 0U, nullptr);
  EXPECT_TRUE(collector.transactionHasUnmatchedError({{route1.id, 5U}}));
  collector.rollbackTransaction();

  collector.beginTransaction();
  collector.appendDecodeError(route1, 4U, "earlier unexplained error");
  EXPECT_TRUE(collector.transactionHasUnmatchedError({{route1.id, 5U}}));
  collector.rollbackTransaction();

  collector.beginTransaction();
  collector.appendDecodeError(route2, 2U, "warning", TraceIssueCode::DecodeError, false, TraceIssueSeverity::Warning);
  collector.appendDataLossError(route1, 1U, "known recovery interval", 4U);
  EXPECT_FALSE(collector.transactionHasUnmatchedError({{route1.id, 5U}}));
  packet.setPktType(ITM_PKT_INCOMPLETE_EOT);
  collector.rawPacketForRoute(route1, OCSD_OP_EOT, 6U, &packet, 0U, nullptr);
  EXPECT_TRUE(collector.transactionHasUnmatchedError({{route1.id, 5U}}));
  collector.rollbackTransaction();
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorRequiresExplicitRawRouteForFormattedInput)
{
  CollectingOpenCsdElementSink sink;
  const TraceRouteIdentity route{TraceRouteId{3U}, 7U};
  const TraceRouteIdentity diagnosticRoute{TraceRouteId{4U}, 3U};
  OpenCsdPacketCollector collector(std::vector<TraceRouteIdentity>{route, diagnosticRoute}, sink);
  ItmTrcPacket packet;
  packet.setPktType(ITM_PKT_ASYNC);

  collector.RawPacketDataMon(OCSD_OP_DATA, 1U, &packet, 0U, nullptr);
  collector.appendDecodeError(2U, "input-wide fatal error");
  collector.prependDiscontinuity(3U, "input-wide recovery", TraceIssueCode::DataLoss);
  collector.prependDataLossError(4U, "input-wide loss", 1U);
  ASSERT_EQ(sink.elements().size(), 3U);
  for (const auto& element : sink.elements()) {
    EXPECT_EQ(element.route, diagnosticRoute);
  }

  collector.rawPacketForRoute(route, OCSD_OP_DATA, 5U, &packet, 0U, nullptr);
  ASSERT_EQ(sink.elements().size(), 4U);
  EXPECT_EQ(sink.elements().back().route, route);
  EXPECT_EQ(sink.elements().back().kind, OpenCsdTraceElement::Kind::Sync);

  const TraceRouteIdentity unknownRoute{TraceRouteId{4U}, 7U};
  collector.rawPacketForRoute(unknownRoute, OCSD_OP_DATA, 6U, &packet, 0U, nullptr);
  EXPECT_THROW(collector.rethrowOutputError(), std::invalid_argument);
  EXPECT_NO_THROW(collector.rethrowOutputError());
  EXPECT_EQ(sink.elements().size(), 4U);

  collector.rawPacketForRoute(TraceRouteIdentity{TraceRouteId{5U}, std::nullopt}, OCSD_OP_DATA, 7U, &packet, 0U,
                              nullptr);
  EXPECT_THROW(collector.rethrowOutputError(), std::invalid_argument);
  EXPECT_EQ(sink.elements().size(), 4U);
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorRetainsOnlySelectedRoutedTransactionErrors)
{
  CollectingOpenCsdElementSink sink;
  const TraceRouteIdentity route1{TraceRouteId{3U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{8U}, 2U};
  OpenCsdPacketCollector collector(std::vector<TraceRouteIdentity>{route1, route2}, sink);
  ItmTrcPacket packet;

  collector.beginTransaction();
  EXPECT_EQ(collector.TraceElemIn(3U, 1U, itmElement(SWIT_PAYLOAD, 1U, 1U, 0x41U)), OCSD_RESP_CONT);
  packet.setPktType(ITM_PKT_RESERVED);
  collector.rawPacketForRoute(route1, OCSD_OP_DATA, 4U, &packet, 0U, nullptr);
  packet.setPktType(ITM_PKT_INCOMPLETE_EOT);
  collector.rawPacketForRoute(route2, OCSD_OP_EOT, 6U, &packet, 0U, nullptr);

  EXPECT_EQ(collector.commitTransactionErrors(TraceIssueCode::OpenCsdIncompleteTail), 1U);
  ASSERT_EQ(sink.elements().size(), 1U);
  EXPECT_EQ(sink.elements().front().kind, OpenCsdTraceElement::Kind::Error);
  EXPECT_EQ(sink.elements().front().issueCode, TraceIssueCode::OpenCsdIncompleteTail);
  EXPECT_EQ(sink.elements().front().sourceIndex, 6U);
  EXPECT_EQ(sink.elements().front().route, route2);
  EXPECT_EQ(collector.transactionElementCount(), 0U);
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorValidatesFormattedRouteCatalogue)
{
  CollectingOpenCsdElementSink sink;
  const auto construct = [&](std::vector<TraceRouteIdentity> routes) {
    return OpenCsdPacketCollector(std::move(routes), sink);
  };

  EXPECT_THROW((void)construct({}), std::invalid_argument);
  EXPECT_THROW((void)construct({{TraceRouteId{0U}, std::nullopt}}), std::invalid_argument);
  EXPECT_THROW((void)construct({{TraceRouteId{0U}, 0U}}), std::invalid_argument);
  EXPECT_THROW((void)construct({{TraceRouteId{0U}, 112U}}), std::invalid_argument);
  EXPECT_THROW((void)construct({{TraceRouteId{0U}, 1U}, {TraceRouteId{1U}, 1U}}), std::invalid_argument);
  EXPECT_THROW((void)construct({{TraceRouteId{0U}, 1U}, {TraceRouteId{0U}, 2U}}), std::invalid_argument);

  OpenCsdPacketCollector collector(std::vector<TraceRouteIdentity>{{TraceRouteId{0U}, 1U}}, sink);
  EXPECT_THROW(collector.appendDecodeError({TraceRouteId{1U}, 1U}, 1U, "unknown route"), std::invalid_argument);
  EXPECT_THROW(collector.appendDataLossError({TraceRouteId{1U}, 1U}, 1U, "unknown route", 1U), std::invalid_argument);
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorTransactionsPreserveOnlyCommittedElements)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(TraceRouteIdentity{}, sink);
  EXPECT_NO_THROW(collector.rethrowOutputError());
  EXPECT_FALSE(collector.reserveTransactionOrder().has_value());
  EXPECT_FALSE(collector.transactionFirstSourceOffset().has_value());

  collector.beginTransaction();
  collector.appendDecodeError(2U, "warning", TraceIssueCode::DecodeError, false, TraceIssueSeverity::Warning);
  collector.appendDecodeError(5U, "decode", TraceIssueCode::DecodeError);
  collector.prependDiscontinuity(4U, "gap", TraceIssueCode::DataLoss, 2U);
  collector.prependDataLossError(3U, "loss", 3U);
  EXPECT_EQ(collector.transactionElementCount(), 4U);
  EXPECT_EQ(collector.transactionFirstSourceOffset(), 2U);
  collector.commitTransactionBefore(5U);
  ASSERT_EQ(sink.elements().size(), 1U);
  EXPECT_EQ(sink.elements().front().kind, OpenCsdTraceElement::Kind::Discontinuity);

  collector.beginTransaction();
  collector.appendDecodeError(7U, "rolled back");
  EXPECT_EQ(collector.transactionElementCount(), 1U);
  collector.rollbackTransaction();
  EXPECT_EQ(collector.transactionElementCount(), 0U);
  EXPECT_EQ(sink.elements().size(), 1U);

  collector.prependDiscontinuity(8U, "outside", TraceIssueCode::DataLoss);
  collector.prependDataLossError(9U, "outside loss", 4U);
  ASSERT_EQ(sink.elements().size(), 3U);
  EXPECT_EQ(sink.elements().back().rawBytesConsumed, 4U);

  collector.beginTransaction();
  collector.appendDecodeError(10U, "committed");
  collector.commitTransaction();
  ASSERT_EQ(sink.elements().size(), 4U);
  EXPECT_EQ(sink.elements().back().kind, OpenCsdTraceElement::Kind::Error);
  EXPECT_EQ(sink.elements().back().sourceIndex, 10U);
}

TEST(CtraceUnitTests, testOpenCsdPacketCollectorDefersOutputFailures)
{
  ThrowingTraceElementSink sink;
  OpenCsdPacketCollector collector(TraceRouteIdentity{}, sink);
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
