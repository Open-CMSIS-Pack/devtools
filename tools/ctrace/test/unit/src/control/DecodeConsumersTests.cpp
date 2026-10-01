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
#include "TraceRoute.h"

#include <gtest/gtest.h>

#include <cstddef>
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

  const TraceDecodeAbort decodeAbort{16U, "fatal decode error"};
  consumers.finishOutputs(&decodeAbort);
  EXPECT_EQ((std::vector<std::string>{"start", "write", "diagnostic", "write", "diagnostic", "abort"}), calls);
}

TEST(CtraceUnitTests, testDecodeConsumersRetainsSkippedBytesAsNonFailingInfo)
{
  std::vector<std::string> calls;
  std::vector<std::unique_ptr<TraceOutput>> outputs;
  outputs.push_back(std::make_unique<TestTraceOutput>(calls));
  CollectingDiagnosticSink diagnostics;
  DecodeConsumers consumers(std::move(outputs), diagnostics);

  const std::vector<TraceByteSkip> skipped{
      {0U, 1U},
      {16U, 5U, TraceByteSkipReason::NullSourceId, 0U},
      {32U, 3U, TraceByteSkipReason::ReservedSourceId, 127U},
      {48U, 2U, TraceByteSkipReason::UnconfiguredSourceId, 42U},
      {64U, 8U, TraceByteSkipReason::MissingSync, 1U},
  };
  for (const auto& item : skipped) {
    consumers.appendByteSkip(item);
  }
  consumers.append(softwarePacket(1U));
  consumers.finishIssues();
  consumers.finishOutputs();

  EXPECT_EQ((std::vector<std::string>{"start", "write-byte-skip", "write-byte-skip", "write-byte-skip",
                                     "write-byte-skip", "write-byte-skip", "write", "stop"}), calls);
  EXPECT_EQ(consumers.eventCount(), 6U);
  ASSERT_EQ(diagnostics.events().size(), skipped.size());
  for (std::size_t index = 0U; index < skipped.size(); ++index) {
    const auto& diagnostic = diagnostics.events()[index];
    EXPECT_EQ(diagnostic.message, traceByteSkipMessage(skipped[index]));
    EXPECT_EQ(diagnostic.severity, DiagnosticSink::Severity::Info);
    EXPECT_EQ(diagnostic.impact, DiagnosticSink::Impact::NonFailing);
    if (skipped[index].traceId.has_value()) {
      const std::vector<std::pair<std::string, std::string>> expectedContext{
          {"stream", std::to_string(*skipped[index].traceId)},
      };
      EXPECT_EQ(diagnostic.context, expectedContext);
    } else {
      EXPECT_TRUE(diagnostic.context.empty());
    }
  }
  EXPECT_EQ(diagnostics.failureCount(), 0U);
}

TEST(CtraceUnitTests, testDecodeConsumersWarnsForDisabledItmChannelsOnce)
{
  CollectingDiagnosticSink unknownDiagnostics;
  DecodeConsumers unknownConsumers({}, unknownDiagnostics);
  unknownConsumers.append(softwarePacket(3U));
  EXPECT_TRUE(unknownDiagnostics.events().empty());

  CollectingDiagnosticSink diagnostics;
  const TraceRouteIdentity stream2{TraceRouteId{20U}, 2U};
  DecodeConsumers consumers({}, diagnostics, {{TraceRouteId{0U}, 0x00000002U}, {stream2.id, 0x00000004U}});

  auto enabled = softwarePacket(1U);
  consumers.append(enabled);

  auto disabled = softwarePacket(2U);
  consumers.append(disabled);
  consumers.append(disabled);

  auto printfChannel = softwarePacket(0U);
  consumers.append(printfChannel);

  auto invalidChannel = softwarePacket(32U);
  consumers.append(invalidChannel);

  auto streamSpecificDisabled = onRoute(softwarePacket(1U), stream2);
  consumers.append(streamSpecificDisabled);

  auto streamSpecificEnabled = onRoute(softwarePacket(2U), stream2);
  consumers.append(streamSpecificEnabled);

  ASSERT_EQ(2U, diagnostics.events().size());
  for (const auto& event : diagnostics.events()) {
    EXPECT_EQ(DiagnosticSink::Severity::Warning, event.severity);
    EXPECT_EQ(DiagnosticSink::Impact::NonFailing, event.impact);
  }
}

TEST(CtraceUnitTests, testDecodeConsumersTracksEnableWarningsByInternalRouteIdentity)
{
  CollectingDiagnosticSink diagnostics;
  const TraceRouteIdentity noBusA{TraceRouteId{30U}, std::nullopt};
  const TraceRouteIdentity noBusB{TraceRouteId{31U}, std::nullopt};
  DecodeConsumers consumers({}, diagnostics, {{noBusA.id, 0U}, {noBusB.id, 0U}});

  const auto disabledA = onRoute(softwarePacket(3U), noBusA);
  const auto disabledB = onRoute(softwarePacket(3U), noBusB);
  consumers.append(disabledA);
  consumers.append(disabledA);
  consumers.append(disabledB);
  consumers.append(disabledB);

  ASSERT_EQ(diagnostics.events().size(), 2U) << "warning-once state must be independent for distinct no-bus route IDs";
  EXPECT_TRUE(diagnostics.events()[0].context.size() == 2U && diagnostics.events()[1].context.size() == 2U)
      << "an internal route ordinal must not be exposed as public stream context";
  EXPECT_EQ(diagnostics.events()[0].context.front(), (std::pair<std::string, std::string>{"channel", "3"}));
  EXPECT_EQ(diagnostics.events()[1].context.front(), (std::pair<std::string, std::string>{"channel", "3"}));
}
