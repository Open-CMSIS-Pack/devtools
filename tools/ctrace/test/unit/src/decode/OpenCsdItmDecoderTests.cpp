/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdTestSupport.h"
#include "OpenCsdSessionTestSupport.h"
#include "FormattedTraceTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "OpenCsdErrorController.h"
#include "OpenCsdFormattedItmSession.h"
#include "OpenCsdItmDecoder.h"
#include "OpenCsdItmSession.h"
#include "OpenCsdPacketCollector.h"
#include "TraceEvent.h"
#include "opencsd/ocsd_if_types.h"

#include <cstdint>
#include <array>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using FormattedTraceTestSupport::itmHardwareSync;
using FormattedTraceTestSupport::memoryAlignedFrames;
using OpenCsdSessionTestSupport::ScriptedDecoderHarness;
using OpenCsdTestSupport::CollectingOpenCsdElementSink;

namespace {

/** @brief Tracks calls made after a synthetic formatted-session failure. */
struct ThrowingSessionState {
  std::uint32_t resetCalls = 0U;
};

/** @brief Emits transactional output and then simulates a post-root session failure. */
class ThrowingFormattedSession final : public OpenCsdItmSessionInterface {
public:
  /** @brief Binds shared observations and the collector under test. */
  ThrowingFormattedSession(std::shared_ptr<ThrowingSessionState> state, OpenCsdPacketCollector& collector,
                           std::optional<TraceRouteIdentity> incompleteTailRoute = std::nullopt)
    : m_state(std::move(state)),
      m_collector(collector),
      m_incompleteTailRoute(std::move(incompleteTailRoute))
  {
  }

  /** @brief Consumes the input, emits temporary semantics, and throws. */
  ocsd_datapath_resp_t pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t*,
                                std::uint32_t& processed) override
  {
    processed = size;
    if (m_incompleteTailRoute.has_value()) {
      return OCSD_RESP_CONT;
    }
    m_collector.appendDecodeError(index + 3U, "temporary callback output", TraceIssueCode::DecodeError, false,
                                  TraceIssueSeverity::Warning);
    throw std::runtime_error("synthetic post-root session failure");
  }

  /** @brief Accepts a flush operation. */
  ocsd_datapath_resp_t flush() override
  {
    return OCSD_RESP_CONT;
  }

  /** @brief Records an unexpected recovery reset. */
  ocsd_datapath_resp_t reset() override
  {
    ++m_state->resetCalls;
    return OCSD_RESP_CONT;
  }

  /** @brief Accepts end of trace. */
  ocsd_datapath_resp_t endOfTrace() override
  {
    if (m_incompleteTailRoute.has_value()) {
      m_collector.appendDecodeError(5U, "temporary end-of-trace output", TraceIssueCode::DecodeError, false,
                                    TraceIssueSeverity::Warning);
      ItmTrcPacket packet;
      packet.setPktType(ITM_PKT_INCOMPLETE_EOT);
      m_collector.rawPacketForRoute(*m_incompleteTailRoute, OCSD_OP_EOT, 6U, &packet, 0U, nullptr);
      throw std::runtime_error("synthetic post-root end-of-trace failure");
    }
    return OCSD_RESP_CONT;
  }

private:
  std::shared_ptr<ThrowingSessionState> m_state;
  OpenCsdPacketCollector& m_collector;
  std::optional<TraceRouteIdentity> m_incompleteTailRoute;
};

} // namespace

TEST(CtraceUnitTests, testOpenCsdItmDecoderConstructsDefaultSession)
{
  CollectingOpenCsdElementSink sink;
  {
    OpenCsdItmDecoder decoder({}, sink);
    EXPECT_EQ(decoder.finish().bytesIn, 0U);
  }
  EXPECT_FALSE(sink.hasIssue(TraceIssueCode::OpenCsdInitializationError));

  const TraceRouteIdentity formattedRoute{TraceRouteId{0U}, 1U};
  OpenCsdItmDecoder formatted({formattedRoute}, OpenCsdItmInputMode::CoreSightFormatted, sink);
  EXPECT_EQ(formatted.finish().bytesIn, 0U);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderValidatesInputRouteCountAndDataPointer)
{
  CollectingOpenCsdElementSink sink;
  EXPECT_THROW((void)OpenCsdItmDecoder({}, OpenCsdItmInputMode::Single, sink), std::invalid_argument);
  EXPECT_THROW((void)OpenCsdItmDecoder({TraceRouteIdentity{}, TraceRouteIdentity{}}, OpenCsdItmInputMode::Single, sink),
               std::invalid_argument);

  OpenCsdItmDecoder decoder({}, sink);
  EXPECT_THROW(decoder.push(nullptr, 1U), std::invalid_argument);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderTreatsProtocolRecoveryAsInputFatal)
{
  const auto route = TraceRouteIdentity{TraceRouteId{0U}, 1U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes = {
      {OCSD_RESP_ERR_CONT, 16U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 5U, "bad packet"}}},
  };
  OpenCsdItmDecoder decoder({route}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  EXPECT_THROW(decoder.push(frame.data(), frame.size()), OpenCsdFatalError);
  EXPECT_EQ(script->resetCalls, 0U);
  EXPECT_TRUE(sink.hasIssue(TraceIssueCode::OpenCsdInvalidPacketHeader));
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderDoesNotInferDataLossFromSilentFrames)
{
  const auto route = TraceRouteIdentity{TraceRouteId{0U}, 1U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes = {{OCSD_RESP_CONT, 16U, false}};
  OpenCsdItmDecoder decoder({route}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  decoder.push(frame.data(), frame.size());
  EXPECT_EQ(decoder.finish().bytesIn, frame.size());
  EXPECT_FALSE(sink.hasIssue(TraceIssueCode::DataLoss));
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderRejectsNoProgressAndPartialFramesWithoutReset)
{
  const auto route = TraceRouteIdentity{TraceRouteId{0U}, 1U};
  const std::array<std::uint8_t, 16U> frame{};

  CollectingOpenCsdElementSink stalledSink;
  const auto stalledScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  stalledScript->pushes = {{OCSD_RESP_CONT, 0U, false}};
  OpenCsdItmDecoder stalled({route}, OpenCsdItmInputMode::CoreSightFormatted, stalledSink,
                            OpenCsdSessionTestSupport::scriptedFactory(stalledScript));
  EXPECT_THROW(stalled.push(frame.data(), frame.size()), OpenCsdFatalError);
  EXPECT_EQ(stalledScript->resetCalls, 0U);
  EXPECT_TRUE(stalledSink.hasIssue(TraceIssueCode::OpenCsdNoProgress));

  CollectingOpenCsdElementSink partialSink;
  OpenCsdItmDecoder partial(
      {route}, OpenCsdItmInputMode::CoreSightFormatted, partialSink,
      OpenCsdSessionTestSupport::scriptedFactory(std::make_shared<OpenCsdSessionTestSupport::SessionScript>()));
  EXPECT_THROW(partial.push(frame.data(), frame.size() - 1U), OpenCsdFatalError);
  EXPECT_TRUE(partialSink.hasIssue(TraceIssueCode::OpenCsdDecodeError));
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderPreservesRoutedIncompleteTailAsFatal)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  CollectingOpenCsdElementSink sink;
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink);
  const auto capture = memoryAlignedFrames({
      {2U, itmHardwareSync()},
      {2U, {0x03U, 0x12U}},
  });

  decoder.push(capture.data(), static_cast<std::uint32_t>(capture.size()));
  try {
    (void)decoder.finish();
    FAIL() << "incomplete formatted ITM packet did not fail the input";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_EQ(error.bytesProcessed(), capture.size());
  }

  std::vector<const OpenCsdTraceElement*> issues;
  for (const auto& element : sink.elements()) {
    if (element.issueCode.has_value()) {
      issues.push_back(&element);
    }
  }
  ASSERT_EQ(issues.size(), 1U);
  EXPECT_EQ(issues.front()->issueCode, TraceIssueCode::OpenCsdIncompleteTail);
  EXPECT_EQ(issues.front()->route, route2);
  EXPECT_EQ(issues.front()->sourceIndex, 6U);
  EXPECT_TRUE(issues.front()->discontinuity);
  EXPECT_EQ(issues.front()->issueSeverity, TraceIssueSeverity::Error);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderNormalizesUnassignedDataWithExactOffset)
{
  const TraceRouteIdentity route{TraceRouteId{0U}, 1U};
  CollectingOpenCsdElementSink sink;
  OpenCsdItmDecoder decoder({route}, OpenCsdItmInputMode::CoreSightFormatted, sink);
  const std::array<std::uint8_t, 16U> unassignedFrame{};

  try {
    decoder.push(unassignedFrame.data(), unassignedFrame.size());
    FAIL() << "formatted data before the first source ID did not fail";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_EQ(error.bytesProcessed(), unassignedFrame.size());
    EXPECT_NE(std::string(error.what()).find("has no source ID"), std::string::npos);
  }

  ASSERT_EQ(sink.elements().size(), 1U);
  EXPECT_EQ(sink.elements().front().kind, OpenCsdTraceElement::Kind::Error);
  EXPECT_EQ(sink.elements().front().issueCode, TraceIssueCode::OpenCsdFormattedInputError);
  EXPECT_EQ(sink.elements().front().sourceIndex, 0U);
  EXPECT_TRUE(sink.elements().front().discontinuity);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderNormalizesPostRootSessionExceptionsAndRollsBack)
{
  const TraceRouteIdentity route{TraceRouteId{0U}, 1U};
  CollectingOpenCsdElementSink sink;
  const auto state = std::make_shared<ThrowingSessionState>();
  const OpenCsdItmSessionFactory factory =
      [state](OpenCsdPacketCollector& collector,
              OpenCsdErrorController&) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    return std::make_unique<ThrowingFormattedSession>(state, collector);
  };
  OpenCsdItmDecoder decoder({route}, OpenCsdItmInputMode::CoreSightFormatted, sink, factory);
  const std::array<std::uint8_t, 16U> frame{};

  try {
    decoder.push(frame.data(), frame.size());
    FAIL() << "formatted session exception escaped without normalization";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_EQ(error.bytesProcessed(), frame.size());
    EXPECT_NE(std::string(error.what()).find("formatted OpenCSD session operation failed"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("synthetic post-root session failure"), std::string::npos);
  }

  EXPECT_EQ(state->resetCalls, 0U);
  ASSERT_EQ(sink.elements().size(), 1U) << "temporary transaction output was not rolled back";
  EXPECT_EQ(sink.elements().front().issueCode, TraceIssueCode::OpenCsdDecodeError);
  EXPECT_EQ(sink.elements().front().sourceIndex, 3U);
  EXPECT_EQ(sink.elements().front().issueSeverity, TraceIssueSeverity::Error);
}

TEST(CtraceUnitTests, testSingleOpenCsdItmDecoderLeavesSessionExceptionBehaviorUnchanged)
{
  CollectingOpenCsdElementSink sink;
  const auto state = std::make_shared<ThrowingSessionState>();
  const OpenCsdItmSessionFactory factory =
      [state](OpenCsdPacketCollector& collector,
              OpenCsdErrorController&) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    return std::make_unique<ThrowingFormattedSession>(state, collector);
  };
  OpenCsdItmDecoder decoder(TraceRouteIdentity{}, sink, factory);
  const std::uint8_t byte = 0U;

  try {
    decoder.push(&byte, 1U);
    FAIL() << "SINGLE session exception did not propagate";
  } catch (const OpenCsdFatalError&) {
    FAIL() << "SINGLE session exception was normalized by formatted-input policy";
  } catch (const std::runtime_error& error) {
    EXPECT_EQ(std::string(error.what()), "synthetic post-root session failure");
  }
  EXPECT_EQ(state->resetCalls, 0U);
  EXPECT_TRUE(sink.elements().empty());
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderKeepsIncompleteTailWhenEndOperationThrows)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  CollectingOpenCsdElementSink sink;
  const auto state = std::make_shared<ThrowingSessionState>();
  const OpenCsdItmSessionFactory factory =
      [state, route2](OpenCsdPacketCollector& collector,
                      OpenCsdErrorController&) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    return std::make_unique<ThrowingFormattedSession>(state, collector, route2);
  };
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink, factory);
  const std::array<std::uint8_t, 16U> frame{};
  decoder.push(frame.data(), frame.size());

  EXPECT_THROW((void)decoder.finish(), OpenCsdFatalError);
  EXPECT_EQ(state->resetCalls, 0U);
  ASSERT_EQ(sink.elements().size(), 2U) << "temporary end-of-trace transaction output was not discarded";
  EXPECT_EQ(sink.elements()[0].issueCode, TraceIssueCode::OpenCsdIncompleteTail);
  EXPECT_EQ(sink.elements()[0].route, route2);
  EXPECT_EQ(sink.elements()[0].sourceIndex, 6U);
  EXPECT_EQ(sink.elements()[1].issueCode, TraceIssueCode::OpenCsdDecodeError);
  EXPECT_EQ(sink.elements()[1].route, route1);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderNormalizesObserverExceptionAfterRootReturn)
{
  const TraceRouteIdentity route{TraceRouteId{0U}, 1U};
  CollectingOpenCsdElementSink sink;
  OpenCsdItmDecoder decoder({route}, OpenCsdItmInputMode::CoreSightFormatted, sink, [](std::uint8_t, std::uint64_t) {
    throw std::runtime_error("synthetic observer failure");
  });
  const auto capture = memoryAlignedFrames({{42U, {0xdeU, 0xadU}}});

  try {
    decoder.push(capture.data(), static_cast<std::uint32_t>(capture.size()));
    FAIL() << "formatted observer exception escaped without normalization";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_EQ(error.bytesProcessed(), capture.size());
    EXPECT_NE(std::string(error.what()).find("formatted OpenCSD session operation failed"), std::string::npos);
    EXPECT_NE(std::string(error.what()).find("synthetic observer failure"), std::string::npos);
  }

  ASSERT_EQ(sink.elements().size(), 1U);
  EXPECT_EQ(sink.elements().front().issueCode, TraceIssueCode::OpenCsdDecodeError);
  EXPECT_EQ(sink.elements().front().sourceIndex, 0U);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderChunksAndFinishesOnce)
{
  ScriptedDecoderHarness harness;
  harness.push(5000U);
  EXPECT_EQ(harness.script().pushCalls, 2U);
  EXPECT_EQ(harness.decoder().finish().bytesIn, 5000U);
  EXPECT_EQ(harness.decoder().finish().bytesIn, 5000U);
  EXPECT_EQ(harness.script().endCalls, 1U);
  EXPECT_THROW(harness.push(1U), std::runtime_error);
  EXPECT_TRUE(harness.sink().hasIssue(TraceIssueCode::DataLoss));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderRecoversAndMarksConsumedDataLoss)
{
  ScriptedDecoderHarness harness;
  harness.script().pushes = {
      {OCSD_RESP_ERR_CONT, 2U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 1U, "reserved header"}}},
      {OCSD_RESP_CONT, 2U, false, {}},
      {OCSD_RESP_CONT, 2U, true, {}},
  };
  harness.push(6U);
  EXPECT_EQ(harness.decoder().finish().bytesIn, 6U);
  EXPECT_EQ(harness.script().resetCalls, 1U);
  EXPECT_TRUE(harness.sink().hasIssue(TraceIssueCode::OpenCsdInvalidPacketHeader));
  EXPECT_TRUE(harness.sink().hasIssue(TraceIssueCode::DataLoss));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderMarksUnframedDataBeforeSync)
{
  ScriptedDecoderHarness harness;
  harness.script().pushes = {
      {OCSD_RESP_CONT, 2U, false, {}},
      {OCSD_RESP_CONT, 2U, true, {}},
  };
  harness.push(4U);
  harness.decoder().finish();
  EXPECT_TRUE(harness.sink().hasIssue(TraceIssueCode::DataLoss));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderFlushesWaitResponses)
{
  ScriptedDecoderHarness harness;
  harness.script().pushes = {
      {OCSD_RESP_WAIT, 1U, false, {}},
      {OCSD_RESP_WAIT, 1U, true, {}},
  };
  harness.script().flushes = {{OCSD_RESP_CONT}, {OCSD_RESP_CONT}};
  harness.push(2U);
  harness.decoder().finish();
  EXPECT_EQ(harness.script().flushCalls, 2U);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderReportsWarningsAndImplicitErrors)
{
  ScriptedDecoderHarness harness;
  harness.script().pushes = {{OCSD_RESP_ERR_CONT,
                              1U,
                              false,
                              {
                                  {OCSD_ERR_SEV_INFO, OCSD_ERR_FAIL, 0U, "info"},
                                  {OCSD_ERR_SEV_WARN, OCSD_ERR_BAD_PACKET_SEQ, 0U, "warning"},
                              }}};
  harness.push(1U);
  harness.decoder().finish();
  ASSERT_GE(harness.sink().elements().size(), 2U);
  EXPECT_EQ(harness.sink().elements()[0].issueSeverity, TraceIssueSeverity::Warning);
  EXPECT_EQ(harness.sink().elements()[1].issueSeverity, TraceIssueSeverity::Error);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderHandlesEndOfTraceDecisions)
{
  ScriptedDecoderHarness recovery;
  recovery.script().ends = {
      {OCSD_RESP_ERR_CONT, std::nullopt, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_BAD_PACKET_SEQ, 0U, "bad tail"}}}};
  EXPECT_NO_THROW(recovery.decoder().finish());
  EXPECT_TRUE(recovery.sink().hasIssue(TraceIssueCode::OpenCsdBadPacketSequence));

  ScriptedDecoderHarness wait;
  wait.script().ends = {{OCSD_RESP_WAIT}};
  wait.script().flushes = {{OCSD_RESP_CONT}};
  EXPECT_NO_THROW(wait.decoder().finish());
  EXPECT_EQ(wait.script().flushCalls, 1U);

  ScriptedDecoderHarness fatal;
  fatal.script().ends = {{OCSD_RESP_FATAL_SYS_ERR}};
  EXPECT_THROW((void)fatal.decoder().finish(), OpenCsdFatalError);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderAbortsFatalAndNoProgressData)
{
  ScriptedDecoderHarness fatal;
  fatal.script().pushes = {{OCSD_RESP_FATAL_SYS_ERR, 1U}};
  try {
    fatal.push(1U);
    FAIL() << "fatal OpenCSD response did not abort decoding";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_EQ(error.bytesProcessed(), 1U);
  }

  ScriptedDecoderHarness stalled;
  stalled.script().pushes = {{OCSD_RESP_CONT, 0U}, {OCSD_RESP_CONT, 0U}};
  EXPECT_THROW(stalled.push(1U), OpenCsdFatalError);
  EXPECT_TRUE(stalled.sink().hasIssue(TraceIssueCode::OpenCsdNoProgress));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderBoundsZeroProgressRecoveryAndWaitRetries)
{
  ScriptedDecoderHarness recovery;
  recovery.script().pushes = {
      {OCSD_RESP_ERR_CONT, 0U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 0U, "bad packet"}}},
      {OCSD_RESP_ERR_CONT, 0U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 0U, "bad packet"}}},
  };
  EXPECT_THROW(recovery.push(1U), OpenCsdFatalError);
  EXPECT_EQ(recovery.script().pushCalls, 2U);
  EXPECT_EQ(recovery.script().resetCalls, 1U);
  EXPECT_TRUE(recovery.sink().hasIssue(TraceIssueCode::OpenCsdNoProgress));

  ScriptedDecoderHarness wait;
  wait.script().pushes = {
      {OCSD_RESP_WAIT, 0U},
      {OCSD_RESP_WAIT, 0U},
  };
  wait.script().flushes = {{OCSD_RESP_CONT}};
  EXPECT_THROW(wait.push(1U), OpenCsdFatalError);
  EXPECT_EQ(wait.script().pushCalls, 2U);
  EXPECT_EQ(wait.script().flushCalls, 1U);
  EXPECT_TRUE(wait.sink().hasIssue(TraceIssueCode::OpenCsdNoProgress));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderAllowsZeroProgressRetriesToResume)
{
  ScriptedDecoderHarness harness;
  harness.script().pushes = {
      {OCSD_RESP_WAIT, 0U},
      {OCSD_RESP_CONT, 1U, true},
  };
  harness.script().flushes = {{OCSD_RESP_CONT}};

  EXPECT_NO_THROW(harness.push(1U));
  EXPECT_EQ(harness.decoder().finish().bytesIn, 1U);
  EXPECT_EQ(harness.script().pushCalls, 2U);
  EXPECT_EQ(harness.script().flushCalls, 1U);

  ScriptedDecoderHarness recovery;
  recovery.script().pushes = {
      {OCSD_RESP_ERR_CONT, 0U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 0U, "bad packet"}}},
      {OCSD_RESP_CONT, 1U, true},
  };
  EXPECT_NO_THROW(recovery.push(1U));
  EXPECT_EQ(recovery.decoder().finish().bytesIn, 1U);
  EXPECT_EQ(recovery.script().pushCalls, 2U);
  EXPECT_EQ(recovery.script().resetCalls, 1U);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderHandlesFlushRecoveryAndTimeout)
{
  ScriptedDecoderHarness recovery;
  recovery.script().ends = {{OCSD_RESP_WAIT}};
  recovery.script().flushes = {
      {OCSD_RESP_ERR_CONT, std::nullopt, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 0U, "bad flush"}}}};
  EXPECT_NO_THROW(recovery.decoder().finish());
  EXPECT_EQ(recovery.script().resetCalls, 1U);

  ScriptedDecoderHarness committed;
  committed.script().ends = {{OCSD_RESP_WAIT}};
  committed.script().flushes = {{OCSD_RESP_CONT, std::nullopt, true}};
  EXPECT_NO_THROW(committed.decoder().finish());
  EXPECT_FALSE(committed.sink().elements().empty());

  ScriptedDecoderHarness fatal;
  fatal.script().ends = {{OCSD_RESP_WAIT}};
  fatal.script().flushes = {{OCSD_RESP_FATAL_SYS_ERR}};
  EXPECT_THROW((void)fatal.decoder().finish(), OpenCsdFatalError);

  ScriptedDecoderHarness timeout;
  timeout.script().ends = {{OCSD_RESP_WAIT}};
  timeout.script().defaultFlushResponse = OCSD_RESP_WAIT;
  EXPECT_THROW((void)timeout.decoder().finish(), OpenCsdFatalError);
  EXPECT_EQ(timeout.script().flushCalls, 1024U);
  EXPECT_TRUE(timeout.sink().hasIssue(TraceIssueCode::OpenCsdWaitTimeout));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderReportsResetAndInitializationFailures)
{
  ScriptedDecoderHarness reset;
  reset.script().pushes = {
      {OCSD_RESP_ERR_CONT, 1U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 0U, "bad packet"}}}};
  reset.script().resets = {{OCSD_RESP_WAIT}};
  EXPECT_THROW(reset.push(1U), OpenCsdFatalError);

  CollectingOpenCsdElementSink nullSink;
  const OpenCsdItmSessionFactory nullFactory =
      [](OpenCsdPacketCollector&, OpenCsdErrorController&) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    return nullptr;
  };
  EXPECT_THROW((void)OpenCsdItmDecoder({}, nullSink, nullFactory), OpenCsdFatalError);
  EXPECT_TRUE(nullSink.hasIssue(TraceIssueCode::OpenCsdInitializationError));

  CollectingOpenCsdElementSink errorSink;
  const OpenCsdItmSessionFactory errorFactory =
      [](OpenCsdPacketCollector&, OpenCsdErrorController&) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    throw OpenCsdItmSessionError("synthetic session setup failure");
  };
  EXPECT_THROW((void)OpenCsdItmDecoder({}, errorSink, errorFactory), OpenCsdFatalError);
  EXPECT_TRUE(errorSink.hasIssue(TraceIssueCode::OpenCsdInitializationError));
}

TEST(CtraceUnitTests, testOpenCsdItmSessionAcceptsEmptyDataPathOperations)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(TraceRouteIdentity{}, sink);
  OpenCsdErrorController errors;
  OpenCsdItmSession session(collector, errors);

  EXPECT_NE(errors.decide(session.reset()).action, OpenCsdErrorController::Action::Abort);
  EXPECT_NE(errors.decide(session.flush()).action, OpenCsdErrorController::Action::Abort);
  EXPECT_NE(errors.decide(session.endOfTrace()).action, OpenCsdErrorController::Action::Abort);
}

TEST(CtraceUnitTests, testOpenCsdItmSessionUsesSingleChannelAndAssociatedErrorLogger)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdItmDecoder decoder({}, sink);
  const std::uint8_t trace[]{
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('A'), 0x04U,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U, 0x01U, static_cast<std::uint8_t>('B'),
  };

  decoder.push(trace, sizeof(trace));
  EXPECT_EQ(decoder.finish().bytesIn, sizeof(trace));
  EXPECT_TRUE(sink.hasIssue(TraceIssueCode::OpenCsdInvalidPacketHeader));

  bool foundSoftware = false;
  for (const auto& element : sink.elements()) {
    if (element.kind == OpenCsdTraceElement::Kind::Software) {
      foundSoftware = true;
      EXPECT_EQ(element.route, TraceRouteIdentity{})
          << "OpenCSD SINGLE channel 0 must retain the synthetic ctrace route";
    }
  }
  EXPECT_TRUE(foundSoftware);
}

TEST(CtraceUnitTests, testOpenCsdSessionValidationRejectsInvalidApiResults)
{
  const std::uint32_t object = 1U;
  EXPECT_NO_THROW(OpenCsdSessionValidation::requireObject(&object, "valid object"));
  EXPECT_THROW(OpenCsdSessionValidation::requireObject(nullptr, "missing object"), OpenCsdItmSessionError);

  EXPECT_NO_THROW(OpenCsdSessionValidation::requireSuccess(OCSD_OK, "successful call"));
  const auto message = captureExceptionMessage<OpenCsdItmSessionError>(
      [] { OpenCsdSessionValidation::requireSuccess(OCSD_ERR_MEM, "decoder setup failed"); });
  ASSERT_TRUE(message.has_value());
  EXPECT_NE(message->find("OCSD_ERR_MEM"), std::string::npos);
  EXPECT_NE(message->find("decoder setup failed"), std::string::npos);
}
