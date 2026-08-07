/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"
#include "TraceOutputTestSupport.h"
#include "DecodeConsumers.h"
#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceOutput.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

using TraceOutputTestSupport::TestTraceOutput;

/** @brief Records diagnostic delivery in a shared lifecycle call sequence. */
class OrderingDiagnosticSink final : public DiagnosticSink {
public:
  /** @brief Creates a sink that appends to the supplied call sequence. */
  explicit OrderingDiagnosticSink(std::vector<std::string>& calls)
    : m_calls(calls)
  {
  }

protected:
  /** @brief Records one diagnostic delivery. */
  void write(const Event&) override
  {
    m_calls.push_back("diagnostic");
  }

private:
  std::vector<std::string>& m_calls;
};

TEST(CtraceUnitTests, testDecodeConsumersForwardsWarningsAndFailsOnErrorsWithOutputs)
{
  std::vector<std::string> calls;
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<TestTraceOutput>(calls));
  OrderingDiagnosticSink diagnostics(calls);
  DecodeConsumers consumers(std::move(outputs), diagnostics);

  TraceEvent warning = issuePacket(TraceIssueCode::OpenCsdDecodeError, "decoder warning", TraceIssueSeverity::Warning);
  consumers.append(warning);
  EXPECT_EQ((std::vector<std::string>{"start", "write", "diagnostic"}), calls);
  EXPECT_EQ(1U, consumers.eventCount());
  EXPECT_EQ(0U, diagnostics.failureCount());

  TraceEvent error = warning;
  auto& errorIssue = std::get<TraceIssueEvent>(error.payload);
  errorIssue.severity = TraceIssueSeverity::Error;
  errorIssue.code = TraceIssueCode::OpenCsdDecodeError;
  errorIssue.message = "decoder error";
  consumers.append(error);
  EXPECT_EQ((std::vector<std::string>{"start", "write", "diagnostic", "write", "diagnostic"}), calls);
  EXPECT_EQ(2U, consumers.eventCount());
  EXPECT_EQ(1U, diagnostics.failureCount());

  consumers.abortOutputs();
  EXPECT_EQ((std::vector<std::string>{"start", "write", "diagnostic", "write", "diagnostic", "abort"}), calls);
}

TEST(CtraceUnitTests, testDecodeConsumersWarnsForDisabledItmChannelsOnce)
{
  CollectingDiagnosticSink unknownDiagnostics;
  DecodeConsumers unknownConsumers({}, unknownDiagnostics);
  unknownConsumers.append(softwarePacket(3U));
  EXPECT_TRUE(unknownDiagnostics.events().empty());

  CollectingDiagnosticSink diagnostics;
  DecodeConsumers consumers({}, diagnostics, 0x00000002U, {{2U, 0x00000004U}});

  auto enabled = softwarePacket(1U);
  consumers.append(enabled);

  auto disabled = softwarePacket(2U);
  consumers.append(disabled);
  consumers.append(disabled);

  auto printfChannel = softwarePacket(0U);
  consumers.append(printfChannel);

  auto invalidChannel = softwarePacket(32U);
  consumers.append(invalidChannel);

  auto streamSpecificDisabled = softwarePacket(1U);
  streamSpecificDisabled.traceBusId = 2U;
  consumers.append(streamSpecificDisabled);

  auto streamSpecificEnabled = softwarePacket(2U);
  streamSpecificEnabled.traceBusId = 2U;
  consumers.append(streamSpecificEnabled);

  ASSERT_EQ(2U, diagnostics.events().size());
  for (const auto& event : diagnostics.events()) {
    EXPECT_EQ(DiagnosticSink::Severity::Warning, event.severity);
    EXPECT_EQ(DiagnosticSink::Impact::NonFailing, event.impact);
  }
}
