/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"
#include "TraceOutputTestSupport.h"
#include <gtest/gtest.h>
#include "DecodeConsumers.h"
#include "TraceOutput.h"
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using TraceOutputTestSupport::TestTraceOutput;

/** @brief Records diagnostic delivery in a shared lifecycle call sequence. */
class OrderingDiagnosticSink final : public DiagnosticSink {
public:
  /** @brief Creates a sink that appends to the supplied call sequence. */
  explicit OrderingDiagnosticSink(std::vector<std::string>& calls) : m_calls(calls) {}

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

  TraceEvent warning = issuePacket("opencsd-warning", "decoder warning", TraceIssueSeverity::Warning);
  consumers.append(warning);
  require(calls == std::vector<std::string>({"start", "write", "diagnostic"}),
          "warning packets must reach outputs before issue reporting");
  require(consumers.eventCount() == 1U, "DecodeConsumers warning packet count mismatch");
  require(diagnostics.failureCount() == 0U, "decoder warnings must not fail validation");

  TraceEvent error = warning;
  auto& errorIssue = std::get<TraceIssueEvent>(error.payload);
  errorIssue.severity = TraceIssueSeverity::Error;
  errorIssue.code = "opencsd-error";
  errorIssue.message = "decoder error";
  consumers.append(error);
  require(calls == std::vector<std::string>({"start", "write", "diagnostic", "write", "diagnostic"}),
          "decoder error packets must reach outputs before issue reporting");
  require(consumers.eventCount() == 2U, "DecodeConsumers error packet count mismatch");
  require(diagnostics.failureCount() == 1U, "decoder errors must fail validation even when outputs are configured");

  consumers.abortOutputs();
  require(calls == std::vector<std::string>({"start", "write", "diagnostic", "write", "diagnostic", "abort"}),
          "failed decode cleanup must abort and remove partial outputs");
}

TEST(CtraceUnitTests, testDecodeConsumersWarnsForDisabledItmChannelsOnce)
{
  CollectingDiagnosticSink unknownDiagnostics;
  DecodeConsumers unknownConsumers({}, unknownDiagnostics);
  unknownConsumers.append(softwarePacket(3U));
  require(unknownDiagnostics.events().empty(), "an absent ctrace-setup.itm node must not invent an ITM enable warning");

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

  require(diagnostics.events().size() == 2U, "disabled ITM channels must produce one warning per Trace Bus ID/channel");
  for (const auto& event : diagnostics.events()) {
    require(event.severity == DiagnosticSink::Severity::Warning && event.category == DiagnosticSink::Category::Input &&
                event.code == "itm-channel-not-enabled" && event.impact == DiagnosticSink::Impact::NonFailing,
            "ITM enable mismatch diagnostic classification mismatch");
  }
}
