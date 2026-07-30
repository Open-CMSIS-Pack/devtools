/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "csv/CsvRowMapper.hpp"
#include "OpenCsdPacketCollector.hpp"
#include "OpenCsdTraceElement.hpp"
#include "TraceEvent.hpp"
#include "common/trc_gen_elem.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"
#include "opencsd/ocsd_if_types.h"
#include "opencsd/trc_gen_elem_types.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

class CollectingTraceElementSink final : public OpenCsdTraceElementSink {
public:
  void append(OpenCsdTraceElement element) override
  {
    elements.push_back(std::move(element));
  }

  std::vector<OpenCsdTraceElement> elements;
};

} // namespace

TEST(CtraceUnitTests, testOpenCsdPacketCollectorUsesReconstructedGlobalTimestamp)
{
  CollectingTraceElementSink sink;
  OpenCsdPacketCollector collector(sink);

  OcsdTraceElement globalTimestamp;
  globalTimestamp.setType(OCSD_GEN_TRC_ELEM_ITMTRACE);
  swt_itm_info info{};
  info.pkt_type = TS_GLOBAL;
  globalTimestamp.setSWT_ITMInfo(info);
  globalTimestamp.setTS(0xfedcba9876543210ULL, true);

  require(collector.TraceElemIn(42U, 7U, globalTimestamp) == OCSD_RESP_CONT,
          "OpenCSD global timestamp collection failed");
  require(sink.elements.size() == 1U, "reconstructed global timestamp element missing");
  const auto element = sink.elements.front();
  require(element.kind == OpenCsdTraceElement::Kind::GlobalTimestamp, "reconstructed global timestamp kind mismatch");
  require(element.sourceIndex == 42U, "reconstructed global timestamp source index mismatch");
  require(element.traceBusId == 7U, "OpenCSD Trace Bus ID was not preserved");
  require(element.timestampValue == 0xfedcba9876543210ULL, "reconstructed global timestamp value mismatch");
  require(element.clockChange, "reconstructed global timestamp clock-change flag missing");

  require(collector.TraceElemIn(43U, 0xffU, globalTimestamp) == OCSD_RESP_CONT,
          "OpenCSD global timestamp collection with missing source ID failed");
  require(sink.elements.back().traceBusId == 0U, "an unavailable OpenCSD Trace Bus ID must fall back to zero");

  ItmTrcPacket rawGts1;
  rawGts1.setPktType(ITM_PKT_TS_GLOBAL_1);
  rawGts1.setValue(0x123456U, 4U);
  collector.RawPacketDataMon(OCSD_OP_DATA, 43U, &rawGts1, 0U, nullptr);

  ItmTrcPacket rawGts2;
  rawGts2.setPktType(ITM_PKT_TS_GLOBAL_2);
  rawGts2.setExtValue(0x123456789ULL);
  collector.RawPacketDataMon(OCSD_OP_DATA, 44U, &rawGts2, 0U, nullptr);
  require(sink.elements.size() == 2U, "raw GTS fragments must not be published as independent global timestamps");

  TraceEvent packet{GlobalTimestampTraceEvent{element.timestampValue, false}};
  require(CsvRowMapper::row(packet) == std::to_string(element.timestampValue) + ",0,global_ts,,,,,",
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

  CollectingTraceElementSink sink;
  OpenCsdPacketCollector collector(sink);
  for (std::size_t index = 0; index < cases.size(); ++index) {
    OcsdTraceElement timestamp;
    timestamp.setType(OCSD_GEN_TRC_ELEM_ITMTRACE);
    swt_itm_info info{};
    info.pkt_type = cases[index].first;
    timestamp.setSWT_ITMInfo(info);
    timestamp.setTS(100U + index, false);

    require(collector.TraceElemIn(index, 0U, timestamp) == OCSD_RESP_CONT, "OpenCSD local timestamp collection failed");
  }

  require(sink.elements.size() == cases.size(), "local timestamp relation count mismatch");
  for (std::size_t index = 0; index < cases.size(); ++index) {
    require(sink.elements[index].kind == OpenCsdTraceElement::Kind::LocalTimestamp, "local timestamp kind mismatch");
    require(sink.elements[index].timestampRelation == cases[index].second, "local timestamp relation mismatch");
  }
}
