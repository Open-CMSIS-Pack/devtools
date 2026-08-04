/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "OpenCsdTestSupport.h"
#include "OpenCsdSessionTestSupport.h"
#include "TestSupport.h"

#include <gtest/gtest.h>

#include "OpenCsdErrorController.h"
#include "OpenCsdItmDecoder.h"
#include "OpenCsdItmSession.h"
#include "OpenCsdPacketCollector.h"
#include "OpenCsdTraceElement.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

using OpenCsdSessionTestSupport::ScriptedDecoderHarness;
using OpenCsdTestSupport::CollectingOpenCsdElementSink;

TEST(CtraceUnitTests, testOpenCsdItmDecoderChunksAndFinishesOnce)
{
  ScriptedDecoderHarness harness;
  harness.push(5000U);
  EXPECT_EQ(harness.script().pushCalls, 2U);
  EXPECT_EQ(harness.decoder().finish().bytesIn, 5000U);
  EXPECT_EQ(harness.decoder().finish().bytesIn, 5000U);
  EXPECT_THROW(harness.push(1U), std::runtime_error);
  EXPECT_TRUE(harness.sink().hasIssue("data-loss"));
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
  EXPECT_TRUE(harness.sink().hasIssue("opencsd-invalid-packet-header"));
  EXPECT_TRUE(harness.sink().hasIssue("data-loss"));
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
  EXPECT_TRUE(harness.sink().hasIssue("data-loss"));
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
  EXPECT_TRUE(recovery.sink().hasIssue("opencsd-bad-packet-sequence"));

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
  EXPECT_TRUE(stalled.sink().hasIssue("opencsd-no-progress"));
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
  EXPECT_TRUE(recovery.sink().hasIssue("opencsd-no-progress"));

  ScriptedDecoderHarness wait;
  wait.script().pushes = {
      {OCSD_RESP_WAIT, 0U},
      {OCSD_RESP_WAIT, 0U},
  };
  wait.script().flushes = {{OCSD_RESP_CONT}};
  EXPECT_THROW(wait.push(1U), OpenCsdFatalError);
  EXPECT_EQ(wait.script().pushCalls, 2U);
  EXPECT_EQ(wait.script().flushCalls, 1U);
  EXPECT_TRUE(wait.sink().hasIssue("opencsd-no-progress"));
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
  EXPECT_TRUE(timeout.sink().hasIssue("opencsd-wait-timeout"));
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
  EXPECT_THROW((void)OpenCsdItmDecoder(nullSink, nullFactory), OpenCsdFatalError);
  EXPECT_TRUE(nullSink.hasIssue("opencsd-initialization-error"));

  CollectingOpenCsdElementSink errorSink;
  const OpenCsdItmSessionFactory errorFactory =
      [](OpenCsdPacketCollector&, OpenCsdErrorController&) -> std::unique_ptr<OpenCsdItmSessionInterface> {
    throw OpenCsdItmSessionError("synthetic session setup failure");
  };
  EXPECT_THROW((void)OpenCsdItmDecoder(errorSink, errorFactory), OpenCsdFatalError);
  EXPECT_TRUE(errorSink.hasIssue("opencsd-initialization-error"));
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

TEST(CtraceUnitTests, testOpenCsdSessionValidationRejectsInvalidApiResults)
{
  const std::uint32_t object = 1U;
  EXPECT_NO_THROW(OpenCsdSessionValidation::requireObject(&object, "valid object"));
  EXPECT_THROW(OpenCsdSessionValidation::requireObject(nullptr, "missing object"), OpenCsdItmSessionError);

  EXPECT_NO_THROW(OpenCsdSessionValidation::requireSuccess(OCSD_OK, "successful call"));
  const auto message = captureExceptionMessage<OpenCsdItmSessionError>(
      [] { OpenCsdSessionValidation::requireSuccess(OCSD_ERR_MEM, "decoder setup failed"); });
  ASSERT_TRUE(message.has_value());
  EXPECT_NE(message->find("decoder setup failed (OCSD_ERR_MEM)"), std::string::npos);
}

TEST(CtraceUnitTests, testOpenCsdItmSessionRejectsMissingDecoderRegistry)
{
  CollectingOpenCsdElementSink sink;
  OpenCsdPacketCollector collector(sink);
  OpenCsdErrorController errors;
  const auto missingRegistry = []() -> OcsdLibDcdRegister* { return nullptr; };

  EXPECT_THROW((void)OpenCsdItmSession(collector, errors, nullptr), OpenCsdItmSessionError);
  EXPECT_THROW((void)OpenCsdItmSession(collector, errors, missingRegistry), OpenCsdItmSessionError);
}
