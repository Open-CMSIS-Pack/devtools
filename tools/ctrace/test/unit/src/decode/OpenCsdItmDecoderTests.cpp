/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "OpenCsdErrorController.hpp"
#include "OpenCsdItmDecoder.hpp"
#include "OpenCsdItmSession.hpp"
#include "OpenCsdPacketCollector.hpp"
#include "OpenCsdTraceElement.hpp"
#include "common/ocsd_error.h"
#include "opencsd/itm/trc_pkt_elem_itm.h"
#include "opencsd/itm/trc_pkt_types_itm.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class CollectingOpenCsdElementSink final : public OpenCsdTraceElementSink {
public:
  void append(OpenCsdTraceElement element) override { elements.push_back(std::move(element)); }

  std::vector<OpenCsdTraceElement> elements;
};

struct ScriptedError {
  ocsd_err_severity_t severity = OCSD_ERR_SEV_ERROR;
  ocsd_err_t code = OCSD_ERR_FAIL;
  std::uint64_t index = 0U;
  std::string message;
};

struct SessionStep {
  SessionStep(ocsd_datapath_resp_t stepResponse = OCSD_RESP_CONT,
              std::optional<std::uint32_t> stepProcessed = std::nullopt, bool stepEmitSync = false,
              std::vector<ScriptedError> stepErrors = {})
    : response(stepResponse), processed(stepProcessed), emitSync(stepEmitSync), errors(std::move(stepErrors))
  {
  }

  ocsd_datapath_resp_t response = OCSD_RESP_CONT;
  std::optional<std::uint32_t> processed;
  bool emitSync = false;
  std::vector<ScriptedError> errors;
};

struct SessionScript {
  std::deque<SessionStep> pushes;
  std::deque<SessionStep> flushes;
  std::deque<SessionStep> resets;
  std::deque<SessionStep> ends;
  ocsd_datapath_resp_t defaultFlushResponse = OCSD_RESP_CONT;
  std::uint32_t pushCalls = 0U;
  std::uint32_t flushCalls = 0U;
  std::uint32_t resetCalls = 0U;
};

class ScriptedOpenCsdSession final : public OpenCsdItmSessionInterface {
public:
  ScriptedOpenCsdSession(std::shared_ptr<SessionScript> script, OpenCsdPacketCollector& collector,
                         OpenCsdErrorController& errors)
    : script_(std::move(script)), collector_(collector), errors_(errors)
  {
  }

  ocsd_datapath_resp_t pushData(ocsd_trc_index_t index, std::uint32_t size, const std::uint8_t*,
                                std::uint32_t& processed) override
  {
    ++script_->pushCalls;
    auto step = take(script_->pushes, SessionStep{});
    processed = step.processed.value_or(size);
    apply(step, index);
    return step.response;
  }

  ocsd_datapath_resp_t flush() override
  {
    ++script_->flushCalls;
    auto step = take(script_->flushes, SessionStep{script_->defaultFlushResponse});
    apply(step, 0U);
    return step.response;
  }

  ocsd_datapath_resp_t reset() override
  {
    ++script_->resetCalls;
    auto step = take(script_->resets, SessionStep{});
    apply(step, 0U);
    return step.response;
  }

  ocsd_datapath_resp_t endOfTrace() override
  {
    auto step = take(script_->ends, SessionStep{});
    apply(step, 0U);
    return step.response;
  }

private:
  static SessionStep take(std::deque<SessionStep>& steps, SessionStep fallback)
  {
    if (steps.empty()) {
      return fallback;
    }
    auto step = std::move(steps.front());
    steps.pop_front();
    return step;
  }

  void apply(const SessionStep& step, ocsd_trc_index_t index)
  {
    for (const auto& error : step.errors) {
      const ocsdError reported(error.severity, error.code, error.index, 1U, error.message);
      errors_.LogError(0U, &reported);
    }
    if (step.emitSync) {
      ItmTrcPacket packet;
      packet.setPktType(ITM_PKT_ASYNC);
      collector_.RawPacketDataMon(OCSD_OP_DATA, index, &packet, 0U, nullptr);
    }
  }

  std::shared_ptr<SessionScript> script_;
  OpenCsdPacketCollector& collector_;
  OpenCsdErrorController& errors_;
};

OpenCsdItmSessionFactory scriptedFactory(const std::shared_ptr<SessionScript>& script)
{
  return [script](OpenCsdPacketCollector& collector,
                  OpenCsdErrorController& errors) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    return std::make_unique<ScriptedOpenCsdSession>(script, collector, errors);
  };
}

std::vector<std::uint8_t> bytes(std::size_t size)
{
  return std::vector<std::uint8_t>(size, 0U);
}

bool hasIssue(const CollectingOpenCsdElementSink& sink, const std::string& issueCode)
{
  for (const auto& element : sink.elements) {
    if (element.issueCode == issueCode) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST(CtraceUnitTests, testOpenCsdItmDecoderChunksAndFinishesOnce)
{
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<SessionScript>();
  OpenCsdItmDecoder decoder(sink, scriptedFactory(script));
  const auto input = bytes(5000U);
  decoder.push(input.data(), static_cast<std::uint32_t>(input.size()));
  EXPECT_EQ(script->pushCalls, 2U);
  EXPECT_EQ(decoder.finish().bytesIn, input.size());
  EXPECT_EQ(decoder.finish().bytesIn, input.size());
  EXPECT_THROW(decoder.push(input.data(), 1U), std::runtime_error);
  EXPECT_TRUE(hasIssue(sink, "data-loss"));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderRecoversAndMarksConsumedDataLoss)
{
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<SessionScript>();
  script->pushes = {
      {OCSD_RESP_ERR_CONT,
       2U,
       false,
       {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 1U, "reserved header"}}},
      {OCSD_RESP_CONT, 2U, false, {}},
      {OCSD_RESP_CONT, 2U, true, {}},
  };
  OpenCsdItmDecoder decoder(sink, scriptedFactory(script));
  const auto input = bytes(6U);
  decoder.push(input.data(), static_cast<std::uint32_t>(input.size()));
  EXPECT_EQ(decoder.finish().bytesIn, 6U);
  EXPECT_EQ(script->resetCalls, 1U);
  EXPECT_TRUE(hasIssue(sink, "opencsd-invalid-packet-header"));
  EXPECT_TRUE(hasIssue(sink, "data-loss"));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderMarksUnframedDataBeforeSync)
{
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<SessionScript>();
  script->pushes = {
      {OCSD_RESP_CONT, 2U, false, {}},
      {OCSD_RESP_CONT, 2U, true, {}},
  };
  OpenCsdItmDecoder decoder(sink, scriptedFactory(script));
  const auto input = bytes(4U);
  decoder.push(input.data(), static_cast<std::uint32_t>(input.size()));
  decoder.finish();
  EXPECT_TRUE(hasIssue(sink, "data-loss"));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderFlushesWaitResponses)
{
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<SessionScript>();
  script->pushes = {
      {OCSD_RESP_WAIT, 1U, false, {}},
      {OCSD_RESP_WAIT, 1U, true, {}},
  };
  script->flushes = {{OCSD_RESP_CONT}, {OCSD_RESP_CONT}};
  OpenCsdItmDecoder decoder(sink, scriptedFactory(script));
  const auto input = bytes(2U);
  decoder.push(input.data(), static_cast<std::uint32_t>(input.size()));
  decoder.finish();
  EXPECT_EQ(script->flushCalls, 2U);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderReportsWarningsAndImplicitErrors)
{
  CollectingOpenCsdElementSink sink;
  const auto script = std::make_shared<SessionScript>();
  script->pushes = {{OCSD_RESP_ERR_CONT,
                     1U,
                     false,
                     {
                         {OCSD_ERR_SEV_INFO, OCSD_ERR_FAIL, 0U, "info"},
                         {OCSD_ERR_SEV_WARN, OCSD_ERR_BAD_PACKET_SEQ, 0U, "warning"},
                     }}};
  OpenCsdItmDecoder decoder(sink, scriptedFactory(script));
  const auto input = bytes(1U);
  decoder.push(input.data(), 1U);
  decoder.finish();
  ASSERT_GE(sink.elements.size(), 2U);
  EXPECT_EQ(sink.elements[0].issueSeverity, TraceIssueSeverity::Warning);
  EXPECT_EQ(sink.elements[1].issueSeverity, TraceIssueSeverity::Error);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderHandlesEndOfTraceDecisions)
{
  CollectingOpenCsdElementSink recoverySink;
  const auto recovery = std::make_shared<SessionScript>();
  recovery->ends = {{OCSD_RESP_ERR_CONT,
                     std::nullopt,
                     false,
                     {{OCSD_ERR_SEV_ERROR, OCSD_ERR_BAD_PACKET_SEQ, 0U, "bad tail"}}}};
  OpenCsdItmDecoder recoveryDecoder(recoverySink, scriptedFactory(recovery));
  EXPECT_NO_THROW(recoveryDecoder.finish());
  EXPECT_TRUE(hasIssue(recoverySink, "opencsd-bad-packet-sequence"));

  CollectingOpenCsdElementSink waitSink;
  const auto wait = std::make_shared<SessionScript>();
  wait->ends = {{OCSD_RESP_WAIT}};
  wait->flushes = {{OCSD_RESP_CONT}};
  OpenCsdItmDecoder waitDecoder(waitSink, scriptedFactory(wait));
  EXPECT_NO_THROW(waitDecoder.finish());
  EXPECT_EQ(wait->flushCalls, 1U);

  CollectingOpenCsdElementSink fatalSink;
  const auto fatal = std::make_shared<SessionScript>();
  fatal->ends = {{OCSD_RESP_FATAL_SYS_ERR}};
  OpenCsdItmDecoder fatalDecoder(fatalSink, scriptedFactory(fatal));
  EXPECT_THROW((void)fatalDecoder.finish(), OpenCsdFatalError);
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderAbortsFatalAndNoProgressData)
{
  CollectingOpenCsdElementSink fatalSink;
  const auto fatal = std::make_shared<SessionScript>();
  fatal->pushes = {{OCSD_RESP_FATAL_SYS_ERR, 1U}};
  OpenCsdItmDecoder fatalDecoder(fatalSink, scriptedFactory(fatal));
  const auto input = bytes(1U);
  try {
    fatalDecoder.push(input.data(), 1U);
    FAIL() << "fatal OpenCSD response did not abort decoding";
  } catch (const OpenCsdFatalError& error) {
    EXPECT_EQ(error.bytesProcessed(), 1U);
  }

  CollectingOpenCsdElementSink stalledSink;
  const auto stalled = std::make_shared<SessionScript>();
  stalled->pushes = {{OCSD_RESP_CONT, 0U}, {OCSD_RESP_CONT, 0U}};
  OpenCsdItmDecoder stalledDecoder(stalledSink, scriptedFactory(stalled));
  EXPECT_THROW(stalledDecoder.push(input.data(), 1U), OpenCsdFatalError);
  EXPECT_TRUE(hasIssue(stalledSink, "opencsd-no-progress"));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderHandlesFlushRecoveryAndTimeout)
{
  CollectingOpenCsdElementSink recoverySink;
  const auto recovery = std::make_shared<SessionScript>();
  recovery->ends = {{OCSD_RESP_WAIT}};
  recovery->flushes = {{OCSD_RESP_ERR_CONT,
                        std::nullopt,
                        false,
                        {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 0U, "bad flush"}}}};
  OpenCsdItmDecoder recoveryDecoder(recoverySink, scriptedFactory(recovery));
  EXPECT_NO_THROW(recoveryDecoder.finish());
  EXPECT_EQ(recovery->resetCalls, 1U);

  CollectingOpenCsdElementSink committedSink;
  const auto committed = std::make_shared<SessionScript>();
  committed->ends = {{OCSD_RESP_WAIT}};
  committed->flushes = {{OCSD_RESP_CONT, std::nullopt, true}};
  OpenCsdItmDecoder committedDecoder(committedSink, scriptedFactory(committed));
  EXPECT_NO_THROW(committedDecoder.finish());
  EXPECT_FALSE(committedSink.elements.empty());

  CollectingOpenCsdElementSink fatalSink;
  const auto fatal = std::make_shared<SessionScript>();
  fatal->ends = {{OCSD_RESP_WAIT}};
  fatal->flushes = {{OCSD_RESP_FATAL_SYS_ERR}};
  OpenCsdItmDecoder fatalDecoder(fatalSink, scriptedFactory(fatal));
  EXPECT_THROW((void)fatalDecoder.finish(), OpenCsdFatalError);

  CollectingOpenCsdElementSink timeoutSink;
  const auto timeout = std::make_shared<SessionScript>();
  timeout->ends = {{OCSD_RESP_WAIT}};
  timeout->defaultFlushResponse = OCSD_RESP_WAIT;
  OpenCsdItmDecoder timeoutDecoder(timeoutSink, scriptedFactory(timeout));
  EXPECT_THROW((void)timeoutDecoder.finish(), OpenCsdFatalError);
  EXPECT_EQ(timeout->flushCalls, 1024U);
  EXPECT_TRUE(hasIssue(timeoutSink, "opencsd-wait-timeout"));
}

TEST(CtraceUnitTests, testOpenCsdItmDecoderReportsResetAndInitializationFailures)
{
  CollectingOpenCsdElementSink resetSink;
  const auto reset = std::make_shared<SessionScript>();
  reset->pushes = {{OCSD_RESP_ERR_CONT,
                    1U,
                    false,
                    {{OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 0U, "bad packet"}}}};
  reset->resets = {{OCSD_RESP_WAIT}};
  OpenCsdItmDecoder resetDecoder(resetSink, scriptedFactory(reset));
  const auto input = bytes(1U);
  EXPECT_THROW(resetDecoder.push(input.data(), 1U), OpenCsdFatalError);

  CollectingOpenCsdElementSink nullSink;
  const OpenCsdItmSessionFactory nullFactory =
      [](OpenCsdPacketCollector&, OpenCsdErrorController&) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    return nullptr;
  };
  EXPECT_THROW((void)OpenCsdItmDecoder(nullSink, nullFactory), OpenCsdFatalError);
  EXPECT_TRUE(hasIssue(nullSink, "opencsd-initialization-error"));

  CollectingOpenCsdElementSink errorSink;
  const OpenCsdItmSessionFactory errorFactory =
      [](OpenCsdPacketCollector&, OpenCsdErrorController&) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    throw OpenCsdItmSessionError("synthetic session setup failure");
  };
  EXPECT_THROW((void)OpenCsdItmDecoder(errorSink, errorFactory), OpenCsdFatalError);
  EXPECT_TRUE(hasIssue(errorSink, "opencsd-initialization-error"));
}

TEST(CtraceUnitTests, testOpenCsdItmSessionAcceptsEmptyDataPathOperations)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(sink);
  OpenCsdErrorController errors;
  OpenCsdItmSession session(collector, errors);

  EXPECT_NE(errors.decide(session.reset()).action, OpenCsdErrorController::Action::Abort);
  EXPECT_NE(errors.decide(session.flush()).action, OpenCsdErrorController::Action::Abort);
  EXPECT_NE(errors.decide(session.endOfTrace()).action, OpenCsdErrorController::Action::Abort);
}
