/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "FormattedTraceTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "OpenCsdErrorController.h"
#include "OpenCsdFormattedItmSession.h"
#include "OpenCsdTreeSession.h"
#include "TraceRoute.h"
#include "common/trc_gen_elem.h"
#include "interfaces/trc_data_rawframe_in_i.h"
#include "interfaces/trc_gen_elem_in_i.h"
#include "interfaces/trc_pkt_raw_in_i.h"
#include "opencsd/itm/trc_cmp_cfg_itm.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"
#include "opencsd/trc_gen_elem_types.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using FormattedTraceTestSupport::itmGlobalTimestampPacket;
using FormattedTraceTestSupport::itmHardwareSync;
using FormattedTraceTestSupport::itmSoftwarePacket;
using FormattedTraceTestSupport::memoryAlignedFrames;
using FormattedTraceTestSupport::Segment;

namespace {

/** @brief Records routed ITM software elements emitted by the formatted tree. */
class RecordingElementOutput final : public ITrcGenElemIn {
public:
  /** @brief Stores the fields needed to verify deformatter routing. */
  struct SoftwareElement {
    std::uint8_t traceId = 0U;
    std::uint8_t channel = 0U;
    std::uint32_t value = 0U;
  };

  /** @brief Records software elements and accepts every OpenCSD callback. */
  ocsd_datapath_resp_t TraceElemIn(ocsd_trc_index_t, std::uint8_t traceId, const OcsdTraceElement& element) override
  {
    m_callbackTraceIds.push_back(traceId);
    if (element.getType() == OCSD_GEN_TRC_ELEM_ITMTRACE && element.swt_itm.pkt_type == SWIT_PAYLOAD) {
      m_software.push_back({traceId, element.swt_itm.payload_src_id, element.swt_itm.value});
    }
    return OCSD_RESP_CONT;
  }

  /** @brief Returns all generic callback channels observed by the tree. */
  const std::vector<std::uint8_t>& callbackTraceIds() const
  {
    return m_callbackTraceIds;
  }

  /** @brief Returns decoded software elements in callback order. */
  const std::vector<SoftwareElement>& software() const
  {
    return m_software;
  }

private:
  std::vector<std::uint8_t> m_callbackTraceIds;
  std::vector<SoftwareElement> m_software;
};

/** @brief Records the normalized route supplied with every raw ITM packet. */
class RecordingPacketSink final : public OpenCsdFormattedItmPacketSink {
public:
  /** @brief Stores one routed packet observation without retaining external pointers. */
  struct PacketObservation {
    TraceRouteIdentity route;
    ocsd_itm_pkt_type type = ITM_PKT_NOTSYNC;
  };

  /** @brief Stores one route-local packet-processor reset notification. */
  struct ResetObservation {
    TraceRouteIdentity route;
    ocsd_trc_index_t index = 0U;
  };

  /** @brief Stores the formatter group and deformatted byte count delivered to one route. */
  struct DataObservation {
    TraceRouteIdentity route;
    ocsd_trc_index_t formatterOffset = 0U;
    std::uint32_t byteCount = 0U;
  };

  /** @brief Copies packet identity while ignoring callback-only control operations. */
  void rawPacketForRoute(const TraceRouteIdentity& route, ocsd_datapath_op_t operation, ocsd_trc_index_t index,
                         const ItmTrcPacket* packet, std::uint32_t, const std::uint8_t*) override
  {
    if (packet != nullptr) {
      m_packets.push_back({route, packet->getPktType()});
    } else if (operation == OCSD_OP_RESET) {
      m_resets.push_back({route, index});
    }
  }

  /** @brief Records received protocol bytes independently of decoded packets. */
  void formattedDataForRoute(const TraceRouteIdentity& route, ocsd_trc_index_t index, std::uint32_t size) override
  {
    m_data.push_back({route, index, size});
  }

  /** @brief Counts software packets attributed to an exact normalized route. */
  std::size_t softwarePacketsFor(const TraceRouteIdentity& route) const
  {
    return static_cast<std::size_t>(std::count_if(m_packets.begin(), m_packets.end(), [&](const auto& packet) {
      return packet.route == route && packet.type == ITM_PKT_SWIT;
    }));
  }

  /** @brief Returns every copied raw-packet observation. */
  const std::vector<PacketObservation>& packets() const
  {
    return m_packets;
  }

  /** @brief Returns every route-local reset notification in callback order. */
  const std::vector<ResetObservation>& resets() const
  {
    return m_resets;
  }

  /** @brief Returns input observations in formatter callback order. */
  const std::vector<DataObservation>& data() const
  {
    return m_data;
  }

private:
  std::vector<PacketObservation> m_packets;
  std::vector<ResetObservation> m_resets;
  std::vector<DataObservation> m_data;
};

/** @brief Throws only when a real software element crosses the callback boundary. */
class ThrowingElementOutput final : public ITrcGenElemIn {
public:
  /** @brief Raises a stable test exception from a real decoded software callback. */
  ocsd_datapath_resp_t TraceElemIn(ocsd_trc_index_t, std::uint8_t, const OcsdTraceElement& element) override
  {
    if (element.getType() == OCSD_GEN_TRC_ELEM_ITMTRACE && element.swt_itm.pkt_type == SWIT_PAYLOAD) {
      throw std::runtime_error("synthetic generic-element failure");
    }
    return OCSD_RESP_CONT;
  }
};

/** @brief Throws only when a decoded software packet reaches its routed sink. */
class ThrowingPacketSink final : public OpenCsdFormattedItmPacketSink {
public:
  /** @brief Raises a stable test exception from a real routed packet callback. */
  void rawPacketForRoute(const TraceRouteIdentity&, ocsd_datapath_op_t, ocsd_trc_index_t, const ItmTrcPacket* packet,
                         std::uint32_t, const std::uint8_t*) override
  {
    if (packet != nullptr && packet->getPktType() == ITM_PKT_SWIT) {
      throw std::runtime_error("synthetic routed-packet failure");
    }
  }
};

/** @brief Throws from received-data accounting before protocol decoding begins. */
class ThrowingDataSink final : public OpenCsdFormattedItmPacketSink {
public:
  /** @brief Accepts protocol packet callbacks without additional work. */
  void rawPacketForRoute(const TraceRouteIdentity&, ocsd_datapath_op_t, ocsd_trc_index_t, const ItmTrcPacket*,
                         std::uint32_t, const std::uint8_t*) override
  {
  }

  /** @brief Raises a stable test exception from a real deformatter callback. */
  void formattedDataForRoute(const TraceRouteIdentity&, ocsd_trc_index_t, std::uint32_t) override
  {
    throw std::runtime_error("synthetic routed-data failure");
  }
};

/** @brief Accepts raw ITM packet callbacks for direct tree-wrapper tests. */
class NoopPacketMonitor final : public IPktRawDataMon<ItmTrcPacket> {
public:
  /** @brief Ignores one raw packet callback. */
  void RawPacketDataMon(ocsd_datapath_op_t, ocsd_trc_index_t, const ItmTrcPacket*, std::uint32_t,
                        const std::uint8_t*) override
  {
  }
};

/** @brief Accepts raw formatter callbacks for direct tree-wrapper tests. */
class NoopRawFrameMonitor final : public ITrcRawFrameIn {
public:
  /** @brief Ignores one raw-frame callback. */
  ocsd_err_t TraceRawFrameIn(ocsd_datapath_op_t, ocsd_trc_index_t, ocsd_rawframe_elem_t, int, const std::uint8_t*,
                             std::uint8_t) override
  {
    return OCSD_OK;
  }
};

/** @brief Feeds one complete formatted capture and checks root byte accounting. */
void feedCapture(OpenCsdFormattedItmSession& session, const std::vector<std::uint8_t>& capture,
                 ocsd_trc_index_t index = 0U)
{
  std::uint32_t processed = 0U;
  EXPECT_EQ(session.pushData(index, static_cast<std::uint32_t>(capture.size()), capture.data(), processed),
            OCSD_RESP_CONT);
  EXPECT_EQ(processed, capture.size());
}

/** @brief Tests whether a generic callback used a forbidden formatter ID. */
bool observedTraceId(const RecordingElementOutput& output, std::uint8_t traceId)
{
  return std::find(output.callbackTraceIds().begin(), output.callbackTraceIds().end(), traceId) !=
         output.callbackTraceIds().end();
}

/** @brief Restricts prefix tests to initial bytes without an ID while other skip categories remain observable. */
OpenCsdSkippedBytesSink recordUnassignedPrefix(std::vector<std::pair<std::uint64_t, std::uint64_t>>& discarded)
{
  return [&discarded](const TraceByteSkip& skipped) {
    if (skipped.reason == TraceByteSkipReason::NoSourceId) {
      EXPECT_FALSE(skipped.traceId.has_value());
      discarded.emplace_back(skipped.formatterOffset, skipped.byteCount);
    }
  };
}

} // namespace

TEST(CtraceUnitTests, testFormattedTraceTestSupportBuildsCanonicalMemoryAlignedFrames)
{
  EXPECT_TRUE(memoryAlignedFrames({}).empty());
  EXPECT_THROW((void)itmSoftwarePacket(32U, 0U), std::invalid_argument);
  EXPECT_THROW((void)memoryAlignedFrames({Segment{128U, {0U}}}), std::invalid_argument);

  const auto single = memoryAlignedFrames({
      {1U, itmHardwareSync()},
      {1U, itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'))},
  });
  const std::vector<std::uint8_t> expectedSingle{
      0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x09U, 0x01U, 0x41U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x10U,
  };
  EXPECT_EQ(single, expectedSingle);

  const auto boundaryIds = memoryAlignedFrames({
      {1U, itmHardwareSync()},
      {111U, itmHardwareSync()},
      {1U, itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'))},
      {111U, itmSoftwarePacket(2U, static_cast<std::uint8_t>('B'))},
  });
  const std::vector<std::uint8_t> expectedBoundaryIds{
      0x03U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xdfU, 0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x03U, 0x08U,
      0x08U, 0x41U, 0xdfU, 0x11U, 0x01U, 0x42U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x05U,
  };
  EXPECT_EQ(boundaryIds, expectedBoundaryIds);
}

TEST(CtraceUnitTests, testFormattedTraceTestSupportBuildsUnflaggedGlobalTimestampPair)
{
  const std::vector<std::uint8_t> expected{
      0x94U, 0xd6U, 0xe8U, 0xc8U, 0x07U, 0xb4U, 0x80U, 0xa6U, 0xb0U, 0xc0U, 0xc0U, 0x00U,
  };
  EXPECT_EQ(itmGlobalTimestampPacket(0x1020304c00f23456ULL), expected);
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionRoutesBoundaryIdsIndependently)
{
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity last{TraceRouteId{20U}, 111U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  OpenCsdFormattedItmSession session({first, last}, elements, errors, packets);
  const auto capture = memoryAlignedFrames({
      {1U, itmHardwareSync()},
      {111U, itmHardwareSync()},
      {1U, itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'))},
      {111U, itmSoftwarePacket(2U, static_cast<std::uint8_t>('B'))},
  });

  ASSERT_EQ(capture.size(), 32U);
  feedCapture(session, capture);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);

  ASSERT_EQ(elements.software().size(), 2U);
  EXPECT_EQ(elements.software()[0U].traceId, 1U);
  EXPECT_EQ(elements.software()[0U].channel, 1U);
  EXPECT_EQ(elements.software()[0U].value, static_cast<std::uint32_t>('A'));
  EXPECT_EQ(elements.software()[1U].traceId, 111U);
  EXPECT_EQ(elements.software()[1U].channel, 2U);
  EXPECT_EQ(elements.software()[1U].value, static_cast<std::uint32_t>('B'));
  EXPECT_EQ(packets.softwarePacketsFor(first), 1U);
  EXPECT_EQ(packets.softwarePacketsFor(last), 1U);
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionKeepsFormatterIdZeroSilent)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::vector<std::uint8_t> unsupported;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets,
                                     [&](std::uint8_t traceId, ocsd_trc_index_t) { unsupported.push_back(traceId); });
  const auto capture = memoryAlignedFrames({Segment{0U, std::vector<std::uint8_t>(14U, 0U)}});

  ASSERT_EQ(capture.size(), 16U);
  feedCapture(session, capture);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);

  EXPECT_TRUE(elements.software().empty());
  EXPECT_FALSE(observedTraceId(elements, 0U));
  EXPECT_TRUE(packets.packets().empty());
  EXPECT_TRUE(unsupported.empty());
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionKeepsReservedFormatterIdsSilent)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::vector<std::uint8_t> unsupported;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets,
                                     [&](std::uint8_t traceId, ocsd_trc_index_t) { unsupported.push_back(traceId); });
  const auto capture = memoryAlignedFrames({Segment{112U, std::vector<std::uint8_t>(12U, 0U)}});

  ASSERT_EQ(capture.size(), 16U);
  feedCapture(session, capture);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);

  EXPECT_TRUE(elements.software().empty());
  EXPECT_FALSE(observedTraceId(elements, 112U));
  EXPECT_TRUE(packets.packets().empty());
  EXPECT_TRUE(unsupported.empty());
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionValidatesRouteCatalogue)
{
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  const auto construct = [&](std::vector<TraceRouteIdentity> routes) {
    OpenCsdFormattedItmSession session(std::move(routes), elements, errors, packets);
  };

  EXPECT_TRUE(throwsWithMessage<OpenCsdItmSessionError>([&] { construct({}); }, "requires at least one route"));
  EXPECT_TRUE(throwsWithMessage<OpenCsdItmSessionError>(
      [&] { construct({TraceRouteIdentity{TraceRouteId{1U}, std::nullopt}}); }, "between 1 and 111"));
  EXPECT_TRUE(throwsWithMessage<OpenCsdItmSessionError>([&] { construct({TraceRouteIdentity{TraceRouteId{1U}, 0U}}); },
                                                        "between 1 and 111"));
  EXPECT_TRUE(throwsWithMessage<OpenCsdItmSessionError>(
      [&] { construct({TraceRouteIdentity{TraceRouteId{1U}, 112U}}); }, "between 1 and 111"));
  EXPECT_TRUE(throwsWithMessage<OpenCsdItmSessionError>(
      [&] { construct({TraceRouteIdentity{TraceRouteId{1U}, 1U}, TraceRouteIdentity{TraceRouteId{2U}, 1U}}); },
      "unique Trace Bus IDs"));
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionRethrowsGenericCallbackFailureAfterPush)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  ThrowingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets);
  const auto capture = memoryAlignedFrames({
      {1U, itmHardwareSync()},
      {1U, itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'))},
  });

  std::uint32_t processed = 0U;
  const auto message = captureExceptionMessage(
      [&] { (void)session.pushData(0U, static_cast<std::uint32_t>(capture.size()), capture.data(), processed); });
  EXPECT_EQ(message, std::optional<std::string>("synthetic generic-element failure"));
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionRethrowsPacketCallbackFailureAfterPush)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  ThrowingPacketSink packets;
  OpenCsdErrorController errors;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets);
  const auto capture = memoryAlignedFrames({
      {1U, itmHardwareSync()},
      {1U, itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'))},
  });

  std::uint32_t processed = 0U;
  const auto message = captureExceptionMessage(
      [&] { (void)session.pushData(0U, static_cast<std::uint32_t>(capture.size()), capture.data(), processed); });
  EXPECT_EQ(message, std::optional<std::string>("synthetic routed-packet failure"));
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionReportsUnassignedPrefixAtEndOfTrace)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> discarded;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets, {}, recordUnassignedPrefix(discarded));
  const std::vector<std::uint8_t> unassignedFrame(16U, 0U);

  feedCapture(session, unassignedFrame, 64U);
  EXPECT_TRUE(discarded.empty());
  EXPECT_EQ(session.flush(), OCSD_RESP_CONT);
  EXPECT_TRUE(discarded.empty());
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  ASSERT_EQ(discarded.size(), 1U);
  EXPECT_EQ(discarded.front().first, 64U);
  EXPECT_EQ(discarded.front().second, 15U);
  EXPECT_TRUE(elements.software().empty());
  EXPECT_TRUE(packets.data().empty());
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  EXPECT_EQ(discarded.size(), 1U);
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionAggregatesPrefixAndContinuesRegardlessOfChunking)
{
  for (const std::size_t chunkSize : {16U, 48U}) {
    SCOPED_TRACE(chunkSize);
    const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
    RecordingElementOutput elements;
    RecordingPacketSink packets;
    OpenCsdErrorController errors;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> discarded;
    OpenCsdFormattedItmSession session({route}, elements, errors, packets, {}, recordUnassignedPrefix(discarded));
    const auto synchronization = itmHardwareSync();
    const auto software = itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'));
    const auto payload = memoryAlignedFrames({{1U, synchronization}, {1U, software}});
    std::vector<std::uint8_t> capture(32U, 0U);
    capture.insert(capture.end(), payload.begin(), payload.end());
    ASSERT_EQ(capture.size(), 48U);
    for (std::size_t offset = 0U; offset < capture.size(); offset += chunkSize) {
      const std::vector<std::uint8_t> chunk(capture.begin() + offset, capture.begin() + offset + chunkSize);
      feedCapture(session, chunk, 64U + offset);
      EXPECT_EQ(discarded.size(), offset + chunkSize == capture.size() ? 1U : 0U);
    }
    EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
    ASSERT_EQ(discarded.size(), 1U);
    EXPECT_EQ(discarded.front().first, 64U);
    EXPECT_EQ(discarded.front().second, 30U);
    ASSERT_EQ(elements.software().size(), 1U);
    EXPECT_EQ(elements.software().front().value, static_cast<std::uint32_t>('A'));
    ASSERT_EQ(packets.data().size(), 1U);
    EXPECT_EQ(packets.data().front().route, route);
    EXPECT_EQ(packets.data().front().formatterOffset, 96U);
    // The fixture assigns all remaining frame padding to ID 0, not this ITM route.
    EXPECT_EQ(packets.data().front().byteCount, synchronization.size() + software.size());
  }
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionClosesPrefixOnNullOrReservedId)
{
  for (const std::uint8_t traceId : {0U, 112U, 127U}) {
    SCOPED_TRACE(traceId);
    const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
    RecordingElementOutput elements;
    RecordingPacketSink packets;
    OpenCsdErrorController errors;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> discarded;
    OpenCsdFormattedItmSession session({route}, elements, errors, packets, {}, recordUnassignedPrefix(discarded));
    feedCapture(session, std::vector<std::uint8_t>(16U, 0U));
    EXPECT_TRUE(discarded.empty());
    const auto assigned = memoryAlignedFrames({Segment{traceId, std::vector<std::uint8_t>(14U, 0U)}});
    feedCapture(session, assigned, 16U);
    ASSERT_EQ(discarded.size(), 1U);
    EXPECT_EQ(discarded.front().first, 0U);
    EXPECT_EQ(discarded.front().second, 15U);
    EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
    EXPECT_EQ(discarded.size(), 1U);
    EXPECT_TRUE(elements.software().empty());
    EXPECT_TRUE(packets.data().empty());
  }
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionAllowsOmittedSkippedBytesSink)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets);
  feedCapture(session, std::vector<std::uint8_t>(16U, 0U));
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  EXPECT_TRUE(elements.software().empty());
  EXPECT_TRUE(packets.data().empty());
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionDoesNotCountReservedFfDataAsUnassigned)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::vector<std::pair<std::uint64_t, std::uint64_t>> discarded;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets, {}, recordUnassignedPrefix(discarded));
  feedCapture(session, std::vector<std::uint8_t>(16U, 0xffU));
  ASSERT_EQ(discarded.size(), 1U);
  EXPECT_EQ(discarded.front().first, 0U);
  EXPECT_EQ(discarded.front().second, 1U);

  const auto payload = memoryAlignedFrames({
      {1U, itmHardwareSync()},
      {1U, itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'))},
  });
  feedCapture(session, payload, 16U);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  EXPECT_EQ(discarded.size(), 1U);
  ASSERT_EQ(elements.software().size(), 1U);
  EXPECT_EQ(elements.software().front().value, static_cast<std::uint32_t>('A'));
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionRethrowsDataCallbackFailureAfterPush)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  ThrowingDataSink packets;
  OpenCsdErrorController errors;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets);
  const auto capture = memoryAlignedFrames({Segment{1U, itmHardwareSync()}});
  std::uint32_t processed = 0U;
  const auto message = captureExceptionMessage(
      [&] { (void)session.pushData(0U, static_cast<std::uint32_t>(capture.size()), capture.data(), processed); });
  EXPECT_EQ(message, std::optional<std::string>("synthetic routed-data failure"));
  EXPECT_EQ(processed, capture.size());
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionDoesNotRepeatThrowingPrefixObserver)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::size_t observed = 0U;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets, {},
                                     [&](const TraceByteSkip&) {
                                       ++observed;
                                       throw std::runtime_error("synthetic prefix-observer failure");
                                     });
  feedCapture(session, std::vector<std::uint8_t>(16U, 0U));
  EXPECT_EQ(captureExceptionMessage([&] { (void)session.endOfTrace(); }),
            std::optional<std::string>("synthetic prefix-observer failure"));
  EXPECT_EQ(observed, 1U);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  EXPECT_EQ(observed, 1U);
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionReportsSkippedSourceTotalsAtEndOfTrace)
{
  for (const std::size_t chunkSize : {16U, 80U}) {
    SCOPED_TRACE(chunkSize);
    const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
    RecordingElementOutput elements;
    RecordingPacketSink packets;
    OpenCsdErrorController errors;
    std::vector<TraceByteSkip> skipped;
    std::vector<std::uint8_t> unsupported;
    OpenCsdFormattedItmSession session(
        {route}, elements, errors, packets,
        [&](std::uint8_t traceId, ocsd_trc_index_t) { unsupported.push_back(traceId); },
        [&](const TraceByteSkip& item) { skipped.push_back(item); });
    std::vector<std::uint8_t> capture;
    for (const std::uint8_t traceId : {0U, 112U, 127U, 42U, 42U}) {
      // One ID marker + fourteen payload bytes + one formatter flag byte.
      std::vector<std::uint8_t> frame(16U, 0U);
      frame.front() = static_cast<std::uint8_t>((traceId << 1U) | 1U);
      capture.insert(capture.end(), frame.begin(), frame.end());
    }
    for (std::size_t offset = 0U; offset < capture.size(); offset += chunkSize) {
      feedCapture(session, {capture.begin() + offset, capture.begin() + offset + chunkSize}, offset);
      EXPECT_TRUE(skipped.empty());
    }
    EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
    ASSERT_EQ(skipped.size(), 4U);
    const std::vector<std::uint8_t> expectedIds{0U, 42U, 112U, 127U};
    const std::vector<std::uint64_t> expectedCounts{14U, 28U, 14U, 14U};
    const std::vector<std::uint64_t> expectedOffsets{0U, 48U, 16U, 32U};
    const std::vector<TraceByteSkipReason> expectedReasons{
        TraceByteSkipReason::NullSourceId, TraceByteSkipReason::UnconfiguredSourceId,
        TraceByteSkipReason::ReservedSourceId, TraceByteSkipReason::ReservedSourceId};
    for (std::size_t index = 0U; index < skipped.size(); ++index) {
      EXPECT_EQ(skipped[index].traceId, expectedIds[index]);
      EXPECT_EQ(skipped[index].byteCount, expectedCounts[index]);
      EXPECT_EQ(skipped[index].formatterOffset, expectedOffsets[index]);
      EXPECT_EQ(skipped[index].reason, expectedReasons[index]);
    }
    EXPECT_EQ(unsupported, std::vector<std::uint8_t>{42U});
    EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
    EXPECT_EQ(skipped.size(), 4U);
    EXPECT_TRUE(packets.data().empty());
    EXPECT_TRUE(elements.software().empty());
  }
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionSeparatesSkippedPayloadFromFormatterControlBytes)
{
  const std::vector<std::uint8_t> capture{
      0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU,
      0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU,
      0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU, 0xffU,
      0xfeU, 0x57U, 0x00U, 0x00U, 0x00U, 0x00U, 0x03U, 0x13U,
      0x00U, 0x80U, 0x16U, 0xacU, 0x5eU, 0x00U, 0x08U, 0xc0U,
      0x01U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
  };
  for (const std::size_t chunkSize : {16U, 64U}) {
    SCOPED_TRACE(chunkSize);
    const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
    RecordingElementOutput elements;
    RecordingPacketSink packets;
    OpenCsdErrorController errors;
    std::vector<TraceByteSkip> skipped;
    OpenCsdFormattedItmSession session({route}, elements, errors, packets, {},
                                       [&](const TraceByteSkip& item) { skipped.push_back(item); });
    for (std::size_t offset = 0U; offset < capture.size(); offset += chunkSize) {
      feedCapture(session, {capture.begin() + offset, capture.begin() + offset + chunkSize}, offset);
    }
    ASSERT_EQ(skipped.size(), 1U);
    EXPECT_EQ(skipped.front().reason, TraceByteSkipReason::NoSourceId);
    EXPECT_EQ(skipped.front().formatterOffset, 0U);
    EXPECT_EQ(skipped.front().byteCount, 1U);
    EXPECT_FALSE(skipped.front().traceId.has_value());
    EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
    ASSERT_EQ(skipped.size(), 3U);
    EXPECT_EQ(skipped[1U].reason, TraceByteSkipReason::NullSourceId);
    EXPECT_EQ(skipped[1U].traceId, 0U);
    EXPECT_EQ(skipped[1U].formatterOffset, 40U);
    EXPECT_EQ(skipped[1U].byteCount, 21U);
    EXPECT_EQ(skipped[2U].reason, TraceByteSkipReason::ReservedSourceId);
    EXPECT_EQ(skipped[2U].traceId, 127U);
    EXPECT_EQ(skipped[2U].formatterOffset, 0U);
    EXPECT_EQ(skipped[2U].byteCount, 16U);
    ASSERT_EQ(packets.data().size(), 1U);
    EXPECT_EQ(packets.data().front().route, route);
    EXPECT_EQ(packets.data().front().byteCount, 8U);
    // 38 skipped + 8 routed bytes leave 18 processed formatter-control bytes.
    EXPECT_EQ(capture.size() - skipped[0U].byteCount - skipped[1U].byteCount -
                  skipped[2U].byteCount - packets.data().front().byteCount,
              18U);
    EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
    EXPECT_EQ(skipped.size(), 3U);
  }
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionDoesNotRepeatThrowingSkippedSourceObserver)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::size_t observed = 0U;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets, {}, [&](const TraceByteSkip&) {
    ++observed;
    throw std::runtime_error("synthetic skipped-source failure");
  });
  feedCapture(session, memoryAlignedFrames({Segment{0U, std::vector<std::uint8_t>(14U, 0U)}}));
  EXPECT_EQ(observed, 0U);
  EXPECT_EQ(captureExceptionMessage([&] { (void)session.endOfTrace(); }),
            std::optional<std::string>("synthetic skipped-source failure"));
  EXPECT_EQ(observed, 1U);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  EXPECT_EQ(observed, 1U);
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionAllowsOmittedUnsupportedIdSink)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets);
  const auto capture = memoryAlignedFrames({
      {1U, itmHardwareSync()},
      {42U, {0xdeU, 0xadU}},
      {1U, itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'))},
  });

  feedCapture(session, capture);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  ASSERT_EQ(elements.software().size(), 1U);
  EXPECT_EQ(elements.software().front().traceId, 1U);
}

TEST(CtraceUnitTests, testOpenCsdTreeSessionValidatesSourceAndDecoderChannels)
{
  RecordingElementOutput elements;
  OpenCsdErrorController errors;
  bool createCalled = false;
  const OpenCsdTreeSession::TreeLifecycle lifecycle{
      [&](ocsd_dcd_tree_src_t, std::uint32_t) -> DecodeTree* {
        createCalled = true;
        return nullptr;
      },
      [](DecodeTree*) {},
  };
  EXPECT_THROW((void)OpenCsdTreeSession(static_cast<ocsd_dcd_tree_src_t>(99), 0U, errors, elements, lifecycle),
               OpenCsdTreeSessionError);
  EXPECT_FALSE(createCalled);

  NoopPacketMonitor packetMonitor;
  ITMConfig config;
  {
    OpenCsdTreeSession session(OCSD_TRC_SRC_SINGLE, 0U, errors, elements);
    config.setTraceID(1U);
    EXPECT_THROW(session.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config),
                 OpenCsdTreeSessionError);
    config.setTraceID(0U);
    session.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config);
    EXPECT_THROW(session.attachDecoderCallbacks(1U, packetMonitor), OpenCsdTreeSessionError);
    EXPECT_NO_THROW(session.attachDecoderCallbacks(0U, packetMonitor));
  }

  {
    constexpr std::uint32_t flags = OCSD_DFRMTR_FRAME_MEM_ALIGN | OCSD_DFRMTR_UNPACKED_RAW_OUT;
    OpenCsdTreeSession session(OCSD_TRC_SRC_FRAME_FORMATTED, flags, errors, elements);
    for (const auto invalidId : {0U, 112U}) {
      config.setTraceID(invalidId);
      EXPECT_THROW(session.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config),
                   OpenCsdTreeSessionError);
    }
    config.setTraceID(1U);
    session.createDecoder(OCSD_BUILTIN_DCD_ITM, OCSD_CREATE_FLG_FULL_DECODER, config);
    EXPECT_THROW(session.attachDecoderCallbacks(0U, packetMonitor), OpenCsdTreeSessionError);
    EXPECT_THROW(session.attachDecoderCallbacks(112U, packetMonitor), OpenCsdTreeSessionError);
    EXPECT_NO_THROW(session.attachDecoderCallbacks(1U, packetMonitor));
  }
}

TEST(CtraceUnitTests, testOpenCsdTreeSessionRestrictsRawFrameMonitorAttachment)
{
  RecordingElementOutput elements;
  OpenCsdErrorController errors;
  NoopRawFrameMonitor first;
  NoopRawFrameMonitor second;
  {
    OpenCsdTreeSession session(OCSD_TRC_SRC_SINGLE, 0U, errors, elements);
    EXPECT_THROW(session.attachRawFrameMonitor(first), OpenCsdTreeSessionError);
  }
  {
    constexpr std::uint32_t flags = OCSD_DFRMTR_FRAME_MEM_ALIGN | OCSD_DFRMTR_UNPACKED_RAW_OUT;
    OpenCsdTreeSession session(OCSD_TRC_SRC_FRAME_FORMATTED, flags, errors, elements);
    EXPECT_NO_THROW(session.attachRawFrameMonitor(first));
    EXPECT_THROW(session.attachRawFrameMonitor(second), OpenCsdTreeSessionError);
  }
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionReportsUnsupportedNormalIdOnce)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::vector<std::pair<std::uint8_t, ocsd_trc_index_t>> unsupported;
  OpenCsdFormattedItmSession session(
      {route}, elements, errors, packets,
      [&](std::uint8_t traceId, ocsd_trc_index_t index) { unsupported.emplace_back(traceId, index); });
  const auto capture = memoryAlignedFrames({
      {1U, itmHardwareSync()},
      {42U, {0xdeU, 0xadU}},
      {1U, itmSoftwarePacket(1U, static_cast<std::uint8_t>('A'))},
      {42U, {0xbeU, 0xefU}},
      {1U, itmSoftwarePacket(2U, static_cast<std::uint8_t>('B'))},
  });

  ASSERT_EQ(capture.size(), 32U);
  const std::vector<std::uint8_t> firstFrame(capture.begin(), capture.begin() + 16U);
  const std::vector<std::uint8_t> secondFrame(capture.begin() + 16U, capture.end());
  feedCapture(session, firstFrame);
  ASSERT_EQ(unsupported.size(), 1U);
  feedCapture(session, secondFrame, 16U);
  EXPECT_EQ(unsupported.size(), 1U);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);

  ASSERT_EQ(unsupported.size(), 1U);
  EXPECT_EQ(unsupported.front().first, 42U);
  ASSERT_EQ(elements.software().size(), 2U);
  EXPECT_EQ(elements.software()[0U].value, static_cast<std::uint32_t>('A'));
  EXPECT_EQ(elements.software()[1U].value, static_cast<std::uint32_t>('B'));
  EXPECT_FALSE(observedTraceId(elements, 0U));
  EXPECT_FALSE(observedTraceId(elements, 42U));
  EXPECT_EQ(packets.softwarePacketsFor(route), 2U);
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionAcceptsEmptyInput)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::vector<std::uint8_t> unsupported;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets,
                                     [&](std::uint8_t traceId, ocsd_trc_index_t) { unsupported.push_back(traceId); });

  EXPECT_EQ(session.flush(), OCSD_RESP_CONT);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  EXPECT_TRUE(elements.software().empty());
  EXPECT_FALSE(observedTraceId(elements, 0U));
  EXPECT_TRUE(packets.packets().empty());
  EXPECT_TRUE(unsupported.empty());
}

/** @brief Verifies route-local reset without notifying another formatted decoder. */
TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionResetsOnlySelectedRoute)
{
  const TraceRouteIdentity first{TraceRouteId{10U}, 1U};
  const TraceRouteIdentity last{TraceRouteId{20U}, 111U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  OpenCsdFormattedItmSession session({first, last}, elements, errors, packets);

  EXPECT_EQ(session.resetRoute(1U, 37U), OCSD_RESP_CONT);
  ASSERT_EQ(packets.resets().size(), 1U);
  EXPECT_EQ(packets.resets().front().route, first);
  EXPECT_EQ(packets.resets().front().index, 37U);

  EXPECT_THROW(session.resetRoute(0U, 40U), OpenCsdTreeSessionError);
  EXPECT_THROW(session.resetRoute(42U, 41U), OpenCsdTreeSessionError);
  EXPECT_THROW(session.resetRoute(112U, 42U), OpenCsdTreeSessionError);
  EXPECT_EQ(packets.resets().size(), 1U);
}

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionPreservesSourceIdAcrossFrameBoundaryAndRouteReset)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  std::vector<TraceByteSkip> skipped;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets, {},
                                     [&](const TraceByteSkip& item) { skipped.push_back(item); });
  auto protocol = itmHardwareSync();
  for (const auto value : {'A', 'B', 'C', 'D'}) {
    const auto packet = itmSoftwarePacket(1U, static_cast<std::uint8_t>(value));
    protocol.insert(protocol.end(), packet.begin(), packet.end());
  }
  ASSERT_EQ(protocol.size(), 14U); // The first frame has one ID marker and fourteen protocol bytes.
  const auto synchronization = itmHardwareSync();
  protocol.insert(protocol.end(), synchronization.begin(), synchronization.end());
  const auto lastPacket = itmSoftwarePacket(1U, 'Z');
  protocol.insert(protocol.end(), lastPacket.begin(), lastPacket.end());
  const auto capture = memoryAlignedFrames({{1U, protocol}});
  ASSERT_EQ(capture.size(), 32U);
  for (std::size_t slot = 16U; slot < 24U; slot += 2U) {
    ASSERT_EQ(capture[slot] & 1U, 0U) << "the second frame must not repeat the source ID before its payload";
  }

  feedCapture(session, {capture.begin(), capture.begin() + 16U});
  ASSERT_EQ(elements.software().size(), 4U);
  EXPECT_EQ(session.flush(), OCSD_RESP_CONT);
  EXPECT_EQ(session.resetRoute(1U, 16U), OCSD_RESP_CONT);
  feedCapture(session, {capture.begin() + 16U, capture.end()}, 16U);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  ASSERT_EQ(elements.software().size(), 5U);
  EXPECT_EQ(elements.software().back().traceId, 1U);
  EXPECT_EQ(elements.software().back().value, static_cast<std::uint32_t>('Z'));
  ASSERT_EQ(packets.data().size(), 2U);
  EXPECT_EQ(packets.data()[1U].route, route);
  EXPECT_EQ(packets.data()[1U].formatterOffset, 16U);
  EXPECT_EQ(packets.data()[1U].byteCount, 8U);
  ASSERT_EQ(skipped.size(), 1U);
  EXPECT_EQ(skipped.front().reason, TraceByteSkipReason::NullSourceId);
  EXPECT_EQ(skipped.front().byteCount, 6U);
}
