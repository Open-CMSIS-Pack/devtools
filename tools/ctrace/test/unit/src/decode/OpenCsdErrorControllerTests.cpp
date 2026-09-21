/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"

#include <gtest/gtest.h>

#include "OpenCsdErrorController.h"
#include "TraceEvent.h"
#include "common/ocsd_error.h"
#include "common/ocsd_msg_logger.h"
#include "opencsd/ocsd_if_types.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

/** @brief Collects text forwarded by the OpenCSD logger. */
class MessageSink final : public ocsdMsgLogStrOutI {
public:
  /** @brief Appends one logger message. */
  void printOutStr(const std::string& message) override
  {
    m_messages += message;
  }

  /** @brief Returns all collected logger text. */
  const std::string& messages() const
  {
    return m_messages;
  }

private:
  std::string m_messages;
};

/** @brief Creates an OpenCSD error decision for formatting tests. */
static OpenCsdErrorController::Decision makeDecision(ocsd_datapath_resp_t response, ocsd_err_t code,
                                                     std::optional<std::uint64_t> offset = std::nullopt)
{
  OpenCsdErrorController::Decision decision;
  decision.response = response;
  OpenCsdErrorRecord error;
  error.code = code;
  error.index = offset.value_or(0U);
  error.hasIndex = offset.has_value();
  decision.error = std::move(error);
  return decision;
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerClassifiesResponses)
{
  OpenCsdErrorController controller;
  const auto source = controller.RegisterErrorSource("ITM packet processor");

  controller.beginDataPathCall();
  const ocsdError recoverable(OCSD_ERR_SEV_ERROR, OCSD_ERR_BAD_PACKET_SEQ, 123U, 1U, "invalid async sequence");
  controller.LogError(source, &recoverable);
  const auto recovery = controller.decide(OCSD_RESP_FATAL_INVALID_DATA);
  ASSERT_TRUE(recovery.action == OpenCsdErrorController::Action::RecoverStream)
      << "bad packet sequence should be recoverable";
  ASSERT_TRUE(recovery.error.has_value() && recovery.error->hasIndex && recovery.error->index == 123U)
      << "OpenCSD error offset should be preserved";
  ASSERT_TRUE(recovery.errors.size() == 1U && recovery.errors[0].code == OCSD_ERR_BAD_PACKET_SEQ)
      << "OpenCSD decision should retain every error reported for the datapath call";
  ASSERT_TRUE(OpenCsdErrorController::describeSummary(recovery) ==
              "OpenCSD detected an invalid ITM packet sequence at raw offset 123. "
              "0x0013 (OCSD_ERR_BAD_PACKET_SEQ) [Bad packet sequence]; invalid async sequence")
      << "OpenCSD summary should retain the native error and detail beside the raw offset";
  ASSERT_TRUE(OpenCsdErrorController::issueCode(recovery) == TraceIssueCode::OpenCsdBadPacketSequence)
      << "OpenCSD error module should own issue-code selection";
  const auto apiError = OpenCsdErrorController::describeApiError(OCSD_ERR_MEM, "decoder setup failed");
  ASSERT_TRUE(apiError.find("OCSD_ERR_MEM") != std::string::npos &&
              apiError.find("decoder setup failed") != std::string::npos)
      << "OpenCSD error module should own API error formatting";

  controller.beginDataPathCall();
  ASSERT_TRUE(controller.decide(OCSD_RESP_FATAL_INVALID_DATA).action == OpenCsdErrorController::Action::Abort)
      << "fatal response without a classified stream error should abort";

  controller.beginDataPathCall();
  const ocsdError invalidHeader(OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 456U, 1U, "reserved packet header");
  controller.LogError(source, &invalidHeader);
  ASSERT_TRUE(controller.decide(OCSD_RESP_CONT).action == OpenCsdErrorController::Action::RecoverStream)
      << "invalid packet header should be recoverable even when forwarding is suppressed";
  ASSERT_TRUE(controller.decide(OCSD_RESP_FATAL_SYS_ERR).action == OpenCsdErrorController::Action::Abort)
      << "system-fatal response must not be reclassified as a stream recovery";

  controller.beginDataPathCall();
  const ocsdError internalFailure(OCSD_ERR_SEV_ERROR, OCSD_ERR_MEM, "allocation failed");
  controller.LogError(source, &internalFailure);
  ASSERT_TRUE(controller.decide(OCSD_RESP_FATAL_INVALID_DATA).action == OpenCsdErrorController::Action::Abort)
      << "non-stream OpenCSD error should abort on a fatal response";
  ASSERT_TRUE(controller.decide(OCSD_RESP_ERR_CONT).action == OpenCsdErrorController::Action::Continue)
      << "a non-stream OpenCSD error should continue when the datapath explicitly permits it";

  controller.beginDataPathCall();
  controller.LogError(source, &recoverable);
  controller.LogError(source, &internalFailure);
  const auto mixedFailure = controller.decide(OCSD_RESP_FATAL_INVALID_DATA);
  ASSERT_TRUE(mixedFailure.action == OpenCsdErrorController::Action::Abort)
      << "a recoverable stream error must not hide a non-recoverable error from the same call";
  ASSERT_TRUE(mixedFailure.error.has_value() && mixedFailure.error->code == OCSD_ERR_MEM)
      << "fatal mixed-error calls should identify the non-recoverable cause";
  ASSERT_TRUE(mixedFailure.errors.size() == 2U) << "mixed OpenCSD errors should all be retained";

  controller.beginDataPathCall();
  const ocsdError warning(OCSD_ERR_SEV_WARN, OCSD_ERR_BAD_PACKET_SEQ, 789U, 1U, "warning-only packet observation");
  controller.LogError(source, &warning);
  const auto warningDecision = controller.decide(OCSD_RESP_WARN_CONT);
  ASSERT_TRUE(warningDecision.action == OpenCsdErrorController::Action::Continue)
      << "warning-only packet observations should not trigger stream recovery";
  ASSERT_TRUE(warningDecision.error.has_value());
  EXPECT_EQ(warningDecision.error->severity, OCSD_ERR_SEV_WARN);
  EXPECT_NE(OpenCsdErrorController::describeSummary(warningDecision).find("warning-only packet observation"),
            std::string::npos);
  ASSERT_TRUE(controller.decide(OCSD_RESP_FATAL_INVALID_DATA).action == OpenCsdErrorController::Action::Abort)
      << "a warning must not justify recovery from a fatal invalid-data response";

  controller.beginDataPathCall();
  ASSERT_TRUE(controller.decide(OCSD_RESP_WAIT).action == OpenCsdErrorController::Action::Wait)
      << "WAIT response should request a flush";
  ASSERT_TRUE(controller.decide(OCSD_RESP_CONT).action == OpenCsdErrorController::Action::Continue)
      << "CONT response should continue";
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerClassifiesErrorConditions)
{
  EXPECT_FALSE(OpenCsdErrorController::responseReportsError(OCSD_RESP_CONT));
  EXPECT_FALSE(OpenCsdErrorController::responseReportsError(OCSD_RESP_WARN_CONT));
  EXPECT_FALSE(OpenCsdErrorController::responseReportsError(OCSD_RESP_WARN_WAIT));
  EXPECT_TRUE(OpenCsdErrorController::responseReportsError(OCSD_RESP_ERR_CONT));
  EXPECT_TRUE(OpenCsdErrorController::responseReportsError(OCSD_RESP_ERR_WAIT));
  EXPECT_TRUE(OpenCsdErrorController::responseReportsError(OCSD_RESP_FATAL_SYS_ERR));
  EXPECT_TRUE(OpenCsdErrorController::isRecoverableStreamError(OCSD_ERR_BAD_PACKET_SEQ));
  EXPECT_TRUE(OpenCsdErrorController::isRecoverableStreamError(OCSD_ERR_INVALID_PCKT_HDR));
  EXPECT_FALSE(OpenCsdErrorController::isRecoverableStreamError(OCSD_ERR_MEM));
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerMapsDecisionDetails)
{
  OpenCsdErrorController::Decision noError;
  noError.response = OCSD_RESP_CONT;
  EXPECT_EQ(OpenCsdErrorController::errorOffset(noError, 42U), 42U);
  EXPECT_EQ(OpenCsdErrorController::issueCode(noError), TraceIssueCode::OpenCsdDecodeError);

  const auto headerError = makeDecision(OCSD_RESP_ERR_CONT, OCSD_ERR_INVALID_PCKT_HDR, 17U);
  EXPECT_EQ(OpenCsdErrorController::errorOffset(headerError, 42U), 17U);
  EXPECT_EQ(OpenCsdErrorController::issueCode(headerError), TraceIssueCode::OpenCsdInvalidPacketHeader);

  const auto ordinaryError = makeDecision(OCSD_RESP_ERR_CONT, OCSD_ERR_MEM);
  EXPECT_EQ(OpenCsdErrorController::errorOffset(ordinaryError, 42U), 42U);
  EXPECT_EQ(OpenCsdErrorController::issueCode(ordinaryError), TraceIssueCode::OpenCsdDecodeError);
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
    const auto text = OpenCsdErrorController::describeSummary(makeDecision(OCSD_RESP_ERR_CONT, code));
    EXPECT_EQ(text.find(summary), 0U);
    EXPECT_NE(text.find("OCSD_ERR_"), std::string::npos);
  }

  auto offsetError = makeDecision(OCSD_RESP_ERR_CONT, OCSD_ERR_MEM, 0U);
  EXPECT_EQ(OpenCsdErrorController::describeSummary(offsetError),
            "OpenCSD decoder ran out of memory at raw offset 0. "
            "0x0002 (OCSD_ERR_MEM) [Internal memory allocation error.];");
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerSummarizesFatalResponses)
{
  constexpr std::array summaries{
      std::pair{OCSD_RESP_FATAL_NOT_INIT, "OpenCSD decoder is not initialized. OCSD_RESP_FATAL_NOT_INIT:"},
      std::pair{OCSD_RESP_FATAL_INVALID_OP, "OpenCSD rejected a decoder operation. OCSD_RESP_FATAL_INVALID_OP:"},
      std::pair{OCSD_RESP_FATAL_INVALID_PARAM, "OpenCSD rejected a decoder parameter. OCSD_RESP_FATAL_INVALID_PARAM:"},
      std::pair{OCSD_RESP_FATAL_INVALID_DATA, "OpenCSD rejected invalid trace data. OCSD_RESP_FATAL_INVALID_DATA:"},
      std::pair{OCSD_RESP_FATAL_SYS_ERR, "OpenCSD reported a system error. OCSD_RESP_FATAL_SYS_ERR:"},
      std::pair{OCSD_RESP_CONT, "OpenCSD decoder error. OCSD_RESP_CONT:"},
      std::pair{OCSD_RESP_ERR_CONT, "OpenCSD decoder error. OCSD_RESP_ERR_CONT:"},
      std::pair{OCSD_RESP_ERR_WAIT, "OpenCSD decoder error. OCSD_RESP_ERR_WAIT:"},
  };
  for (const auto& [response, summary] : summaries) {
    OpenCsdErrorController::Decision decision;
    decision.response = response;
    EXPECT_EQ(OpenCsdErrorController::describeSummary(decision).find(summary), 0U);
  }
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerSummarizesWarningsWithoutErrorRecords)
{
  OpenCsdErrorController controller;
  const auto continueWarning = controller.decide(OCSD_RESP_WARN_CONT);
  EXPECT_FALSE(continueWarning.error.has_value());
  EXPECT_EQ(continueWarning.action, OpenCsdErrorController::Action::Continue);
  EXPECT_EQ(OpenCsdErrorController::describeSummary(continueWarning),
            "OpenCSD decoder warning. OCSD_RESP_WARN_CONT: Continue processing -> a component logged a warning.");

  const auto waitWarning = controller.decide(OCSD_RESP_WARN_WAIT);
  EXPECT_FALSE(waitWarning.error.has_value());
  EXPECT_EQ(waitWarning.action, OpenCsdErrorController::Action::Wait);
  EXPECT_EQ(OpenCsdErrorController::describeSummary(waitWarning),
            "OpenCSD decoder warning. OCSD_RESP_WARN_WAIT: Pause processing -> a component logged a warning.");
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerNormalizesNativeDiagnosticWhitespace)
{
  OpenCsdErrorController controller;
  const ocsdError error(OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 42U, 1U,
                        " \treserved\r\n packet\t header \v\f at    byte 0xff \r\n");
  controller.LogError(0U, &error);
  const auto decision = controller.decide(OCSD_RESP_ERR_CONT);
  ASSERT_TRUE(decision.error.has_value());
  EXPECT_EQ(decision.error->message, "reserved packet header at byte 0xff");
  EXPECT_EQ(OpenCsdErrorController::describeSummary(decision),
            "OpenCSD detected an invalid ITM packet header at raw offset 42. "
            "0x0014 (OCSD_ERR_INVALID_PCKT_HDR) [Invalid packet header]; reserved packet header at byte 0xff");
  EXPECT_EQ(OpenCsdErrorController::describeApiError(OCSD_ERR_MEM, " \r\n allocation\t  failed \n"),
            "0x0002 (OCSD_ERR_MEM) [Internal memory allocation error.]; allocation failed");
  EXPECT_EQ(OpenCsdErrorController::describeApiError(OCSD_ERR_MEM, " \r\n\t"),
            "0x0002 (OCSD_ERR_MEM) [Internal memory allocation error.];");
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
  EXPECT_FALSE(firstDecision.error->channel.has_value());
  EXPECT_EQ(firstDecision.error->message, "allocation failed");

  const ocsdError traceError(OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 73U, 3U, "bad header");
  controller.LogError(0U, &traceError);
  ASSERT_NE(controller.GetLastIDError(3U), nullptr);
  EXPECT_EQ(controller.GetLastIDError(3U)->getErrorIndex(), 73U);
  EXPECT_EQ(controller.GetLastIDError(4U), nullptr);
}

TEST(CtraceUnitTests, testOpenCsdErrorControllerNormalizesOptionalChannels)
{
  OpenCsdErrorController controller;
  const auto source = controller.RegisterErrorSource("ITM packet processor");

  controller.beginDataPathCall();
  const ocsdError singleError(OCSD_ERR_SEV_ERROR, OCSD_ERR_INVALID_PCKT_HDR, 10U, 0U, "single channel");
  controller.LogError(source, &singleError);
  const auto singleDecision = controller.decide(OCSD_RESP_FATAL_INVALID_DATA);
  ASSERT_EQ(singleDecision.errors.size(), 1U);
  ASSERT_TRUE(singleDecision.errors.front().channel.has_value());
  EXPECT_EQ(*singleDecision.errors.front().channel, 0U);

  controller.beginDataPathCall();
  const ocsdError formattedError(OCSD_ERR_SEV_ERROR, OCSD_ERR_BAD_PACKET_SEQ, 20U, 111U, "formatted channel");
  controller.LogError(source, &formattedError);
  const auto formattedDecision = controller.decide(OCSD_RESP_FATAL_INVALID_DATA);
  ASSERT_EQ(formattedDecision.errors.size(), 1U);
  ASSERT_TRUE(formattedDecision.errors.front().channel.has_value());
  EXPECT_EQ(*formattedDecision.errors.front().channel, 111U);

  controller.beginDataPathCall();
  const ocsdError channelLessError(OCSD_ERR_SEV_ERROR, OCSD_ERR_DFMTR_BAD_FHSYNC, 30U, OCSD_BAD_CS_SRC_ID,
                                   "deformatter error");
  controller.LogError(source, &channelLessError);
  const auto channelLessDecision = controller.decide(OCSD_RESP_FATAL_INVALID_DATA);
  ASSERT_EQ(channelLessDecision.errors.size(), 1U);
  EXPECT_FALSE(channelLessDecision.errors.front().channel.has_value());
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
  EXPECT_NE(sink.messages().find("ITM decoder : configured"), std::string::npos);
  EXPECT_NE(sink.messages().find("unknown : fallback"), std::string::npos);

  logger.setLogOpts(ocsdMsgLogger::OUT_NONE);
  controller.LogMessage(source, OCSD_ERR_SEV_INFO, "muted");
  EXPECT_EQ(sink.messages().find("muted"), std::string::npos);

  controller.setOutputLogger(nullptr);
  controller.LogMessage(source, OCSD_ERR_SEV_ERROR, "disabled");
  EXPECT_EQ(sink.messages().find("disabled"), std::string::npos);
}
