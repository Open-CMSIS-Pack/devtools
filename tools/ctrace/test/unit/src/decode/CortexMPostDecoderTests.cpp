/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "CortexMPostDecoder.h"
#include "OpenCsdTraceElement.h"

#include <array>
#include <utility>

using OpenCsdTestSupport::openCsdElement;
using OpenCsdTestSupport::openCsdSoftwareElement;
using OpenCsdTestSupport::openCsdTimestampElement;

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

    decoder.append(openCsdSoftwareElement(1U));

    decoder.append(openCsdTimestampElement(100U, 0U, 0U, relation));
    decoder.finish();

    require(sink.events().size() == 2U, "local timestamp relation event count mismatch");
    require(sink.events().front().quality.has_value() &&
                sink.events().front().quality->timestampReliable == expectedReliable,
            "local timestamp relation reliability mismatch");
  }
}

TEST(CtraceUnitTests, testCortexMPostDecoderOffsetsTimestampsAfterOverflow)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(openCsdTimestampElement(100U));

  decoder.append(openCsdElement(OpenCsdTraceElement::Kind::Overflow));

  decoder.append(openCsdTimestampElement(10U));
  decoder.append(openCsdTimestampElement(15U));

  require(sink.events().size() == 4, "timestamp segment overflow event count mismatch");
  require(sink.events()[0].tcyc == 100U, "timestamp segment first tcyc mismatch");
  require(sink.events()[2].tcyc == 110U, "timestamp segment post-overflow tcyc mismatch");
  require(sink.events()[3].tcyc == 115U, "timestamp segment post-overflow increment mismatch");
}

TEST(CtraceUnitTests, testCortexMPostDecoderLeavesInitialOverflowTimestampUnknown)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(openCsdElement(OpenCsdTraceElement::Kind::Overflow));

  ASSERT_EQ(sink.events().size(), 1U);
  EXPECT_FALSE(sink.events().front().tcyc.has_value());
}

TEST(CtraceUnitTests, testCortexMPostDecoderUsesTimestampOverflowFlag)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(openCsdTimestampElement(100U));

  auto timestamp = openCsdTimestampElement(9U);
  timestamp.overflow = true;
  decoder.append(timestamp);

  require(sink.events().size() == 2, "timestamp overflow flag event count mismatch");
  require(sink.events()[1].tcyc == 109U, "post-decoder should use packet overflow flag as segment boundary");
}

TEST(CtraceUnitTests, testCortexMPostDecoderMapsDiscontinuityTimestamp)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(openCsdTimestampElement(200U));
  decoder.append(openCsdElement(OpenCsdTraceElement::Kind::Discontinuity));
  decoder.append(openCsdTimestampElement(7U));

  require(sink.events().size() == 3, "timestamp discontinuity event count mismatch");
  require(sink.events()[2].tcyc == 207U, "post-decoder explicit discontinuity timestamp mismatch");
}
