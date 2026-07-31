/*
 * Copyright (c) 2026 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 * Generated with AI
 */

#include "TestSupport.hpp"
#include <gtest/gtest.h>
#include "TraceIssueReporter.hpp"
#include <optional>
#include <string>

TEST(CtraceUnitTests, testDiagnosticCollection)
{
  CollectingDiagnosticSink sink;
  sink.report({
      DiagnosticSink::Severity::Warning,
      DiagnosticSink::Category::Decode,
      "timestamp",
      "timestamp discontinuity",
      {{"offset", "42"}},
  });

  require(sink.events().size() == 1, "diagnostic event count mismatch");
  require(sink.events()[0].severity == DiagnosticSink::Severity::Warning, "diagnostic severity mismatch");
  require(sink.events()[0].category == DiagnosticSink::Category::Decode, "diagnostic category mismatch");
  require(toString(sink.events()[0].severity) == "warning", "diagnostic severity text mismatch");
  require(toString(sink.events()[0].category) == "decode", "diagnostic category text mismatch");
  require(sink.fatalCount() == 0U, "warnings must not be fatal");

  sink.report({
      DiagnosticSink::Severity::Error,
      DiagnosticSink::Category::Input,
      "generator-error",
      "generator could not create an irrelevant register",
      {},
      std::nullopt,
      DiagnosticSink::Impact::NonFatal,
  });
  require(sink.events().back().impact == DiagnosticSink::Impact::NonFatal && sink.fatalCount() == 0U,
          "non-fatal errors must retain error severity without failing the job");

  sink.report({
      DiagnosticSink::Severity::Error,
      DiagnosticSink::Category::Input,
      "required-input-missing",
      "required input is missing",
  });
  require(sink.events().back().impact == DiagnosticSink::Impact::Fatal && sink.fatalCount() == 1U,
          "fatal errors must fail the job");
}

TEST(CtraceUnitTests, testDiagnosticTextCoversAllValuesAndFormatting)
{
  EXPECT_EQ(toString(DiagnosticSink::Severity::Info), "info");
  EXPECT_EQ(toString(DiagnosticSink::Severity::Warning), "warning");
  EXPECT_EQ(toString(DiagnosticSink::Severity::Error), "error");
  EXPECT_EQ(toString(static_cast<DiagnosticSink::Severity>(99)), "unknown");
  EXPECT_EQ(toString(DiagnosticSink::Category::Cli), "cli");
  EXPECT_EQ(toString(DiagnosticSink::Category::Input), "input");
  EXPECT_EQ(toString(DiagnosticSink::Category::Decode), "decode");
  EXPECT_EQ(toString(DiagnosticSink::Category::Output), "output");
  EXPECT_EQ(toString(static_cast<DiagnosticSink::Category>(99)), "unknown");
  EXPECT_EQ(toString(DiagnosticSink::Impact::NonFatal), "nonfatal");
  EXPECT_EQ(toString(DiagnosticSink::Impact::Fatal), "fatal");
  EXPECT_EQ(toString(static_cast<DiagnosticSink::Impact>(99)), "unknown");

  StderrDiagnosticSink sink;
  testing::internal::CaptureStderr();
  sink.report(
      {DiagnosticSink::Severity::Info, DiagnosticSink::Category::Cli, "", "full message", {{"argument", "--all"}}});
  sink.report({DiagnosticSink::Severity::Error,
               DiagnosticSink::Category::Output,
               "write",
               "long message",
               {},
               "compact",
               DiagnosticSink::Impact::Fatal});
  const auto text = testing::internal::GetCapturedStderr();
  EXPECT_NE(text.find("[info] cli: full message argument=--all"), std::string::npos);
  EXPECT_NE(text.find("[fatal] output/write: compact"), std::string::npos);
}

TEST(CtraceUnitTests, testTraceIssueReporterReportsEveryIssue)
{
  CollectingDiagnosticSink payloadIndependentDiagnostics;
  TraceIssueReporter payloadIndependentReporter(payloadIndependentDiagnostics);
  TraceEvent payloadIndependentError = issuePacket("opencsd-incomplete-tail");
  payloadIndependentReporter.append(payloadIndependentError);
  require(payloadIndependentDiagnostics.events().size() == 1U,
          "decoder errors must remain visible independently of payload filtering");
  require(payloadIndependentDiagnostics.fatalCount() == 1U,
          "decoder errors must fail validation independently of payload filtering");

  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);

  reporter.append(overflowPacket(10));
  reporter.append(overflowPacket(20));
  reporter.finish();
  reporter.finish();

  TraceEvent dataLoss = issuePacket("data-loss", "trace data was lost");
  dataLoss.index = 12U;
  std::get<TraceIssueEvent>(dataLoss.payload).rawBytesConsumed = 3U;
  reporter.append(dataLoss);
  reporter.append(dataLoss);

  TraceEvent warning = issuePacket("opencsd-warning", "decoder warning", TraceIssueSeverity::Warning);
  reporter.append(warning);

  TraceEvent initializationError = issuePacket("opencsd-initialization-error", "decoder setup failed");
  reporter.append(initializationError);

  require(diagnostics.events().size() == 5U, "TraceIssueReporter should report every error and warning occurrence");
  require(diagnostics.events()[0].code == "overflow", "TraceIssueReporter overflow code mismatch");
  require(diagnostics.events()[0].severity == DiagnosticSink::Severity::Warning,
          "TraceIssueReporter overflow severity mismatch");
  require(diagnostics.events()[1].code == "data-loss", "TraceIssueReporter data-loss code mismatch");
  require(diagnostics.events()[1].severity == DiagnosticSink::Severity::Error,
          "TraceIssueReporter data-loss severity mismatch");
  require(diagnostics.events()[1].context.empty(), "TraceIssueReporter context mismatch");
  require(diagnostics.events()[1].compactMessage.has_value() &&
              diagnostics.events()[1].compactMessage->find("3 raw bytes") != std::string::npos,
          "TraceIssueReporter should provide a compact stderr message");
  require(diagnostics.events()[2].code == "data-loss", "TraceIssueReporter repeated data-loss missing");
  require(diagnostics.events()[3].severity == DiagnosticSink::Severity::Warning,
          "TraceIssueReporter should preserve warning severity");
  require(diagnostics.events()[4].context.empty(), "TraceIssueReporter context mismatch");
  require(diagnostics.fatalCount() == 3U, "TraceIssueReporter should classify decoder errors as fatal");
}

TEST(CtraceUnitTests, testTraceIssueReporterFormatsEveryCompactErrorKind)
{
  CollectingDiagnosticSink diagnostics;
  TraceIssueReporter reporter(diagnostics);

  struct Case {
    const char* code;
    const char* compactMessage;
  };
  constexpr Case cases[]{
      {"opencsd-bad-packet-sequence", "invalid ITM packet sequence at raw offset 42"},
      {"opencsd-invalid-packet-header", "invalid ITM packet header at raw offset 42"},
      {"opencsd-incomplete-tail", "incomplete ITM packet starting at raw offset 42 at end of input"},
      {"opencsd-no-progress", "OpenCSD made no decode progress at raw offset 42"},
      {"opencsd-wait-timeout", "OpenCSD remained blocked while flushing pending data"},
      {"opencsd-initialization-error", "OpenCSD initialization failed"},
      {"other-error", "trace decode error at raw offset 42"},
  };
  for (const auto& testCase : cases) {
    auto event = issuePacket(testCase.code);
    event.index = 42U;
    reporter.append(event);
  }

  auto dataLoss = issuePacket("data-loss");
  dataLoss.index = 43U;
  reporter.append(dataLoss);

  auto warningDataLoss = issuePacket("data-loss", "warning loss", TraceIssueSeverity::Warning);
  reporter.append(warningDataLoss);
  reporter.append(issuePacket(""));

  ASSERT_EQ(diagnostics.events().size(), std::size(cases) + 3U);
  for (std::size_t index = 0U; index < std::size(cases); ++index) {
    EXPECT_EQ(diagnostics.events()[index].compactMessage, cases[index].compactMessage);
  }
  EXPECT_NE(diagnostics.events()[std::size(cases)].message.find("Trace data loss detected"), std::string::npos);
  EXPECT_NE(diagnostics.events()[std::size(cases)].compactMessage->find("raw offset 43"), std::string::npos);
  EXPECT_EQ(diagnostics.events()[std::size(cases) + 1U].severity, DiagnosticSink::Severity::Warning);
  EXPECT_EQ(diagnostics.events().back().code, "decode-error");
}
