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

    ASSERT_TRUE(sink.events().size() == 2U) << "local timestamp relation event count mismatch";
    ASSERT_TRUE(sink.events().front().quality.has_value() &&
                sink.events().front().quality->timestampReliable == expectedReliable)
        << "local timestamp relation reliability mismatch";
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

  ASSERT_TRUE(sink.events().size() == 4) << "timestamp segment overflow event count mismatch";
  ASSERT_TRUE(sink.events()[0].tcyc == 100U) << "timestamp segment first tcyc mismatch";
  ASSERT_TRUE(sink.events()[2].tcyc == 110U) << "timestamp segment post-overflow tcyc mismatch";
  ASSERT_TRUE(sink.events()[3].tcyc == 115U) << "timestamp segment post-overflow increment mismatch";
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

  ASSERT_TRUE(sink.events().size() == 2) << "timestamp overflow flag event count mismatch";
  ASSERT_TRUE(sink.events()[1].tcyc == 109U) << "post-decoder should use packet overflow flag as segment boundary";
}

TEST(CtraceUnitTests, testCortexMPostDecoderMapsDiscontinuityTimestamp)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(openCsdTimestampElement(200U));
  decoder.append(openCsdElement(OpenCsdTraceElement::Kind::Discontinuity));
  decoder.append(openCsdTimestampElement(7U));

  ASSERT_TRUE(sink.events().size() == 3) << "timestamp discontinuity event count mismatch";
  ASSERT_TRUE(sink.events()[2].tcyc == 207U) << "post-decoder explicit discontinuity timestamp mismatch";
}

TEST(CtraceUnitTests, testCortexMPostDecoderLabelsPmuTraceOnOverflowPacket)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(openCsdTimestampElement(100U, 10U, 5U));
  auto pmuElement = openCsdElement(OpenCsdTraceElement::Kind::Hardware, 24U, 5U);
  pmuElement.discriminator = 3U;
  pmuElement.size = 1U;
  pmuElement.value = 0x81U;
  decoder.append(pmuElement);
  decoder.finish();

  ASSERT_TRUE(sink.events().size() == 2U) << "post-decoder PMU event count mismatch";
  const auto& packet = sink.events().back();
  const auto* pmu = traceEventPayload<PmuTraceEvent>(packet);
  ASSERT_TRUE(pmu != nullptr && pmu->overflowMask == 0x81U)
      << "post-decoder must label discriminator 3 as a PMU trace-on-overflow event";
  ASSERT_TRUE(packet.index == 24U && packet.traceBusId == 5U && packet.tcyc == 100U)
      << "post-decoder PMU event context mismatch";
  ASSERT_TRUE(packet.quality.has_value() && packet.quality->timestampReliable)
      << "post-decoder PMU timestamp quality mismatch";
}
