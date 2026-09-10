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

  /** @brief Copies packet identity while ignoring callback-only control operations. */
  void rawPacketForRoute(const TraceRouteIdentity& route, ocsd_datapath_op_t, ocsd_trc_index_t,
                         const ItmTrcPacket* packet, std::uint32_t, const std::uint8_t*) override
  {
    if (packet != nullptr) {
      m_packets.push_back({route, packet->getPktType()});
    }
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

private:
  std::vector<PacketObservation> m_packets;
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

TEST(CtraceUnitTests, testOpenCsdFormattedItmSessionRejectsDataBeforeFirstFormatterId)
{
  const TraceRouteIdentity route{TraceRouteId{10U}, 1U};
  RecordingElementOutput elements;
  RecordingPacketSink packets;
  OpenCsdErrorController errors;
  OpenCsdFormattedItmSession session({route}, elements, errors, packets);
  const std::vector<std::uint8_t> unassignedFrame(16U, 0U);

  std::uint32_t processed = 0U;
  try {
    (void)session.pushData(0U, static_cast<std::uint32_t>(unassignedFrame.size()), unassignedFrame.data(), processed);
    FAIL() << "unassigned formatted data did not fail";
  } catch (const OpenCsdFormattedInputError& error) {
    EXPECT_EQ(error.sourceOffset(), 0U);
    EXPECT_NE(std::string(error.what()).find("has no source ID"), std::string::npos);
    EXPECT_EQ(processed, unassignedFrame.size());
  }
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

  EXPECT_EQ(session.reset(), OCSD_RESP_CONT);
  EXPECT_EQ(session.flush(), OCSD_RESP_CONT);
  EXPECT_EQ(session.endOfTrace(), OCSD_RESP_CONT);
  EXPECT_TRUE(elements.software().empty());
  EXPECT_FALSE(observedTraceId(elements, 0U));
  EXPECT_TRUE(packets.packets().empty());
  EXPECT_TRUE(unsupported.empty());
}
