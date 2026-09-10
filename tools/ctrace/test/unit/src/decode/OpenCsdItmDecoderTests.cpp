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

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using FormattedTraceTestSupport::itmHardwareSync;
using FormattedTraceTestSupport::itmSoftwarePacket;
using FormattedTraceTestSupport::memoryAlignedFrames;
using OpenCsdSessionTestSupport::errorObservation;
using OpenCsdSessionTestSupport::ScriptedDecoderHarness;
using OpenCsdSessionTestSupport::softwareCallback;
using OpenCsdSessionTestSupport::syncCallback;
using OpenCsdTestSupport::CollectingOpenCsdElementSink;

namespace {

/** @brief Tracks calls made after a synthetic formatted-session failure. */
struct ThrowingSessionState {
  std::uint32_t resetCalls = 0U;
};

/** @brief Records callback-target availability during session destruction. */
struct SessionLifetimeState {
  bool sessionDestroyed = false;
  bool collectorAvailable = false;
  bool errorControllerAvailable = false;
};

/** @brief Probes the callback targets while the owning decoder destroys its session. */
class SessionLifetimeProbe final : public OpenCsdItmSessionInterface {
public:
  /** @brief Retains the callback targets installed in a production session. */
  SessionLifetimeProbe(std::shared_ptr<SessionLifetimeState> state, OpenCsdPacketCollector& collector,
                       OpenCsdErrorController& errorController)
    : m_state(std::move(state)),
      m_collector(collector),
      m_errorController(errorController)
  {
  }

  /** @brief Verifies that both callback targets outlive the external session. */
  ~SessionLifetimeProbe() noexcept override
  {
    try {
      m_errorController.beginDataPathCall();
      m_state->errorControllerAvailable =
          m_errorController.decide(OCSD_RESP_CONT).action == OpenCsdErrorController::Action::Continue;
      m_collector.appendDecodeError(0U, "session destruction lifetime probe", TraceIssueCode::DecodeError, false,
                                    TraceIssueSeverity::Warning);
      m_state->collectorAvailable = true;
    } catch (...) {
      // A failed probe is reported through the state without throwing from a destructor.
    }
    m_state->sessionDestroyed = true;
  }

  /** @brief Accepts unused input. */
  ocsd_datapath_resp_t pushData(ocsd_trc_index_t, std::uint32_t, const std::uint8_t*, std::uint32_t&) override
  {
    return OCSD_RESP_CONT;
  }

  /** @brief Accepts an unused flush. */
  ocsd_datapath_resp_t flush() override
  {
    return OCSD_RESP_CONT;
  }

  /** @brief Accepts an unused complete reset. */
  ocsd_datapath_resp_t reset() override
  {
    return OCSD_RESP_CONT;
  }

  /** @brief Accepts an unused route reset. */
  ocsd_datapath_resp_t resetRoute(std::uint8_t, ocsd_trc_index_t) override
  {
    return OCSD_RESP_CONT;
  }

  /** @brief Accepts an unused end-of-trace operation. */
  ocsd_datapath_resp_t endOfTrace() override
  {
    return OCSD_RESP_CONT;
  }

private:
  std::shared_ptr<SessionLifetimeState> m_state;
  OpenCsdPacketCollector& m_collector;
  OpenCsdErrorController& m_errorController;
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

  /** @brief Records an unexpected route-local recovery reset. */
  ocsd_datapath_resp_t resetRoute(std::uint8_t, ocsd_trc_index_t) override
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

TEST(CtraceUnitTests, testOpenCsdItmDecoderDestroysSessionBeforeItsCallbackTargets)
{
  CollectingOpenCsdElementSink sink;
  const auto state = std::make_shared<SessionLifetimeState>();
  const OpenCsdItmSessionFactory factory = [state](OpenCsdPacketCollector& collector,
                                                   OpenCsdErrorController& errorController) {
    return std::make_unique<SessionLifetimeProbe>(state, collector, errorController);
  };

  {
    OpenCsdItmDecoder decoder(TraceRouteIdentity{}, sink, factory);
  }

  EXPECT_TRUE(state->sessionDestroyed);
  EXPECT_TRUE(state->collectorAvailable);
  EXPECT_TRUE(state->errorControllerAvailable);
  EXPECT_TRUE(sink.hasIssue(TraceIssueCode::DecodeError));
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

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderRecoversRoutedProtocolFailure)
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

  EXPECT_NO_THROW(decoder.push(frame.data(), frame.size()));
  EXPECT_EQ(decoder.finish().bytesIn, frame.size());
  EXPECT_EQ(script->resetCalls, 0U);
  EXPECT_EQ(script->routeResetCalls[1U], 1U);
  EXPECT_EQ(script->flushCalls, 1U);
  EXPECT_TRUE(sink.hasIssue(TraceIssueCode::OpenCsdInvalidPacketHeader));
  EXPECT_TRUE(sink.hasIssue(TraceIssueCode::DataLoss));
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderPreservesOtherRoutesAndDrainsAfterLocalReset)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes.emplace_back(OCSD_RESP_FATAL_INVALID_DATA, 16U,
                              std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
                                  softwareCallback(route1, 3U, 0U, 'A'), softwareCallback(route2, 4U, 0U, 'X'),
                                  errorObservation(OCSD_ERR_INVALID_PCKT_HDR, 5U, 2U, "reserved header"),
                                  softwareCallback(route1, 7U, 1U, 'B')});
  script->flushes.emplace_back(
      OCSD_RESP_WAIT, std::nullopt,
      std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{softwareCallback(route1, 8U, 2U, 'C')});
  script->flushes.emplace_back(OCSD_RESP_CONT, std::nullopt,
                               std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
                                   syncCallback(route2, 9U), softwareCallback(route2, 10U, 3U, 'D')});
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  EXPECT_NO_THROW(decoder.push(frame.data(), frame.size()));
  EXPECT_EQ(decoder.finish().bytesIn, frame.size());
  EXPECT_EQ(script->resetCalls, 0U);
  EXPECT_EQ(script->routeResetCalls[1U], 0U);
  EXPECT_EQ(script->routeResetCalls[2U], 1U);
  EXPECT_EQ(script->routeResetOrder, std::vector<std::uint8_t>{2U});
  EXPECT_EQ(script->routeResetIndexes, std::vector<ocsd_trc_index_t>{5U});
  EXPECT_EQ(script->flushCalls, 2U);

  std::vector<std::pair<TraceRouteIdentity, std::uint32_t>> software;
  std::vector<const OpenCsdTraceElement*> losses;
  for (const auto& element : sink.elements()) {
    if (element.kind == OpenCsdTraceElement::Kind::Software) {
      software.emplace_back(element.route, element.value);
    }
    if (element.issueCode == TraceIssueCode::DataLoss) {
      losses.push_back(&element);
    }
  }
  EXPECT_EQ(software, (std::vector<std::pair<TraceRouteIdentity, std::uint32_t>>{
                          {route1, 'A'}, {route2, 'X'}, {route1, 'B'}, {route1, 'C'}, {route2, 'D'}}));
  ASSERT_EQ(losses.size(), 1U);
  EXPECT_EQ(losses.front()->route, route2);
  EXPECT_EQ(losses.front()->sourceIndex, 5U);
  EXPECT_EQ(losses.front()->rawBytesConsumed, 4U);

  ASSERT_EQ(sink.elements().size(), 8U);
  EXPECT_EQ(sink.elements()[0].value, 'A');
  EXPECT_EQ(sink.elements()[1].value, 'X');
  EXPECT_EQ(sink.elements()[2].issueCode, TraceIssueCode::OpenCsdInvalidPacketHeader);
  EXPECT_EQ(sink.elements()[2].route, route2);
  EXPECT_EQ(sink.elements()[3].value, 'B');
  EXPECT_EQ(sink.elements()[4].value, 'C');
  EXPECT_EQ(sink.elements()[5].issueCode, TraceIssueCode::DataLoss);
  EXPECT_EQ(sink.elements()[6].kind, OpenCsdTraceElement::Kind::Sync);
  EXPECT_EQ(sink.elements()[7].value, 'D');
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderMakesUnassignableFailureInputFatal)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes.emplace_back(OCSD_RESP_ERR_CONT, 16U,
                              std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
                                  softwareCallback(route1, 3U, 0U, 'A'),
                                  errorObservation(OCSD_ERR_INVALID_PCKT_HDR, 5U, 2U, "routed error"),
                                  softwareCallback(route1, 7U, 1U, 'B'),
                                  errorObservation(OCSD_ERR_MEM, 8U, std::nullopt, "input-wide failure")});
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  EXPECT_THROW(decoder.push(frame.data(), frame.size()), OpenCsdFatalError);
  EXPECT_TRUE(script->routeResetCalls.empty());
  EXPECT_EQ(script->resetCalls, 0U);
  EXPECT_EQ(script->flushCalls, 0U);
  EXPECT_TRUE(std::none_of(sink.elements().begin(), sink.elements().end(), [](const auto& element) {
    return element.kind == OpenCsdTraceElement::Kind::Software;
  })) << "input-wide fatal failure must roll back the complete callback batch";
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderMakesDeformatterErrorInputFatal)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes.emplace_back(OCSD_RESP_ERR_CONT, 16U,
                              std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
                                  softwareCallback(route1, 2U, 0U, 'A'),
                                  errorObservation(OCSD_ERR_DFMTR_BAD_FHSYNC, 5U, std::nullopt, "bad formatter sync"),
                                  softwareCallback(route2, 7U, 1U, 'B')});
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  EXPECT_THROW(decoder.push(frame.data(), frame.size()), OpenCsdFatalError);
  EXPECT_TRUE(script->routeResetCalls.empty());
  EXPECT_EQ(script->resetCalls, 0U);
  EXPECT_EQ(script->flushCalls, 0U);
  ASSERT_EQ(sink.elements().size(), 1U);
  EXPECT_EQ(sink.elements().front().kind, OpenCsdTraceElement::Kind::Error);
  EXPECT_EQ(sink.elements().front().issueCode, TraceIssueCode::OpenCsdDecodeError);
  EXPECT_EQ(sink.elements().front().sourceIndex, 5U);
  EXPECT_TRUE(sink.elements().front().discontinuity);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderPreservesContinuousFormatterStateAcrossRouteRecovery)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  auto route1Prefix = itmHardwareSync();
  const auto append = [](std::vector<std::uint8_t>& target, const std::vector<std::uint8_t>& bytes) {
    target.insert(target.end(), bytes.begin(), bytes.end());
  };
  append(route1Prefix, itmSoftwarePacket(1U, 'A'));

  auto route2Payload = itmHardwareSync();
  route2Payload.push_back(0x04U);
  append(route2Payload, itmSoftwarePacket(0U, 'X'));
  append(route2Payload, itmHardwareSync());
  for (const auto& item : std::array<std::pair<std::uint8_t, std::uint8_t>, 4U>{
           std::pair<std::uint8_t, std::uint8_t>{2U, 'C'}, {3U, 'D'}, {4U, 'E'}, {5U, 'F'}}) {
    append(route2Payload, itmSoftwarePacket(item.first, item.second));
  }
  const auto capture = memoryAlignedFrames(
      {{1U, route1Prefix}, {2U, route2Payload}, {1U, itmSoftwarePacket(2U, static_cast<std::uint8_t>('B'))}});
  ASSERT_EQ(capture.size(), 48U) << "fixture must cross a frame without repeating the active formatter ID";

  CollectingOpenCsdElementSink sink;
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink);
  EXPECT_NO_THROW(decoder.push(capture.data(), static_cast<std::uint32_t>(capture.size())));
  EXPECT_EQ(decoder.finish().bytesIn, capture.size());

  std::vector<std::pair<TraceRouteIdentity, std::uint32_t>> software;
  std::vector<const OpenCsdTraceElement*> protocolErrors;
  std::vector<const OpenCsdTraceElement*> losses;
  for (const auto& element : sink.elements()) {
    if (element.kind == OpenCsdTraceElement::Kind::Software) {
      software.emplace_back(element.route, element.value);
    }
    if (element.issueCode == TraceIssueCode::OpenCsdInvalidPacketHeader) {
      protocolErrors.push_back(&element);
    }
    if (element.issueCode == TraceIssueCode::DataLoss) {
      losses.push_back(&element);
    }
  }
  EXPECT_EQ(software, (std::vector<std::pair<TraceRouteIdentity, std::uint32_t>>{
                          {route1, 'A'}, {route2, 'C'}, {route2, 'D'}, {route2, 'E'}, {route2, 'F'}, {route1, 'B'}}));
  ASSERT_EQ(protocolErrors.size(), 1U);
  EXPECT_EQ(protocolErrors.front()->route, route2);
  EXPECT_EQ(protocolErrors.front()->sourceIndex, 17U);
  ASSERT_EQ(losses.size(), 1U);
  EXPECT_EQ(losses.front()->route, route2);
  EXPECT_FALSE(sink.hasIssue(TraceIssueCode::OpenCsdFormattedInputError));
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderRecoversSeveralRoutesOnceAtTheirEarliestFailure)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes.emplace_back(
      OCSD_RESP_ERR_CONT, 16U,
      std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
          softwareCallback(route2, 3U, 0U, 'A'), softwareCallback(route1, 4U, 0U, 'B'),
          errorObservation(OCSD_ERR_INVALID_PCKT_HDR, 5U, 2U), softwareCallback(route1, 6U, 1U, 'C'),
          softwareCallback(route2, 7U, 1U, 'D'), errorObservation(OCSD_ERR_BAD_PACKET_SEQ, 8U, 1U),
          errorObservation(OCSD_ERR_BAD_PACKET_SEQ, 9U, 2U), softwareCallback(route1, 10U, 2U, 'E')});
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  EXPECT_NO_THROW(decoder.push(frame.data(), frame.size()));
  EXPECT_EQ(decoder.finish().bytesIn, frame.size());
  EXPECT_EQ(script->routeResetOrder, (std::vector<std::uint8_t>{1U, 2U}));
  EXPECT_EQ(script->routeResetIndexes, (std::vector<ocsd_trc_index_t>{8U, 5U}));
  EXPECT_EQ(script->routeResetCalls[1U], 1U);
  EXPECT_EQ(script->routeResetCalls[2U], 1U);
  EXPECT_EQ(script->flushCalls, 1U);

  std::vector<std::uint32_t> software;
  for (const auto& element : sink.elements()) {
    if (element.kind == OpenCsdTraceElement::Kind::Software) {
      software.push_back(element.value);
    }
  }
  EXPECT_EQ(software, (std::vector<std::uint32_t>{'A', 'B', 'C'}));
  EXPECT_EQ(static_cast<std::size_t>(
                std::count_if(sink.elements().begin(), sink.elements().end(),
                              [](const auto& item) { return item.issueCode == TraceIssueCode::DataLoss; })),
            2U);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderAbortsFailedLocalResetWithoutDraining)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes = {
      {OCSD_RESP_ERR_CONT, 16U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 5U, "bad route", 2U}}}};
  script->routeResets[2U] = {{OCSD_RESP_WAIT}};
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  try {
    decoder.push(frame.data(), frame.size());
    FAIL() << "failed route-local reset did not abort formatted input";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_EQ(error.bytesProcessed(), frame.size());
  }
  EXPECT_EQ(script->routeResetCalls[2U], 1U);
  EXPECT_EQ(script->routeResetCalls[1U], 0U);
  EXPECT_EQ(script->resetCalls, 0U);
  EXPECT_EQ(script->flushCalls, 0U);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderBoundsPendingFrameDrain)
{
  const TraceRouteIdentity route{TraceRouteId{0U}, 2U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes = {
      {OCSD_RESP_ERR_CONT, 16U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 5U, "bad route", 2U}}}};
  script->defaultFlushResponse = OCSD_RESP_WAIT;
  OpenCsdItmDecoder decoder({route}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  try {
    decoder.push(frame.data(), frame.size());
    FAIL() << "unbounded formatted drain did not abort";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_EQ(error.bytesProcessed(), frame.size());
  }
  EXPECT_EQ(script->routeResetCalls[2U], 1U);
  EXPECT_EQ(script->flushCalls, 1024U);
  EXPECT_TRUE(sink.hasIssue(TraceIssueCode::OpenCsdWaitTimeout));
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderClosesUnresolvedRouteLossAtEndOfTrace)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  script->pushes.emplace_back(
      OCSD_RESP_ERR_CONT, 16U,
      std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{errorObservation(OCSD_ERR_INVALID_PCKT_HDR, 5U, 2U),
                                                                  softwareCallback(route1, 9U, 0U, 'A')});
  OpenCsdItmDecoder decoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                            OpenCsdSessionTestSupport::scriptedFactory(script));
  const std::array<std::uint8_t, 16U> frame{};

  decoder.push(frame.data(), frame.size());
  EXPECT_EQ(decoder.finish().bytesIn, frame.size());
  EXPECT_EQ(script->routeResetCalls[2U], 1U);
  EXPECT_EQ(script->flushCalls, 1U);

  std::vector<const OpenCsdTraceElement*> losses;
  for (const auto& element : sink.elements()) {
    if (element.issueCode == TraceIssueCode::DataLoss) {
      losses.push_back(&element);
    }
  }
  ASSERT_EQ(losses.size(), 1U);
  EXPECT_EQ(losses.front()->route, route2);
  EXPECT_EQ(losses.front()->sourceIndex, 5U);
  EXPECT_EQ(losses.front()->rawBytesConsumed, 11U);
  EXPECT_NE(losses.front()->errorMessage.find("no later hardware sync"), std::string::npos);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderRejectsFatalResponseAndInvalidRootProgress)
{
  const TraceRouteIdentity route{TraceRouteId{0U}, 1U};
  const std::array<std::uint8_t, 16U> frame{};
  for (const auto& step : std::array<OpenCsdSessionTestSupport::SessionStep, 3U>{
           OpenCsdSessionTestSupport::SessionStep{OCSD_RESP_FATAL_SYS_ERR, 16U},
           OpenCsdSessionTestSupport::SessionStep{OCSD_RESP_CONT, 17U},
           OpenCsdSessionTestSupport::SessionStep{OCSD_RESP_CONT, 8U}}) {
    CollectingOpenCsdElementSink sink;
    const auto script = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
    script->pushes = {step};
    OpenCsdItmDecoder decoder({route}, OpenCsdItmInputMode::CoreSightFormatted, sink,
                              OpenCsdSessionTestSupport::scriptedFactory(script));

    EXPECT_THROW(decoder.push(frame.data(), frame.size()), OpenCsdFatalError);
    EXPECT_TRUE(script->routeResetCalls.empty());
    EXPECT_EQ(script->flushCalls, 0U);
    EXPECT_TRUE(sink.hasIssue(step.response == OCSD_RESP_FATAL_SYS_ERR ? TraceIssueCode::OpenCsdDecodeError
                                                                       : TraceIssueCode::OpenCsdNoProgress));
  }

  CollectingOpenCsdElementSink mixedSink;
  const auto mixedScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  mixedScript->pushes.emplace_back(OCSD_RESP_FATAL_SYS_ERR, 16U,
                                   std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
                                       softwareCallback(route, 2U, 0U, 'A'),
                                       errorObservation(OCSD_ERR_INVALID_PCKT_HDR, 3U, 1U, "subordinate routed error"),
                                       softwareCallback(route, 4U, 1U, 'B')});
  OpenCsdItmDecoder mixedDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, mixedSink,
                                 OpenCsdSessionTestSupport::scriptedFactory(mixedScript));
  try {
    mixedDecoder.push(frame.data(), frame.size());
    FAIL() << "fatal response was hidden by its routed recoverable logger error";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_NE(std::string(error.what()).find("system error"), std::string::npos);
    EXPECT_EQ(std::string(error.what()).find("invalid ITM packet header"), std::string::npos);
  }
  ASSERT_EQ(mixedSink.elements().size(), 1U);
  EXPECT_NE(mixedSink.elements().front().errorMessage.find("system error"), std::string::npos);
  EXPECT_TRUE(mixedScript->routeResetCalls.empty());
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderReportsWarningsAndImplicitResponseErrors)
{
  const TraceRouteIdentity route{TraceRouteId{0U}, 1U};
  const std::array<std::uint8_t, 16U> frame{};
  CollectingOpenCsdElementSink warningSink;
  const auto warningScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  warningScript->pushes.emplace_back(OCSD_RESP_CONT, 16U,
                                     std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
                                         errorObservation(OCSD_ERR_FAIL, 3U, 1U, "info", OCSD_ERR_SEV_INFO),
                                         errorObservation(OCSD_ERR_FAIL, 4U, 1U, "warning", OCSD_ERR_SEV_WARN)});
  OpenCsdItmDecoder warningDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, warningSink,
                                   OpenCsdSessionTestSupport::scriptedFactory(warningScript));
  warningDecoder.push(frame.data(), frame.size());
  warningDecoder.finish();
  ASSERT_EQ(warningSink.elements().size(), 1U);
  EXPECT_EQ(warningSink.elements().front().route, route);
  EXPECT_EQ(warningSink.elements().front().issueSeverity, TraceIssueSeverity::Warning);

  CollectingOpenCsdElementSink implicitSink;
  const auto implicitScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  implicitScript->pushes = {{OCSD_RESP_ERR_CONT, 16U}};
  OpenCsdItmDecoder implicitDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, implicitSink,
                                    OpenCsdSessionTestSupport::scriptedFactory(implicitScript));
  EXPECT_THROW(implicitDecoder.push(frame.data(), frame.size()), OpenCsdFatalError);
  EXPECT_TRUE(implicitSink.hasIssue(TraceIssueCode::OpenCsdDecodeError));
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderHandlesWaitAndBoundsZeroProgressRetry)
{
  const TraceRouteIdentity route{TraceRouteId{0U}, 1U};
  const std::array<std::uint8_t, 16U> frame{};
  CollectingOpenCsdElementSink waitSink;
  const auto waitScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  waitScript->pushes = {{OCSD_RESP_WAIT, 16U}};
  waitScript->flushes = {{OCSD_RESP_CONT}};
  OpenCsdItmDecoder waitDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, waitSink,
                                OpenCsdSessionTestSupport::scriptedFactory(waitScript));
  EXPECT_NO_THROW(waitDecoder.push(frame.data(), frame.size()));
  EXPECT_EQ(waitScript->flushCalls, 1U);

  CollectingOpenCsdElementSink stalledSink;
  const auto stalledScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  stalledScript->pushes = {{OCSD_RESP_WAIT, 0U}, {OCSD_RESP_WAIT, 0U}};
  stalledScript->flushes = {{OCSD_RESP_CONT}, {OCSD_RESP_CONT}};
  OpenCsdItmDecoder stalledDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, stalledSink,
                                   OpenCsdSessionTestSupport::scriptedFactory(stalledScript));
  EXPECT_THROW(stalledDecoder.push(frame.data(), frame.size()), OpenCsdFatalError);
  EXPECT_EQ(stalledScript->pushCalls, 2U);
  EXPECT_EQ(stalledScript->flushCalls, 2U);
  EXPECT_TRUE(stalledSink.hasIssue(TraceIssueCode::OpenCsdNoProgress));
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderHandlesRecoveryAndFatalErrorDuringDrain)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  const std::array<std::uint8_t, 16U> frame{};
  CollectingOpenCsdElementSink recoverySink;
  const auto recoveryScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  recoveryScript->pushes = {
      {OCSD_RESP_ERR_CONT, 16U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 5U, "route 2", 2U}}}};
  recoveryScript->flushes = {
      {OCSD_RESP_ERR_CONT, std::nullopt, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_BAD_PACKET_SEQ, 7U, "route 1", 1U}}},
      {OCSD_RESP_CONT}};
  OpenCsdItmDecoder recoveryDecoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, recoverySink,
                                    OpenCsdSessionTestSupport::scriptedFactory(recoveryScript));
  EXPECT_NO_THROW(recoveryDecoder.push(frame.data(), frame.size()));
  EXPECT_EQ(recoveryScript->routeResetOrder, (std::vector<std::uint8_t>{2U, 1U}));
  EXPECT_EQ(recoveryScript->flushCalls, 2U);

  CollectingOpenCsdElementSink fatalSink;
  const auto fatalScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  fatalScript->pushes = {
      {OCSD_RESP_ERR_CONT, 16U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 5U, "route 2", 2U}}}};
  fatalScript->flushes = {{OCSD_RESP_FATAL_SYS_ERR}};
  OpenCsdItmDecoder fatalDecoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, fatalSink,
                                 OpenCsdSessionTestSupport::scriptedFactory(fatalScript));
  EXPECT_THROW(fatalDecoder.push(frame.data(), frame.size()), OpenCsdFatalError);
  EXPECT_EQ(fatalScript->routeResetCalls[2U], 1U);
  EXPECT_EQ(fatalScript->flushCalls, 1U);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderHandlesResetCallbacksAndReportedResetError)
{
  const TraceRouteIdentity route1{TraceRouteId{0U}, 1U};
  const TraceRouteIdentity route2{TraceRouteId{1U}, 2U};
  const std::array<std::uint8_t, 16U> frame{};
  CollectingOpenCsdElementSink callbackSink;
  const auto callbackScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  callbackScript->pushes = {
      {OCSD_RESP_ERR_CONT, 16U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 5U, "route 2", 2U}}}};
  callbackScript->routeResets[2U].emplace_back(
      OCSD_RESP_CONT, std::nullopt,
      std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{softwareCallback(route1, 7U, 1U, 'A')});
  OpenCsdItmDecoder callbackDecoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, callbackSink,
                                    OpenCsdSessionTestSupport::scriptedFactory(callbackScript));
  EXPECT_NO_THROW(callbackDecoder.push(frame.data(), frame.size()));
  EXPECT_TRUE(std::any_of(callbackSink.elements().begin(), callbackSink.elements().end(), [](const auto& element) {
    return element.kind == OpenCsdTraceElement::Kind::Software && element.value == 'A';
  }));

  CollectingOpenCsdElementSink errorSink;
  const auto errorScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  errorScript->pushes = {
      {OCSD_RESP_ERR_CONT, 16U, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 5U, "route 2", 2U}}}};
  errorScript->routeResets[2U] = {{OCSD_RESP_ERR_CONT,
                                   std::nullopt,
                                   false,
                                   {{OCSD_ERR_SEV_ERROR, OCSD_ERR_BAD_PACKET_SEQ, 5U, "reset error", 2U}}}};
  OpenCsdItmDecoder errorDecoder({route1, route2}, OpenCsdItmInputMode::CoreSightFormatted, errorSink,
                                 OpenCsdSessionTestSupport::scriptedFactory(errorScript));
  EXPECT_THROW(errorDecoder.push(frame.data(), frame.size()), OpenCsdFatalError);
  EXPECT_EQ(errorScript->routeResetCalls[2U], 1U);
  EXPECT_EQ(errorScript->flushCalls, 0U);
}

TEST(CtraceUnitTests, testFormattedOpenCsdItmDecoderClosesRecoveryRepeatedBeforeAndDuringEndOfTrace)
{
  const TraceRouteIdentity route{TraceRouteId{0U}, 2U};
  const std::array<std::uint8_t, 32U> frames{};
  CollectingOpenCsdElementSink repeatedSink;
  const auto repeatedScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  repeatedScript->pushes.emplace_back(
      OCSD_RESP_ERR_CONT, 16U,
      std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{errorObservation(OCSD_ERR_INVALID_PCKT_HDR, 5U, 2U)});
  repeatedScript->pushes.emplace_back(
      OCSD_RESP_ERR_CONT, 16U,
      std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{syncCallback(route, 18U),
                                                                  errorObservation(OCSD_ERR_BAD_PACKET_SEQ, 20U, 2U)});
  OpenCsdItmDecoder repeatedDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, repeatedSink,
                                    OpenCsdSessionTestSupport::scriptedFactory(repeatedScript));
  repeatedDecoder.push(frames.data(), frames.size());
  repeatedDecoder.finish();
  EXPECT_EQ(repeatedScript->routeResetCalls[2U], 2U);
  EXPECT_EQ(static_cast<std::size_t>(
                std::count_if(repeatedSink.elements().begin(), repeatedSink.elements().end(),
                              [](const auto& element) { return element.issueCode == TraceIssueCode::DataLoss; })),
            2U);

  CollectingOpenCsdElementSink endRecoverySink;
  const auto endRecoveryScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  endRecoveryScript->ends = {{OCSD_RESP_ERR_CONT,
                              std::nullopt,
                              false,
                              {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 3U, "end error", 2U}}}};
  OpenCsdItmDecoder endRecoveryDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, endRecoverySink,
                                       OpenCsdSessionTestSupport::scriptedFactory(endRecoveryScript));
  EXPECT_NO_THROW((void)endRecoveryDecoder.finish());
  EXPECT_EQ(endRecoveryScript->routeResetCalls[2U], 0U);
  EXPECT_TRUE(endRecoverySink.hasIssue(TraceIssueCode::DataLoss));

  CollectingOpenCsdElementSink endWaitSink;
  const auto endWaitScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  endWaitScript->ends = {{OCSD_RESP_WAIT}};
  endWaitScript->flushes = {{OCSD_RESP_CONT}};
  OpenCsdItmDecoder endWaitDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, endWaitSink,
                                   OpenCsdSessionTestSupport::scriptedFactory(endWaitScript));
  EXPECT_NO_THROW((void)endWaitDecoder.finish());
  EXPECT_EQ(endWaitScript->flushCalls, 1U);

  CollectingOpenCsdElementSink endRecoveryWaitSink;
  const auto endRecoveryWaitScript = std::make_shared<OpenCsdSessionTestSupport::SessionScript>();
  endRecoveryWaitScript->ends.emplace_back(OCSD_RESP_ERR_WAIT, std::nullopt,
                                           std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
                                               errorObservation(OCSD_ERR_INVALID_PCKT_HDR, 3U, 2U, "end error")});
  endRecoveryWaitScript->flushes.emplace_back(OCSD_RESP_CONT, std::nullopt,
                                              std::vector<OpenCsdSessionTestSupport::ScriptedObservation>{
                                                  syncCallback(route, 5U), softwareCallback(route, 6U, 0U, 'A')});
  OpenCsdItmDecoder endRecoveryWaitDecoder({route}, OpenCsdItmInputMode::CoreSightFormatted, endRecoveryWaitSink,
                                           OpenCsdSessionTestSupport::scriptedFactory(endRecoveryWaitScript));
  EXPECT_NO_THROW((void)endRecoveryWaitDecoder.finish());
  EXPECT_EQ(endRecoveryWaitScript->routeResetCalls[2U], 1U);
  EXPECT_EQ(endRecoveryWaitScript->flushCalls, 1U);
  EXPECT_TRUE(endRecoveryWaitSink.hasIssue(TraceIssueCode::DataLoss));
  EXPECT_TRUE(std::any_of(
      endRecoveryWaitSink.elements().begin(), endRecoveryWaitSink.elements().end(),
      [](const auto& element) { return element.kind == OpenCsdTraceElement::Kind::Software && element.value == 'A'; }));
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
  EXPECT_EQ(harness.script().resetCalls, 0U);
  EXPECT_EQ(harness.script().routeResetCalls[0U], 1U);
  EXPECT_EQ(harness.script().routeResetIndexes, (std::vector<ocsd_trc_index_t>{1U}));
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
  EXPECT_EQ(recovery.script().resetCalls, 0U);
  EXPECT_EQ(recovery.script().routeResetCalls[0U], 1U);
  EXPECT_EQ(recovery.script().routeResetIndexes, (std::vector<ocsd_trc_index_t>{0U}));
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
  EXPECT_EQ(recovery.script().resetCalls, 0U);
  EXPECT_EQ(recovery.script().routeResetCalls[0U], 1U);
  EXPECT_EQ(recovery.script().routeResetIndexes, (std::vector<ocsd_trc_index_t>{0U}));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderHandlesFlushRecoveryAndTimeout)
{
  ScriptedDecoderHarness recovery;
  recovery.script().ends = {{OCSD_RESP_WAIT}};
  recovery.script().flushes = {
      {OCSD_RESP_ERR_CONT, std::nullopt, false, {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 0U, "bad flush"}}}};
  EXPECT_NO_THROW(recovery.decoder().finish());
  EXPECT_EQ(recovery.script().resetCalls, 0U);
  EXPECT_EQ(recovery.script().routeResetCalls[0U], 1U);
  EXPECT_EQ(recovery.script().routeResetIndexes, (std::vector<ocsd_trc_index_t>{0U}));

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
  reset.script().routeResets[0U] = {{OCSD_RESP_WAIT}};
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

TEST(CtraceUnitTests, testOpenCsdItmSessionUsesDecoderLocalResetForSingleInput)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(TraceRouteIdentity{}, sink);
  OpenCsdErrorController errors;
  OpenCsdItmSession session(collector, errors);

  EXPECT_NE(errors.decide(session.reset()).action, OpenCsdErrorController::Action::Abort);
  EXPECT_NE(errors.decide(session.resetRoute(0U, 37U)).action, OpenCsdErrorController::Action::Abort);
  EXPECT_THROW(session.resetRoute(1U, 38U), OpenCsdItmSessionError);
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
