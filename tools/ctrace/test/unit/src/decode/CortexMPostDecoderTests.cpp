/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "CortexMPostDecoder.hpp"
#include "OpenCsdTraceElement.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace {

OpenCsdTraceElement localTimestampElement(std::uint64_t tcyc)
{
  OpenCsdTraceElement element;
  element.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  element.tcyc = tcyc;
  return element;
}

} // namespace

TEST(CtraceUnitTests, testCortexMPostDecoderUsesLocalTimestampRelations)
{
  const std::array<std::pair<LocalTimestampRelation, bool>, 4> cases{{
      {LocalTimestampRelation::Synchronous, true},
      {LocalTimestampRelation::TimestampDelayed, true},
      {LocalTimestampRelation::PayloadDelayed, false},
      {LocalTimestampRelation::TimestampAndPayloadDelayed, false},
  }};

  for (const auto& [relation, expectedReliable] : cases) {
    CollectingEventSink sink;
    CortexMPostDecoder decoder(sink);

    OpenCsdTraceElement software;
    software.kind = OpenCsdTraceElement::Kind::Software;
    software.channel = 1U;
    software.size = 1U;
    decoder.append(software);

    auto timestamp = localTimestampElement(100U);
    timestamp.timestampRelation = relation;
    decoder.append(timestamp);
    decoder.finish();

    require(sink.events.size() == 2U, "local timestamp relation event count mismatch");
    require(sink.events.front().quality.has_value() &&
                sink.events.front().quality->timestampReliable == expectedReliable,
            "local timestamp relation reliability mismatch");
  }
}

TEST(CtraceUnitTests, testCortexMPostDecoderOffsetsTimestampsAfterOverflow)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(localTimestampElement(100));

  OpenCsdTraceElement overflow;
  overflow.kind = OpenCsdTraceElement::Kind::Overflow;
  decoder.append(overflow);

  decoder.append(localTimestampElement(10));
  decoder.append(localTimestampElement(15));

  require(sink.events.size() == 4, "timestamp segment overflow event count mismatch");
  require(sink.events[0].tcyc == 100U, "timestamp segment first tcyc mismatch");
  require(sink.events[2].tcyc == 110U, "timestamp segment post-overflow tcyc mismatch");
  require(sink.events[3].tcyc == 115U, "timestamp segment post-overflow increment mismatch");
}

TEST(CtraceUnitTests, testCortexMPostDecoderUsesTimestampOverflowFlag)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(localTimestampElement(100));

  auto timestamp = localTimestampElement(9);
  timestamp.overflow = true;
  decoder.append(timestamp);

  require(sink.events.size() == 2, "timestamp overflow flag event count mismatch");
  require(sink.events[1].tcyc == 109U, "post-decoder should use packet overflow flag as segment boundary");
}

TEST(CtraceUnitTests, testCortexMPostDecoderMapsDiscontinuityTimestamp)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(localTimestampElement(200));
  OpenCsdTraceElement discontinuity;
  discontinuity.kind = OpenCsdTraceElement::Kind::Discontinuity;
  decoder.append(discontinuity);
  decoder.append(localTimestampElement(7));

  require(sink.events.size() == 3, "timestamp discontinuity event count mismatch");
  require(sink.events[2].tcyc == 207U, "post-decoder explicit discontinuity timestamp mismatch");
}
