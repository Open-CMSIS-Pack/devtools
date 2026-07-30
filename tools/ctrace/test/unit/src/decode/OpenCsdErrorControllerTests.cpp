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
#include "opencsd/ocsd_if_types.h"

#include <string>

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
