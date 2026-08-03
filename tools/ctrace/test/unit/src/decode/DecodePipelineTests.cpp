/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

// Cortex-M post-decoder and end-to-end decode pipeline tests.
#include "OpenCsdTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "CortexMPostDecoder.h"
#include "CortexMStreamDecoder.h"
#include "DecodePipeline.h"
#include "OpenCsdTraceElement.h"
#include "TraceEvent.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using OpenCsdTestSupport::openCsdElement;
using OpenCsdTestSupport::openCsdSoftwareElement;
using OpenCsdTestSupport::openCsdTimestampElement;

static const SoftwareTraceEvent* softwareEvent(const TraceEvent& event)
{
  return traceEventPayload<SoftwareTraceEvent>(event);
}

static const TraceIssueEvent* issueEvent(const TraceEvent& event)
{
  return traceEventPayload<TraceIssueEvent>(event);
}

static bool hasSoftwareValue(const std::vector<TraceEvent>& events, std::uint8_t value, std::uint32_t channel = 0U)
{
  for (const auto& event : events) {
    const auto* software = softwareEvent(event);
    if (software != nullptr && software->channel == channel && software->size == 1U && software->value == value) {
      return true;
    }
  }
  return false;
}

static const TraceIssueEvent* findIssue(const std::vector<TraceEvent>& events, const std::string& code,
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

static std::size_t countIssues(const std::vector<TraceEvent>& events, const std::string& code)
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

template <std::size_t Size> static constexpr RawByteView rawBytes(const std::uint8_t (&bytes)[Size])
{
  return {bytes, Size};
}

static RawByteView rawBytes(const std::vector<std::uint8_t>& bytes)
{
  return {bytes.data(), bytes.size()};
}

struct DecodedTrace {
  DecodeResult result;
  std::vector<TraceEvent> events;
};

static DecodedTrace decodeTrace(std::initializer_list<RawByteView> chunks, std::uint32_t timestampPrescaler = 16U)
{
  CollectingEventSink sink;
  DecodePipeline pipeline(timestampPrescaler, sink);
  for (const auto chunk : chunks) {
    pipeline.push(chunk);
  }
  return {pipeline.finish(), std::move(sink.events)};
}

TEST(CtraceUnitTests, testCortexMPostDecoderSoftwareTimestampBoundary)
{
  CollectingEventSink sink;
  CortexMPostDecoder decoder(sink);

  decoder.append(openCsdSoftwareElement(0U, 'A', 4U, 1U));
  decoder.append(openCsdTimestampElement(120U, 5U, 1U));

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

  const auto stream1Software = openCsdSoftwareElement(1U, 0x11U, 0U, 1U);
  decoder.append(stream1Software);

  const auto stream2Software = openCsdSoftwareElement(1U, 0x22U, 0U, 2U);
  decoder.append(stream2Software);

  const auto stream1Timestamp = openCsdTimestampElement(10U, 0U, 1U);
  decoder.append(stream1Timestamp);

  const auto stream2Timestamp = openCsdTimestampElement(10U, 0U, 2U);
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
  auto timestamp = openCsdTimestampElement(std::numeric_limits<std::uint64_t>::max());

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

  const auto timestampWithoutValue = openCsdElement(OpenCsdTraceElement::Kind::LocalTimestamp);
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

  auto discontinuity = openCsdElement(OpenCsdTraceElement::Kind::Discontinuity);
  discontinuity.issueCode = "data-loss";
  discontinuity.errorMessage = "OpenCSD consumed 2 raw bytes";
  discontinuity.rawBytesConsumed = 2U;
  decoder.append(discontinuity);

  decoder.append(openCsdSoftwareElement(0U, 'B', 8U));
  decoder.append(openCsdTimestampElement(42U, 10U));

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

  auto cause = openCsdElement(OpenCsdTraceElement::Kind::Error, 8U);
  cause.discontinuity = true;
  cause.issueCode = "opencsd-bad-packet-sequence";
  cause.errorMessage = "OpenCSD detected an invalid ITM packet sequence at raw offset 8.";
  decoder.append(cause);

  auto loss = openCsdElement(OpenCsdTraceElement::Kind::Error, 10U);
  loss.awaitingResumeTimestamp = true;
  loss.issueCode = "data-loss";
  loss.errorMessage = "OpenCSD consumed 2 raw bytes";
  loss.rawBytesConsumed = 2U;
  decoder.append(loss);

  decoder.append(openCsdTimestampElement(42U, 12U));

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

  const auto firstTimestamp = openCsdTimestampElement(100U, 1U, 1U);
  decoder.append(firstTimestamp);

  auto pc = openCsdElement(OpenCsdTraceElement::Kind::Hardware, 2U, 1U);
  pc.discriminator = 8;
  pc.size = 4;
  pc.value = 0x08001234U;
  decoder.append(pc);

  const auto overflow = openCsdElement(OpenCsdTraceElement::Kind::Overflow, 3U);
  decoder.append(overflow);

  auto value = openCsdElement(OpenCsdTraceElement::Kind::Hardware, 4U, 1U);
  value.discriminator = 16;
  value.size = 4;
  value.value = 0x55U;
  decoder.append(value);

  const auto secondTimestamp = openCsdTimestampElement(20U, 5U, 1U);
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

  const auto firstTimestamp = openCsdTimestampElement(100U, (std::uint64_t{1} << 32U) + 1U);
  decoder.append(firstTimestamp);

  const auto secondTimestamp = openCsdTimestampElement(10U, 2U);
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

  const auto software = openCsdSoftwareElement(1U, 'A', 1U);
  decoder.append(software);

  auto dwtPc = openCsdElement(OpenCsdTraceElement::Kind::Hardware, 2U);
  dwtPc.discriminator = 8U;
  dwtPc.size = 4U;
  dwtPc.value = 0x08001234U;
  decoder.append(dwtPc);

  auto globalTimestamp = openCsdElement(OpenCsdTraceElement::Kind::GlobalTimestamp, 3U);
  globalTimestamp.timestampValue = 0x123456789abcdef0ULL;
  decoder.append(globalTimestamp);

  auto warning = openCsdElement(OpenCsdTraceElement::Kind::Error, 4U);
  warning.issueCode = "opencsd-warning";
  warning.issueSeverity = TraceIssueSeverity::Warning;
  warning.errorMessage = "decoder warning";
  decoder.append(warning);

  const auto localTimestamp = openCsdTimestampElement(42U, 5U);
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
  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x00U, 0xfeU,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };
  const auto decoded = decodeTrace({rawBytes(trace)});

  require(decoded.result.bytesIn == sizeof(trace), "recovery byte count mismatch");
  require(hasSoftwareValue(decoded.events, static_cast<std::uint8_t>('A')),
          "recovery should preserve packets before the damaged section");
  const auto* error = findIssue(decoded.events, "opencsd-bad-packet-sequence", 8U);
  require(error != nullptr && error->message == "OpenCSD detected an invalid ITM packet sequence at raw offset 8.",
          "recovery should report the exact OpenCSD error offset");
  require(hasSoftwareValue(decoded.events, static_cast<std::uint8_t>('B')),
          "recovery should resume after the next real ITM sync");
}

TEST(CtraceUnitTests, testDecodePipelineKeepsCallbacksAttachedAcrossRepeatedResets)
{
  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x00U, 0xfeU,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'), 0x00U, 0xfeU,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('C'),
  };
  const auto decoded = decodeTrace({rawBytes(trace)});

  require(decoded.result.bytesIn == sizeof(trace), "repeated recovery byte count mismatch");
  require(hasSoftwareValue(decoded.events, static_cast<std::uint8_t>('A')) &&
              hasSoftwareValue(decoded.events, static_cast<std::uint8_t>('B')) &&
              hasSoftwareValue(decoded.events, static_cast<std::uint8_t>('C')),
          "decoder resets must retain all OpenCSD callbacks");
  require(countIssues(decoded.events, "opencsd-bad-packet-sequence") == 2U,
          "each damaged section must trigger one recoverable reset");
}

TEST(CtraceUnitTests, testDecodePipelineReconstructsGlobalTimestamp)
{
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
  const auto decoded = decodeTrace({rawBytes(trace)}, 1U);

  require(decoded.result.bytesIn == trace.size(), "global timestamp input byte count mismatch");
  std::size_t globalTimestampCount = 0U;
  for (const auto& packet : decoded.events) {
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
  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x09U, 0x41U, 0x20U,
  };

  const auto decoded = decodeTrace({rawBytes(trace)});

  bool foundScaledSoftware = false;
  for (const auto& event : decoded.events) {
    const auto* software = softwareEvent(event);
    foundScaledSoftware = foundScaledSoftware || (software != nullptr && software->channel == 1U &&
                                                  event.tcyc == std::optional<std::uint64_t>(32U));
  }
  require(foundScaledSoftware, "DecodePipeline must apply the ITM prescaler after OpenCSD decoding");
}

TEST(CtraceUnitTests, testDecodePipelineRecoversWhenErrorSpansChunks)
{
  const std::uint8_t firstChunk[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x00U,
  };
  const std::uint8_t secondChunk[] = {
      0xfeU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };
  const auto decoded = decodeTrace({rawBytes(firstChunk), rawBytes(secondChunk)});

  require(decoded.result.bytesIn == sizeof(firstChunk) + sizeof(secondChunk), "split recovery byte count mismatch");
  require(findIssue(decoded.events, "opencsd-bad-packet-sequence", 8U) != nullptr,
          "split bad packet should retain its header offset");
  require(hasSoftwareValue(decoded.events, static_cast<std::uint8_t>('B')),
          "recovery should resume after a split bad packet");
}

TEST(CtraceUnitTests, testDecodePipelineRecoversFromReservedHeader)
{
  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x04U,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };
  const auto decoded = decodeTrace({rawBytes(trace)});

  const auto* error = findIssue(decoded.events, "opencsd-invalid-packet-header", 8U);
  require(error != nullptr && error->message == "OpenCSD detected an invalid ITM packet header at raw offset 8.",
          "reserved header should report its exact OpenCSD error");
  require(hasSoftwareValue(decoded.events, static_cast<std::uint8_t>('B')),
          "recovery should resume after a reserved header");
}

TEST(CtraceUnitTests, testDecodePipelineFinishesWithoutLaterSync)
{
  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x00U, 0xfeU, 0xffU, 0xffU,
  };
  const auto decoded = decodeTrace({rawBytes(trace)});

  require(decoded.result.bytesIn == sizeof(trace), "bad-tail byte count mismatch");
  require(findIssue(decoded.events, "opencsd-bad-packet-sequence", 8U) != nullptr,
          "bad tail should report its exact OpenCSD error and finish");
}

TEST(CtraceUnitTests, testDecodePipelineReportsIncompletePacketAtEndOfInput)
{
  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x03U, 0x12U,
  };
  const auto decoded = decodeTrace({rawBytes(trace)});

  require(!decoded.events.empty(), "incomplete trace should emit packets");
  const auto& last = decoded.events.back();
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
    std::vector<std::uint8_t> trace{
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'),
    };
    trace.insert(trace.end(), damaged.begin(), damaged.end());
    trace.insert(trace.end(), {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U});
    trace.insert(trace.end(), {0x01U, static_cast<std::uint8_t>('B')});

    const auto decoded = decodeTrace({rawBytes(trace)});

    require(findIssue(decoded.events, "opencsd-bad-packet-sequence") != nullptr,
            "overlong continuation packet should emit the OpenCSD error");
    require(hasSoftwareValue(decoded.events, static_cast<std::uint8_t>('B')),
            "decode should resume after an overlong continuation packet");
  }
}

TEST(CtraceUnitTests, testDecodePipelinePreservesDwtEventAndPmuPackets)
{
  const std::uint8_t trace[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x05U, 0x21U, 0x1dU, 0x81U,
  };
  const auto decoded = decodeTrace({rawBytes(trace)});

  bool foundEvent = false;
  bool foundPmu = false;
  for (const auto& packet : decoded.events) {
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
  const std::uint8_t validWithoutAsync[] = {0x01U, static_cast<std::uint8_t>('A')};
  const auto decoded = decodeTrace({rawBytes(validWithoutAsync)});

  require(decoded.result.bytesIn == sizeof(validWithoutAsync), "decode byte count mismatch");
  bool foundPayload = false;
  bool foundDataLoss = false;
  for (const auto& packet : decoded.events) {
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
  const std::uint8_t leading[] = {0x01U, static_cast<std::uint8_t>('A')};
  const std::uint8_t synchronized[] = {
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };
  const auto decoded = decodeTrace({rawBytes(leading), rawBytes(synchronized)});

  bool foundDataLoss = false;
  bool foundPayloadAfterDataLoss = false;
  for (const auto& packet : decoded.events) {
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
