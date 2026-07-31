/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

// Cortex-M post-decoder and end-to-end decode pipeline tests.
#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "CortexMPostDecoder.hpp"
#include "CortexMStreamDecoder.hpp"
#include "DecodePipeline.hpp"
#include "OpenCsdTraceElement.hpp"
#include "TraceEvent.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace {

const SoftwareTraceEvent* softwareEvent(const TraceEvent& event)
{
  return traceEventPayload<SoftwareTraceEvent>(event);
}

const TraceIssueEvent* issueEvent(const TraceEvent& event)
{
  return traceEventPayload<TraceIssueEvent>(event);
}

bool hasSoftwareValue(const std::vector<TraceEvent>& events, std::uint8_t value, std::uint32_t channel = 0U)
{
  for (const auto& event : events) {
    const auto* software = softwareEvent(event);
    if (software != nullptr && software->channel == channel && software->size == 1U && software->value == value) {
      return true;
    }
  }
  return false;
}

const TraceIssueEvent* findIssue(const std::vector<TraceEvent>& events, const std::string& code,
                                 std::optional<std::uint64_t> index = std::nullopt)
{
  for (const auto& event : events) {
    const auto* issue = issueEvent(event);
    if (issue != nullptr && issue->code == code && (!index.has_value() || event.index == index.value())) {
      return issue;
    }
  }
  return nullptr;
}

std::size_t countIssues(const std::vector<TraceEvent>& events, const std::string& code)
{
  std::size_t count = 0U;
  for (const auto& event : events) {
    const auto* issue = issueEvent(event);
    if (issue != nullptr && issue->code == code) {
      ++count;
    }
  }
  return count;
}

} // namespace

TEST(CtraceUnitTests, testCortexMPostDecoderSoftwareTimestampBoundary)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  OpenCsdTraceElement software;
  software.kind = OpenCsdTraceElement::Kind::Software;
  software.sourceIndex = 4;
  software.traceBusId = 1;
  software.channel = 0;
  software.size = 1;
  software.value = 'A';
  decoder.append(software);

  OpenCsdTraceElement timestamp;
  timestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  timestamp.sourceIndex = 5;
  timestamp.traceBusId = 1;
  timestamp.timestampRelation = LocalTimestampRelation::Synchronous;
  timestamp.tcyc = 120;
  decoder.append(timestamp);

  decoder.finish();
  const auto& packets = sink.events;
  require(packets.size() == 2, "CortexMPostDecoder packet count mismatch");
  const auto* decodedSoftware = softwareEvent(packets[0]);
  require(decodedSoftware != nullptr, "CortexMPostDecoder first packet type mismatch");
  require(decodedSoftware->channel == 0, "CortexMPostDecoder software channel mismatch");
  require(decodedSoftware->size == 1U && decodedSoftware->value == 'A', "CortexMPostDecoder payload mismatch");
  require(packets[0].tcyc.has_value() && packets[0].tcyc.value() == 120, "CortexMPostDecoder software tcyc mismatch");
  require(packets[0].quality.has_value() && packets[0].quality->timestampReliable,
          "CortexMPostDecoder software timestamp reliability mismatch");
  require(isTraceEvent<LocalTimestampTraceEvent>(packets[1]), "CortexMPostDecoder timestamp packet type mismatch");
  require(packets[1].tcyc.has_value() && packets[1].tcyc.value() == 120, "CortexMPostDecoder timestamp tcyc mismatch");
}

TEST(CtraceUnitTests, testCortexMStreamDecoderAppliesPerStreamPrescalers)
{
  CollectingEventSink sink;
  CortexMStreamDecoder decoder(ItmTimestampPrescalers{1U, {{1U, 4U}, {2U, 16U}}}, sink);

  OpenCsdTraceElement stream1Software;
  stream1Software.kind = OpenCsdTraceElement::Kind::Software;
  stream1Software.traceBusId = 1U;
  stream1Software.channel = 1U;
  stream1Software.size = 1U;
  stream1Software.value = 0x11U;
  decoder.append(stream1Software);

  auto stream2Software = stream1Software;
  stream2Software.traceBusId = 2U;
  stream2Software.value = 0x22U;
  decoder.append(stream2Software);

  OpenCsdTraceElement stream1Timestamp;
  stream1Timestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  stream1Timestamp.traceBusId = 1U;
  stream1Timestamp.tcyc = 10U;
  decoder.append(stream1Timestamp);

  auto stream2Timestamp = stream1Timestamp;
  stream2Timestamp.traceBusId = 2U;
  decoder.append(stream2Timestamp);
  decoder.finish();

  require(sink.events.size() == 4U, "per-stream timestamp event count mismatch");
  require(sink.events[0].traceBusId == 1U && sink.events[0].tcyc == std::optional<std::uint64_t>(40U),
          "stream 1 timestamp prescaler mismatch");
  require(sink.events[2].traceBusId == 2U && sink.events[2].tcyc == std::optional<std::uint64_t>(160U),
          "stream 2 timestamp prescaler mismatch");
}

TEST(CtraceUnitTests, testCortexMStreamDecoderValidatesAndSaturatesPrescalers)
{
  CollectingEventSink sink;
  OpenCsdTraceElement timestamp;
  timestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  timestamp.tcyc = std::numeric_limits<std::uint64_t>::max();

  CortexMStreamDecoder saturating(ItmTimestampPrescalers{2U, {}}, sink);
  saturating.append(timestamp);
  saturating.finish();
  ASSERT_EQ(sink.events.size(), 1U);
  EXPECT_EQ(sink.events.front().tcyc, std::numeric_limits<std::uint64_t>::max());
  EXPECT_EQ(saturating.eventCount(), 1U);

  CortexMStreamDecoder zero(ItmTimestampPrescalers{0U, {}}, sink);
  EXPECT_THROW(zero.append(timestamp), std::invalid_argument);

  CortexMStreamDecoder unresolved(ItmTimestampPrescalers{std::nullopt, {}}, sink);
  EXPECT_THROW(unresolved.append(timestamp), std::runtime_error);
  timestamp.traceBusId = 7U;
  EXPECT_THROW(unresolved.append(timestamp), std::runtime_error);

  OpenCsdTraceElement timestampWithoutValue;
  timestampWithoutValue.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  EXPECT_NO_THROW(saturating.append(timestampWithoutValue));
}

TEST(CtraceUnitTests, testDecodePipelineRejectsInvalidChunkSizes)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(1U, sink);
  EXPECT_NO_THROW(pipeline.push({nullptr, 0U}));
  EXPECT_THROW(pipeline.push({nullptr, static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()) + 1U}),
               std::runtime_error);
  const auto result = pipeline.finish();
  EXPECT_EQ(result.bytesIn, 0U);
  EXPECT_EQ(result.packetsOut, 0U);
}

TEST(CtraceUnitTests, testCortexMPostDecoderReportsDiscontinuityInterval)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  OpenCsdTraceElement discontinuity;
  discontinuity.kind = OpenCsdTraceElement::Kind::Discontinuity;
  discontinuity.sourceIndex = 0U;
  discontinuity.issueCode = "data-loss";
  discontinuity.errorMessage = "OpenCSD consumed 2 raw bytes";
  discontinuity.rawBytesConsumed = 2U;
  decoder.append(discontinuity);

  OpenCsdTraceElement software;
  software.kind = OpenCsdTraceElement::Kind::Software;
  software.sourceIndex = 8U;
  software.channel = 0U;
  software.size = 1U;
  software.value = static_cast<std::uint8_t>('B');
  decoder.append(software);

  OpenCsdTraceElement timestamp;
  timestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  timestamp.sourceIndex = 10U;
  timestamp.timestampRelation = LocalTimestampRelation::Synchronous;
  timestamp.tcyc = 42U;
  decoder.append(timestamp);

  decoder.finish();
  const auto& packets = sink.events;
  require(packets.size() == 3U, "discontinuity interval packet count mismatch");
  const auto* discontinuityIssue = issueEvent(packets[0]);
  require(discontinuityIssue != nullptr, "discontinuity interval should start with its error");
  require(packets[0].tcyc == std::optional<std::uint64_t>(0U), "discontinuity last valid timestamp mismatch");
  require(discontinuityIssue->lastValidTcyc == std::optional<std::uint64_t>(0U), "discontinuity start field mismatch");
  require(discontinuityIssue->message.find("timestamp 0 .. 42.") != std::string::npos,
          "discontinuity interval message mismatch");
  require(isTraceEvent<SoftwareTraceEvent>(packets[1]), "discontinuity interval payload order mismatch");
  require(packets[1].tcyc == std::optional<std::uint64_t>(42U), "resumed payload timestamp mismatch");
  require(isTraceEvent<LocalTimestampTraceEvent>(packets[2]), "discontinuity interval timestamp order mismatch");
}

TEST(CtraceUnitTests, testCortexMPostDecoderSeparatesRecoveryCauseAndDataLoss)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  OpenCsdTraceElement cause;
  cause.kind = OpenCsdTraceElement::Kind::Error;
  cause.sourceIndex = 8U;
  cause.discontinuity = true;
  cause.issueCode = "opencsd-bad-packet-sequence";
  cause.errorMessage = "OpenCSD detected an invalid ITM packet sequence at raw offset 8.";
  decoder.append(cause);

  OpenCsdTraceElement loss;
  loss.kind = OpenCsdTraceElement::Kind::Error;
  loss.sourceIndex = 10U;
  loss.awaitingResumeTimestamp = true;
  loss.issueCode = "data-loss";
  loss.errorMessage = "OpenCSD consumed 2 raw bytes";
  loss.rawBytesConsumed = 2U;
  decoder.append(loss);

  OpenCsdTraceElement timestamp;
  timestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  timestamp.sourceIndex = 12U;
  timestamp.timestampRelation = LocalTimestampRelation::Synchronous;
  timestamp.tcyc = 42U;
  decoder.append(timestamp);

  decoder.finish();
  const auto& packets = sink.events;
  require(packets.size() == 3U, "recovery cause/data-loss packet count mismatch");
  const auto* causeIssue = issueEvent(packets[0]);
  const auto* lossIssue = issueEvent(packets[1]);
  require(causeIssue != nullptr && causeIssue->code == "opencsd-bad-packet-sequence",
          "recovery cause should be emitted first");
  require(causeIssue->message.find("timestamp") == std::string::npos,
          "recovery cause should not contain the data-loss timestamp interval");
  require(lossIssue != nullptr && lossIssue->code == "data-loss", "recovery data loss should be a separate error");
  require(lossIssue->message.find("timestamp 0 .. 42.") != std::string::npos, "recovery data-loss interval mismatch");
  require(packets[0].quality.has_value() && packets[0].quality->overflowCount == 1U && packets[1].quality.has_value() &&
              packets[1].quality->overflowCount == 1U,
          "separate data-loss error must not count a second discontinuity");
  require(isTraceEvent<LocalTimestampTraceEvent>(packets[2]), "recovery timestamp packet missing");
}

TEST(CtraceUnitTests, testCortexMPostDecoderOverflowFlushesDwtSegments)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  OpenCsdTraceElement firstTimestamp;
  firstTimestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  firstTimestamp.sourceIndex = 1;
  firstTimestamp.traceBusId = 1;
  firstTimestamp.timestampRelation = LocalTimestampRelation::Synchronous;
  firstTimestamp.tcyc = 100;
  decoder.append(firstTimestamp);

  OpenCsdTraceElement pc;
  pc.kind = OpenCsdTraceElement::Kind::Hardware;
  pc.sourceIndex = 2;
  pc.traceBusId = 1;
  pc.discriminator = 8;
  pc.size = 4;
  pc.value = 0x08001234U;
  decoder.append(pc);

  OpenCsdTraceElement overflow;
  overflow.kind = OpenCsdTraceElement::Kind::Overflow;
  overflow.sourceIndex = 3;
  decoder.append(overflow);

  OpenCsdTraceElement value;
  value.kind = OpenCsdTraceElement::Kind::Hardware;
  value.sourceIndex = 4;
  value.traceBusId = 1;
  value.discriminator = 16;
  value.size = 4;
  value.value = 0x55U;
  decoder.append(value);

  OpenCsdTraceElement secondTimestamp;
  secondTimestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  secondTimestamp.sourceIndex = 5;
  secondTimestamp.traceBusId = 1;
  secondTimestamp.timestampRelation = LocalTimestampRelation::Synchronous;
  secondTimestamp.tcyc = 20;
  decoder.append(secondTimestamp);

  decoder.finish();
  const auto& packets = sink.events;
  require(packets.size() == 5, "CortexMPostDecoder overflow segment packet count mismatch");
  require(isTraceEvent<LocalTimestampTraceEvent>(packets[0]), "overflow segment first packet should be timestamp");
  const auto* address = traceEventPayload<DwtAddressTraceEvent>(packets[1]);
  require(address != nullptr, "overflow should flush pending DWT fragment as an address event");
  require(dwtAddressPc(*address) == std::optional<std::uint32_t>(0x08001234U), "flushed DWT PC mismatch");
  require(packets[1].tcyc.has_value() && packets[1].tcyc.value() == 100, "flushed DWT PC timestamp mismatch");
  require(packets[1].quality.has_value() && packets[1].quality->overflow,
          "flushed DWT PC should carry overflow status");
  require(!packets[1].quality->timestampReliable, "flushed DWT PC should be timestamp-unreliable");
  require(isTraceEvent<OverflowTraceEvent>(packets[2]), "overflow segment should emit overflow marker");
  const auto* data = traceEventPayload<DwtDataTraceEvent>(packets[3]);
  require(data != nullptr, "post-overflow DWT value missing");
  require(!data->pc.has_value(), "post-overflow DWT value must not inherit prior PC");
  require(data->value == 0x55U, "post-overflow DWT value mismatch");
  require(packets[3].tcyc.has_value() && packets[3].tcyc.value() == 120, "post-overflow DWT value timestamp mismatch");
  require(packets[3].quality.has_value() && packets[3].quality->overflow,
          "post-overflow DWT value should carry overflow status");
  require(!packets[3].quality->timestampReliable, "post-overflow DWT value should be timestamp-unreliable");
  require(isTraceEvent<LocalTimestampTraceEvent>(packets[4]), "overflow segment final packet should be timestamp");
}

TEST(CtraceUnitTests, testCortexMPostDecoderPreservesDecoderTimestamps)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  OpenCsdTraceElement firstTimestamp;
  firstTimestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  firstTimestamp.sourceIndex = (std::uint64_t{1} << 32U) + 1U;
  firstTimestamp.timestampRelation = LocalTimestampRelation::Synchronous;
  firstTimestamp.tcyc = 100;
  decoder.append(firstTimestamp);

  OpenCsdTraceElement secondTimestamp;
  secondTimestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  secondTimestamp.sourceIndex = 2;
  secondTimestamp.timestampRelation = LocalTimestampRelation::Synchronous;
  secondTimestamp.tcyc = 10;
  decoder.append(secondTimestamp);

  decoder.finish();
  const auto& packets = sink.events;
  require(packets.size() == 2, "preserve timestamp packet count mismatch");
  require(packets[0].index == firstTimestamp.sourceIndex, "raw offsets must not truncate above 4 GiB");
  require(packets[0].tcyc.has_value() && packets[0].tcyc.value() == 100, "first preserved timestamp mismatch");
  require(packets[1].tcyc.has_value() && packets[1].tcyc.value() == 10, "second preserved timestamp mismatch");
}

TEST(CtraceUnitTests, testCortexMPostDecoderPreservesGlobalTimestampOrder)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  OpenCsdTraceElement software;
  software.kind = OpenCsdTraceElement::Kind::Software;
  software.sourceIndex = 1U;
  software.channel = 1U;
  software.size = 1U;
  software.value = static_cast<std::uint8_t>('A');
  decoder.append(software);

  OpenCsdTraceElement dwtPc;
  dwtPc.kind = OpenCsdTraceElement::Kind::Hardware;
  dwtPc.sourceIndex = 2U;
  dwtPc.discriminator = 8U;
  dwtPc.size = 4U;
  dwtPc.value = 0x08001234U;
  decoder.append(dwtPc);

  OpenCsdTraceElement globalTimestamp;
  globalTimestamp.kind = OpenCsdTraceElement::Kind::GlobalTimestamp;
  globalTimestamp.sourceIndex = 3U;
  globalTimestamp.timestampValue = 0x123456789abcdef0ULL;
  decoder.append(globalTimestamp);

  OpenCsdTraceElement warning;
  warning.kind = OpenCsdTraceElement::Kind::Error;
  warning.sourceIndex = 4U;
  warning.issueCode = "opencsd-warning";
  warning.issueSeverity = TraceIssueSeverity::Warning;
  warning.errorMessage = "decoder warning";
  decoder.append(warning);

  OpenCsdTraceElement localTimestamp;
  localTimestamp.kind = OpenCsdTraceElement::Kind::LocalTimestamp;
  localTimestamp.sourceIndex = 5U;
  localTimestamp.timestampRelation = LocalTimestampRelation::Synchronous;
  localTimestamp.tcyc = 42U;
  decoder.append(localTimestamp);
  decoder.finish();

  require(sink.events.size() == 5U, "global timestamp order packet count mismatch");
  require(isTraceEvent<SoftwareTraceEvent>(sink.events[0]),
          "global timestamp must not overtake preceding software data");
  require(isTraceEvent<DwtAddressTraceEvent>(sink.events[1]),
          "global timestamp must flush and follow preceding DWT data");
  const auto* timestamp = traceEventPayload<GlobalTimestampTraceEvent>(sink.events[2]);
  require(timestamp != nullptr && timestamp->value == globalTimestamp.timestampValue,
          "global timestamp payload/order mismatch");
  require(sink.events[0].tcyc == std::optional<std::uint64_t>(42U) &&
              sink.events[1].tcyc == std::optional<std::uint64_t>(42U),
          "preceding payloads must retain the following local timestamp");
  require(!sink.events[2].tcyc.has_value(), "global timestamp must remain independent of the local timestamp domain");
  require(!sink.events[2].quality.has_value(), "global timestamp must not acquire local trace quality");
  require(isTraceEvent<TraceIssueEvent>(sink.events[3]) && sink.events[3].tcyc == std::optional<std::uint64_t>(42U),
          "a warning must not overtake pending payload or global timestamp packets");
  require(isTraceEvent<LocalTimestampTraceEvent>(sink.events[4]),
          "local timestamp must remain after the global timestamp boundary");
}

TEST(CtraceUnitTests, testDecodePipelineRecoversAtRealSync)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16, sink);

  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x00U, 0xfeU,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };
  pipeline.push({trace, sizeof(trace)});
  const auto result = pipeline.finish();

  require(result.bytesIn == sizeof(trace), "recovery byte count mismatch");
  require(hasSoftwareValue(sink.events, static_cast<std::uint8_t>('A')),
          "recovery should preserve packets before the damaged section");
  const auto* error = findIssue(sink.events, "opencsd-bad-packet-sequence", 8U);
  require(error != nullptr && error->message == "OpenCSD detected an invalid ITM packet sequence at raw offset 8.",
          "recovery should report the exact OpenCSD error offset");
  require(hasSoftwareValue(sink.events, static_cast<std::uint8_t>('B')),
          "recovery should resume after the next real ITM sync");
}

TEST(CtraceUnitTests, testDecodePipelineKeepsCallbacksAttachedAcrossRepeatedResets)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16U, sink);

  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x00U, 0xfeU,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'), 0x00U, 0xfeU,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('C'),
  };
  pipeline.push({trace, sizeof(trace)});
  const auto result = pipeline.finish();

  require(result.bytesIn == sizeof(trace), "repeated recovery byte count mismatch");
  require(hasSoftwareValue(sink.events, static_cast<std::uint8_t>('A')) &&
              hasSoftwareValue(sink.events, static_cast<std::uint8_t>('B')) &&
              hasSoftwareValue(sink.events, static_cast<std::uint8_t>('C')),
          "decoder resets must retain all OpenCSD callbacks");
  require(countIssues(sink.events, "opencsd-bad-packet-sequence") == 2U,
          "each damaged section must trigger one recoverable reset");
}

TEST(CtraceUnitTests, testDecodePipelineReconstructsGlobalTimestamp)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(1U, sink);

  constexpr std::uint64_t lower = 0x00f23456ULL;
  constexpr std::uint64_t upper = 0x1020304c000000ULL;
  std::vector<std::uint8_t> trace{
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x00U,
      0x80U,
      0x94U,
      static_cast<std::uint8_t>(0x80U | ((lower >> 0U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((lower >> 7U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((lower >> 14U) & 0x7fU)),
      static_cast<std::uint8_t>((lower >> 21U) & 0x1fU),
      0xb4U,
      static_cast<std::uint8_t>(0x80U | ((upper >> 26U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((upper >> 33U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((upper >> 40U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((upper >> 47U) & 0x7fU)),
      static_cast<std::uint8_t>(0x80U | ((upper >> 54U) & 0x7fU)),
      static_cast<std::uint8_t>((upper >> 61U) & 0x07U),
  };
  pipeline.push({trace.data(), trace.size()});
  const auto result = pipeline.finish();

  require(result.bytesIn == trace.size(), "global timestamp input byte count mismatch");
  std::size_t globalTimestampCount = 0U;
  for (const auto& packet : sink.events) {
    if (const auto* timestamp = traceEventPayload<GlobalTimestampTraceEvent>(packet)) {
      ++globalTimestampCount;
      require(timestamp->value == (upper | lower), "GTS1/GTS2 reconstruction lost timestamp bits");
      require(!timestamp->clockChange, "unexpected global timestamp clock-change flag");
    }
  }
  require(globalTimestampCount == 1U, "GTS1/GTS2 must produce exactly one reconstructed global timestamp");
}

TEST(CtraceUnitTests, testDecodePipelineAppliesPrescalerAfterOpenCsd)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16U, sink);
  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x09U, 0x41U, 0x20U,
  };

  pipeline.push({trace, sizeof(trace)});
  pipeline.finish();

  bool foundScaledSoftware = false;
  for (const auto& event : sink.events) {
    const auto* software = softwareEvent(event);
    foundScaledSoftware = foundScaledSoftware || (software != nullptr && software->channel == 1U &&
                                                  event.tcyc == std::optional<std::uint64_t>(32U));
  }
  require(foundScaledSoftware, "DecodePipeline must apply the ITM prescaler after OpenCSD decoding");
}

TEST(CtraceUnitTests, testDecodePipelineRecoversWhenErrorSpansChunks)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16, sink);

  const std::uint8_t firstChunk[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x00U,
  };
  const std::uint8_t secondChunk[] = {
      0xfeU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };
  pipeline.push({firstChunk, sizeof(firstChunk)});
  pipeline.push({secondChunk, sizeof(secondChunk)});
  const auto result = pipeline.finish();

  require(result.bytesIn == sizeof(firstChunk) + sizeof(secondChunk), "split recovery byte count mismatch");
  require(findIssue(sink.events, "opencsd-bad-packet-sequence", 8U) != nullptr,
          "split bad packet should retain its header offset");
  require(hasSoftwareValue(sink.events, static_cast<std::uint8_t>('B')),
          "recovery should resume after a split bad packet");
}

TEST(CtraceUnitTests, testDecodePipelineRecoversFromReservedHeader)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16, sink);

  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x04U,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };
  pipeline.push({trace, sizeof(trace)});
  pipeline.finish();

  const auto* error = findIssue(sink.events, "opencsd-invalid-packet-header", 8U);
  require(error != nullptr && error->message == "OpenCSD detected an invalid ITM packet header at raw offset 8.",
          "reserved header should report its exact OpenCSD error");
  require(hasSoftwareValue(sink.events, static_cast<std::uint8_t>('B')),
          "recovery should resume after a reserved header");
}

TEST(CtraceUnitTests, testDecodePipelineFinishesWithoutLaterSync)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16, sink);

  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x00U, 0xfeU, 0xffU, 0xffU,
  };
  pipeline.push({trace, sizeof(trace)});
  const auto result = pipeline.finish();

  require(result.bytesIn == sizeof(trace), "bad-tail byte count mismatch");
  require(findIssue(sink.events, "opencsd-bad-packet-sequence", 8U) != nullptr,
          "bad tail should report its exact OpenCSD error and finish");
}

TEST(CtraceUnitTests, testDecodePipelineReportsIncompletePacketAtEndOfInput)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16, sink);

  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x03U, 0x12U,
  };
  pipeline.push({trace, sizeof(trace)});
  pipeline.finish();

  require(!sink.events.empty(), "incomplete trace should emit packets");
  const auto& last = sink.events.back();
  const auto* issue = issueEvent(last);
  require(issue != nullptr, "incomplete trace should end with an error packet");
  require(issue->code == "opencsd-incomplete-tail", "incomplete trace issue code mismatch");
  require(last.index == 8U, "incomplete trace should report the partial packet header offset");
}

TEST(CtraceUnitTests, testDecodePipelineRecoversFromOverlongContinuationPackets)
{
  const std::vector<std::vector<std::uint8_t>> damagedPackets{
      {0x80U, 0x80U, 0x80U, 0x80U, 0x80U},
      {0x94U, 0x80U, 0x80U, 0x80U, 0x80U},
      {0xb4U, 0x80U, 0x80U, 0x80U, 0x80U, 0x80U, 0x80U},
      {0x88U, 0x80U, 0x80U, 0x80U, 0x80U},
  };

  for (const auto& damaged : damagedPackets) {
    CollectingEventSink sink;
    DecodePipeline pipeline(16, sink);
    std::vector<std::uint8_t> trace{
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'),
    };
    trace.insert(trace.end(), damaged.begin(), damaged.end());
    trace.insert(trace.end(), {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U});
    trace.insert(trace.end(), {0x01U, static_cast<std::uint8_t>('B')});

    pipeline.push({trace.data(), trace.size()});
    pipeline.finish();

    require(findIssue(sink.events, "opencsd-bad-packet-sequence") != nullptr,
            "overlong continuation packet should emit the OpenCSD error");
    require(hasSoftwareValue(sink.events, static_cast<std::uint8_t>('B')),
            "decode should resume after an overlong continuation packet");
  }
}

TEST(CtraceUnitTests, testDecodePipelinePreservesDwtEventAndPmuPackets)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16, sink);

  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x05U, 0x21U, 0x1dU, 0x81U,
  };
  pipeline.push({trace, sizeof(trace)});
  pipeline.finish();

  bool foundEvent = false;
  bool foundPmu = false;
  for (const auto& packet : sink.events) {
    const auto* event = traceEventPayload<DwtEventTraceEvent>(packet);
    const auto* pmu = traceEventPayload<PmuTraceEvent>(packet);
    foundEvent =
        foundEvent || (event != nullptr && event->discriminator == 0U && event->size == 1U && event->value == 0x21U);
    foundPmu = foundPmu || (pmu != nullptr && pmu->discriminator == 3U && pmu->size == 1U && pmu->value == 0x81U);
  }
  require(foundEvent, "OpenCSD DWT event-counter packet must survive post-decoding");
  require(foundPmu, "OpenCSD PMU-overflow packet must survive post-decoding");
}

TEST(CtraceUnitTests, testDecodePipelineDoesNotInjectSync)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16, sink);

  const std::uint8_t validWithoutAsync[] = {0x01U, static_cast<std::uint8_t>('A')};
  pipeline.push({validWithoutAsync, sizeof(validWithoutAsync)});
  const auto result = pipeline.finish();

  require(result.bytesIn == sizeof(validWithoutAsync), "decode byte count mismatch");
  bool foundPayload = false;
  bool foundDataLoss = false;
  for (const auto& packet : sink.events) {
    if (softwareEvent(packet) != nullptr) {
      foundPayload = true;
    }
    const auto* issue = issueEvent(packet);
    if (issue != nullptr && issue->code == "data-loss") {
      foundDataLoss = true;
      require(issue->rawBytesConsumed == std::optional<std::uint64_t>(sizeof(validWithoutAsync)),
              "decode should count all raw bytes consumed before synchronization");
      require(issue->message.find("OpenCSD consumed 2 raw bytes") != std::string::npos,
              "decode data-loss message should include the consumed raw-byte count");
      require(issue->message.find("timestamp 0 .. unknown.") != std::string::npos,
              "decode should mark a missing post-recovery timestamp as unknown");
    }
  }
  require(!foundPayload, "decode should not decode sync-less data by injecting sync");
  require(foundDataLoss, "decode should mark sync-less data loss");
}

TEST(CtraceUnitTests, testDecodePipelineCountsBytesUntilRealSync)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(16, sink);

  const std::uint8_t leading[] = {0x01U, static_cast<std::uint8_t>('A')};
  pipeline.push({leading, sizeof(leading)});
  const std::uint8_t synchronized[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };
  pipeline.push({synchronized, sizeof(synchronized)});
  pipeline.finish();

  bool foundDataLoss = false;
  bool foundPayloadAfterDataLoss = false;
  for (const auto& packet : sink.events) {
    const auto* issue = issueEvent(packet);
    if (issue != nullptr && issue->code == "data-loss") {
      foundDataLoss = true;
      require(issue->rawBytesConsumed == std::optional<std::uint64_t>(sizeof(leading)),
              "decode should count bytes only up to the real sync offset");
      continue;
    }
    const auto* software = softwareEvent(packet);
    if (foundDataLoss && software != nullptr && software->channel == 0U && software->size == 1U &&
        software->value == static_cast<std::uint8_t>('B')) {
      foundPayloadAfterDataLoss = true;
    }
  }

  require(foundDataLoss, "decode should emit data loss before the recovered stream");
  require(foundPayloadAfterDataLoss, "decode should emit synchronized payload after the counted data loss");
}
