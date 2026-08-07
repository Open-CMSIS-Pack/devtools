/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.h"
#include <gtest/gtest.h>
#include "DiagnosticSink.h"
#include "TraceEvent.h"
#include "TraceIssueReporter.h"
#include <cstddef>
#include <string>

TEST(CtraceUnitTests, testDiagnosticCollection)
{
  CollectingDiagnosticSink sink;
  sink.report({
      DiagnosticSink::Severity::Warning,
      "timestamp discontinuity",
      {{"offset", "42"}},
  });

  ASSERT_TRUE(sink.events().size() == 1) << "diagnostic event count mismatch";
  ASSERT_TRUE(sink.events()[0].severity == DiagnosticSink::Severity::Warning) << "diagnostic severity mismatch";
  ASSERT_TRUE(toString(sink.events()[0].severity) == "warning") << "diagnostic severity text mismatch";
  ASSERT_TRUE(sink.failureCount() == 0U) << "warnings must not fail the command";

  sink.report({
      DiagnosticSink::Severity::Error,
      "generator could not create an irrelevant register",
      {},
      DiagnosticSink::Impact::NonFailing,
  });
  ASSERT_TRUE(sink.events().back().impact == DiagnosticSink::Impact::NonFailing && sink.failureCount() == 0U)
      << "non-failing errors must retain error severity without failing the job";

  sink.report({
      DiagnosticSink::Severity::Error,
      "required input is missing",
  });
  ASSERT_TRUE(sink.events().back().impact == DiagnosticSink::Impact::Failing && sink.failureCount() == 1U)
      << "failing errors must fail the job";
}

TEST(CtraceUnitTests, testDiagnosticTextCoversSeverityAndFormatting)
{
  EXPECT_EQ(toString(DiagnosticSink::Severity::Info), "info");
  EXPECT_EQ(toString(DiagnosticSink::Severity::Warning), "warning");
  EXPECT_EQ(toString(DiagnosticSink::Severity::Error), "error");
  EXPECT_EQ(toString(static_cast<DiagnosticSink::Severity>(99)), "unknown");

  StderrDiagnosticSink sink;
  testing::internal::CaptureStderr();
  sink.report({DiagnosticSink::Severity::Info, "message with context", {{"argument", "--all"}}});
  sink.report({DiagnosticSink::Severity::Error, "plain message", {}, DiagnosticSink::Impact::Failing});
  const auto text = testing::internal::GetCapturedStderr();
  EXPECT_NE(text.find("[info] message with context: argument=--all"), std::string::npos);
  EXPECT_NE(text.find("[error] plain message"), std::string::npos);
  EXPECT_EQ(text.find("cli"), std::string::npos);
  EXPECT_EQ(text.find("output/write"), std::string::npos);
}

TEST(CtraceUnitTests, testTraceIssueReporterReportsEveryIssue)
{
  CollectingDiagnosticSink payloadIndependentDiagnostics;
  TraceIssueReporter payloadIndependentReporter(payloadIndependentDiagnostics);
  TraceEvent payloadIndependentError = issuePacket(TraceIssueCode::OpenCsdIncompleteTail);
  payloadIndependentReporter.append(payloadIndependentError);
  ASSERT_TRUE(payloadIndependentDiagnostics.events().size() == 1U)
      << "decoder errors must remain visible independently of payload filtering";
  ASSERT_TRUE(payloadIndependentDiagnostics.failureCount() == 1U)
      << "decoder errors must fail validation independently of payload filtering";

  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);

  reporter.append(overflowPacket(10));
  reporter.append(overflowPacket(20));
  reporter.finish();
  reporter.finish();

  TraceEvent dataLoss = issuePacket(TraceIssueCode::DataLoss, "trace data was lost");
  dataLoss.index = 12U;
  std::get<TraceIssueEvent>(dataLoss.payload).rawBytesConsumed = 3U;
  reporter.append(dataLoss);
  reporter.append(dataLoss);

  TraceEvent warning = issuePacket(TraceIssueCode::OpenCsdDecodeError, "decoder warning", TraceIssueSeverity::Warning);
  reporter.append(warning);

  TraceEvent initializationError = issuePacket(TraceIssueCode::OpenCsdInitializationError, "decoder setup failed");
  reporter.append(initializationError);

  ASSERT_TRUE(diagnostics.events().size() == 5U)
      << "TraceIssueReporter should report every error and warning occurrence";
  ASSERT_TRUE(diagnostics.events()[0].severity == DiagnosticSink::Severity::Warning)
      << "TraceIssueReporter overflow severity mismatch";
  EXPECT_NE(diagnostics.events()[0].message.find("1 more occurred"), std::string::npos);
  ASSERT_TRUE(diagnostics.events()[1].severity == DiagnosticSink::Severity::Error)
      << "TraceIssueReporter data-loss severity mismatch";
  ASSERT_TRUE(diagnostics.events()[1].context.empty()) << "TraceIssueReporter context mismatch";
  ASSERT_TRUE(diagnostics.events()[1].message.find("3 raw bytes") != std::string::npos)
      << "TraceIssueReporter should include the lost byte count";
  ASSERT_TRUE(diagnostics.events()[2].message == diagnostics.events()[1].message)
      << "TraceIssueReporter repeated data-loss message mismatch";
  ASSERT_TRUE(diagnostics.events()[3].severity == DiagnosticSink::Severity::Warning)
      << "TraceIssueReporter should preserve warning severity";
  ASSERT_TRUE(diagnostics.events()[4].context.empty()) << "TraceIssueReporter context mismatch";
  ASSERT_TRUE(diagnostics.failureCount() == 3U) << "TraceIssueReporter should classify decoder errors as failing";
}

TEST(CtraceUnitTests, testTraceIssueReporterFormatsUnknownOverflowTimestamp)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);
  reporter.append(TraceEvent{OverflowTraceEvent{"overflow"}});
  reporter.finish();

  ASSERT_EQ(diagnostics.events().size(), 1U);
  EXPECT_NE(diagnostics.events().front().message.find("unknown cycle timestamp"), std::string::npos);
  EXPECT_EQ(diagnostics.events().front().message.find("cycle timestamp 0"), std::string::npos);
  EXPECT_EQ(diagnostics.events().front().message.find("0 more occurred"), std::string::npos);
}

TEST(CtraceUnitTests, testTraceIssueReporterFormatsEveryErrorKind)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);

  /** @brief Describes one diagnostic formatting test case. */
  struct Case {
    TraceIssueCode code;
    const char* message;
  };
  constexpr Case cases[]{
      {TraceIssueCode::OpenCsdBadPacketSequence, "invalid ITM packet sequence at raw offset 42"},
      {TraceIssueCode::OpenCsdInvalidPacketHeader, "invalid ITM packet header at raw offset 42"},
      {TraceIssueCode::OpenCsdIncompleteTail, "incomplete ITM packet starting at raw offset 42 at end of input"},
      {TraceIssueCode::OpenCsdNoProgress, "OpenCSD made no decode progress at raw offset 42"},
      {TraceIssueCode::OpenCsdWaitTimeout, "OpenCSD remained blocked while flushing pending data"},
      {TraceIssueCode::OpenCsdInitializationError, "OpenCSD initialization failed"},
      {TraceIssueCode::DecodeError, "trace decode error at raw offset 42"},
      {static_cast<TraceIssueCode>(255U), "trace decode error at raw offset 42"},
  };
  for (const auto& testCase : cases) {
    auto event = issuePacket(testCase.code);
    event.index = 42U;
    reporter.append(event);
  }

  auto dataLoss = issuePacket(TraceIssueCode::DataLoss);
  dataLoss.index = 43U;
  reporter.append(dataLoss);

  auto warningDataLoss = issuePacket(TraceIssueCode::DataLoss, "warning loss", TraceIssueSeverity::Warning);
  reporter.append(warningDataLoss);
  reporter.append(issuePacket(TraceIssueCode::DecodeError));

  ASSERT_EQ(diagnostics.events().size(), std::size(cases) + 3U);
  for (std::size_t index = 0U; index < std::size(cases); ++index) {
    EXPECT_EQ(diagnostics.events()[index].message, cases[index].message);
  }
  EXPECT_NE(diagnostics.events()[std::size(cases)].message.find("raw offset 43"), std::string::npos);
  EXPECT_EQ(diagnostics.events()[std::size(cases) + 1U].severity, DiagnosticSink::Severity::Warning);
  EXPECT_EQ(diagnostics.events().back().message, "trace decode error at raw offset 0");
}
