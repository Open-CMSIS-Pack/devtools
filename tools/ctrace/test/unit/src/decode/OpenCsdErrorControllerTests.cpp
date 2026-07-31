/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"

#include <gtest/gtest.h>

#include "OpenCsdErrorController.hpp"
#include "common/ocsd_error.h"
#include "common/ocsd_msg_logger.h"
#include "opencsd/ocsd_if_types.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace {

class MessageSink final : public ocsdMsgLogStrOutI {
public:
  void printOutStr(const std::string& message) override
  {
    messages += message;
  }

  std::string messages;
};

OpenCsdErrorController::Decision makeDecision(ocsd_datapath_resp_t response, ocsd_err_t code, std::uint64_t offset = 0U,
                                              bool hasOffset = false)
{
  OpenCsdErrorController::Decision decision;
  decision.response = response;
  OpenCsdErrorRecord error;
  error.code = code;
  error.index = offset;
  error.hasIndex = hasOffset;
  decision.error = std::move(error);
  return decision;
}

} // namespace

TEST(CtraceUnitTests, testOpenCsdErrorControllerClassifiesResponses)
{
  OpenCsdErrorController controller;
  const auto source = controller.RegisterErrorSource("ITM packet processor");
  require(controller.RegisterErrorSource("ITM packet processor") == source,
          "recreated OpenCSD components should reuse their logger source handle");

  controller.beginDataPathCall();
  const ocsdError recoverable(OCSD_ERR_SEV_ERROR, OCSD_ERR_BAD_PACKET_SEQ, 123U, 1U, "invalid async sequence");
  controller.LogError(source, &recoverable);
  const auto recovery = controller.decide(OCSD_RESP_FATAL_INVALID_DATA);
  require(recovery.action == OpenCsdErrorController::Action::RecoverStream,
          "bad packet sequence should be recoverable");
  require(recovery.error.has_value() && recovery.error->hasIndex && recovery.error->index == 123U,
          "OpenCSD error offset should be preserved");
  require(recovery.errors.size() == 1U && recovery.errors[0].code == OCSD_ERR_BAD_PACKET_SEQ,
          "OpenCSD decision should retain every error reported for the datapath call");
  const auto description = OpenCsdErrorController::describe(recovery);
  require(description.find("OCSD_ERR_BAD_PACKET_SEQ") != std::string::npos &&
              description.find("raw offset 123") != std::string::npos,
          "OpenCSD recovery description should contain code and offset");
  require(OpenCsdErrorController::describeSummary(recovery) ==
              "OpenCSD detected an invalid ITM packet sequence at raw offset 123.",
          "OpenCSD error module should provide a concise user-facing summary");
  require(OpenCsdErrorController::issueCode(recovery) == "opencsd-bad-packet-sequence",
          "OpenCSD error module should own issue-code selection");
  require(OpenCsdErrorController::describeApiError(OCSD_ERR_MEM, "decoder setup failed") ==
              "decoder setup failed (OCSD_ERR_MEM)",
          "OpenCSD error module should own API error formatting");

  controller.beginDataPathCall();
  require(controller.decide(OCSD_RESP_FATAL_INVALID_DATA).action == OpenCsdErrorController::Action::Abort,
          "fatal response without a classified stream error should abort");

  controller.beginDataPathCall();
  const ocsdError invalidHeader(OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 456U, 1U, "reserved packet header");
  controller.LogError(source, &invalidHeader);
  require(controller.decide(OCSD_RESP_CONT).action == OpenCsdErrorController::Action::RecoverStream,
          "invalid packet header should be recoverable even when forwarding is suppressed");
  require(controller.decide(OCSD_RESP_FATAL_SYS_ERR).action == OpenCsdErrorController::Action::Abort,
          "system-fatal response must not be reclassified as a stream recovery");

  controller.beginDataPathCall();
  const ocsdError internalFailure(OCSD_ERR_SEV_ERROR, OCSD_ERR_MEM, "allocation failed");
  controller.LogError(source, &internalFailure);
  require(controller.decide(OCSD_RESP_FATAL_INVALID_DATA).action == OpenCsdErrorController::Action::Abort,
          "non-stream OpenCSD error should abort on a fatal response");
  require(controller.decide(OCSD_RESP_ERR_CONT).action == OpenCsdErrorController::Action::Continue,
          "a non-stream OpenCSD error should continue when the datapath explicitly permits it");

  controller.beginDataPathCall();
  controller.LogError(source, &recoverable);
  controller.LogError(source, &internalFailure);
  const auto mixedFailure = controller.decide(OCSD_RESP_FATAL_INVALID_DATA);
  require(mixedFailure.action == OpenCsdErrorController::Action::Abort,
          "a recoverable stream error must not hide a non-recoverable error from the same call");
  require(mixedFailure.error.has_value() && mixedFailure.error->code == OCSD_ERR_MEM,
          "fatal mixed-error calls should identify the non-recoverable cause");
  require(mixedFailure.errors.size() == 2U, "mixed OpenCSD errors should all be retained");

  controller.beginDataPathCall();
  const ocsdError warning(OCSD_ERR_SEV_WARN, OCSD_ERR_BAD_PACKET_SEQ, 789U, 1U, "warning-only packet observation");
  controller.LogError(source, &warning);
  require(controller.decide(OCSD_RESP_WARN_CONT).action == OpenCsdErrorController::Action::Continue,
          "warning-only packet observations should not trigger stream recovery");
  require(controller.decide(OCSD_RESP_FATAL_INVALID_DATA).action == OpenCsdErrorController::Action::Abort,
          "a warning must not justify recovery from a fatal invalid-data response");

  controller.beginDataPathCall();
  require(controller.decide(OCSD_RESP_WAIT).action == OpenCsdErrorController::Action::Wait,
          "WAIT response should request a flush");
  require(controller.decide(OCSD_RESP_CONT).action == OpenCsdErrorController::Action::Continue,
          "CONT response should continue");
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerNamesResponsesAndErrors)
{
  constexpr std::array responses{
      std::pair{OCSD_RESP_CONT, "OCSD_RESP_CONT"},
      std::pair{OCSD_RESP_WARN_CONT, "OCSD_RESP_WARN_CONT"},
      std::pair{OCSD_RESP_ERR_CONT, "OCSD_RESP_ERR_CONT"},
      std::pair{OCSD_RESP_WAIT, "OCSD_RESP_WAIT"},
      std::pair{OCSD_RESP_WARN_WAIT, "OCSD_RESP_WARN_WAIT"},
      std::pair{OCSD_RESP_ERR_WAIT, "OCSD_RESP_ERR_WAIT"},
      std::pair{OCSD_RESP_FATAL_NOT_INIT, "OCSD_RESP_FATAL_NOT_INIT"},
      std::pair{OCSD_RESP_FATAL_INVALID_OP, "OCSD_RESP_FATAL_INVALID_OP"},
      std::pair{OCSD_RESP_FATAL_INVALID_PARAM, "OCSD_RESP_FATAL_INVALID_PARAM"},
      std::pair{OCSD_RESP_FATAL_INVALID_DATA, "OCSD_RESP_FATAL_INVALID_DATA"},
      std::pair{OCSD_RESP_FATAL_SYS_ERR, "OCSD_RESP_FATAL_SYS_ERR"},
  };
  for (const auto& [response, name] : responses) {
    EXPECT_EQ(OpenCsdErrorController::responseName(response), name);
  }
  EXPECT_EQ(OpenCsdErrorController::responseName(static_cast<ocsd_datapath_resp_t>(-1)), "OCSD_RESP_UNKNOWN(-1)");

  EXPECT_FALSE(OpenCsdErrorController::responseReportsError(OCSD_RESP_CONT));
  EXPECT_TRUE(OpenCsdErrorController::responseReportsError(OCSD_RESP_ERR_CONT));
  EXPECT_TRUE(OpenCsdErrorController::responseReportsError(OCSD_RESP_ERR_WAIT));
  EXPECT_TRUE(OpenCsdErrorController::responseReportsError(OCSD_RESP_FATAL_SYS_ERR));
  EXPECT_TRUE(OpenCsdErrorController::isRecoverableStreamError(OCSD_ERR_BAD_PACKET_SEQ));
  EXPECT_TRUE(OpenCsdErrorController::isRecoverableStreamError(OCSD_ERR_INVALID_PCKT_HDR));
  EXPECT_FALSE(OpenCsdErrorController::isRecoverableStreamError(OCSD_ERR_MEM));

  EXPECT_EQ(OpenCsdErrorController::errorCodeName(OCSD_OK), "OCSD_OK");
  EXPECT_EQ(OpenCsdErrorController::errorCodeName(OCSD_ERR_LAST), "OCSD_ERR_LAST");
  EXPECT_EQ(OpenCsdErrorController::errorCodeName(static_cast<ocsd_err_t>(-1)), "OCSD_ERR_UNKNOWN(-1)");
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerFormatsDecisionDetails)
{
  OpenCsdErrorController::Decision noError;
  noError.response = OCSD_RESP_CONT;
  EXPECT_EQ(OpenCsdErrorController::errorOffset(noError, 42U), 42U);
  EXPECT_EQ(OpenCsdErrorController::issueCode(noError), "opencsd-decode-error");
  EXPECT_EQ(OpenCsdErrorController::describe(noError), "OCSD_RESP_CONT");

  auto headerError = makeDecision(OCSD_RESP_ERR_CONT, OCSD_ERR_INVALID_PCKT_HDR, 17U, true);
  headerError.error->message = "reserved header";
  EXPECT_EQ(OpenCsdErrorController::errorOffset(headerError, 42U), 17U);
  EXPECT_EQ(OpenCsdErrorController::issueCode(headerError), "opencsd-invalid-packet-header");
  EXPECT_EQ(OpenCsdErrorController::describe(headerError),
            "OCSD_RESP_ERR_CONT: OCSD_ERR_INVALID_PCKT_HDR at raw offset 17: reserved header");

  auto ordinaryError = makeDecision(OCSD_RESP_ERR_CONT, OCSD_ERR_MEM);
  EXPECT_EQ(OpenCsdErrorController::errorOffset(ordinaryError, 42U), 42U);
  EXPECT_EQ(OpenCsdErrorController::issueCode(ordinaryError), "opencsd-decode-error");
  EXPECT_EQ(OpenCsdErrorController::describe(ordinaryError), "OCSD_RESP_ERR_CONT: OCSD_ERR_MEM");
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerSummarizesKnownFailures)
{
  constexpr std::array summaries{
      std::pair{OCSD_ERR_BAD_PACKET_SEQ, "OpenCSD detected an invalid ITM packet sequence."},
      std::pair{OCSD_ERR_INVALID_PCKT_HDR, "OpenCSD detected an invalid ITM packet header."},
      std::pair{OCSD_ERR_NOT_INIT, "OpenCSD decoder is not initialized."},
      std::pair{OCSD_ERR_MEM, "OpenCSD decoder ran out of memory."},
      std::pair{OCSD_ERR_INVALID_PARAM_VAL, "OpenCSD rejected a decoder parameter."},
      std::pair{OCSD_ERR_INVALID_PARAM_TYPE, "OpenCSD rejected a decoder parameter."},
      std::pair{OCSD_ERR_FILE_ERROR, "OpenCSD could not read required input data."},
      std::pair{OCSD_ERR_RDR_FILE_NOT_FOUND, "OpenCSD could not read required input data."},
      std::pair{OCSD_ERR_DATA_DECODE_FATAL, "OpenCSD could not decode the trace data."},
      std::pair{OCSD_ERR_INVALID_ID, "OpenCSD decoder error."},
  };
  for (const auto& [code, summary] : summaries) {
    EXPECT_EQ(OpenCsdErrorController::describeSummary(makeDecision(OCSD_RESP_ERR_CONT, code)), summary);
  }

  auto offsetError = makeDecision(OCSD_RESP_ERR_CONT, OCSD_ERR_MEM, 0U, true);
  EXPECT_EQ(OpenCsdErrorController::describeSummary(offsetError), "OpenCSD decoder ran out of memory at raw offset 0.");
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerSummarizesFatalResponses)
{
  constexpr std::array summaries{
      std::pair{OCSD_RESP_FATAL_NOT_INIT, "OpenCSD decoder is not initialized."},
      std::pair{OCSD_RESP_FATAL_INVALID_OP, "OpenCSD rejected a decoder operation."},
      std::pair{OCSD_RESP_FATAL_INVALID_PARAM, "OpenCSD rejected a decoder parameter."},
      std::pair{OCSD_RESP_FATAL_INVALID_DATA, "OpenCSD rejected invalid trace data."},
      std::pair{OCSD_RESP_FATAL_SYS_ERR, "OpenCSD reported a system error."},
      std::pair{OCSD_RESP_CONT, "OpenCSD decoder error."},
  };
  for (const auto& [response, summary] : summaries) {
    OpenCsdErrorController::Decision decision;
    decision.response = response;
    EXPECT_EQ(OpenCsdErrorController::describeSummary(decision), summary);
  }
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerTracksLoggerState)
{
  OpenCsdErrorController controller;
  EXPECT_EQ(controller.GetErrorLogVerbosity(), OCSD_ERR_SEV_INFO);
  EXPECT_EQ(controller.GetLastError(), nullptr);
  EXPECT_EQ(controller.GetLastIDError(0U), nullptr);
  EXPECT_EQ(controller.GetLastIDError(0x70U), nullptr);
  EXPECT_EQ(controller.getOutputLogger(), nullptr);
  controller.LogError(0U, nullptr);
  EXPECT_EQ(controller.GetLastError(), nullptr);

  const ocsdError withoutIndex(OCSD_ERR_SEV_ERROR, OCSD_ERR_MEM, "allocation failed \r\n\t");
  controller.LogError(0U, &withoutIndex);
  EXPECT_EQ(controller.GetLastError()->getErrorCode(), OCSD_ERR_MEM);
  const auto firstDecision = controller.decide(OCSD_RESP_ERR_CONT);
  ASSERT_TRUE(firstDecision.error.has_value());
  EXPECT_FALSE(firstDecision.error->hasIndex);
  EXPECT_EQ(firstDecision.error->index, 0U);
  EXPECT_EQ(firstDecision.error->message, "allocation failed");

  const ocsdError traceError(OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 73U, 3U, "bad header");
  controller.LogError(0U, &traceError);
  ASSERT_NE(controller.GetLastIDError(3U), nullptr);
  EXPECT_EQ(controller.GetLastIDError(3U)->getErrorIndex(), 73U);
  EXPECT_EQ(controller.GetLastIDError(4U), nullptr);
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerForwardsLoggerMessages)
{
  OpenCsdErrorController controller;
  ocsdMsgLogger logger;
  MessageSink sink;
  logger.setStrOutFn(&sink);
  controller.setOutputLogger(&logger);
  EXPECT_EQ(controller.getOutputLogger(), &logger);

  const auto source = controller.RegisterErrorSource("ITM decoder");
  controller.LogMessage(source, OCSD_ERR_SEV_INFO, "configured");
  controller.LogMessage(99U, OCSD_ERR_SEV_ERROR, "fallback");
  controller.LogMessage(source, static_cast<ocsd_err_severity_t>(OCSD_ERR_SEV_INFO + 1), "filtered");
  EXPECT_NE(sink.messages.find("ITM decoder: configured"), std::string::npos);
  EXPECT_NE(sink.messages.find("OpenCSD: fallback"), std::string::npos);
  EXPECT_EQ(sink.messages.find("filtered"), std::string::npos);

  controller.setOutputLogger(nullptr);
  controller.LogMessage(source, OCSD_ERR_SEV_ERROR, "disabled");
  EXPECT_EQ(sink.messages.find("disabled"), std::string::npos);
}
